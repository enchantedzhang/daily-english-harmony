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

#ifndef BOOST_SS_HASH_DATA_BLOCK_H
#define BOOST_SS_HASH_DATA_BLOCK_H

#include "binary/lsm_binary.h"
#include "common/block.h"
#include "common/util/var_encoding_util.h"
#include "data_block_writer.h"
#include "lsm_store/key/primary_key_info.h"
#include "lsm_store/key/secondary_level_info.h"

namespace ock {
namespace bss {
enum class DataBlockType : uint8_t { HASH };

class DataBlock : public Block, public std::enable_shared_from_this<DataBlock> {
public:
    explicit DataBlock(ByteBufferRef &buffer) : Block(buffer)
    {
    }

    BResult Init(const MemManagerRef &memManager, FileProcHolder holder)
    {
        mMemManager = memManager;
        mHolder = holder;
        uint32_t capacity = mBuffer->Capacity();
        if (UNLIKELY(capacity < NO_10)) {
            LOG_ERROR("Buffer capacity should not less than NO_10.");
            return BSS_ERR;
        }
        mBuffer->ReadUint32(mPrimaryKeyIndexOffset, capacity - NO_10);
        mBuffer->ReadAt(reinterpret_cast<uint8_t *>(&mPrimaryKeyIndexElementSize), NO_1, capacity - NO_6);
        mBuffer->ReadUint32(mNumPrimaryKey, capacity - NO_5);
        mBinarySearchThreshold = NO_32 - BssMath::NumberOfLeadingZeros(mNumPrimaryKey);
        mIndexReader = InitIndexReader();
        return BSS_OK;
    }

    ~DataBlock() override = default;

    BlockType GetBlockType() override
    {
        return BlockType::DATA;
    }

    DataBlockIndexReaderRef InitIndexReader();

    BResult GetKey(const Key &key, Value &value);

    inline uint32_t GetKeyIndexElement(uint32_t indexOffset, uint32_t numBytesForElement, uint32_t index) override
    {
        return static_cast<uint32_t>(FullKeyUtil::ReadValueWithNumberOfBytes(mBuffer,
                                                                             indexOffset + numBytesForElement * index,
                                                                             numBytesForElement));
    }

    inline uint32_t GetKeyIndexElement(uint32_t index)
    {
        return static_cast<uint32_t>(FullKeyUtil::ReadValueWithNumberOfBytes(
            mBuffer, mPrimaryKeyIndexOffset + mPrimaryKeyIndexElementSize * index, mPrimaryKeyIndexElementSize));
    }

    inline static bool IsSingleSecondaryKey(uint8_t flag)
    {
        return (flag & 1) == 0;
    }

    inline static uint32_t GetNumBytesForSecondaryKeyIndexElement(uint8_t flag)
    {
        return static_cast<uint32_t>(flag >> 1);
    }

    BResult FindKeyValueInfo(const Key &key, LsmKeyValueInfo &keyValueInfo);

    BResult FindPrimaryKey(const Key &key, PrimaryKeyInfo &primaryKeyInfo, SecondaryLevelInfoRef &secondaryLevelInfo);

    SecondaryLevelInfoRef GetSecondaryLevelInfo(PrimaryKeyInfo &primaryKeyInfo);

    KeyValueIteratorRef Iterator(KeyFilter keyFilter = nullptr);

    KeyValueIteratorRef SubIterator(const Key &startKey, const Key &endKey, bool reverseOrder);

    void BuildPrimaryInfo(PrimaryKeyInfo &primaryKeyInfo, uint32_t midPrimaryKeyIndex, uint32_t primaryKeyOffset,
                          uint64_t primaryKeyLenInfo);

private:
    BResult LinearSearch(const Key &key, Value &value, DataBlockIndexIterator &iterator);
    BResult BinarySearch(const Key &key, Value &value);
    int32_t BinarySearch(const LsmKeyValueInfo &keyValueInfo, const Key &key, int32_t leftIndex, int32_t rightIndex);

private:
    uint32_t mPrimaryKeyIndexOffset = 0;
    uint32_t mPrimaryKeyIndexElementSize = 0;
    uint32_t mNumPrimaryKey = 0;
    uint32_t mBinarySearchThreshold = 0;
    DataBlockIndexReaderRef mIndexReader = nullptr;
    MemManagerRef mMemManager = nullptr;
    FileProcHolder mHolder;
};
using DataBlockRef = std::shared_ptr<DataBlock>;

class DataBlockSubIterator : public Iterator<KeyValueRef> {
public:
    DataBlockSubIterator(LsmKeyValueInfo &&keyValueInfo, int32_t leftIndex, int32_t rightIndex, bool reverseOrder)
        : mKeyValueInfo(std::move(keyValueInfo)),
          mLeftIndex(leftIndex),
          mRightIndex(rightIndex),
          mCurrentIndex(reverseOrder ? rightIndex : leftIndex),
          mReverseOrder(reverseOrder),
          mHasNext(leftIndex <= rightIndex)
    {
    }

    bool HasNext() override
    {
        return mHasNext;
    }

    KeyValueRef Next() override
    {
        if (UNLIKELY(!mHasNext)) {
            LOG_ERROR("No more elements in the data block sub iterator.");
            return {};
        }
        auto keyValue = MakeRef<KeyValue>();
        BResult result = mKeyValueInfo.GetKeyAndValue(static_cast<uint32_t>(mCurrentIndex), keyValue);
        if (UNLIKELY(result != BSS_OK)) {
            LOG_ERROR("Get key and value failed, result:" << result << ", secKeyIndex:" << mCurrentIndex);
            mHasNext = false;
            return nullptr;
        }
        if (mReverseOrder) {
            mHasNext = (mCurrentIndex != mLeftIndex);
            --mCurrentIndex;
        } else {
            mHasNext = (mCurrentIndex != mRightIndex);
            ++mCurrentIndex;
        }
        return keyValue;
    }

    void Close() override
    {
    }

private:
    LsmKeyValueInfo mKeyValueInfo;
    int32_t mLeftIndex;
    int32_t mRightIndex;
    int32_t mCurrentIndex;
    bool mReverseOrder;
    bool mHasNext;
};

}  // namespace bss
}  // namespace ock
#endif  // BOOST_SS_HASH_DATA_BLOCK_H