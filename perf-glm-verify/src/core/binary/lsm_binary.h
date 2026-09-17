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

#ifndef BOOST_SS_LSM_BINARY_H
#define BOOST_SS_LSM_BINARY_H

#include "binary_reader.h"
#include "bss_def.h"
#include "key_value.h"

namespace ock {
namespace bss {
#pragma pack(1)
struct LsmPriKeyNode {
    /**
     * for primary key, hashcode is KeyHashCode
     */
    uint32_t mKeyHashCode;
    uint32_t mKeyLen;
    uint8_t mKeyData[0];

public:
    inline uint32_t NsHashCode(uint16_t stateId)
    {
        bool hasNamespace = StateId::HasNameSpace(stateId);
        return hasNamespace ? *reinterpret_cast<uint32_t *>(mKeyData) : 0;
    }

    inline int32_t CompareKeyNode(const PriKeyNode &other)
    {
        if (StateId::GetStateType(other.StateId()) == PQ) {
            return PQBinaryDataComparator::ComparePrefix(mKeyData, mKeyLen, other.KeyData(), other.KeyLen());
        }
        int32_t cmp = BssMath::IntegerCompare(mKeyHashCode, other.KeyHashCode());
        if (cmp != 0) {
            return cmp;
        }
        return ((cmp = memcmp(mKeyData, other.KeyData(), std::min(mKeyLen, other.KeyLen()))) != 0) ?
                   cmp :
                   BssMath::IntegerCompare(mKeyLen, other.KeyLen());
    }

    inline static LsmPriKeyNode &From(uint8_t *buffer)
    {
        return *reinterpret_cast<LsmPriKeyNode *>(buffer);
    }
};

struct LsmSecKeyNode {
    /**
     * for secondary key, hashcode is calculate by HashCode::Hash(SecondaryKeyData, SecondaryKeyLen);
     */
    uint16_t mStateId;
    uint32_t mHashCode;
    uint32_t mKeyLen;
    uint8_t mKeyData[0];

public:
    inline int32_t CompareKeyNode(const KeyNode &other) const
    {
        return -other.CompareKeyNode(*this);
    }

    inline static LsmSecKeyNode &From(uint8_t *buffer)
    {
        return *reinterpret_cast<LsmSecKeyNode *>(buffer);
    }
};

struct LsmValue {
    uint64_t mSeqId;
    uint8_t mValueType;
    uint32_t mValueLen;
    uint8_t mValueData[0];
};

struct LsmMultiSecKeyInfo {
    uint32_t mSecKeyCount;
    uint32_t mFirstSecKeyOffset;
    uint8_t mSecKeyOffset[0];

public:
    BResult SecKeyOffset(uint32_t index, uint32_t byteNum, uint32_t &secKeyOffset)
    {
        uint32_t offset = byteNum * index;
        uint64_t resOffset = 0;
        RETURN_NO_OK_AS_READ_BUFFER_ERROR(BinaryReader::ReadBytes(mSecKeyOffset + offset, byteNum, resOffset));
        secKeyOffset = mFirstSecKeyOffset + static_cast<uint32_t>(resOffset);
        return BSS_OK;
    }
};

struct LsmKeyValueInfo {
public:
    LsmKeyValueInfo() = default;
    LsmKeyValueInfo(const ByteBufferRef &buffer, uint32_t bufferOffset)
    {
    }

    BResult Unpack(const ByteBufferRef &buffer, uint32_t bufferOffset)
    {
        RETURN_INVALID_PARAM_AS_NULLPTR(buffer->Data());
        uint8_t *data = buffer->Data() + bufferOffset;
        RETURN_INNER_ERR_AS_BUFFER_OVER_FLOW(buffer->mCapacity, bufferOffset + sizeof(LsmPriKeyNode));
        uint32_t offset = 0;
        mPriKey = reinterpret_cast<LsmPriKeyNode *>(data);
        RETURN_INVALID_PARAM_AS_NULLPTR(mPriKey);
        offset += sizeof(LsmPriKeyNode) + mPriKey->mKeyLen;
        uint8_t flag = *(data + offset);
        offset += NO_1;  // flag size.
        mHasMultiSecKey = (flag & 0x1) != 0;
        BResult result;
        if (mHasMultiSecKey) {
            result = mMulti.Unpack(flag, bufferOffset + offset, buffer);
        } else {
            result = mSgl.Unpack(data + offset, buffer->Capacity() - bufferOffset - offset);
        }
        if (UNLIKELY(result != BSS_OK)) {
            LOG_ERROR("unpack secKey faild, offset: " << (bufferOffset + offset));
            return result;
        }
        mBuffer = buffer;
        return BSS_OK;
    }

    inline uint16_t StateId() const
    {
        return mHasMultiSecKey ? mMulti.mStateId : mSgl.mStateId;
    }

    inline uint32_t GetSecKeyCount()
    {
        return mHasMultiSecKey ? mMulti.mMultiSecKey->mSecKeyCount : 1;
    }

    inline bool HasMultiSecKey()
    {
        return mHasMultiSecKey;
    }

    inline BResult GetKeyAndValue(uint32_t secIndex, KeyValueRef &keyValue)
    {
        BResult result = BSS_OK;
        if (mHasMultiSecKey) {
            result = GetKeyAndValueForMulti(secIndex, keyValue->key, keyValue->value);
        } else {
            GetKeyAndValueForSingle(keyValue->key, keyValue->value);
        }
        return result;
    }

    //  For only one secondary key: | PriHashCode | PriKeyLen | PriKeyData| StateId |SecHashCode | SecKeyLen |
    //  SecKeyData
    BResult UnpackToSglSecKey(const ByteBufferRef &buffer, uint32_t bufferOffset)
    {
        RETURN_INVALID_PARAM_AS_NULLPTR(buffer);
        RETURN_INVALID_PARAM_AS_NULLPTR(buffer->Data());
        uint8_t *data = buffer->Data() + bufferOffset;
        RETURN_INNER_ERR_AS_BUFFER_OVER_FLOW(buffer->Capacity(), bufferOffset + sizeof(LsmPriKeyNode));
        mPriKey = reinterpret_cast<LsmPriKeyNode *>(data);
        RETURN_INVALID_PARAM_AS_NULLPTR(mPriKey);
        mHasMultiSecKey = false;
        uint32_t offset = sizeof(LsmPriKeyNode) + mPriKey->mKeyLen;
        RETURN_INNER_ERR_AS_BUFFER_OVER_FLOW(buffer->Capacity(), bufferOffset + offset);
        RETURN_NOT_OK(mSgl.Unpack(data + offset, buffer->Capacity() - bufferOffset - offset));
        mBuffer = buffer;
        return BSS_OK;
    }

    inline int32_t Compare(const Key &other) const
    {
        if (StateId::GetStateType(other.StateId()) == PQ) {
            int32_t cmp = BssMath::IntegerCompare(mSgl.mStateId, other.StateId());
            if (cmp != 0) {
                return cmp;
            }
            cmp = PQBinaryDataComparator::ComparePrefix(mPriKey->mKeyData, mPriKey->mKeyLen, other.PriKey().KeyData(),
                                                        other.PriKey().KeyLen());
            return cmp != 0 ? cmp : other.IsEndKey() ? -1 : cmp;
        }
        // compare primary key.
        int32_t cmp = mPriKey->CompareKeyNode(other.PriKey());
        if (cmp != 0) {
            return cmp;
        }

        // compare state id and secondary key.
        return CompareStateIdAndSecKey(mSgl.mStateId, mSgl.mSecKey, other);
    }

    inline int32_t CompareSecKeyNode(int32_t secIndex, const Key &other) const
    {
        uint16_t stateId = 0;
        LsmSecKeyNode *secKey = nullptr;
        GetStateIdAndSecKey(secIndex, stateId, secKey);

        // compare state id and secondary key.
        return CompareStateIdAndSecKey(stateId, secKey, other);
    }

    inline bool IsSecKeyInRange(int32_t secIndex, const Key &startKey, const Key &endKey) const
    {
        uint16_t stateId = 0;
        LsmSecKeyNode *secKey = nullptr;
        GetStateIdAndSecKey(secIndex, stateId, secKey);

        return CompareStateIdAndSecKey(stateId, secKey, startKey) >= 0 &&
               CompareStateIdAndSecKey(stateId, secKey, endKey) <= 0;
    }

private:
    inline void GetKeyAndValueForSingle(Key &key, Value &value)
    {
        PriKeyNode priKey(mSgl.mStateId, mPriKey->mKeyHashCode ^ mPriKey->NsHashCode(mSgl.mStateId), mPriKey->mKeyData,
                          mPriKey->mKeyLen);
        auto &sglSecKey = mSgl.mSecKey;
        SecKeyNode secKey = sglSecKey != nullptr ?
                                SecKeyNode(sglSecKey->mHashCode, sglSecKey->mKeyData, sglSecKey->mKeyLen) :
                                SecKeyNode();
        key.Init(priKey, secKey, mBuffer);
        auto &sglValue = mSgl.mValue;
        value.Init(sglValue->mValueType, sglValue->mValueLen, sglValue->mValueData, sglValue->mSeqId, mBuffer);
    }

    inline BResult GetKeyAndValueForMulti(uint32_t secIndex, Key &key, Value &value)
    {
        uint8_t *data = mBuffer->Data();
        uint32_t offset = 0;
        RETURN_NO_OK_AS_READ_BUFFER_ERROR(
            mMulti.mMultiSecKey->SecKeyOffset(secIndex, mMulti.mSecKeyOffsetByteNum, offset));
        RETURN_INNER_ERR_AS_BUFFER_OVER_FLOW(mBuffer->Capacity(), offset + sizeof(uint16_t));
        uint16_t stateId = *(reinterpret_cast<const uint16_t *>(data + offset));

        PriKeyNode priKey(stateId, mPriKey->mKeyHashCode ^ mPriKey->NsHashCode(mSgl.mStateId), mPriKey->mKeyData,
                          mPriKey->mKeyLen);
        bool hasSecKey = StateId::HasSecKey(stateId);
        if (hasSecKey) {
            SecKeyNode secKey;
            RETURN_INNER_ERR_AS_BUFFER_OVER_FLOW(mBuffer->Capacity(), offset + sizeof(LsmSecKeyNode));
            const LsmSecKeyNode *lsmSecKey = reinterpret_cast<const LsmSecKeyNode *>(data + offset);
            offset += sizeof(LsmSecKeyNode) + lsmSecKey->mKeyLen;
            secKey.Init(lsmSecKey->mHashCode, lsmSecKey->mKeyData, lsmSecKey->mKeyLen);
            key.Init(priKey, secKey, mBuffer);
        } else {
            key.Init(priKey, mBuffer);
        }
        RETURN_INNER_ERR_AS_BUFFER_OVER_FLOW(mBuffer->Capacity(), offset + sizeof(LsmValue));
        const LsmValue *lsmValue = reinterpret_cast<const LsmValue *>(data + offset);
        value.Init(lsmValue->mValueType, lsmValue->mValueLen, lsmValue->mValueData, lsmValue->mSeqId, mBuffer);
        return BSS_OK;
    }

    inline void GetStateIdAndSecKeyForSingle(uint16_t &stateId, LsmSecKeyNode *&secKey) const
    {
        stateId = mSgl.mStateId, secKey = mSgl.mSecKey;
    }

    inline BResult GetStateIdAndSecKeyForMulti(uint32_t secIndex, uint16_t &stateId, LsmSecKeyNode *&secKey) const
    {
        uint8_t *data = mBuffer->Data();
        uint32_t offset = 0;
        RETURN_NO_OK_AS_READ_BUFFER_ERROR(
            mMulti.mMultiSecKey->SecKeyOffset(secIndex, mMulti.mSecKeyOffsetByteNum, offset));
        RETURN_INNER_ERR_AS_BUFFER_OVER_FLOW(mBuffer->Capacity(), offset + sizeof(uint16_t));
        stateId = *(reinterpret_cast<const uint16_t *>(data + offset));

        if (StateId::HasSecKey(stateId)) {
            RETURN_INNER_ERR_AS_BUFFER_OVER_FLOW(mBuffer->Capacity(), offset + sizeof(LsmSecKeyNode));
            secKey = reinterpret_cast<LsmSecKeyNode *>(data + offset);
        } else {
            secKey = nullptr;
        }
        return BSS_OK;
    }

    inline void GetStateIdAndSecKey(int32_t secIndex, uint16_t &stateId, LsmSecKeyNode *&secKey) const
    {
        if (mHasMultiSecKey) {
            GetStateIdAndSecKeyForMulti(secIndex, stateId, secKey);
        } else {
            GetStateIdAndSecKeyForSingle(stateId, secKey);
        }
    }

    inline int32_t CompareStateIdAndSecKey(uint16_t stateId, const LsmSecKeyNode *secKey, const Key &other) const
    {
        int32_t cmp = BssMath::IntegerCompare(stateId, other.StateId());
        if (cmp != 0) {
            return cmp;
        }
        if (other.IsEndKey()) {
            return -1;
        }
        if (secKey == nullptr) {
            return 0;
        }
        if (other.IsPrefixKey()) {
            return 1;
        }
        return secKey->CompareKeyNode(other.SecKey());
    }

private:
    /**
     * has single secondary key or multi secondary key?
     */
    bool mHasMultiSecKey{ false };

    /**
     * primary key.
     */
    LsmPriKeyNode *mPriKey{ nullptr };

    union {
        /**
         * for single secondary key.
         */
        struct {
            uint16_t mStateId{ INVALID_U16 };
            LsmSecKeyNode *mSecKey{ nullptr };
            LsmValue *mValue{ nullptr };

            inline BResult Unpack(uint8_t *data, uint32_t capacity)
            {
                RETURN_INVALID_PARAM_AS_NULLPTR(data);
                RETURN_INNER_ERR_AS_BUFFER_OVER_FLOW(capacity, sizeof(uint16_t));
                mStateId = *(reinterpret_cast<uint16_t *>(data));

                bool hasSecKey = StateId::HasSecKey(mStateId);
                uint32_t offset = 0;
                if (hasSecKey) {
                    RETURN_INNER_ERR_AS_BUFFER_OVER_FLOW(capacity, sizeof(LsmSecKeyNode));
                    mSecKey = reinterpret_cast<LsmSecKeyNode *>(data);
                    RETURN_INVALID_PARAM_AS_NULLPTR(mSecKey);
                    offset = sizeof(LsmSecKeyNode) + mSecKey->mKeyLen;
                } else {
                    mSecKey = nullptr;
                    offset = sizeof(uint16_t);
                }
                RETURN_INNER_ERR_AS_BUFFER_OVER_FLOW(capacity, offset + sizeof(LsmValue));
                mValue = reinterpret_cast<LsmValue *>(data + offset);
                return BSS_OK;
            }
        } mSgl{};
        /**
         * for multi secondary key
         */
        struct {
            uint16_t mStateId;
            uint8_t mSecKeyOffsetByteNum;
            uint8_t mReserved2[3];
            LsmMultiSecKeyInfo *mMultiSecKey;

            inline BResult Unpack(uint8_t flag, uint32_t offset, const ByteBufferRef &buffer)
            {
                uint8_t *data = buffer->Data();
                RETURN_INVALID_PARAM_AS_NULLPTR(data);
                mSecKeyOffsetByteNum = (flag >> 1) & 0x7;
                RETURN_INNER_ERR_AS_BUFFER_OVER_FLOW(buffer->Capacity(), offset + sizeof(LsmMultiSecKeyInfo));
                mMultiSecKey = reinterpret_cast<LsmMultiSecKeyInfo *>(data + offset);
                RETURN_INVALID_PARAM_AS_NULLPTR(mMultiSecKey);
                uint32_t secKeyOffset = 0;
                RETURN_NO_OK_AS_READ_BUFFER_ERROR(mMultiSecKey->SecKeyOffset(0, mSecKeyOffsetByteNum, secKeyOffset));
                RETURN_INNER_ERR_AS_BUFFER_OVER_FLOW(buffer->Capacity(), secKeyOffset + sizeof(uint16_t));
                mStateId = *(reinterpret_cast<uint16_t *>(data + secKeyOffset));
                return BSS_OK;
            }
        } mMulti;
    };

    ByteBufferRef mBuffer{ nullptr };
};
#pragma pack()
}  // namespace bss
}  // namespace ock
#endif  // BOOST_SS_LSM_BINARY_H
