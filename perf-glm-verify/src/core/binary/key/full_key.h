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

#ifndef BOOST_SS_FULL_KEY_H
#define BOOST_SS_FULL_KEY_H

#include "binary/value/value_type.h"
#include "key.h"

namespace ock {
namespace bss {
struct FullKey;
using FullKeyRef = std::shared_ptr<FullKey>;
struct FullKey : public Key {
public:
    FullKey() = default;
    /**
     * Construct full key.
     * @param priKey primary key.
     * @param secKey secondary key.
     * @param seqId value seqId.
     * @param valueType value type.
     * @param buffer buffer object, store key data for key lifecycle management.
     */
    FullKey(const PriKeyNode &priKey, const SecKeyNode &secKey, uint64_t seqId, ValueType valueType,
            const BufferRef &buffer)
    {
        Init(priKey, secKey, seqId, valueType, buffer);
    }

    /**
     * Initialize full key for dual key.
     * @param priKey primary key.
     * @param secKey secondary key.
     * @param seqId value seqId.
     * @param valueType value type.
     * @param buffer buffer object, store key data for key lifecycle management.
     */
    inline void Init(const PriKeyNode &priKey, const SecKeyNode &secKey, uint64_t seqId, ValueType valueType,
                     const BufferRef &buffer)
    {
        Key::Init(priKey, secKey, buffer);
        mSeqId = seqId;
        mValueType = valueType;
    }

    /**
     * Initialize full key for single key.
     * @param priKey primary key.
     * @param seqId value seqId.
     * @param valueType value type.
     * @param buffer buffer object, store key data for key lifecycle management.
     */
    inline void Init(const PriKeyNode &priKey, uint64_t seqId, ValueType valueType, const BufferRef &buffer)
    {
        Key::Init(priKey, buffer);
        mSeqId = seqId;
        mValueType = valueType;
    }

    /**
     * Get seqId.
     * @return seqId
     */
    inline uint64_t SeqId() const
    {
        return mSeqId;
    }

    /**
     * Get value type.
     * @return value type.
     */
    inline ValueType ValueType() const
    {
        return mValueType;
    }

    /**
     * Compare key and value seqId.
     * @param otherKey other key
     * @param otherSeqId other value seqId
     * @return If the current key is less than other, return -1. If they are equal, return 0.
     * If the current key is greater than other, return 1.
     */
    inline int32_t CompareFullKey(const Key &key, uint64_t otherSeqId, bool reverseOrder = false)
    {
        return CompareFullKey(*this, mSeqId, key, otherSeqId, reverseOrder);
    }

    inline bool EqualsFullKey(const FullKeyRef &otherKey)
    {
        if (otherKey == nullptr) {
            return false;
        }
        return CompareFullKey(*this, mSeqId, *otherKey, otherKey->mSeqId) == 0 && mValueType == otherKey->ValueType();
    }
    /**
     * Just compare key.
     * @param otherKey other key
     * @return If the current key is less than other, return -1. If they are equal, return 0.
     * If the current key is greater than other, return 1.
     */
    inline int32_t CompareKey(const Key &otherKey)
    {
        return Key::Compare(otherKey);
    }

    /**
     * Compare with other prefix key, prefix key just have primary key, so we just compare it.
     * Since the LSM STORE stores only one copy of the same PrimaryKey, and stores the StateId
     * together with the SecondaryKey,
     * it first compares the KeyNode part of the PrimaryKey, and then compares the StateId.
     * @param otherPrefixKey other prefix key.
     * @return If the current key is less than other, return -1. If they are equal, return 0.
     * If the current key is greater than other, return 1.
     */
    inline int32_t ComparePrefixKey(const Key &otherPrefixKey)
    {
        return mPriKey.CompareStateIdLast(otherPrefixKey.PriKey());
    }

    /**
     * Compare key and value seqId.
     * @param key1 key one
     * @param seqId1 seqId one
     * @param key2 key two
     * @param seqId2 seqId two
     * @param reverseOrder reverse order, default is false.
     * @return If the current key is less than other, return -1. If they are equal, return 0.
     * If the current key is greater than other, return 1. The larger the seqId, the newer it is.
     * When sorting in ascending order, the new key value should be in front of the old key value.
     */
    inline static int32_t CompareFullKey(const Key &key1, uint64_t seqId1, const Key &key2, uint64_t seqId2,
                                         bool reverseOrder = false)
    {
        int32_t cmp = key1.Compare(key2);
        if (cmp != 0) {
            return reverseOrder ? -cmp : cmp;
        }
        // The larger the seqId, the newer it is. When sorting in ascending order, the new key value should be in front
        // of the old key value.
        return BssMath::IntegerCompare(seqId2, seqId1);
    }

    inline uint32_t GetFullKeyLen()
    {
        // 27 包含stateId 2 seqId and valueType 9 priKey: hashcode 4 keyLen 4 secKey：hashcode 4 keyLen：4
        if (HasSecKey()) {
            return mPriKey.KeyLen() + mSecKey.KeyLen() + NO_27;
        }
        return mPriKey.KeyLen() + NO_19;
    }

    inline std::string ToString() const
    {
        std::ostringstream oss;
        oss << "MixedHashCode:" << mMixedHashCode << ", PriKey:" << mPriKey.ToString()
            << ", SecKey:" << mSecKey.ToString() << ", SeqId:" << SeqId();
        return oss.str();
    }

private:
    uint64_t mSeqId;            // value seqId.
    enum ValueType mValueType;  // value type.
};
}  // namespace bss
}  // namespace ock
#endif  // BOOST_SS_FULL_KEY_H
