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

#include "replace_logical_slice_task.h"

namespace ock {
namespace bss {
BResult ReplaceLogicalSlice::SyncReplaceChainAndSlice(LogicalSliceChainRef &logicalSliceChain, uint32_t sliceIndexSlot,
                                                      uint32_t compactionStartChainIndex,
                                                      uint32_t compactionEndChainIndex,
                                                      DataSliceRef &compactedDataSLice,
                                                      std::vector<SliceAddressRef> &invalidSliceAddressList)
{
    if (logicalSliceChain != mBucketIndex->GetLogicChainedSlice(sliceIndexSlot)) {
        LOG_ERROR("Logical slice chain not matched, sliceIndexSlot:" << sliceIndexSlot);
        return BSS_INVALID_PARAM;
    }

    if (logicalSliceChain->GetBaseSliceIndex() > compactionStartChainIndex) {
        LOG_INFO("slice has been flushed, compactionStartChainIndex:"
                 << compactionStartChainIndex << ", baseSliceIndex:" << logicalSliceChain->GetBaseSliceIndex());
        return BSS_OK;
    }

    auto baseSliceAddress =
        logicalSliceChain->GetSliceAddress(static_cast<int32_t>(logicalSliceChain->GetBaseSliceIndex()));
    if (baseSliceAddress == nullptr || baseSliceAddress->IsFlush()) {
        LOG_ERROR("Base slice address is null or slice is already flush, sliceIndexSlot:"
                  << sliceIndexSlot << mBucketIndex->ToString(sliceIndexSlot) << logicalSliceChain->ToString());
        return BSS_INNER_ERR;
    }

    LogicalSliceChainRef compactedLogicalSliceChain = mBucketIndex->CreateLogicalChainedSlice();
    if (compactedLogicalSliceChain == nullptr) {
        LOG_ERROR("Create compacted logic slice chain failed.");
        return BSS_INNER_ERR;
    }

    uint32_t evictedPageCount = 0;
    mBucketIndex->LockWrite(sliceIndexSlot);  // 互斥evict流程去更改bucketIndex上的logicChain.
    // 1. 重新插进去compaction区间之前的数据, 如果为evicted的sliceAddress不放到新sliceChain中.
    for (uint32_t i = 0; i < compactionStartChainIndex; i++) {
        SliceAddressRef sliceAddress = logicalSliceChain->GetSliceAddress(i);
        if (UNLIKELY(sliceAddress == nullptr)) {
            LOG_ERROR("Slice address is nullptr.");
            mBucketIndex->Unlock(sliceIndexSlot);
            return BSS_INVALID_PARAM;
        }
        if (sliceAddress->IsEvicted()) {
            evictedPageCount++;
        } else if (sliceAddress->IsTriggerFlush()) {
            while (!logicalSliceChain->HasFilePage()) {
                usleep(NO_20);
            }
            compactedLogicalSliceChain->InsertSlice(sliceAddress);
        } else {
            compactedLogicalSliceChain->InsertSlice(sliceAddress);
        }
    }
    if (logicalSliceChain->HasFilePage()) {
        BucketGroupRef bucketGroup = GetBucketGroup();
        RETURN_ERROR_AS_NULLPTR(bucketGroup);
        compactedLogicalSliceChain->InsertFilePageIfEmpty(std::make_shared<FilePage>(bucketGroup->GetLsmStore()));
    }

    // 2. 插入compacted的sliceAddress.
    SliceAddressRef compactedSliceAddress = compactedLogicalSliceChain->CreateSlice(compactedDataSLice,
                                                                                    mSliceTable->GetTick());
    if (UNLIKELY(compactedSliceAddress == nullptr)) {
        LOG_ERROR("Create compacted slice address failed.");
        mBucketIndex->Unlock(sliceIndexSlot);
        return BSS_INNER_ERR;
    }
    compactedSliceAddress->ReduceRequestCount(invalidSliceAddressList);

    // 3. 重新插进去compaction区间之后的数据.
    for (uint32_t i = compactionEndChainIndex + 1; i < logicalSliceChain->GetCurrentSliceChainLen(); i++) {
        compactedLogicalSliceChain->InsertSlice(logicalSliceChain->GetSliceAddress(i));
    }

    // 4. 计算新sliceChain的baseIndex.
    uint32_t baseSliceIndex = logicalSliceChain->GetBaseSliceIndex() <= evictedPageCount ?
                                  0 :
                                  logicalSliceChain->GetBaseSliceIndex() - evictedPageCount;
    if (compactedDataSLice->GetSize() > mConfig->GetSliceMaxSize()) {
        compactedLogicalSliceChain->SetBaseSliceIndex(baseSliceIndex + 1);
    } else {
        compactedLogicalSliceChain->SetBaseSliceIndex(baseSliceIndex);
    }

    // 5. 新sliceChain替换旧sliceChain.
    auto ret = SyncUpdateChain(logicalSliceChain, compactedLogicalSliceChain, sliceIndexSlot);
    mBucketIndex->Unlock(sliceIndexSlot);
    return ret;
}

BResult ReplaceLogicalSlice::SyncUpdateChain(LogicalSliceChainRef &oldLogicalSliceChain,
                                             LogicalSliceChainRef &newLogicalSliceChain, uint32_t sliceIndexSlot)
{
    // 替换sliceChain
    mBucketIndex->UpdateLogicalSliceChain(sliceIndexSlot, oldLogicalSliceChain, newLogicalSliceChain);
    return BSS_OK;
}

void ReplaceLogicalSliceTask::Run()
{
    BResult ret = mReplaceLogicalSlice->SyncReplaceChainAndSlice(mLogicalSliceChain, mSliceIndexSlot,
                                                                 mCompactionStartChainIndex, mCompactionEndChainIndex,
                                                                 mCompactedDataSlice, mInvalidSliceAddressList);
    if (UNLIKELY(ret != BSS_OK)) {
        LOG_ERROR("Replace slice chain failed, ret: " << ret << ", index:" << mSliceIndexSlot);
        mLogicalSliceChain->SetCompactionToNormal();
    } else {
        Clean();
    }
}

void ReplaceLogicalSliceTask::Clean()
{
    mLogicalSliceChain->CompareAndSetStatus(SliceStatus::COMPACTING, SliceStatus::NORMAL);
}

}  // namespace bss
}  // namespace ock