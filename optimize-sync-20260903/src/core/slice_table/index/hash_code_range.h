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

#ifndef BOOST_SS_HASH_CODE_RANGE_H
#define BOOST_SS_HASH_CODE_RANGE_H

#include <cstdint>
#include <memory>
#include <string>

namespace ock {
namespace bss {

class HashCodeRange {
public:
    HashCodeRange() = default;

    HashCodeRange(uint32_t start, uint32_t end) : mStartHashCode(start), mEndHashCode(end)
    {
    }

    HashCodeRange(const HashCodeRange &other)
    {
        mStartHashCode = other.GetStartHashCode();
        mEndHashCode = other.GetEndHashCode();
    }

    HashCodeRange &operator=(const HashCodeRange &other)
    {
        mStartHashCode = other.GetStartHashCode();
        mEndHashCode = other.GetEndHashCode();
        return *this;
    }

    bool operator==(const HashCodeRange &other) const
    {
        return (mStartHashCode == other.GetStartHashCode()) && (mEndHashCode == other.GetEndHashCode());
    }

    inline uint32_t GetStartHashCode() const
    {
        return mStartHashCode;
    }

    inline uint32_t GetEndHashCode() const
    {
        return mEndHashCode;
    }

    std::string ToString() const
    {
        return "HashCodeRange{mStartHashCode=" + std::to_string(mStartHashCode) +
               ", mEndHashCode=" + std::to_string(mEndHashCode) + '}';
    }

private:
    uint32_t mStartHashCode = 0;
    uint32_t mEndHashCode = 0;
};
using HashCodeRangeRef = std::shared_ptr<HashCodeRange>;

}  // namespace bss
}  // namespace ock
#endif  // BOOST_SS_HASH_CODE_RANGE_H
