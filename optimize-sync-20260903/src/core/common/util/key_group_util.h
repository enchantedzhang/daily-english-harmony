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

#ifndef BOOST_SS_KEY_GROUP_UTIL_H
#define BOOST_SS_KEY_GROUP_UTIL_H

#include <cstdint>
#include <memory>

#include "include/bss_err.h"

namespace ock {
namespace bss {

class KeyGroupUtil;
using KeyGroupUtilRef = std::shared_ptr<KeyGroupUtil>;

class KeyGroupUtil {
public:
    static BResult Create(uint32_t maxParallelism, KeyGroupUtilRef &result);
    ~KeyGroupUtil() = default;

    inline uint32_t ComputeKeyGroupForKeyHash(uint32_t orderHash) const
    {
        return mPowerOfTwo ? ComputePowerOfTwoKeyGroup(orderHash) : ComputeGeneralKeyGroup(orderHash);
    }

    inline void SetKeyGroup(uint32_t &rawHash, uint32_t keyGroup) const
    {
        if (mPowerOfTwo) {
            SetPowerOfTwoKeyGroup(rawHash, keyGroup);
        } else {
            SetGeneralKeyGroup(rawHash, keyGroup);
        }
    }

    inline uint32_t ComputePQKeyGroupForKeyHash(uint32_t keyHash) const
    {
        return mMaxParallelism < 129 ? ComputeOneByteKeyGroup(keyHash) : ComputeTwoBytesKeyGroup(keyHash);
    }

    inline void SetPQKeyGroup(uint32_t &keyHashCode, uint32_t keyGroup) const
    {
        if (mMaxParallelism < 129) {
            SetOneByteKeyGroup(keyHashCode, keyGroup);
        } else {
            SetTwoBytesKeyGroup(keyHashCode, keyGroup);
        }
    }

    inline uint32_t GetMaxParallelism() const
    {
        return mMaxParallelism;
    }

private:
    KeyGroupUtil(uint32_t maxParallelism, uint64_t base, uint64_t extra);

    inline void SetPowerOfTwoKeyGroup(uint32_t &rawHash, uint32_t keyGroup) const
    {
        rawHash = (keyGroup << mQuotientBits) | (rawHash >> mGroupBits);
    }
    inline uint32_t ComputePowerOfTwoKeyGroup(uint32_t orderHash) const
    {
        return orderHash >> mQuotientBits;
    }
    inline void SetGeneralKeyGroup(uint32_t &rawHash, uint32_t keyGroup) const
    {
        uint64_t quotient = rawHash / mMaxParallelism;
        uint64_t extraBefore = keyGroup < mExtra ? keyGroup : mExtra;
        uint64_t prefix = static_cast<uint64_t>(keyGroup) * mBase + extraBefore;
        rawHash = static_cast<uint32_t>(prefix + quotient);
    }
    inline uint32_t ComputeGeneralKeyGroup(uint32_t orderHash) const
    {
        uint64_t value = orderHash;
        if (value < mLongSpan) {
            return static_cast<uint32_t>(value / (mBase + 1));
        }
        return static_cast<uint32_t>(mExtra + (value - mLongSpan) / mBase);
    }
    static inline uint32_t ComputeOneByteKeyGroup(uint32_t keyHash)
    {
        return keyHash >> 24;
    }
    static inline uint32_t ComputeTwoBytesKeyGroup(uint32_t keyHash)
    {
        return keyHash >> 16;
    }
    static inline void SetOneByteKeyGroup(uint32_t &keyHashCode, uint32_t keyGroup)
    {
        keyHashCode = (keyHashCode & 0x00FFFFFF) | ((keyGroup & 0xFF) << 24);
    }
    static inline void SetTwoBytesKeyGroup(uint32_t &keyHashCode, uint32_t keyGroup)
    {
        keyHashCode = (keyHashCode & 0x0000FFFF) | ((keyGroup & 0xFFFF) << 16);
    }

    static constexpr uint32_t VALID_HASH_BITS = 31;
    static constexpr uint64_t HASH_SPACE = 1ULL << VALID_HASH_BITS;
    const uint32_t mMaxParallelism;
    const uint32_t mGroupBits;
    const uint32_t mQuotientBits;
    const uint64_t mBase;
    const uint64_t mExtra;
    const uint64_t mLongSpan;
    const bool mPowerOfTwo;
};
}  // namespace bss
}  // namespace ock

#endif  // BOOST_SS_KEY_GROUP_UTIL_H
