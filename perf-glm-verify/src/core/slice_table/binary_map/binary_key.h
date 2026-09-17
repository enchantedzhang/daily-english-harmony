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

#ifndef PRI_KEY_H
#define PRI_KEY_H

#include "binary/fresh_binary.h"
#include "util/bss_math.h"

namespace ock {
namespace bss {
struct PriKey {
    uint16_t mStateId;
    uint32_t mHashCode;
    uint32_t mKeyDataLength;
    uint8_t *mKeyData;
    bool mWithNamespace = false;
    inline void Parse(const FreshKeyNodePtr &key)
    {
        RETURN_AS_NULLPTR(key);
        mHashCode = key->PriKeyHashCode();  // keyHashcode ^ nsHashcode
        mStateId = key->PriKeyStateId();
        mKeyData = const_cast<uint8_t *>(key->PriKeyData());
        mKeyDataLength = key->PriKeyDataLen();
        mWithNamespace = StateId::HasNameSpace(mStateId);
    }

    inline int32_t CompareData(const PriKey &other) const
    {
        uint32_t minLen = std::min(mKeyDataLength, other.mKeyDataLength);
        int32_t cmp = memcmp(mKeyData, other.mKeyData, minLen);
        if (cmp == 0) {
            return BssMath::IntegerCompare<uint32_t>(mKeyDataLength, other.mKeyDataLength);
        }
        return cmp;
    }

    inline std::string ToString() const
    {
        std::ostringstream oss;
        oss << "StateId: " << mStateId << ", HashCode: " << mHashCode << ", KeyLen: " << mKeyDataLength
            << ", KeyData: [";
        if (UNLIKELY(mKeyData != nullptr)) {
            uint32_t printLen = std::min(NO_10, mKeyDataLength);
            for (uint32_t i = 0; i < printLen; i++) {
                oss << std::hex << std::uppercase << std::setw(NO_2) << std::setfill('0')
                    << static_cast<int>(mKeyData[i]);
            }
        } else {
            oss << "null";
        }
        oss << "]";
        return oss.str();
    }
};

struct SecKey {
    uint32_t mHashCode;
    uint32_t mKeyDataLength;
    uint8_t *mKeyData;

    inline void Parse(FreshKeyNodePtr key)
    {
        RETURN_AS_NULLPTR(key);
        mHashCode = key->SecKeyHashCode();
        mKeyData = const_cast<uint8_t *>(key->SecKeyData());
        mKeyDataLength = key->SecKeyDataLen();
    }

    inline int32_t CompareData(const SecKey &other) const
    {
        uint32_t minLen = std::min(mKeyDataLength, other.mKeyDataLength);
        int32_t cmp = memcmp(mKeyData, other.mKeyData, minLen);
        if (cmp == 0) {
            return BssMath::IntegerCompare<uint32_t>(mKeyDataLength, other.mKeyDataLength);
        }
        return cmp;
    }

    inline std::string ToString() const
    {
        std::ostringstream oss;
        oss << "HashCode: " << mHashCode << ", KeyLen: " << mKeyDataLength << ", KeyData: [";
        if (UNLIKELY(mKeyData != nullptr)) {
            uint32_t printLen = std::min(NO_20, mKeyDataLength);
            for (uint32_t i = 0; i < printLen; i++) {
                oss << std::hex << std::uppercase << std::setw(NO_2) << std::setfill('0')
                    << static_cast<int>(mKeyData[i]);
            }
        } else {
            oss << "null";
        }
        oss << "]";
        return oss.str();
    }
};

struct BinaryKey {
    uint32_t mMixedHashCode;
    uint32_t mKeyHashCode;
    bool hasSecondKey;
    bool mIsValue;
    PriKey mPrimaryKey;
    SecKey mSecondKey;
    inline void Parse(const PriKey &primaryKey, const SecKey &secondKey)
    {
        Parse(primaryKey, true);
        mSecondKey = secondKey;
        hasSecondKey = true;
        mMixedHashCode = mPrimaryKey.mHashCode ^ mSecondKey.mHashCode ^ mPrimaryKey.mStateId;
    }

    inline void Parse(const PriKey &primaryKey, bool isValue)
    {
        hasSecondKey = false;
        mIsValue = isValue;
        mPrimaryKey = primaryKey;
        mMixedHashCode = mPrimaryKey.mHashCode ^ mPrimaryKey.mStateId;
        if (primaryKey.mWithNamespace) {
            mKeyHashCode = mPrimaryKey.mHashCode ^ *reinterpret_cast<uint32_t *>(mPrimaryKey.mKeyData);
        } else {
            mKeyHashCode = mPrimaryKey.mHashCode;
        }
    }

    inline uint32_t Serialize(const ByteBufferRef &buffer, uint32_t offset) const
    {
        buffer->WriteUint16(mPrimaryKey.mStateId, offset);
        if (!hasSecondKey) {
            auto ret = memcpy_s(buffer->Data() + offset + NO_2, buffer->Capacity() - offset - NO_2,
                                mPrimaryKey.mKeyData, mPrimaryKey.mKeyDataLength);
            if (UNLIKELY(ret != EOK)) {
                LOG_ERROR("PrimaryKey serialize failed, ret: " << ret << ".");
                return 0;
            }
            return NO_2 + mPrimaryKey.mKeyDataLength;
        }
        buffer->WriteUint32(mKeyHashCode, offset + NO_2);
        buffer->WriteUint32(mPrimaryKey.mKeyDataLength, offset + NO_6);
        auto ret = memcpy_s(buffer->Data() + offset + NO_10, buffer->Capacity() - offset - NO_10, mPrimaryKey.mKeyData,
                            mPrimaryKey.mKeyDataLength);
        if (UNLIKELY(ret != EOK)) {
            LOG_ERROR("PrimaryKey serialize failed, ret: " << ret << ".");
            return 0;
        }
        ret = memcpy_s(buffer->Data() + offset + NO_10 + mPrimaryKey.mKeyDataLength,
                       buffer->Capacity() - offset - NO_10 - mPrimaryKey.mKeyDataLength, mSecondKey.mKeyData,
                       mSecondKey.mKeyDataLength);
        if (UNLIKELY(ret != EOK)) {
            LOG_ERROR("SecondKey serialize failed, ret: " << ret << ".");
            return 0;
        }
        return NO_10 + mPrimaryKey.mKeyDataLength + mSecondKey.mKeyDataLength;
    }

    inline uint32_t KeyHashCode() const
    {
        return mKeyHashCode;
    }

    inline uint32_t StateId() const
    {
        return mPrimaryKey.mStateId;
    }

    inline uint32_t GetSerializeLength() const
    {
        if (!hasSecondKey) {
            return NO_2 + mPrimaryKey.mKeyDataLength;
        }
        return NO_10 + mPrimaryKey.mKeyDataLength + mSecondKey.mKeyDataLength;
    }

    inline int32_t ComparePrimaryKeyAndSecondKey(const BinaryKey &k2) const
    {
        // compare state id
        int32_t cmpStateId = BssMath::IntegerCompare<uint32_t>(mPrimaryKey.mStateId, k2.mPrimaryKey.mStateId);
        if (cmpStateId != 0) {
            return cmpStateId;
        }
        // compare hash code first.
        int32_t cmp = BssMath::IntegerCompare(mKeyHashCode, k2.mKeyHashCode);
        if (cmp != 0) {
            return cmp;
        }

        // compare primary key
        auto priKey1 = mPrimaryKey;
        auto priKey2 = k2.mPrimaryKey;
        int32_t cmpPriKey = priKey1.CompareData(priKey2);
        if (cmpPriKey != 0) {
            return cmpPriKey;
        }

        // compare second key
        if (UNLIKELY(hasSecondKey != k2.hasSecondKey)) {
            if (hasSecondKey) {
                return 1;
            }
            return -1;
        }
        auto secKey1 = mSecondKey;
        auto secKey2 = k2.mSecondKey;
        return secKey1.CompareData(secKey2);
    }

    inline std::string ToString() const
    {
        std::ostringstream oss;
        oss << "MixedHashCode: " << mMixedHashCode << ", PriKey: " << mPrimaryKey.ToString();
        if (hasSecondKey) {
            oss << ", SecKey: " << mSecondKey.ToString();
        }
        return oss.str();
    }
};
}  // namespace bss
}  // namespace ock

#endif  // PRI_KEY_H
