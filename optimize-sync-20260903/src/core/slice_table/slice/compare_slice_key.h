
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

#ifndef BOOST_SS_COMPAREBINARYROWKEY_H
#define BOOST_SS_COMPAREBINARYROWKEY_H

#include "binary/slice_binary.h"

namespace ock {
namespace bss {
class CompareSliceKey {
public:
    explicit CompareSliceKey(uint32_t rightShiftBits) : mRightShiftBits(rightShiftBits){};
    bool operator()(const std::pair<SliceKey, Value> pair1, const std::pair<SliceKey, Value> pair2) const
    {
        return BssMath::RotateRight(pair1.first.MixedHashCode(), mRightShiftBits) <
               BssMath::RotateRight(pair2.first.MixedHashCode(), mRightShiftBits);
    }

private:
    uint32_t mRightShiftBits;
};

template <typename TVal> class CompareHashCode {
public:
    explicit CompareHashCode(std::vector<uint32_t> &mixHashCodeVec, uint32_t rightShiftBits)
        : mMixHashCodeVec(mixHashCodeVec), mRightShiftBits(rightShiftBits){};
    bool operator()(const uint32_t index1, uint32_t index2) const
    {
        return BssMath::RotateRight(mMixHashCodeVec[index1], mRightShiftBits) <
               BssMath::RotateRight(mMixHashCodeVec[index2], mRightShiftBits);
    }

private:
    std::vector<uint32_t> &mMixHashCodeVec;
    uint32_t mRightShiftBits;
};
}  // namespace bss
}  // namespace ock
#endif  // BOOST_SS_COMPAREBINARYROWKEY_H
