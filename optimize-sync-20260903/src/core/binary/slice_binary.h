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

#ifndef BOOST_SS_SLICE_BINARY_H
#define BOOST_SS_SLICE_BINARY_H

#include <iostream>

#include "binary/key_value.h"
#include "common/bss_def.h"
#include "common/state_id_interval.h"
#include "common/util/bss_math.h"
#include "state_id.h"

namespace ock {
namespace bss {
#pragma pack(1)
/**
 * The data structure of the key in the memory of the slice table
 */
struct SlicePriKey {
    uint16_t mStateId;
    uint32_t mKeyHashCode;
    uint32_t mKeyLen;
    uint8_t mKeyData[0];
};

struct SliceKey : public Key {
public:
    SliceKey() = default;
    inline void Unpack(const ByteBufferRef &buffer, uint32_t bufferOffset, uint32_t bufferLen, uint32_t mixedHashCode)
    {
        if (UNLIKELY(buffer == nullptr)) {
            LOG_ERROR("unexpected input buffer is null.");
            return;
        }

        uint8_t *data = buffer->Data() + bufferOffset;
        uint16_t stateId = *reinterpret_cast<const uint16_t *>(data);
        mHasSecKey = StateId::HasSecKey(stateId);
        mHasNameSpace = StateId::HasNameSpace(stateId);
        mMixedHashCode = mixedHashCode;
        if (mHasSecKey) {
            auto priKey = const_cast<SlicePriKey *>(reinterpret_cast<const SlicePriKey *>(data));
            mPriKey.StateId(stateId);
            mPriKey.KeyLen(priKey->mKeyLen);
            mPriKey.KeyData(priKey->mKeyData);
            uint32_t nsHashCode = mHasNameSpace ? *reinterpret_cast<uint32_t *>(priKey->mKeyData) : 0;
            mPriKey.HashCode(priKey->mKeyHashCode ^ nsHashCode);

            mSecKey.KeyLen(bufferLen - (mPriKey.KeyLen() + sizeof(SlicePriKey)));
            mSecKey.KeyData(data + sizeof(SlicePriKey) + mPriKey.KeyLen());
            mSecKey.HashCode(mixedHashCode ^ stateId ^ mPriKey.HashCode());
        } else {
            mPriKey.StateId(stateId);
            mPriKey.KeyLen(bufferLen - sizeof(uint16_t));
            mPriKey.KeyData(data + sizeof(uint16_t));
            mPriKey.HashCode(mixedHashCode ^ stateId);
        }
        mBuffer = buffer;
    }

    inline bool ComparePrimaryKeyAndSecondKey(const SliceKey &k2) const
    {
        auto comp = mPriKey.CompareStateIdFirst(k2.mPriKey);
        if (comp != 0) {
            return comp < 0;
        }
        // compare second key
        if (UNLIKELY(mHasSecKey != k2.mHasSecKey)) {
            return mHasSecKey;
        }

        return mSecKey.CompareKeyNode(k2.mSecKey) < 0;
    }

    inline uint32_t GetSerializeLength() const
    {
        return mHasSecKey ? NO_10 + mPriKey.KeyLen() + mSecKey.KeyLen() : NO_2 + mPriKey.KeyLen();
    }

    inline BResult Serialize(const ByteBufferRef &buffer, uint32_t offset, uint32_t &serializeSize) const
    {
        if (UNLIKELY(buffer == nullptr)) {
            return BSS_ERR;
        }
        serializeSize = GetSerializeLength();
        if (mHasSecKey) {
            RETURN_NOT_OK(buffer->WriteUint16(StateId(), offset));
            RETURN_NOT_OK(buffer->WriteUint32(KeyHashCode(), offset + NO_2));
            RETURN_NOT_OK(buffer->WriteUint32(mPriKey.KeyLen(), offset + NO_6));
            auto pos = offset + NO_10;
            auto ret = memcpy_s(buffer->Data() + pos, buffer->Capacity() - pos, mPriKey.KeyData(), mPriKey.KeyLen());
            if (UNLIKELY(ret != EOK)) {
                LOG_ERROR("memcpy_s failed, ret: " << ret);
                return BSS_INNER_ERR;
            }
            pos = pos + mPriKey.KeyLen();
            ret = memcpy_s(buffer->Data() + pos, buffer->Capacity() - pos, mSecKey.KeyData(), mSecKey.KeyLen());
            if (UNLIKELY(ret != EOK)) {
                LOG_ERROR("memcpy_s failed, ret: " << ret);
                return BSS_INNER_ERR;
            }
        } else {
            RETURN_NOT_OK(buffer->WriteUint16(StateId(), offset));
            auto pos = offset + NO_2;
            auto ret = memcpy_s(buffer->Data() + pos, buffer->Capacity() - pos, mPriKey.KeyData(), mPriKey.KeyLen());
            if (UNLIKELY(ret != EOK)) {
                LOG_ERROR("memcpy_s failed, ret: " << ret);
                return BSS_INNER_ERR;
            }
        }
        return BSS_OK;
    }

    inline uint32_t Pack(uint8_t *buffer) const
    {
        return 0;
    }
    inline static SliceKey &Of(Key &key)
    {
        return *reinterpret_cast<SliceKey *>(&key);
    }
    inline static const SliceKey &Of(const Key &key)
    {
        return *reinterpret_cast<const SliceKey *>(&key);
    }
};
using SliceKeyPtr = SliceKey *;

struct SliceKeyHash {
    std::size_t operator()(const SliceKey &data) const
    {
        return data.MixedHashCode();
    }
};

struct SliceKeyEqual {
    bool operator()(const SliceKey &a, const SliceKey &b) const
    {
        return a.Compare(b) == 0;
    }
};

struct SliceValue : public Value {
public:
    inline uint32_t Pack(uint8_t *buffer) const
    {
        return 0;
    }
    inline static SliceValue &Of(Value &value)
    {
        return *reinterpret_cast<SliceValue *>(&value);
    }
};
using SliceValuePtr = SliceValue *;

struct SliceHead {
    uint64_t sliceId;
    uint64_t version;
    uint32_t keyCount;
    uint32_t indexCount;
    uint32_t keyOffsetBase;
    uint32_t valueOffsetBase;
    uint32_t sortedKeyCount;
    StateIdInterval stateIdInterval;
    uint16_t formatVersion;
};

struct SliceShortIndex {
    uint16_t count;
    uint16_t firstIndex;
};

struct SliceLongIndex {
    uint32_t count;
    uint32_t firstIndex;
};

struct SliceIndex {
public:
    using IndexGet = bool (SliceIndex::*)(uint32_t, uint32_t &, uint32_t &);
    using IndexPut = void (SliceIndex::*)(uint32_t, uint32_t);

    inline void Init(uint8_t *buffer, uint32_t bufferSize, uint32_t indexCount, uint32_t indexWidth, bool isRestore)
    {
        mIndexBase = buffer;
        mTotalSize = indexCount * indexWidth;
        if (!isRestore) {
            auto ret = memset_s(mIndexBase, bufferSize, 0, mTotalSize);
            if (UNLIKELY(ret != EOK)) {
                LOG_ERROR("SliceIndex Init failed, ret" << ret << ".");
                return;
            }
        }

        if (indexWidth == NO_4) {
            mGet = &SliceIndex::ShortIndexGet;
            mPut = &SliceIndex::ShortIndexPut;
        } else {
            mGet = &SliceIndex::LongIndexGet;
            mPut = &SliceIndex::LongIndexPut;
        }
    }

    inline uint32_t TotalSize()
    {
        return mTotalSize;
    }

    inline bool Get(uint32_t indexId, uint32_t &count, uint32_t &firstIndex)
    {
        return (this->*mGet)(indexId, count, firstIndex);
    }

    inline void Put(uint32_t indexId, uint32_t curIndex)
    {
        (this->*mPut)(indexId, curIndex);
    }

    bool ShortIndexGet(uint32_t indexId, uint32_t &count, uint32_t &firstIndex)
    {
        SliceShortIndex *index = reinterpret_cast<SliceShortIndex *>(mIndexBase) + indexId;
        count = index->count;
        firstIndex = index->firstIndex;
        return (index->count != 0);
    }

    bool LongIndexGet(uint32_t indexId, uint32_t &count, uint32_t &firstIndex)
    {
        SliceLongIndex *index = reinterpret_cast<SliceLongIndex *>(mIndexBase) + indexId;
        count = index->count;
        firstIndex = index->firstIndex;
        return (index->count != 0);
    }

    void ShortIndexPut(uint32_t indexId, uint32_t curIndex)
    {
        SliceShortIndex *index = reinterpret_cast<SliceShortIndex *>(mIndexBase) + indexId;
        if (UNLIKELY(index->count == 0)) {
            index->firstIndex = static_cast<uint16_t>(curIndex);
        }
        index->count++;
    }

    void LongIndexPut(uint32_t indexId, uint32_t curIndex)
    {
        SliceLongIndex *index = reinterpret_cast<SliceLongIndex *>(mIndexBase) + indexId;
        if (UNLIKELY(index->count == 0)) {
            index->firstIndex = curIndex;
        }
        index->count++;
    }

private:
    IndexPut mPut{ nullptr };
    IndexGet mGet{ nullptr };
    uint8_t *mIndexBase{ nullptr };
    uint32_t mTotalSize{ 0 };
};

struct SliceHeadSpace {
private:
    SliceHead *mHead{ nullptr };
    SliceIndex mIndex;
    uint32_t *mSortedIndex{ nullptr };
    uint32_t *mMixedHashCode{ nullptr };
    uint64_t *mSeqId{ nullptr };

public:
    inline uint32_t EvaluateSize(uint32_t keyCount)
    {
        uint32_t size = 0;
        // confirm index width.
        uint32_t indexCount = BssMath::RoundUpToPowerOfTwo(keyCount);
        uint32_t indexWidth = NO_4;
        if (indexCount > BYTE4_MAX_SLOT_SIZE) {
            indexWidth = NO_8;
        }
        // head size.
        size += sizeof(SliceHead);
        // index size.
        size += indexCount * indexWidth;
        // sorted index size.
        size += indexCount * NO_4;
        // mix hash code size.
        size += sizeof(uint32_t) * keyCount;
        // seq id size.
        size += sizeof(uint64_t) * keyCount;
        return size;
    }

    inline void Init(const ByteBufferRef &buffer, uint32_t &bufferOffset, uint32_t keyCount, uint32_t sortedKeyCount,
                     bool isRestore)
    {
        uint8_t *data = buffer->Data() + bufferOffset;
        // confirm index width.
        uint32_t indexCount = BssMath::RoundUpToPowerOfTwo(keyCount);
        uint32_t indexWidth = NO_4;
        uint32_t originOffset = bufferOffset;
        if (indexCount > BYTE4_MAX_SLOT_SIZE) {
            indexWidth = NO_8;
        }

        // init head.
        mHead = reinterpret_cast<SliceHead *>(data);
        mHead->indexCount = indexCount;
        mHead->keyCount = keyCount;
        bufferOffset += sizeof(SliceHead);

        // init index.
        mIndex.Init(data + bufferOffset, buffer->Capacity() - originOffset - bufferOffset, indexCount, indexWidth,
                    isRestore);
        bufferOffset += mIndex.TotalSize();

        // init sorted index.
        mSortedIndex = reinterpret_cast<uint32_t *>(data + bufferOffset);
        bufferOffset += sortedKeyCount * sizeof(uint32_t);

        // init mixed hash code.
        mMixedHashCode = reinterpret_cast<uint32_t *>(data + bufferOffset);
        bufferOffset += sizeof(uint32_t) * keyCount;

        // init seq id.
        mSeqId = reinterpret_cast<uint64_t *>(data + bufferOffset);
        bufferOffset += sizeof(uint64_t) * keyCount;
    }

    inline SliceHead &Head() const
    {
        return *mHead;
    }
    inline uint32_t MixedHashCode(uint32_t index) const
    {
        return mMixedHashCode[index];
    }
    inline uint64_t SeqId(uint32_t index) const
    {
        return mSeqId[index];
    }
    inline uint32_t GetIndexBySortedIndex(uint32_t sortedIndex) const
    {
        return mSortedIndex[sortedIndex];
    }
    inline void MixedHashCode(uint32_t index, uint32_t mixedHashCode)
    {
        mMixedHashCode[index] = mixedHashCode;
    }
    inline void SeqId(uint32_t index, uint64_t seqId)
    {
        mSeqId[index] = seqId;
    }
};

struct SliceKeySpace {
private:
    uint32_t *mKeyOffset{ nullptr };
    uint8_t *mKey{ nullptr };
    uint32_t mKeyBase{ 0 };

    ByteBufferRef mBuffer{ nullptr };

public:
    inline uint32_t EvaluateSize(uint32_t keyCount, uint32_t totalKeySize)
    {
        return sizeof(uint32_t) * keyCount + totalKeySize;
    }

    inline void Init(const ByteBufferRef &buffer, uint32_t &bufferOffset, uint32_t keyCount, uint32_t totalKeySize)
    {
        uint8_t *data = buffer->Data() + bufferOffset;

        // init key offset.
        mKeyOffset = reinterpret_cast<uint32_t *>(data);
        uint32_t keyOffsetTotalSize = sizeof(uint32_t) * keyCount;

        // init key.
        bufferOffset += keyOffsetTotalSize;
        mKey = data + keyOffsetTotalSize;
        mKeyBase = bufferOffset;

        bufferOffset += totalKeySize;

        mBuffer = buffer;
    }

    inline void Get(uint32_t index, uint32_t mixedHashCode, Key &key) const
    {
        uint32_t startOffset = (index == 0) ? 0 : mKeyOffset[index - 1];
        uint32_t endOffset = mKeyOffset[index];
        SliceKey::Of(key).Unpack(mBuffer, mKeyBase + startOffset, endOffset - startOffset, mixedHashCode);
    }

    inline int32_t FindPriKey(uint32_t index, uint32_t mixedHashCode, const PriKeyNode &key) const
    {
        uint32_t startOffset = (index == 0) ? 0 : mKeyOffset[index - 1];
        uint32_t endOffset = mKeyOffset[index];
        return ComparePriKeyStateIdFirst(mBuffer, mKeyBase + startOffset, endOffset - startOffset, mixedHashCode, key);
    }

    inline int32_t ComparePriKeyStateIdFirst(const ByteBufferRef &buffer, uint32_t bufferOffset, uint32_t bufferLen,
                                             uint32_t mixedHashCode, const PriKeyNode &key) const
    {
        const uint8_t *data = buffer->Data() + bufferOffset;
        uint16_t stateId = *reinterpret_cast<const uint16_t *>(data);
        int32_t cmp = BssMath::IntegerCompare(stateId, key.StateId());
        if (cmp != 0) {
            return cmp;
        }

        uint32_t priKeyLen;
        const uint8_t *priKeyData;
        uint32_t priKeyHashCode;
        if (StateId::HasSecKey(stateId)) {
            auto priKey = reinterpret_cast<const SlicePriKey *>(data);
            priKeyLen = priKey->mKeyLen;
            priKeyData = priKey->mKeyData;
            priKeyHashCode = priKey->mKeyHashCode;
        } else {
            priKeyLen = bufferLen - sizeof(uint16_t);
            priKeyData = data + sizeof(uint16_t);
            priKeyHashCode = mixedHashCode ^ stateId;
        }
        cmp = BssMath::IntegerCompare(priKeyHashCode, key.KeyHashCode());
        if (cmp != 0) {
            return cmp;
        }

        cmp = memcmp(priKeyData, key.KeyData(), std::min(priKeyLen, key.KeyLen()));
        if (cmp != 0) {
            return cmp;
        }
        return BssMath::IntegerCompare(priKeyLen, key.KeyLen());
    }

    inline void Put(uint32_t index, const Key &key)
    {
        uint32_t preOffset = index == 0 ? 0 : mKeyOffset[index - 1];
        mKeyOffset[index] = preOffset + SliceKey::Of(key).Pack(mKey + preOffset);
    }
};

struct SliceValueIndicator {
private:
    uint32_t mType : 4;
    uint32_t mOffset : 28;

public:
    inline SliceValueIndicator(uint32_t indicator)
    {
        mType = (indicator >> NO_28) & 0xF;
        mOffset = indicator & 0xFFFFFFF;
    }
    inline void Init(uint32_t offset, uint8_t valueType)
    {
        mType = valueType & 0xF;
        mOffset = offset & 0xFFFFFFF;
    }
    inline uint32_t Type() const
    {
        return mType;
    }
    inline uint32_t Offset() const
    {
        return mOffset;
    }
    inline void Type(uint8_t valueType)
    {
        mType = valueType & 0xF;
    }
};

struct SliceValueSpace {
private:
    // todo: SliceValueIndicator
    uint32_t *mValueIndicator{ nullptr };
    uint8_t *mValue{ nullptr };

    ByteBufferRef mBuffer{ nullptr };

public:
    inline uint32_t EvaluateSize(uint32_t valueCount, uint32_t totalValueSize)
    {
        return sizeof(SliceValueIndicator) * valueCount + totalValueSize;
    }
    inline void Init(const ByteBufferRef &buffer, uint32_t bufferOffset, uint32_t valueCount)
    {
        uint8_t *data = buffer->Data() + bufferOffset;
        mValueIndicator = reinterpret_cast<uint32_t *>(data);
        mValue = data + sizeof(SliceValueIndicator) * valueCount;
        mBuffer = buffer;
    }

    inline void Get(uint32_t index, uint64_t seqId, Value &value) const
    {
        uint32_t startOffset = (index == 0) ? 0 : SliceValueIndicator(mValueIndicator[index - 1]).Offset();

        SliceValueIndicator indicator(mValueIndicator[index]);
        uint8_t valueType = indicator.Type();
        uint32_t endOffset = indicator.Offset();

        value.Init(valueType, endOffset - startOffset, mValue + startOffset, seqId, mBuffer);
    }

    inline void Put(uint32_t index, const SliceValue &value)
    {
        // pack value.
        uint32_t preOffset = index == 0 ? 0 : SliceValueIndicator(mValueIndicator[index - 1]).Offset();
        uint32_t valueLen = value.Pack(mValue + preOffset);
        uint32_t offset = preOffset + valueLen;

        // update indicator.
        SliceValueIndicator *indicator = reinterpret_cast<SliceValueIndicator *>(&mValueIndicator[index]);
        indicator->Init(offset, value.ValueType());
    }
};

struct SliceSpace {
private:
    SliceHeadSpace mHeadSpace;
    SliceKeySpace mKeySpace;
    SliceValueSpace mValueSpace;

    ByteBufferRef mBuffer{ nullptr };

public:
    inline void Init(const ByteBufferRef &buffer, uint32_t keyCount, uint32_t sortedKeyCount, uint32_t totalKeySize,
                     bool isRestore)
    {
        mBuffer = buffer;

        // init header space.
        uint32_t bufferOffset = 0;
        mHeadSpace.Init(mBuffer, bufferOffset, keyCount, sortedKeyCount, isRestore);

        auto &head = mHeadSpace.Head();

        // init key space.
        head.keyOffsetBase = bufferOffset;
        mKeySpace.Init(mBuffer, bufferOffset, keyCount, totalKeySize);

        // init value space.
        head.valueOffsetBase = bufferOffset;
        mValueSpace.Init(mBuffer, bufferOffset, keyCount);
    }

    inline uint32_t KeyCount() const
    {
        return mHeadSpace.Head().keyCount;
    }

    inline uint32_t SortedKeyCount() const
    {
        return mHeadSpace.Head().sortedKeyCount;
    }

    inline uint32_t GetKeyMixedHashCode(uint32_t index) const
    {
        return mHeadSpace.MixedHashCode(index);
    }

    inline void GetKey(uint32_t index, Key &key) const
    {
        uint32_t mixedHashCode = GetKeyMixedHashCode(index);
        mKeySpace.Get(index, mixedHashCode, key);
    }

    inline int32_t FindPriKey(uint32_t index, const PriKeyNode &key) const
    {
        uint32_t mixedHashCode = GetKeyMixedHashCode(index);
        return mKeySpace.FindPriKey(index, mixedHashCode, key);
    }

    inline void GetKeyBySortedIndex(uint32_t sortedIndex, Key &key) const
    {
        GetKey(mHeadSpace.GetIndexBySortedIndex(sortedIndex), key);
    }

    inline int32_t FindPriKeyBySortedIndex(uint32_t sortedIndex, const PriKeyNode &key) const
    {
        return FindPriKey(mHeadSpace.GetIndexBySortedIndex(sortedIndex), key);
    }

    inline void GetValue(uint32_t index, Value &value) const
    {
        uint64_t seqId = mHeadSpace.SeqId(index);
        mValueSpace.Get(index, seqId, value);
    }

    inline void GetKeyValue(uint32_t index, KeyValueRef &keyValue) const
    {
        GetKey(index, keyValue->key);
        GetValue(index, keyValue->value);
    }

    inline uint32_t GetIndexBySortIndex(uint32_t index) const
    {
        return mHeadSpace.GetIndexBySortedIndex(index);
    }

    inline void PutKeyValue(uint32_t index, const SliceKey &key, const SliceValue &value)
    {
        mHeadSpace.MixedHashCode(index, key.MixedHashCode());
        mHeadSpace.SeqId(index, value.SeqId());
        mKeySpace.Put(index, key);
        mValueSpace.Put(index, value);
    }
};
#pragma pack()
}  // namespace bss
}  // namespace ock
#endif  // BOOST_SS_SLICE_BINARY_H
