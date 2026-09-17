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

#ifndef BOOST_SS_SLICE_BUCKET_MAPPER_H
#define BOOST_SS_SLICE_BUCKET_MAPPER_H

#include <cstdint>
#include <memory>

#include "common/bss_def.h"
#include "hash_code_range.h"
#include "include/bss_err.h"

namespace ock {
namespace bss {

class SliceBucketMapper {
public:
    BResult Initialize(uint32_t maxParallelism, uint32_t startKeyGroup, uint32_t endKeyGroup, uint32_t bucketNum);

    BResult Map(uint32_t orderHash, uint32_t &bucketIndex) const;

    // The caller must provide a hash in the initialized task range.
    inline uint32_t MapUnchecked(uint32_t orderHash) const
    {
        const uint64_t offset = static_cast<uint64_t>(orderHash) - mTaskStart;
        return static_cast<uint32_t>(mUseShiftFastPath ? offset >> mBucketShift : offset * mBucketNum / mTaskSpan);
    }

    HashCodeRangeRef GetBucketRange(uint32_t bucketIndex) const;

    HashCodeRangeRef GetTaskRange() const;

    inline uint32_t GetBucketNum() const
    {
        return mBucketNum;
    }

private:
    static constexpr uint64_t HASH_SPACE = 1ULL << 31U;

    uint64_t GroupStart(uint32_t keyGroup) const;

    bool mInitialized = false;
    uint32_t mMaxParallelism = 0;
    uint32_t mStartKeyGroup = 0;
    uint32_t mEndKeyGroup = 0;
    uint32_t mBucketNum = 0;
    uint64_t mBase = 0;
    uint64_t mExtra = 0;
    uint64_t mTaskStart = 0;
    uint64_t mTaskEnd = 0;
    uint64_t mTaskSpan = 0;
    bool mUseShiftFastPath = false;
    uint32_t mBucketShift = 0;
};
using SliceBucketMapperRef = std::shared_ptr<SliceBucketMapper>;

}  // namespace bss
}  // namespace ock

#endif  // BOOST_SS_SLICE_BUCKET_MAPPER_H
