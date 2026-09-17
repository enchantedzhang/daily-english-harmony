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

#ifndef BOOST_SS_HASH_DATA_BLOCK_INDEX_BUILDER_H
#define BOOST_SS_HASH_DATA_BLOCK_INDEX_BUILDER_H

#include <memory>
#include <vector>

#include "common/io/output_view.h"
#include "common/util/iterator.h"
#include "lsm_store/file/file_mem_allocator.h"
#include "lsm_store/key/full_key_util.h"
#include "slice_table/binary/byte_buffer.h"

namespace ock {
namespace bss {
class DataBlockIndexWriter {
public:
    DataBlockIndexWriter(float loadRatio, FileProcHolder holder) : mLoadRatio(loadRatio), mHolder(holder)
    {
    }

    inline uint32_t EstimateSize() const
    {
        uint32_t numEntries = mEntryPointer.size();
        uint32_t numBuckets = GetBucketNum(numEntries);
        uint32_t numBitsForPrimaryKeyIndex = FullKeyUtil::MinNumBitsToRepresentValue(mMaxPrimaryKeyIndex);
        uint32_t numBitsForSecondaryKeyIndex = FullKeyUtil::MinNumBitsToRepresentValue(mMaxSecondKeyIndex);
        uint32_t numBytesForPointer = (numBitsForSecondaryKeyIndex + numBitsForPrimaryKeyIndex + NO_8 - NO_1) / NO_8;
        return FullKeyUtil::MinNumBytesToRepresentValue(numEntries) * numBuckets + numEntries * numBytesForPointer +
               NO_12;
    }

    inline void Add(uint32_t primaryKeyIndex, uint32_t secondKeyIndex, uint32_t hash)
    {
        mEntryPointer.emplace_back((static_cast<uint64_t>(primaryKeyIndex) << NO_32) | secondKeyIndex);
        mEntryHash.emplace_back(hash);
        mMaxPrimaryKeyIndex = std::max(primaryKeyIndex, mMaxPrimaryKeyIndex);
        mMaxSecondKeyIndex = std::max(secondKeyIndex, mMaxSecondKeyIndex);
    }

    BResult Finish(const OutputViewRef &outputView);

    inline void Reset()
    {
        mEntryPointer.clear();
        mEntryHash.clear();
        mBucket.clear();
        mSortBucket.clear();
        mMaxPrimaryKeyIndex = 0;
        mMaxSecondKeyIndex = 0;
    }

    static uint32_t GetBucketIndex(uint32_t hash, uint32_t bucketNum);

    static uint64_t EncodePointer(uint64_t pointer, uint32_t numBitsForSecondaryKeyIndex);

    static uint64_t DecodePointer(uint64_t encodedPointer, uint32_t numBitsForSecondaryKeyIndex);

    static uint32_t GetPrimaryKeyIndexFromPointer(uint64_t pointer);

    static uint32_t GetSecondaryKeyIndexFromPointer(uint64_t pointer);

private:
    inline uint32_t GetBucketNum(uint32_t entrySize) const
    {
        auto numBuckets = static_cast<uint32_t>(entrySize / mLoadRatio);
        if (numBuckets == 0) {
            numBuckets = 1;
        }
        return numBuckets;
    }

private:
    float mLoadRatio = 0.75f;
    std::vector<uint64_t> mEntryPointer;
    std::vector<uint32_t> mEntryHash;
    std::vector<uint64_t> mBucket;
    std::vector<uint64_t> mSortBucket;
    uint32_t mMaxPrimaryKeyIndex = 0;
    uint32_t mMaxSecondKeyIndex = 0;
    FileProcHolder mHolder;
};
using DataBlockIndexWriterRef = std::shared_ptr<DataBlockIndexWriter>;

class DataBlockIndexIterator;

class DataBlockIndexReader {
public:
    BResult Init(const ByteBufferRef &buffer, uint32_t indexEndOffset)
    {
        if (UNLIKELY(indexEndOffset < (NO_12 + sizeof(uint32_t)))) {
            LOG_ERROR("Init hash data block index reader failed, indexEndOffer:" << indexEndOffset);
            return BSS_ERR;
        }
        RETURN_NOT_OK(buffer->ReadUint32(mNumEntries, indexEndOffset - NO_12));
        RETURN_NOT_OK(buffer->ReadUint32(mNumBuckets, indexEndOffset - NO_8));
        RETURN_NOT_OK(
            buffer->ReadAt(reinterpret_cast<uint8_t *>(&mNumBytesForBucketElement), NO_1, indexEndOffset - NO_4));
        RETURN_NOT_OK(buffer->ReadAt(reinterpret_cast<uint8_t *>(&mNumBytesForPointer), NO_1, indexEndOffset - NO_3));
        RETURN_NOT_OK(
            buffer->ReadAt(reinterpret_cast<uint8_t *>(&mNumBitsForSecondaryKeyIndex), NO_1, indexEndOffset - NO_1));
        mPointerIndexOffset = indexEndOffset - NO_12 - mNumBytesForPointer * mNumEntries;
        if (indexEndOffset - NO_12 < mNumBytesForBucketElement * mNumBuckets) {
            LOG_ERROR("Read error num, mNumBytesForBucketElement" << mNumBytesForBucketElement
                                                                  << ", mNumBuckets:" << mNumBuckets);
            return BSS_ERR;
        }
        if (UNLIKELY(UINT32_MAX - mPointerIndexOffset < mNumBuckets * mNumBytesForBucketElement)) {
            LOG_ERROR("Read error num, mNumBytesForBucketElement:" << mNumBytesForBucketElement
                                                                   << ", mNumBuckets:" << mNumBuckets
                                                                   << ", mPointerIndexOffset:" << mPointerIndexOffset);
            return BSS_ERR;
        }
        mBucketIndexOffset = mPointerIndexOffset - mNumBuckets * mNumBytesForBucketElement;
        return BSS_OK;
    }

    BResult Lookup(uint32_t hash, const ByteBufferRef &byteBuffer, DataBlockIndexIterator &iterator);

    uint64_t GetPointAt(uint32_t pointerIndex, const ByteBufferRef &byteBuffer) const;

private:
    uint32_t mNumEntries = 0;
    uint32_t mNumBuckets = 0;
    uint32_t mNumBytesForBucketElement = 0;
    uint32_t mNumBytesForPointer = 0;
    uint32_t mNumBitsForSecondaryKeyIndex = 0;
    uint32_t mPointerIndexOffset = 0;
    uint32_t mBucketIndexOffset = 0;
};
using DataBlockIndexReaderRef = std::shared_ptr<DataBlockIndexReader>;

class DataBlockIndexIterator : public Iterator<uint64_t> {
public:
    DataBlockIndexIterator() = default;

    void Init(DataBlockIndexReader *indexReader, const ByteBufferRef &buffer, uint32_t startIndex, uint32_t endIndex)
    {
        mIndexReader = indexReader;
        mBuffer = buffer;
        mNextIndex = startIndex;
        mStartIndex = startIndex;
        mEndIndex = endIndex;
    }

    bool HasNext() override
    {
        return mNextIndex < mEndIndex;
    }

    uint64_t Next() override
    {
        if (mNextIndex > mEndIndex) {
            return UINT64_MAX;
        }
        return mIndexReader->GetPointAt(mNextIndex++, mBuffer);
    }

    uint32_t Size() const
    {
        return mEndIndex - mStartIndex;
    }

private:
    DataBlockIndexReader *mIndexReader = nullptr;
    ByteBufferRef mBuffer = nullptr;
    uint32_t mNextIndex = 0;
    uint32_t mStartIndex = 0;
    uint32_t mEndIndex = 0;
};

}  // namespace bss
}  // namespace ock
#endif  // BOOST_SS_HASH_DATA_BLOCK_INDEX_BUILDER_H