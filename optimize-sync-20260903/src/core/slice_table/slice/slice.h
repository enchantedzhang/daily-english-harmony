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

#ifndef BOOST_SS_SLICE_H
#define BOOST_SS_SLICE_H

#include <iostream>
#include <unordered_map>
#include <utility>
#include <vector>

#include "binary/byte_buffer.h"
#include "binary/fresh_binary.h"
#include "binary/key/key.h"
#include "binary/slice_binary.h"
#include "include/bss_err.h"
#include "serialized_data.h"
#include "slice_table/binary_map/binary_key.h"
#include "slice_table/slice/raw_data_slice.h"
#include "stateId_provider.h"

namespace ock {
namespace bss {
class SliceTable;
using SliceTableManagerRef = std::shared_ptr<SliceTable>;
using SliceKVMap = std::unordered_map<SliceKey, Value, SliceKeyHash, SliceKeyEqual>;

#pragma pack(1)
struct SliceCreateMeta {
    uint64_t version;
    uint64_t logicSliceId;
    uint64_t compactionCount;
};

struct ShortSliceIndex {
    uint16_t count;
    uint16_t firstIndex;
};

struct LongSliceIndex {
    uint32_t count;
    uint32_t firstIndex;
};

#pragma pack()

class IndexSpace {
public:
    using IndexGet = bool (IndexSpace::*)(uint32_t, uint32_t &, uint32_t &);
    using IndexPut = void (IndexSpace::*)(uint32_t, uint32_t);

    IndexSpace(ByteBufferRef &buffer, uint32_t offset, uint32_t indexCount, uint32_t indexWidth, bool isRestore)
    {
        mBase = reinterpret_cast<uint8_t *>(buffer->Data() + offset);
        mIndexWidth = indexWidth;
        mIndexCount = indexCount;
        uint32_t totalSize = indexCount * mIndexWidth;
        if (!isRestore) {
            auto ret = memset_s(mBase, buffer->Capacity() - offset, 0, totalSize);
            if (UNLIKELY(ret != EOK)) {
                LOG_ERROR("IndexSpace construct failed, ret: " << ret << ".");
            }
        }
        if (indexWidth == NO_4) {
            mGet = &IndexSpace::ShortIndexGet;
            mPut = &IndexSpace::ShortIndexPut;
        } else {
            mGet = &IndexSpace::LongIndexGet;
            mPut = &IndexSpace::LongIndexPut;
        }
    }

    inline bool Get(uint32_t indexId, uint32_t &count, uint32_t &firstIndex)
    {
        if (UNLIKELY(indexId >= mIndexCount)) {
            return false;
        }

        if (mIndexWidth == NO_4) {
            ShortSliceIndex *index = reinterpret_cast<ShortSliceIndex *>(mBase) + indexId;
            count = index->count;
            firstIndex = index->firstIndex;
        } else {
            LongSliceIndex *index = reinterpret_cast<LongSliceIndex *>(mBase) + indexId;
            count = index->count;
            firstIndex = index->firstIndex;
        }
        return (count != 0);
    }

    inline void Put(uint32_t indexId, uint32_t curIndex)
    {
        (this->*mPut)(indexId, curIndex);
    }

    bool ShortIndexGet(uint32_t indexId, uint32_t &count, uint32_t &firstIndex)
    {
        if (UNLIKELY(indexId >= mIndexCount)) {
            return false;
        }
        ShortSliceIndex *index = reinterpret_cast<ShortSliceIndex *>(mBase) + indexId;
        count = index->count;
        firstIndex = index->firstIndex;
        return (index->count != 0);
    }

    bool LongIndexGet(uint32_t indexId, uint32_t &count, uint32_t &firstIndex)
    {
        if (UNLIKELY(indexId >= mIndexCount)) {
            return false;
        }
        LongSliceIndex *index = reinterpret_cast<LongSliceIndex *>(mBase) + indexId;
        count = index->count;
        firstIndex = index->firstIndex;
        return (index->count != 0);
    }

    void ShortIndexPut(uint32_t indexId, uint32_t curIndex)
    {
        ShortSliceIndex *index = reinterpret_cast<ShortSliceIndex *>(mBase) + indexId;
        if (UNLIKELY(index->count == 0)) {
            index->firstIndex = static_cast<uint16_t>(curIndex);
        }
        index->count++;
    }

    void LongIndexPut(uint32_t indexId, uint32_t curIndex)
    {
        LongSliceIndex *index = reinterpret_cast<LongSliceIndex *>(mBase) + indexId;
        if (UNLIKELY(index->count == 0)) {
            index->firstIndex = curIndex;
        }
        index->count++;
    }

private:
    IndexPut mPut;
    IndexGet mGet;
    uint32_t mIndexWidth = NO_4;
    uint32_t mIndexCount = 0;
    uint8_t *mBase = nullptr;
};

using IndexSpaceRef = std::shared_ptr<IndexSpace>;

template <typename T> class FixedSizeSpace {
public:
    FixedSizeSpace(ByteBufferRef &buffer, uint32_t offset)
    {
        mValue = reinterpret_cast<T *>(buffer->Data() + offset);
    }

    inline T Get(uint32_t index)
    {
        return mValue[index];
    }

    inline void Put(uint32_t index, T value)
    {
        mValue[index] = value;
    }

    inline T *GetDataPtr()
    {
        return mValue;
    }

private:
    T *mValue;
};

using SortedIndexSpace = FixedSizeSpace<uint32_t>;
using SortedIndexSpaceRef = std::shared_ptr<SortedIndexSpace>;

using HashCodeSpace = FixedSizeSpace<uint32_t>;
using HashCodeSpaceRef = std::shared_ptr<HashCodeSpace>;

using SeqIdSpace = FixedSizeSpace<uint64_t>;
using SeqIdSpaceRef = std::shared_ptr<SeqIdSpace>;

class KeySpace {
public:
    KeySpace(ByteBufferRef &buffer, uint32_t offset, uint32_t keyCount) : mBuffer(buffer)
    {
        mKeyOffsets = (uint32_t *)(buffer->Data() + offset);
        mKeyDataBaseOffset = offset + keyCount * sizeof(uint32_t);
        mKeyData = buffer->Data() + mKeyDataBaseOffset;
    }

    inline void Get(uint32_t index, uint8_t *&data, uint32_t &keyLen)
    {
        uint32_t keyStartOffset = (index == 0) ? 0 : mKeyOffsets[index - 1];
        uint32_t keyEndOffset = mKeyOffsets[index];
        keyLen = keyEndOffset - keyStartOffset;
        data = mKeyData + keyStartOffset;
    }

    template <typename SliceKey> inline void Get(uint32_t index, uint32_t mixedHashCode, SliceKey &sliceKey)
    {
        uint32_t keyStartOffset = (index == 0) ? 0 : mKeyOffsets[index - 1];
        uint32_t keyEndOffset = mKeyOffsets[index];
        sliceKey.Unpack(mKeyData + keyStartOffset, keyEndOffset - keyStartOffset, mixedHashCode);
    }

    inline const SlicePriKey *GetPriKey(uint32_t index)
    {
        uint32_t keyStartOffset = (index == 0) ? 0 : mKeyOffsets[index - 1];
        return reinterpret_cast<SlicePriKey *>(mKeyData + keyStartOffset);
    }

    void Get(uint32_t index, uint32_t mixHashCode, SliceKey &sliceKey);

    inline BResult Put(uint32_t index, const SliceKey &key)
    {
        // key
        uint32_t keyLen = 0;
        RETURN_NOT_OK(key.Serialize(mBuffer, mKeyDataBaseOffset + mWritenBytes, keyLen));
        mWritenBytes += keyLen;

        // key offset
        mKeyOffsets[index] = mWritenBytes;
        return BSS_OK;
    }

    inline void Put(uint32_t index, BinaryKey &key)
    {
        // key
        uint32_t keyLen = key.Serialize(mBuffer, mKeyDataBaseOffset + mWritenBytes);
        mWritenBytes += keyLen;

        // key offset
        mKeyOffsets[index] = mWritenBytes;
    }

    inline uint32_t *GetKeyOffsetBase()
    {
        return mKeyOffsets;
    }

    inline uint8_t *GetKeyDataBase()
    {
        return mKeyData;
    }

    inline void ReleaseByteBuffer()
    {
        mBuffer = nullptr;
    }

private:
    ByteBufferRef mBuffer = nullptr;
    uint32_t *mKeyOffsets = nullptr;
    uint8_t *mKeyData = nullptr;
    uint32_t mKeyDataBaseOffset = 0;
    uint32_t mWritenBytes = 0;
};

using KeySpaceRef = std::shared_ptr<KeySpace>;

class ValueSpace {
public:
    ValueSpace(ByteBufferRef &buffer, uint32_t offset, uint32_t valueCount) : mBuffer(buffer)
    {
        mValueOffsets = reinterpret_cast<uint32_t *>(buffer->Data() + offset);
        mValueDataBaseOffset = offset + valueCount * sizeof(uint32_t);
        mValueData = buffer->Data() + mValueDataBaseOffset;
    }

    Value Get(uint32_t index, uint64_t seqId)
    {
        uint32_t startValueOffset;
        Value value;
        uint32_t valueIndicate = mValueOffsets[index];
        auto valueType = static_cast<ValueType>(valueIndicate >> VALUE_INDICATOR_OFFSET);
        if (UNLIKELY(valueType == DELETE)) {
            value.Init(DELETE, 0, nullptr, seqId, nullptr);
            return value;
        }
        uint32_t endValueOffset = valueIndicate & 0xFFFFFFF;

        startValueOffset = (index == 0) ? 0 : mValueOffsets[index - 1] & 0xFFFFFFF;
        value.Init(valueType, endValueOffset - startValueOffset,
                   mBuffer->Data() + mValueDataBaseOffset + startValueOffset, seqId, mBuffer);
        return value;
    }

    inline BResult Put(uint32_t index, const Value &value)
    {
        // value;
        if (UNLIKELY(value.ValueType() != DELETE)) {
            uint8_t *valueData = mValueData + mWritenBytes;
            auto ret = memcpy_s(valueData, value.ValueLen(), value.ValueData(), value.ValueLen());
            if (UNLIKELY(ret != EOK)) {
                LOG_ERROR("Value Space Put failed, ret: " << ret << ".");
                return BSS_ERR;
            }
            mWritenBytes += value.ValueLen();
        }

        // value offset;
        mValueOffsets[index] = (value.ValueType() << VALUE_INDICATOR_OFFSET) | (mWritenBytes & 0xFFFFFFF);
        return BSS_OK;
    }

    BResult Put(uint32_t index, FreshValueNodePtr &value, const MemorySegment &freshSegment, bool isValue,
                SliceTableManagerRef sliceTable, uint32_t keyHashCode, uint16_t stateId, uint64_t &seqId);

    inline void ReleaseByteBuffer()
    {
        mBuffer = nullptr;
    }

private:
    ByteBufferRef mBuffer = nullptr;
    uint32_t *mValueOffsets = nullptr;
    uint8_t *mValueData = nullptr;
    uint32_t mValueDataBaseOffset = 0;
    uint32_t mWritenBytes = 0;
};

using ValueSpaceRef = std::shared_ptr<ValueSpace>;

class Slice : public std::enable_shared_from_this<Slice> {
public:
    /*
     * slice初始化，将kvPairs写到ByteBuffer中
     */
    BResult Initialize(std::vector<std::pair<SliceKey, Value>> &kvPairs, const SliceCreateMeta &meta,
                       const MemManagerRef &memManager, bool forceMemory = true);

    BResult Initialize(RawDataSlice &rawDataSlice, const SliceCreateMeta &meta, const MemManagerRef &memManager,
                       bool &forceEvict, SliceTableManagerRef sliceTable);

    /**
     * Obtain the value by using the keys from slice
     * @param key dual key
     * @param value value, nullptr if it is not exist.
     * @return Returns true if it exists or has already been deleted, false otherwise.
     */
    bool Get(const Key &key, Value &value);

    /**
     * 获取slice大小，单位byte
     */
    BResult BytesSize(uint32_t &size);

    /**
     * 获取slice中的buffer实例
     */
    inline ByteBufferRef GetByteBuffer()
    {
        return mInit ? mBuffer : nullptr;
    }

    void GetSliceKVMap(SliceKVMap &dataMap, bool skipDeleted);

    KeyValueIteratorRef SubIterator(const Key &prefixKey, bool skipDeleted);

    inline uint32_t KeyCount()
    {
        return mHeader->keyCount;
    }

    inline uint32_t GetSortedIndex(uint32_t slot)
    {
        return mSortedIndexSpace->Get(slot);
    }

    SliceKey GetBinaryKey(uint32_t keyCursor);

    uint32_t GetCompactionCount();

    void ReleaseByteBuffer();

    void RestoreSliceUseByteBuffer(ByteBufferRef byteBuffer, MemManagerRef memManager);

    uint32_t FindStartSortedIndexSlot(const Key &startKey);

    uint32_t FindEndSortedIndexSlot(const Key &endKey);

    const SliceSpace &GetSliceSpace()
    {
        return mSliceSpace;
    }

private:
    BResult CreateAndInitBuffer(const SliceCreateMeta &meta, std::vector<std::pair<SliceKey, Value>> &kvPairs,
                                std::vector<std::pair<SliceKey, uint32_t>> &sortedKeySlotList, bool forceMemory);

    BResult FillBuffer(const std::vector<std::pair<SliceKey, Value>> &kvPairs,
                       std::vector<std::pair<SliceKey, uint32_t>> &sortedKeySlotList);

    inline BResult PutKv(uint32_t curIndex, const std::pair<SliceKey, Value> &kv)
    {
        auto &key = kv.first;
        auto &value = kv.second;

        // update index;
        uint32_t mixHashCode = key.MixedHashCode();
        uint32_t indexId = mixHashCode & (mHeader->indexCount - 1);
        mIndexSpace->Put(indexId, curIndex);

        // hash code
        mHashCodeSpace->Put(curIndex, mixHashCode);

        // seqId;
        mSeqIdSpace->Put(curIndex, value.SeqId());

        // key
        RETURN_NOT_OK(mKeySpace->Put(curIndex, key));

        // value
        RETURN_NOT_OK(mValueSpace->Put(curIndex, value));
        return BSS_OK;
    }

    static void FormatHeader(SliceHead *header, const SliceCreateMeta &meta, uint32_t keyCount, uint32_t indexCount,
                             uint32_t sortedKeyCount, uint32_t keyOffsetBase, uint32_t valueOffsetBase,
                             StateIdInterval stateIdInterval);

    BResult CreateAndInitBuffer(const SliceCreateMeta &meta, RawDataSlice &rawDataSlice,
                                std::vector<std::pair<BinaryKey, uint32_t>> &sortedKeySlotList, bool &forceEvict,
                                SliceTableManagerRef sliceTable);

    BResult FillBuffer(RawDataSlice &rawDataSlice, std::vector<std::pair<BinaryKey, uint32_t>> &sortedKeySlotList,
                       SliceTableManagerRef sliceTable);

    BResult BinarySearchBound(uint32_t targetMixedHashCode, uint32_t startSlot, uint32_t indexCount,
                              uint32_t &lowerBound, uint32_t &upperBound);

    inline bool BinarySearch(const Key &key, uint32_t targetMixedHashCode, uint32_t startSlot, uint32_t indexCount,
                             Value &value)
    {
        uint32_t lowerBound = 0;
        uint32_t upperBound = 0;
        bool found = BinarySearchBound(targetMixedHashCode, startSlot, indexCount, lowerBound, upperBound);
        if (!found) {
            return false;
        }

        uint32_t endSlot = startSlot + upperBound;
        Key sliceKey;
        for (uint32_t slotIndex = startSlot + lowerBound; slotIndex < endSlot; slotIndex++) {
            mSliceSpace.GetKey(slotIndex, sliceKey);
            if (sliceKey.Equal(key)) {
                mSliceSpace.GetValue(slotIndex, value);
                return true;
            }
        }
        return false;
    }

    inline bool LinearSearch(const Key &key, uint32_t targetMixedHashCode, uint32_t startSlot, uint32_t indexCount,
                             Value &value)
    {
        uint32_t endSlot = startSlot + indexCount;
        uint32_t mixedHashCode;
        Key sliceKey;
        for (uint32_t slotIndex = startSlot; slotIndex < endSlot; slotIndex++) {
            mixedHashCode = mSliceSpace.GetKeyMixedHashCode(slotIndex);
            if (mixedHashCode != targetMixedHashCode) {
                continue;
            }

            mSliceSpace.GetKey(slotIndex, sliceKey);
            if (sliceKey.Equal(key)) {
                mSliceSpace.GetValue(slotIndex, value);
                return true;
            }
        }
        return false;
    }

    inline std::string ToString(const std::pair<BinaryKey, FreshValueNodePtr> &kv)
    {
        std::ostringstream oss;
        oss << "Key: {" << kv.first.ToString() << "}, Value: {" << kv.second->ToString() << "}";
        return oss.str();
    }

private:
    ByteBufferRef mBuffer = nullptr;

    SliceHead *mHeader = nullptr;
    HashCodeSpaceRef mHashCodeSpace = nullptr;
    SeqIdSpaceRef mSeqIdSpace = nullptr;
    IndexSpaceRef mIndexSpace = nullptr;
    SortedIndexSpaceRef mSortedIndexSpace = nullptr;
    KeySpaceRef mKeySpace = nullptr;
    ValueSpaceRef mValueSpace = nullptr;
    MemManagerRef mMemManager = nullptr;

    // todo: refactor me.
    SliceSpace mSliceSpace;
    uint32_t mTimeWaitMemory = 100;
    bool mInit = false;
};
using SliceRef = std::shared_ptr<Slice>;

}  // namespace bss
}  // namespace ock

#endif  // BOOST_SS_SLICE_H
