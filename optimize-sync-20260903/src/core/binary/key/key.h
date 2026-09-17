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

#ifndef BOOST_SS_KEY_H
#define BOOST_SS_KEY_H
#include <iomanip>

#include "binary/byte_buffer.h"
#include "common/util/key_group_util.h"
#include "include/binary_data.h"
#include "state_id.h"

namespace ock {
namespace bss {
struct KeyNode {
public:
    KeyNode() = default;
    void Init(uint32_t hashCode, const uint8_t *keyData, uint32_t keyLen)
    {
        mHashCode = hashCode;
        mKeyLen = keyLen;
        mKeyData = keyData;
    }

    KeyNode(uint32_t hashCode, uint32_t keyLen, const uint8_t *keyData)
        : mHashCode(hashCode), mKeyLen(keyLen), mKeyData(keyData)
    {
    }

    inline uint32_t HashCode() const
    {
        return mHashCode;
    }

    inline uint32_t KeyLen() const
    {
        return mKeyLen;
    }

    inline const uint8_t *KeyData() const
    {
        return mKeyData;
    }

    inline void HashCode(uint32_t hashCode)
    {
        mHashCode = hashCode;
    }

    inline void KeyLen(uint32_t keyLen)
    {
        mKeyLen = keyLen;
    }

    inline void KeyData(const uint8_t *keyData)
    {
        mKeyData = keyData;
    }

    template <typename KN> inline int32_t CompareKeyNode(const KN &other) const
    {
        // compare hash code first.
        int32_t cmp = BssMath::IntegerCompare(mHashCode, other.mHashCode);
        // compare key data and length.
        return (cmp != 0) ? cmp :
               ((cmp = memcmp(mKeyData, other.mKeyData, std::min(mKeyLen, other.mKeyLen))) != 0) ?
                            cmp :
                            BssMath::IntegerCompare(mKeyLen, other.mKeyLen);
    }

protected:
    /**
     * for primary key, hashcode is KeyHashCode ^ NameSpaceHashCode
     * for secondary key, hashcode is calculate by HashCode::Hash(SecondaryKeyData, SecondaryKeyLen);
     */
    uint32_t mHashCode{ 0 };
    uint32_t mKeyLen{ 0 };
    const uint8_t *mKeyData{ nullptr };
};

struct PriKeyNode : public KeyNode {
public:
    PriKeyNode() = default;
    PriKeyNode(uint16_t stateId, uint32_t hashCode, const uint8_t *keyData, uint32_t keyLen)
        : KeyNode(hashCode, keyLen, keyData), mStateId(stateId)
    {
    }
    void Init(uint16_t stateId, uint32_t hashCode, const uint8_t *keyData, uint32_t keyLen)
    {
        KeyNode::Init(hashCode, keyData, keyLen);
        mStateId = stateId;
    }

    inline uint32_t KeyHashCode() const
    {
        return mHashCode ^ NsHashCode();
    }

    /**
     * Is primary key node the same with other or not?
     * @param other other primary key node.
     * @return return true if equals, else return false.
     */
    inline bool operator==(const PriKeyNode &other) const
    {
        return CompareStateIdFirst(other) == 0;
    }

    /**
     * Get state id.
     * @return state id.
     */
    inline uint16_t StateId() const
    {
        return mStateId;
    }

    /**
     * Set state id.
     * @param stateId state id.
     */
    inline void StateId(uint16_t stateId)
    {
        mStateId = stateId;
    }

    /**
     * Get namespace hash code, if there is a namespace, hashCode is stored in the first 4 bytes of the main key.
     * @return return namespace hash code.
     */
    inline uint32_t NsHashCode() const
    {
        return StateId::HasNameSpace(mStateId) ? *reinterpret_cast<const uint32_t *>(mKeyData) : 0;
    }

    /**
     * Compare primary key, compare state id first.
     * @param other other key.
     * @return If the current key is less than other, return -1. When they are equal, return 0.
     * If the current key is greater than other, return 1.
     */
    inline int32_t CompareStateIdFirst(const PriKeyNode &other) const
    {
        int32_t cmp = BssMath::IntegerCompare(mStateId, other.mStateId);
        return (cmp != 0) ? cmp : ComparePriKeyNode(other);
    }

    inline int32_t ComparePriKeyNode(const PriKeyNode &other) const
    {
        // compare hash code first.
        int32_t cmp = BssMath::IntegerCompare(KeyHashCode(), other.KeyHashCode());
        // compare key data and length.
        return (cmp != 0) ? cmp :
               ((cmp = memcmp(mKeyData, other.mKeyData, std::min(mKeyLen, other.mKeyLen))) != 0) ?
                            cmp :
                            BssMath::IntegerCompare(mKeyLen, other.mKeyLen);
    }

    /**
     * Compare primary key, compare state id last.
     * @param other other key.
     * @return If the current key is less than other, return -1. When they are equal, return 0.
     * If the current key is greater than other, return 1.
     */
    inline int32_t CompareStateIdLast(const PriKeyNode &other) const
    {
        int32_t cmp = ComparePriKeyNode(other);
        return (cmp != 0) ? cmp : BssMath::IntegerCompare(mStateId, other.mStateId);
    }

    /**
     * If it is a SUBMAP/SUBLIST type State, the primary keydata will contain the namespace, the memory struct of
     * KeyData is : | NsHashCode(4Bytes) | KeyRealData(NBytes) | NsData(NBytes) | KeyRealLength(4Bytes)
     * first 4 bytes is namespace hashcode, last 4 bytes is KeyRealData length.
     */
    /**
     * Get real key data, exclude namespace.
     */
    inline const uint8_t *RealKeyData() const
    {
        return StateId::HasNameSpace(mStateId) ? const_cast<uint8_t *>(mKeyData + NO_4) : mKeyData;
    }

    /**
     * Get real key length, exclude namespace.
     * @return real key length exclude namespace.
     */
    inline uint32_t RealKeyLen() const
    {
        return StateId::HasNameSpace(mStateId) ? *reinterpret_cast<const uint32_t *>(mKeyData + mKeyLen - NO_4) :
                                                 mKeyLen;
    }

    /**
     * There are the same namespace or not?
     * @param nameSpace name space string.
     * @param nameSpaceLen name space length.
     * @return return true if the same, else return false.
     */
    inline bool HasSameNameSpace(const uint8_t *nameSpace, uint32_t nameSpaceLen) const
    {
        if (UNLIKELY(!StateId::HasNameSpace(mStateId))) {
            return false;
        }

        if (UNLIKELY(nameSpace == nullptr)) {
            return false;
        }

        uint32_t thisKeyRealLen = *reinterpret_cast<const uint32_t *>(mKeyData + mKeyLen - NO_4);
        uint32_t thisNameSpaceLen = mKeyLen - NO_8 - thisKeyRealLen;
        if (thisNameSpaceLen != nameSpaceLen) {
            return false;
        }

        const uint8_t *thisNameSpace = mKeyData + NO_4 + thisKeyRealLen;
        return memcmp(thisNameSpace, nameSpace, nameSpaceLen) == 0;
    }

    /**
     * For printing primary key.
     * @return primary key string.
     */
    inline std::string ToString() const
    {
        std::ostringstream oss;
        oss << "StateId: " << mStateId << ", KeyHashCode: " << KeyHashCode() << ", KeyLen: " << mKeyLen
            << ", KeyData: [";
        if (UNLIKELY(mKeyData != nullptr)) {
            uint32_t printLen = std::min(NO_20, mKeyLen);
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

private:
    uint16_t mStateId;
    uint16_t mReserved[NO_3]{};
};
using PriKeyNodePtr = PriKeyNode *;

struct SecKeyNode : public KeyNode {
public:
    SecKeyNode() = default;
    SecKeyNode(uint32_t hashCode, const uint8_t *keyData, uint32_t keyLen) : KeyNode(hashCode, keyLen, keyData)
    {
    }

    /**
     * It equals other secondary key or not.
     * @return return true if it equals, else return false.
     */
    bool operator==(const SecKeyNode &other)
    {
        return CompareKeyNode(other) == 0;
    }

    /**
     * For printing secondary key.
     * @return secondary key string.
     */
    inline std::string ToString() const
    {
        std::ostringstream oss;
        oss << "HashCode: " << mHashCode << ", KeyLen: " << mKeyLen << ", KeyData: [";
        if (UNLIKELY(mKeyData != nullptr)) {
            uint32_t printLen = std::min(NO_20, mKeyLen);
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
using SecKeyNodePtr = SecKeyNode *;

enum KeyFlag {
    NORMAL_KEY_FLAG = 0b00000000,
    END_KEY_FLAG = 0b00000001,
    PRE_KEY_FLAG = 0b00000010,
};

/**
 * NOTICE: Align according to the size of the CPU Cache Line, which is 64 bytes. The key cannot exceed 64 bytes!!!
 */
struct Key;
using KeyPtr = Key *;
using KeyRef = std::shared_ptr<Key>;
struct Key {
public:
    Key() = default;
    /**
     * Initialize key for single key.
     * @param priKey single key.
     * @param buffer buffer object, store key data for key lifecycle management.
     * @param flag key flag, see KeyFlag.
     */
    inline void Init(const PriKeyNode &priKey, const BufferRef &buffer, uint8_t flag = 0)
    {
        uint16_t stateId = priKey.StateId();
        mHasSecKey = StateId::HasSecKey(stateId);
        mHasNameSpace = StateId::HasNameSpace(stateId);
        mPriKey = priKey;
        mMixedHashCode = mPriKey.StateId() ^ mPriKey.HashCode();
        mFlag = flag;
        mBuffer = buffer;
    }

    /**
     * Initialize key for dual key.
     * @param priKey primary key.
     * @param secKey secondary key.
     * @param buffer buffer object, store key data for key lifecycle management.
     * @param flag key flag, see KeyFlag.
     */
    inline void Init(const PriKeyNode &priKey, const SecKeyNode &secKey, const BufferRef &buffer, uint8_t flag = 0)
    {
        uint16_t stateId = priKey.StateId();
        mHasSecKey = StateId::HasSecKey(stateId);
        mHasNameSpace = StateId::HasNameSpace(stateId);
        mPriKey = priKey;
        mSecKey = mHasSecKey ? secKey : SecKeyNode();
        mMixedHashCode = mPriKey.StateId() ^ mPriKey.HashCode() ^ mSecKey.HashCode();
        mFlag = flag;
        mBuffer = buffer;
    }

    /**
     * Construct key for single key.
     * @param priKey single key.
     * @param buffer buffer object, store key data for key lifecycle management.
     * @param flag key flag, see KeyFlag.
     */
    Key(const PriKeyNode &priKey, const BufferRef &buffer, uint8_t flag = 0)
    {
        Init(priKey, buffer, flag);
    }

    /**
     * Construct key for dual key.
     * @param priKey primary key.
     * @param secKey secondary key.
     * @param buffer buffer object, store key data for key lifecycle management.
     * @param flag key flag, see KeyFlag.
     */
    Key(const PriKeyNode &priKey, const SecKeyNode &secKey, const BufferRef &buffer, uint8_t flag = 0)
    {
        Init(priKey, secKey, buffer, flag);
    }

    /**
     * Is equal to other key.
     * @param other other key.
     * @return return true if equals, else return false.
     */
    inline bool operator==(const Key &other)
    {
        return Equal(other);
    }

    /**
     * Compare with other key.
     * @param other other key.
     * @return If the current key is less than other, return -1. When they are equal, return 0.
     * If the current key is greater than other, return 1.
     */
    inline int32_t Compare(const Key &other) const
    {
        int32_t cmp = ComparePQKey(other);
        if (cmp != NO_PQ_DATA) {  // 2 表示不是pq数据，继续执行下面的比较数据方法.
            return cmp;
        }
        // compare primary key.
        cmp = mPriKey.ComparePriKeyNode(other.mPriKey);
        if (cmp != 0) {
            return cmp;
        }
        // compare state id.
        cmp = BssMath::IntegerCompare(mPriKey.StateId(), other.StateId());
        if (cmp != 0) {
            return cmp;
        }
        // they have the same state id, just check k1 is single key or not.
        // if it is single key, return equal.
        if (!mHasSecKey) {
            return 0;
        }
        // compare secondary key.
        return mSecKey.CompareKeyNode(other.mSecKey);
    }

    inline int32_t ComparePQKey(const Key &key2) const
    {
        if (StateId::GetStateType(StateId()) != PQ && StateId::GetStateType(key2.StateId()) != PQ) {
            return NO_PQ_DATA;
        } else if (StateId::GetStateType(StateId()) != PQ) {
            return 1;
        } else if (StateId::GetStateType(key2.StateId()) != PQ) {
            return -1;
        }
        auto cmp = PQBinaryDataComparator::Compare(mPriKey.KeyData(), mPriKey.KeyLen(), key2.mPriKey.KeyData(),
                                                   key2.mPriKey.KeyLen());
        cmp = cmp > 0 ? 1 : (cmp < 0 ? -1 : 0);
        return cmp;
    }

    /**
     * Is equal to other key.
     * @param other other key.
     * @return return true if equals, else return false.
     */
    inline bool Equal(const Key &other)
    {
        // todo: flush table and slice table are sorted stateId first, so we compare stateId first.
        return Compare(other) == 0;
    }

    /**
     * Has the same primary key node with other key.
     * @param other other key.
     * @return return true if has the same primary key node, else return false.
     */
    inline bool HasSamePriKeyNode(const Key &other)
    {
        bool hasSamePriKey = (mPriKey.CompareKeyNode(other.mPriKey) == 0);
        return hasSamePriKey && this->mPriKey.StateId() == other.mPriKey.StateId();
    }

    /**
     * This key has secondary key or not.
     * @return return true if it has secondary key, else return false.
     */
    inline bool HasSecKey() const
    {
        return mHasSecKey;
    }

    /**
     * This key prefix key or not, if it has secondary key and secondary key is nullptr, it is a prefix key.
     * @return return true if it is prefix key, else return false.
     */
    inline bool IsPrefixKey() const
    {
        return mHasSecKey && mSecKey.KeyData() == nullptr;
    }

    /**
     * This key is end key or not, it for prefix search,
     * @return
     */
    inline bool IsEndKey() const
    {
        return mFlag & END_KEY_FLAG;
    }

    inline bool IsPqKey() const
    {
        return StateId::GetStateType(mPriKey.StateId()) == PQ;
    }

    /**
     * Get stateId.
     * @return stateId.
     */
    inline uint16_t StateId() const
    {
        return mPriKey.StateId();
    }

    /**
     * Get mixed hash code, mixed hash code is hash code of hole key,
     * include stateId, primary key (namespace if it exists) and secondary key info.
     * @return mixed hash code.
     */
    inline uint32_t MixedHashCode() const
    {
        return mMixedHashCode;
    }

    /**
     * Get key hash code. key hash code is from flink, PrimaryKeyHashCode is KeyHashCode ^ NamespaceHashCode (if it
     * exists), so KeyHashCode =  PrimaryKeyHashCode ^ NamespaceHashCode.
     * @return key hash code.
     */
    inline uint32_t KeyHashCode() const
    {
        return mPriKey.HashCode() ^ NsHashCode();
    }

    /**
     * Get primary key.
     * @return primary key.
     */
    inline const PriKeyNode &PriKey() const
    {
        return mPriKey;
    }

    /**
     *  Get secondary key.
     * @return secondary key.
     */
    inline const SecKeyNode &SecKey() const
    {
        return mSecKey;
    }

    /**
     * Buffer object to store primary key data and secondary key data, it used for lifecycle management of this key.
     * @return buffer object.
     */
    inline BufferRef Buffer() const
    {
        return mBuffer;
    }

    /**
     * Obtains the offset of primary key data in the data.
     * @return
     */
    inline uint32_t BufferOffsetOfPriKeyData() const
    {
        return BufferOffset(mPriKey);
    }

    /**
     * Obtains the offset of second key data in the data.
     * @return
     */
    inline uint32_t BufferOffsetOfSecKeyData() const
    {
        return BufferOffset(mSecKey);
    }

    inline void PriKeyData(uint8_t *data)
    {
        mPriKey.KeyData(data);
    }

    inline void SecKeyData(uint8_t *data)
    {
        mSecKey.KeyData(data);
    }

    /**
     * For printing key.
     * @return key string.
     */
    inline std::string ToString() const
    {
        std::ostringstream oss;
        oss << "MixedHashCode: " << mMixedHashCode << ", PriKey: " << mPriKey.ToString();
        if (mHasSecKey) {
            oss << ", SecKey: " << mSecKey.ToString();
        }
        return oss.str();
    }

protected:
    /**
     * Get namespace hashcode.
     * @return return namespace hashcode if it exists, else return false.
     */
    inline uint32_t NsHashCode() const
    {
        return mHasNameSpace ? mPriKey.NsHashCode() : 0;
    }

private:
    /**
     * Obtains the offset of key data in the data.
     * @param keyNode
     * @return
     */
    inline uint32_t BufferOffset(const KeyNode &keyNode) const
    {
        if (mBuffer != nullptr && keyNode.KeyData() != nullptr && mBuffer->Data() != nullptr &&
            keyNode.KeyData() >= mBuffer->Data()) {
            return keyNode.KeyData() - mBuffer->Data();
        }
        return 0;
    }

protected:
    /**
     * NOTICE: Align according to the size of the CPU Cache Line, which is 64 bytes. The key cannot exceed 64 bytes!!!
     */
    uint8_t mHasSecKey{ false };
    uint8_t mHasNameSpace{ false };
    uint8_t mFlag{ 0 };
    uint8_t mReserved;
    uint32_t mMixedHashCode{ 0 };

    PriKeyNode mPriKey;
    SecKeyNode mSecKey;

    BufferRef mBuffer{ nullptr };
};

struct PriKeyNodeHash {
    std::size_t operator()(const PriKeyNodePtr &key) const
    {
        return key->HashCode();
    }
};

struct PriKeyNodeEqual {
    bool operator()(const PriKeyNodePtr a, const PriKeyNodePtr b) const
    {
        return a->CompareStateIdFirst(*b) == 0;
    }
};

struct SecKeyNodeHash {
    std::size_t operator()(const SecKeyNodePtr &key) const
    {
        return key->HashCode();
    }
};

struct SecKeyNodeEqual {
    bool operator()(const SecKeyNodePtr a, const SecKeyNodePtr b) const
    {
        return a->CompareKeyNode(*b) == 0;
    }
};

struct KeyHash {
    std::size_t operator()(const KeyPtr key) const
    {
        return key->MixedHashCode();
    }
};

struct KeyEqual {
    bool operator()(const KeyPtr a, const KeyPtr b) const
    {
        return a->Compare(*b) == 0;
    }
};

using PriKeyFilter = std::function<bool(const PriKeyNode &priKey)>;
using SecKeyFilter = std::function<bool(const SecKeyNode &secKey)>;
using KeyFilter = std::function<bool(const Key &key)>;

}  // namespace bss
}  // namespace ock
#endif  // BOOST_SS_KEY_H
