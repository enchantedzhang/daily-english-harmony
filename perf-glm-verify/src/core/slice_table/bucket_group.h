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

#ifndef BOOST_SS_BUCKETGROUP_H
#define BOOST_SS_BUCKETGROUP_H

#include <cstdint>

#include "bucket_group_range.h"
#include "include/bss_err.h"
#include "lsm_store/file/file_store_impl.h"
#include "slice_table/index/slice_bucket_index.h"
#include "slice_table/index/slice_index_context.h"

namespace ock {
namespace bss {

class SliceChainIterator : public Iterator<std::shared_ptr<SliceIndexContext>> {
public:
    SliceChainIterator(SliceBucketIndexRef sliceBucketIndex, BucketGroupRangeRef bucketGroupRange);

    bool HasNext() override;

    SliceIndexContextRef Next() override;

private:
    uint32_t mCurIndex;
    SliceBucketIndexRef mSliceBucketIndex;
    BucketGroupRangeRef mBucketGroupRange;
};

class BucketGroup {
public:
    ~BucketGroup() = default;

    inline uint32_t GetBucketGroupId() const
    {
        return mBucketGroupId;
    }

    inline uint32_t GetSliceChainCount() const
    {
        return mBucketGroupRange->GetEndBucket() - mBucketGroupRange->GetStartBucket() + NO_1;
    }

    inline SliceBucketIndexRef GetSliceBucketIndex()
    {
        return mSliceBucketIndex;
    }

    inline BucketGroupRangeRef GetBucketGroupRange()
    {
        return mBucketGroupRange;
    }

    inline void RegisterMetric(BoostNativeMetricPtr metricPtr)
    {
        mLsmStore->RegisterMetric(metricPtr);
    }

    BResult Initialize(uint32_t bucketGroupId, const LsmStoreRef &lsmStore, const SliceBucketIndexRef &sliceIndex,
                       uint32_t startBucket, uint32_t endBucket);

    inline LsmStoreRef &GetLsmStore()
    {
        return mLsmStore;
    }

    void SnapshotMeta(const FileOutputViewRef &localOutputView);

    static BResult Restore(const FileInputViewRef &reader, uint32_t totalBucketNum,
                           BucketGroupRangeRef &bucketGroupRange);

private:
    uint32_t mBucketGroupId = 0;
    SliceBucketIndexRef mSliceBucketIndex;
    BucketGroupRangeRef mBucketGroupRange;
    LsmStoreRef mLsmStore;
};
using BucketGroupRef = std::shared_ptr<BucketGroup>;

}  // namespace bss
}  // namespace ock

#endif  // BOOST_SS_BUCKETGROUP_H
