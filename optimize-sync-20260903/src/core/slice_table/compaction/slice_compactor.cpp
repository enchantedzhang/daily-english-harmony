/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#include "slice_compactor.h"

#include "slice_compaction_policy.h"
#include "slice_compaction_utils.h"

namespace ock {
namespace bss {

BResult SliceCompactor::Init(const ConfigRef &config, const SliceBucketIndexRef &bucketIndex,
                             const MemManagerRef &memManager, const StateFilterManagerRef &stateFilterManager)
{
    mBucketIndex = bucketIndex;
    mMemManager = memManager;
    mInMemoryCompactionThreshold = config->GetInMemoryCompactionThreshold();
    mSliceCompactionPolicy = std::make_shared<SliceCompactionPolicy>();
    mSliceCompactionPolicy->Init(config, mBucketIndex);
    mStateFilterManager = stateFilterManager;
    LOG_INFO("init SliceCompactor success.");
    return BSS_OK;
}

void SliceCompactor::TryCompaction(uint32_t bucketIndex, const CompactCompletedNotify &compactCompletedNotify,
                                   BoostNativeMetricPtr metricPtr)
{
    LogicalSliceChainRef logicalSliceChain = mBucketIndex->GetLogicChainedSlice(bucketIndex);
    if (UNLIKELY(logicalSliceChain == nullptr)) {
        LOG_ERROR("Slice compaction failed, logicalSliceChain is nullptr, bucketIndex: " << bucketIndex);
        return;
    }
    if (UNLIKELY(logicalSliceChain->IsNone())) {
        logicalSliceChain->SetCompactionToNormal();
        LOG_ERROR("Slice compaction failed, logicalSliceChain is none.");
        return;
    }

    // 1. 判断logicSliceChain的状态为COMPACTING.
    if (UNLIKELY(logicalSliceChain->GetSliceStatus() != SliceStatus::COMPACTING)) {
        logicalSliceChain->SetCompactionToNormal();
        LOG_ERROR("LogicalSliceChain is not compacting.");
        return;
    }

    // 2. 选择待compact的slice.
    SliceIndexContextRef sliceIndexContext = std::make_shared<SliceIndexContext>(logicalSliceChain, bucketIndex);
    int32_t tailIndex = logicalSliceChain->GetSliceChainTailIndex();
    if (UNLIKELY(tailIndex == -1)) {
        logicalSliceChain->SetCompactionToNormal();
        return;
    }
    SelectedSliceContextRef selectedSliceContext =
        mSliceCompactionPolicy->SelectCompactionSlice(sliceIndexContext, tailIndex, mInMemoryCompactionThreshold);
    if (UNLIKELY(selectedSliceContext == nullptr)) {
        LOG_DEBUG("Not select compaction slice, so there's not need to do compaction.");
        logicalSliceChain->SetCompactionToNormal();
        return;
    }
    // 内存未达到限制并且选择的slice数量小于压缩门槛, 则不执行compact.
    if (!selectedSliceContext->IsReachMemoryLimit() && selectedSliceContext->Size() < mInMemoryCompactionThreshold) {
        LOG_DEBUG("Is reach memory limit and the number of selected slices is less than the compression threshold, so"
                  << " there's not need to do compaction. InMemoryCompactionThreshold: " << mInMemoryCompactionThreshold
                  << ", selected slice size: " << selectedSliceContext->Size());
        logicalSliceChain->SetCompactionToNormal();
        return;
    }

    // 3. 执行compaction.
    auto ret = DoCompaction(sliceIndexContext, selectedSliceContext, compactCompletedNotify, metricPtr);
    if (UNLIKELY(ret != BSS_OK)) {
        LOG_ERROR("Do compaction failed, ret:" << ret);
        logicalSliceChain->SetCompactionToNormal();
    }
}

BResult SliceCompactor::DoCompaction(const SliceIndexContextRef &sliceIndexContext,
                                     const SelectedSliceContextRef &selectedSliceContext,
                                     const CompactCompletedNotify &compactCompletedNotify,
                                     BoostNativeMetricPtr metricPtr)
{
    LogicalSliceChainRef logicalSliceChain = sliceIndexContext->GetLogicalSliceChain();
    uint32_t bucketIndex = sliceIndexContext->GetSliceIndexSlot();
    if (UNLIKELY(logicalSliceChain != mBucketIndex->GetLogicChainedSlice(bucketIndex))) {
        LOG_ERROR("Current logic slice chain not matched.");
        return BSS_INNER_ERR;
    }

    uint32_t finalOldSliceSize = selectedSliceContext->GetFinalOldSliceSize();
    auto compactDataSlice = std::make_shared<DataSlice>();
    auto ret = DoCompactSlice(selectedSliceContext->GetSliceListReversed(), compactDataSlice, false, bucketIndex,
                              (logicalSliceChain->HasFilePage() || !selectedSliceContext->IsMajor()));
    if (mTombstoneService != nullptr) {
        mTombstoneService->Commit(ret == BSS_OK);
    }
    if (UNLIKELY(ret == BSS_ALLOC_FAIL)) {
        logicalSliceChain->SetCompactionToNormal();
        return BSS_OK;
    }
    if (UNLIKELY(ret != BSS_OK)) {
        LOG_WARN("Do compact slice failed, require internal retry, ret:" << ret << ".");
    } else {
        LOG_DEBUG("Slice Table do compaction success, oldSliceSize:" << finalOldSliceSize);
        if (metricPtr != nullptr && metricPtr->IsSliceMetricEnabled()) {
            metricPtr->AddSliceCompactionSliceCount(selectedSliceContext->Size());
        }
        compactCompletedNotify(logicalSliceChain, bucketIndex,
                               selectedSliceContext->GetInclusiveCompactionStartChainIndex(),
                               selectedSliceContext->GetInclusiveCompactionEndChainIndex(), compactDataSlice,
                               selectedSliceContext->GetInvalidSliceAddressList(), finalOldSliceSize, false);
    }
    return ret;
}

BResult SliceCompactor::DoCompactSlice(const std::vector<DataSliceRef> &canCompactSliceListReversed,
                                       DataSliceRef &compactedDataSlice, bool forceFilter, uint32_t bucketIndex,
                                       bool reserveDeleteMarker)
{
    if (canCompactSliceListReversed.empty()) {
        return BSS_OK;
    }

    // 1. 执行Merge操作.
    std::vector<std::pair<SliceKey, Value>> resultVec;
    uint32_t compactionCount = 0L;
    auto retVal = SliceCompactionUtils::MergeDataSlicesForCompaction(resultVec, compactionCount,
                                                                     canCompactSliceListReversed, mMemManager,
                                                                     forceFilter, mStateFilterManager,
                                                                     mBucketIndex->GetSlotStateFilter(bucketIndex),
                                                                     reserveDeleteMarker, mTombstoneService);
    if (UNLIKELY((retVal != BSS_OK) || (resultVec.empty()))) {
        LOG_WARN("Merge slice data for compaction failed or data empty, ret:" << retVal << ".");
        return retVal;
    }

    // 2. 创建新的slice.
    auto dataSliceImpl = std::make_shared<DataSlice>();
    SliceRef slice = std::make_shared<Slice>();
    SliceCreateMeta meta = { 1, 0, compactionCount };
    auto ret = slice->Initialize(resultVec, meta, mMemManager, false);
    if (UNLIKELY(ret != BSS_OK)) {
        LOG_WARN("Initialize new slice failed, ret:" << ret << ".");
        return ret;
    }
    dataSliceImpl->Init(slice);
    compactedDataSlice = DataSliceRef(dataSliceImpl);
    return BSS_OK;
}

}  // namespace bss
}  // namespace ock
