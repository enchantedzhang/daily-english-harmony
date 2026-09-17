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

#include "slice_bucket_index.h"

#include <memory>

#include "common/bss_log.h"

namespace ock {
namespace bss {
BResult SliceBucketIndex::Initialize(uint32_t totalBucketNum, const ConfigRef &config)
{
    if (UNLIKELY(totalBucketNum == 0 || config == nullptr)) {
        LOG_ERROR("Input param failed.");
        return BSS_ERR;
    }
    mBucketMapper = std::make_shared<SliceBucketMapper>();
    RETURN_NOT_OK(mBucketMapper->Initialize(config->GetMaxParallelism(), config->GetStartGroup(), config->GetEndGroup(),
                                            totalBucketNum));
    mConfig = config;
    mTotalBucketNum = totalBucketNum;
    mMappingTable.reserve(totalBucketNum);
    for (uint32_t i = 0; i < totalBucketNum; ++i) {
        mMappingTable.emplace_back(LogicalSliceChain::mEmptySliceChain);  // 初始化为empty chain.
    }
    mIsRestoreBuild = false;
    return BSS_OK;
}

void SliceBucketIndex::UpdateLogicalSliceChain(uint32_t bucketIndex, const LogicalSliceChainRef &oldLogicalSliceChain,
                                               const LogicalSliceChainRef &newLogicalSliceChain)
{
    if (!CheckIndex(bucketIndex)) {
        LOG_ERROR("Invalid bucketIndex: " << bucketIndex);
        return;
    }

    WriteLocker<ReadWriteLock> lk(&mMappingLocks[bucketIndex % LOCKS_NUM]);
    if (LIKELY(mMappingTable[bucketIndex] == oldLogicalSliceChain)) {
        mMappingTable[bucketIndex] = newLogicalSliceChain;
    } else {
        LOG_ERROR("Update logic chained slice failed, bucketIndex:" << bucketIndex);
    }
}

HashCodeRangeRef SliceBucketIndex::ComputeHashCodeRange(uint32_t startBucket, uint32_t endBucket)
{
    if (UNLIKELY(mIsRestoreBuild)) {
        LOG_ERROR("Unsupported operation exception, restore build flag:" << mIsRestoreBuild);
        return nullptr;
    }

    if (UNLIKELY(startBucket >= mTotalBucketNum)) {
        LOG_ERROR("Start bucket not legal, startBucket:" << startBucket);
        return nullptr;
    }
    if (UNLIKELY(endBucket >= mTotalBucketNum || startBucket > endBucket)) {
        LOG_ERROR("End bucket not legal, endBucket:" << endBucket);
        return nullptr;
    }
    auto startRange = mBucketMapper->GetBucketRange(startBucket);
    auto endRange = mBucketMapper->GetBucketRange(endBucket);
    return std::make_shared<HashCodeRange>(startRange->GetStartHashCode(), endRange->GetEndHashCode());
}

SliceIndexContextRef SliceBucketIndex::InternalGetSliceIndexContext(uint32_t bucketIndex, bool createIfMiss)
{
    LogicalSliceChainRef logicalSliceChain = GetLogicChainedSlice(bucketIndex);
    if (UNLIKELY(logicalSliceChain == nullptr)) {
        LOG_ERROR("Get logic chained slice is nullptr, bucketIndex:" << bucketIndex);
        return nullptr;
    }
    if (UNLIKELY(createIfMiss && logicalSliceChain->IsNone())) {
        LogicalSliceChainRef newLogicalSliceChain = CreateLogicalChainedSlice();
        if (UNLIKELY(newLogicalSliceChain == nullptr)) {
            LOG_ERROR("Create logical chained slice failed, bucketIndex:" << bucketIndex);
            return nullptr;
        }
        WriteLocker<ReadWriteLock> lk(&mMappingLocks[bucketIndex % LOCKS_NUM]);
        mMappingTable[bucketIndex] = newLogicalSliceChain;
        auto ret = std::make_shared<SliceIndexContext>(newLogicalSliceChain, bucketIndex);
        return ret;
    }
    return std::make_shared<SliceIndexContext>(logicalSliceChain, bucketIndex);
}

// 监控指标不需要某一时刻的绝对精准，不必加锁
uint64_t SliceBucketIndex::GetTotalSliceChainSize()
{
    uint32_t cap = GetIndexCapacity();
    uint64_t totalSize = 0;
    for (uint32_t i = 0; i < cap; i++) {
        totalSize += mMappingTable[i]->GetSliceSize();
    }
    return totalSize;
}

uint64_t SliceBucketIndex::GetAvgSliceChainSize()
{
    uint64_t totalSliceChainSize = GetIndexCapacity();
    return totalSliceChainSize == 0 ? 0 : GetTotalSliceChainSize() / totalSliceChainSize;
}

uint64_t SliceBucketIndex::GetAvgSliceSize()
{
    uint32_t cap = GetIndexCapacity();
    uint64_t totalSize = 0;
    uint64_t totalCount = 0;
    for (uint32_t i = 0; i < cap; i++) {
        totalSize += mMappingTable[i]->GetSliceSize();
        totalCount += mMappingTable[i]->GetCurrentSliceChainLen();
    }
    return totalCount == 0 ? 0 : totalSize / totalCount;
}

}  // namespace bss
}  // namespace ock
