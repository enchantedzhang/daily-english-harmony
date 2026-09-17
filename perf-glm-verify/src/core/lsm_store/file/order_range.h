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

#ifndef BOOST_SS_ORDER_RANGE_H
#define BOOST_SS_ORDER_RANGE_H

#include <memory>

#include "common/bss_log.h"

namespace ock {
namespace bss {
class HashCodeOrderRange;
using HashCodeOrderRangeRef = std::shared_ptr<HashCodeOrderRange>;
class HashCodeOrderRange {
public:
    enum class Type {
        HASHCODE = 1,
        EMPTY_RANGE = 2,
        TEST,
    };

    HashCodeOrderRange() = default;

    HashCodeOrderRange(uint32_t startHashCode, uint32_t endHashCode)
        : mStartHashCode(startHashCode), mEndHashCode(endHashCode), mHasRedundantData(false)
    {
    }

    HashCodeOrderRange(uint32_t startHashCode, uint32_t endHashCode, bool hasRedundantData)
        : mStartHashCode(startHashCode), mEndHashCode(endHashCode), mHasRedundantData(hasRedundantData)
    {
    }

    HashCodeOrderRange &operator=(const HashCodeOrderRange &other)
    {
        if (this != &other) {
            mStartHashCode = other.GetStartHashCode();
            mEndHashCode = other.GetEndHashCode();
            mHasRedundantData = other.HasRedundantData();
        }
        return *this;
    }

    bool operator==(const HashCodeOrderRange &other) const
    {
        return mStartHashCode == other.GetStartHashCode() && mEndHashCode == other.GetEndHashCode() &&
               mHasRedundantData == other.HasRedundantData();
    }

    inline uint32_t GetStartHashCode() const
    {
        return mStartHashCode;
    }

    inline uint32_t GetEndHashCode() const
    {
        return mEndHashCode;
    }

    inline bool Contains(uint32_t hashCode) const
    {
        if (mStartHashCode == hashCode) {
            return true;
        }
        if (mStartHashCode < hashCode) {
            return hashCode <= mEndHashCode;
        }
        return false;
    }

    HashCodeOrderRange Intersection(const HashCodeOrderRangeRef &orderRange) const
    {
        bool hasRedundantData;
        uint32_t start;
        uint32_t end;

        Type type = orderRange->GetType();
        switch (type) {
            case Type::HASHCODE:
                hasRedundantData = mHasRedundantData;
                start = mStartHashCode;
                end = mEndHashCode;
                if (start < orderRange->mStartHashCode) {
                    hasRedundantData = true;
                    start = orderRange->mStartHashCode;
                }
                if (end > orderRange->mEndHashCode) {
                    hasRedundantData = true;
                    end = orderRange->mEndHashCode;
                }
                if (start > end) {
                    return {};
                }
                return { start, end, hasRedundantData };

            // 其它Type类型不支持.
            case Type::EMPTY_RANGE:
            case Type::TEST:
                break;
        }
        return {};
    }

    inline void Serialize(const OutputViewRef &outputView) const
    {
        outputView->WriteUint8(static_cast<uint8_t>(GetType()));
        outputView->WriteUint32(mStartHashCode);
        outputView->WriteUint32(mEndHashCode);
        outputView->WriteBoolean(mHasRedundantData);
    }

    inline Type GetType() const
    {
        return Type::HASHCODE;
    }

    inline bool HasRedundantData() const
    {
        return mHasRedundantData;
    }

    inline uint32_t HashCode() const
    {
        return std::hash<uint32_t>()(mStartHashCode) + std::hash<uint32_t>()(mEndHashCode) +
               std::hash<bool>()(mHasRedundantData);
    }

    std::string ToString() const
    {
        return "HashCodeOrderRange{mStartHashCode=" + std::to_string(mStartHashCode) +
               ", mEndHashCode=" + std::to_string(mEndHashCode) +
               ", hasBeenIntersected=" + std::to_string(mHasRedundantData) + '}';
    }

private:
    uint32_t mStartHashCode = 0;
    uint32_t mEndHashCode = UINT32_MAX;
    bool mHasRedundantData = false;
};

}  // namespace bss
}  // namespace ock
#endif  // BOOST_SS_ORDER_RANGE_H