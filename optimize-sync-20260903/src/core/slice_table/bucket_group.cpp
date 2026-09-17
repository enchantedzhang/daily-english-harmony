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

#include "bucket_group.h"

#include "bucket_group_range.h"
#include "common/bss_log.h"

namespace ock {
namespace bss {

BResult BucketGroup::Initialize(uint32_t bucketGroupId, const LsmStoreRef &lsmStore,
                                const SliceBucketIndexRef &sliceIndex, uint32_t startBucket, uint32_t endBucket)
{
    if (UNLIKELY(sliceIndex == nullptr || lsmStore == nullptr)) {
        LOG_ERROR("Invalid input param.");
        return BSS_ERR;
    }
    mLsmStore = lsmStore;
    mSliceBucketIndex = sliceIndex;
    mBucketGroupId = bucketGroupId;
    mBucketGroupRange = std::make_shared<BucketGroupRange>(startBucket, endBucket, sliceIndex->GetIndexCapacity(),
                                                           bucketGroupId);
    return BSS_OK;
}

void BucketGroup::SnapshotMeta(const FileOutputViewRef &localOutputView)
{
    localOutputView->WriteUint32(mBucketGroupRange->GetStartBucket());
    localOutputView->WriteUint32(mBucketGroupRange->GetEndBucket());
    localOutputView->WriteUint32(mBucketGroupRange->GetBucketGroupId());
}

BResult BucketGroup::Restore(const FileInputViewRef &reader, uint32_t totalBucketNum,
                             BucketGroupRangeRef &bucketGroupRange)
{
    RETURN_INVALID_PARAM_AS_NULLPTR(reader);
    uint32_t startBucket = 0;
    RETURN_NOT_OK_AS_READ_ERROR(reader->Read(startBucket));
    uint32_t endBucket = 0;
    RETURN_NOT_OK_AS_READ_ERROR(reader->Read(endBucket));
    uint32_t bucketGroupId = 0;
    RETURN_NOT_OK_AS_READ_ERROR(reader->Read(bucketGroupId));
    bucketGroupRange = std::make_shared<BucketGroupRange>(startBucket, endBucket, totalBucketNum, bucketGroupId);
    return BSS_OK;
}

SliceChainIterator::SliceChainIterator(SliceBucketIndexRef sliceBucketIndex, BucketGroupRangeRef bucketGroupRange)
{
    mSliceBucketIndex = sliceBucketIndex;
    mBucketGroupRange = bucketGroupRange;
    mCurIndex = bucketGroupRange->GetStartBucket();
}

bool SliceChainIterator::HasNext()
{
    return mCurIndex <= mBucketGroupRange->GetEndBucket();
}

SliceIndexContextRef SliceChainIterator::Next()
{
    SliceIndexContextRef sliceIndexContext = mSliceBucketIndex->GetSliceIndexContext(mCurIndex);
    mCurIndex++;
    return sliceIndexContext;
}
}  // namespace bss
}  // namespace ock