/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of the Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#include "slice_bucket_mapper.h"

#include <algorithm>

#include "common/bss_log.h"
#include "include/bss_types.h"

namespace ock {
namespace bss {

uint64_t SliceBucketMapper::GroupStart(uint32_t keyGroup) const
{
    return static_cast<uint64_t>(keyGroup) * mBase + std::min<uint64_t>(keyGroup, mExtra);
}

BResult SliceBucketMapper::Initialize(uint32_t maxParallelism, uint32_t startKeyGroup, uint32_t endKeyGroup,
                                      uint32_t bucketNum)
{
    if (UNLIKELY(mInitialized)) {
        LOG_ERROR("Initialize slice bucket mapper repeatedly.");
        return BSS_ALREADY_DONE;
    }
    if (UNLIKELY(maxParallelism == 0 || maxParallelism > MAX_PARALLELISM || startKeyGroup > endKeyGroup ||
                 endKeyGroup >= maxParallelism || bucketNum == 0)) {
        LOG_ERROR("Initialize slice bucket mapper failed, maxParallelism:" << maxParallelism << ", keyGroupRange:"
                                                                           << startKeyGroup << "-" << endKeyGroup
                                                                           << ", bucketNum:" << bucketNum);
        return BSS_INVALID_PARAM;
    }

    mMaxParallelism = maxParallelism;
    mStartKeyGroup = startKeyGroup;
    mEndKeyGroup = endKeyGroup;
    mBucketNum = bucketNum;
    mBase = HASH_SPACE / maxParallelism;
    mExtra = HASH_SPACE % maxParallelism;
    mTaskStart = GroupStart(startKeyGroup);
    mTaskEnd = GroupStart(endKeyGroup + 1) - 1;
    mTaskSpan = mTaskEnd - mTaskStart + 1;
    if (UNLIKELY(mTaskSpan < bucketNum)) {
        LOG_ERROR("Slice bucket mapper task hash span is smaller than bucket count, taskSpan:"
                  << mTaskSpan << ", bucketNum:" << bucketNum);
        return BSS_NOT_SUPPORTED;
    }
    const uint64_t bucketSpan = mTaskSpan / mBucketNum;
    mUseShiftFastPath = mTaskSpan % mBucketNum == 0 && (bucketSpan & (bucketSpan - 1)) == 0;
    mBucketShift = 0;
    if (mUseShiftFastPath) {
        uint64_t remainingSpan = bucketSpan;
        while (remainingSpan > 1) {
            remainingSpan >>= 1U;
            ++mBucketShift;
        }
    }
    mInitialized = true;
    return BSS_OK;
}

BResult SliceBucketMapper::Map(uint32_t orderHash, uint32_t &bucketIndex) const
{
    if (UNLIKELY(!mInitialized || orderHash < mTaskStart || orderHash > mTaskEnd)) {
        LOG_ERROR("Map order hash to slice bucket failed, initialized:"
                  << mInitialized << ", orderHash:" << orderHash << ", taskRange:" << mTaskStart << "-" << mTaskEnd);
        return BSS_INVALID_PARAM;
    }
    bucketIndex = MapUnchecked(orderHash);
    return BSS_OK;
}

HashCodeRangeRef SliceBucketMapper::GetBucketRange(uint32_t bucketIndex) const
{
    if (UNLIKELY(!mInitialized || mBucketNum == 0 || bucketIndex >= mBucketNum)) {
        LOG_ERROR("Get slice bucket range failed, initialized:" << mInitialized << ", bucketIndex:" << bucketIndex
                                                                << ", bucketNum:" << mBucketNum);
        return nullptr;
    }
    const uint64_t startNumerator = static_cast<uint64_t>(bucketIndex) * mTaskSpan;
    const uint64_t endNumerator = (static_cast<uint64_t>(bucketIndex) + 1) * mTaskSpan;
    const uint64_t start = mTaskStart + (startNumerator + mBucketNum - 1) / mBucketNum;
    const uint64_t end = mTaskStart + (endNumerator + mBucketNum - 1) / mBucketNum - 1;
    return std::make_shared<HashCodeRange>(static_cast<uint32_t>(start), static_cast<uint32_t>(end));
}

HashCodeRangeRef SliceBucketMapper::GetTaskRange() const
{
    if (UNLIKELY(!mInitialized)) {
        LOG_ERROR("Get task hash range before slice bucket mapper initialization.");
        return nullptr;
    }
    return std::make_shared<HashCodeRange>(static_cast<uint32_t>(mTaskStart), static_cast<uint32_t>(mTaskEnd));
}

}  // namespace bss
}  // namespace ock
