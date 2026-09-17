/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */
#pragma once
#include <algorithm>
#include <cstdint>
#include <limits>

namespace ock {
namespace bss {
template <typename T> struct ClosedInterval {
    T lower = std::numeric_limits<T>::max();
    T high = std::numeric_limits<T>::min();

    void Update(T val)
    {
        lower = std::min(lower, val);
        high = std::max(high, val);
    }

    bool Contains(T val)
    {
        return val >= lower && val <= high;
    }

    bool operator==(const ClosedInterval<T> &rhs) const
    {
        return lower == rhs.lower && high == rhs.high;
    }
};

using StateIdInterval = ClosedInterval<uint16_t>;
}  // namespace bss
}  // namespace ock
