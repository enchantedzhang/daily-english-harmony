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

#ifndef BSS_DEV_GROUP_RANGE_H
#define BSS_DEV_GROUP_RANGE_H

#include <algorithm>
#include <cstdint>
#include <string>

#include "common/io/output_view.h"
#include "include/bss_types.h"

namespace ock {
namespace bss {
class GroupRange;
using GroupRangeRef = std::shared_ptr<GroupRange>;

class GroupRange {
public:
    GroupRange(int32_t startGroup, int32_t endGroup, int64_t epoch = 0L)
        : mStartGroup(startGroup), mEndGroup(endGroup), mEpoch(epoch)
    {
    }

    inline GroupRange(const GroupRange &other)
    {
        mStartGroup = other.mStartGroup;
        mEndGroup = other.mEndGroup;
        mEpoch = other.mEpoch;
    }

    GroupRange &operator=(const GroupRange &other)
    {
        if (this != &other) {
            mStartGroup = other.mStartGroup;
            mEndGroup = other.mEndGroup;
            mEpoch = other.mEpoch;
        }
        return *this;
    }

    inline void SetEpoch(int64_t epoch)
    {
        mEpoch = epoch;
    }

    inline int32_t GetStartGroup() const
    {
        return mStartGroup;
    }

    inline int32_t GetEndGroup() const
    {
        return mEndGroup;
    }

    inline int64_t GetEpoch() const
    {
        return mEpoch;
    }

    inline bool ContainsGroup(const int32_t group) const
    {
        return (group >= mStartGroup && group <= mEndGroup);
    }

    inline bool RangeEquals(const GroupRangeRef &that) const
    {
        if (UNLIKELY(that == nullptr)) {
            return false;
        }
        return (mStartGroup == that->mStartGroup && mEndGroup == that->mEndGroup);
    }

    inline bool Overlaps(const GroupRangeRef &target) const
    {
        if (UNLIKELY(target == nullptr)) {
            return false;
        }
        return (mEndGroup >= target->mStartGroup && mStartGroup <= target->mEndGroup);
    }

    inline void Serialize(const OutputViewRef &outputView) const
    {
        if (UNLIKELY(outputView == nullptr)) {
            LOG_ERROR("Unexpected: outputView param is null.");
            return;
        }
        outputView->WriteUint64(static_cast<uint64_t>(mEpoch));
        outputView->WriteInt32(mStartGroup);
        outputView->WriteInt32(mEndGroup);
    }

    inline GroupRange Intersection(const GroupRangeRef &another) const
    {
        if (UNLIKELY(another == nullptr)) {
            return { 0, -1 };
        }
        int32_t startGroup = std::max<int32_t>(mStartGroup, another->mStartGroup);
        int32_t endGroup = std::min<int32_t>(mEndGroup, another->mEndGroup);
        return (startGroup <= endGroup) ? GroupRange(startGroup, endGroup, mEpoch) : GroupRange(0, -1);
    }

    inline bool IsEmpty() const
    {
        return (mStartGroup > mEndGroup);
    }

    inline bool Equals(const GroupRangeRef &other) const
    {
        return (mEpoch == other->mEpoch) && (mStartGroup == other->mStartGroup) && (mEndGroup == other->mEndGroup);
    }

    inline int32_t HashCode() const
    {
        return static_cast<int32_t>(mEpoch + NO_31 * mStartGroup + NO_31 * mEndGroup);
    }

    inline std::string ToString() const
    {
        return "[" + std::to_string(mEpoch) + "," + std::to_string(mStartGroup) + "," + std::to_string(mEndGroup) + "]";
    }

    inline uint32_t GetKeyGroupRangeSize() const
    {
        return (mEndGroup - mStartGroup + 1);
    }

private:
    int32_t mStartGroup = -1;
    int32_t mEndGroup = 0;
    int64_t mEpoch = 0L;
};

}  // namespace bss
}  // namespace ock
#endif  // BSS_DEV_GROUP_RANGE_H