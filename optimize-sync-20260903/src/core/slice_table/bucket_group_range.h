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

#ifndef BOOST_SS_BUCKET_GROUP_RANGE_H
#define BOOST_SS_BUCKET_GROUP_RANGE_H
#include <cstdint>
#include <vector>

#include "include/bss_err.h"
#include "index/slice_bucket_index.h"

namespace ock {
namespace bss {

class BucketGroupRange;
using BucketGroupRangeRef = std::shared_ptr<BucketGroupRange>;
class BucketGroupRange {
public:
    BucketGroupRange(uint32_t startBucket, uint32_t endBucket, uint32_t totalBucket, uint32_t bucketGroupId)
        : mStartBucket(startBucket), mEndBucket(endBucket), mTotalBucket(totalBucket), mBucketGroupId(bucketGroupId)
    {
    }

    inline BResult Initialize(uint32_t startBucket, uint32_t endBucket, uint32_t totalBucket, uint32_t bucketGroupId)
    {
        mBucketGroupId = bucketGroupId;
        mEndBucket = endBucket;
        mStartBucket = startBucket;
        mTotalBucket = totalBucket;
        return BSS_OK;
    }

    int32_t OverlapCompare(const BucketGroupRangeRef &other) const
    {
        uint32_t mappingStartBucket = 0;
        uint32_t mappingEndBucket = 0;
        if (UNLIKELY(mTotalBucket == 0 || other->mTotalBucket == 0)) {
            LOG_ERROR("mTotalBucket should not be zero.");
            return ERROR_CASE_DIV_BY_ZERO;
        }
        // 1. 判断是扩容还是缩容, 得到oldBucketGroup扩缩容后的范围. mTotalBucket当前固定为1024
        if (mTotalBucket >= other->mTotalBucket) {  // 缩容.
            uint32_t shrink = mTotalBucket / other->mTotalBucket;
            mappingStartBucket = mStartBucket / shrink;
            mappingEndBucket = mEndBucket / shrink;
        } else {  // 扩容.
            uint32_t expand = other->mTotalBucket / mTotalBucket;
            mappingStartBucket = mStartBucket * expand;
            mappingEndBucket = (mEndBucket + 1) * expand - 1;
        }

        // 2. 判断覆盖场景.
        // case1: oldBucketGroup < newBucketGroup.
        if (mappingEndBucket < other->mStartBucket) {
            return OVERLAP_COMPARE_RESULT_CASE_4;
        }
        // case2: oldBucketGroup > newBucketGroup.
        if (mappingStartBucket > other->mEndBucket) {
            return OVERLAP_COMPARE_RESULT_CASE_3;
        }
        // case3: 包含、交集.
        if (mappingEndBucket < other->mEndBucket) {
            return OVERLAP_COMPARE_RESULT_CASE_2;
        }
        // case3: 包含、交集.
        if (mappingEndBucket > other->mEndBucket) {
            return OVERLAP_COMPARE_RESULT_CASE_1;
        }
        // case4: 完全相等.
        return OVERLAP_COMPARE_RESULT_CASE_0;
    }

    inline uint32_t GetStartBucket()
    {
        return mStartBucket;
    }

    inline uint32_t GetEndBucket()
    {
        return mEndBucket;
    }

    inline uint32_t GetTotalBucket()
    {
        return mTotalBucket;
    }

    inline uint32_t GetBucketGroupId()
    {
        return mBucketGroupId;
    }

private:
    uint64_t epoch;
    uint32_t mStartBucket;
    uint32_t mEndBucket;
    uint32_t mTotalBucket;
    uint32_t mBucketGroupId;
};

class SliceBucketGroupRangeGroup {
public:
    SliceBucketGroupRangeGroup(uint32_t totalBucket, const std::vector<BucketGroupRangeRef> &sliceSegments,
                               const SliceBucketIndexRef &sliceBucketIndex)
        : mTotalBucket(totalBucket), mSliceSegments(sliceSegments), mSliceBucketIndex(sliceBucketIndex)
    {
    }

    ~SliceBucketGroupRangeGroup()
    {
        std::vector<BucketGroupRangeRef>().swap(mSliceSegments);
    }

    inline std::string ToString() const
    {
        std::ostringstream oss;
        oss << "totalBucket:" << mTotalBucket << ", bucketGroupRangeSize:" << mSliceSegments.size() << ", info[";
        for (uint32_t idx = 0; idx < mSliceSegments.size(); idx++) {
            oss << "idx:" << idx << "id:" << mSliceSegments[idx]->GetBucketGroupId()
                << " start:" << mSliceSegments[idx]->GetStartBucket() << " end:" << mSliceSegments[idx]->GetEndBucket()
                << ", ";
        }
        oss << "], bucketIndex info[";
        for (uint32_t idx = 0; idx < mSliceBucketIndex->GetSliceChainMappingSize(); idx++) {
            auto chain = mSliceBucketIndex->GetLogicChainedSlice(idx);
            oss << "idx:" << idx << " status:" << static_cast<uint32_t>(chain->GetSliceStatus())
                << " end:" << chain->GetSliceChainTailIndex() << " base:" << chain->GetBaseSliceIndex()
                << " size:" << chain->GetSliceSize() << " fileSize:" << chain->GetFilePageSize() << " ";
        }
        oss << "]";
        return oss.str();
    }

public:
    uint32_t mTotalBucket;
    std::vector<BucketGroupRangeRef> mSliceSegments;
    SliceBucketIndexRef mSliceBucketIndex;
};
using SliceBucketGroupRangeGroupRef = std::shared_ptr<SliceBucketGroupRangeGroup>;

}  // namespace bss
}  // namespace ock
#endif  // BOOST_SS_BUCKET_GROUP_RANGE_H