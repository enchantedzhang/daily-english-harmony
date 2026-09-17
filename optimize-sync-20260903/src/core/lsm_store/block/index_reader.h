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

#ifndef BOOST_SS_INDEX_READER_H
#define BOOST_SS_INDEX_READER_H

#include <memory>
#include <vector>

#include "block_handle.h"
#include "common/block.h"
#include "common/io/output_view.h"
#include "file/file_reader_base.h"
#include "index_block_hash_index_writer.h"
#include "lsm_store/key/full_key_util.h"

namespace ock {
namespace bss {

class BlockHandleIterator;
using BlockHandleIteratorRef = Ref<BlockHandleIterator>;
class IndexReader;
using IndexReaderRef = std::shared_ptr<IndexReader>;

class IndexReader : public std::enable_shared_from_this<IndexReader> {
public:
    IndexReader(const MemManagerRef &memManager, FileProcHolder holder, const FileReaderBaseRef &fileReader)
        : mMemManager(memManager), mHolder(holder), mFileReader(fileReader)

    {
    }

    inline BResult Init()
    {
        ByteBufferRef indexBlockBuffer = GetIndexBlockBuffer();
        RETURN_ERROR_AS_NULLPTR(indexBlockBuffer);
        mNumDataBlocks = GetNumBlocks(indexBlockBuffer);
        if (UNLIKELY(indexBlockBuffer->Capacity() < NO_16)) {
            LOG_ERROR("capacity should not less than NO_16.");
            return BSS_ERR;
        }
        mHashIndexReader = std::make_shared<HashIndexReader>(indexBlockBuffer, NO_4);

        uint32_t offset = mHashIndexReader->GetBucketOffset();
        uint8_t bytesForEndKeyIndex = 0;
        RETURN_NOT_OK(indexBlockBuffer->ReadUint8(bytesForEndKeyIndex, offset - 1));
        mNumBytesForEndKeyIndex = bytesForEndKeyIndex;
        mEndKeyIndexOffset = offset - 1 - mNumDataBlocks * mNumBytesForEndKeyIndex;
        return BSS_OK;
    }

    ByteBufferRef GetIndexBlockBuffer() const
    {
        IndexBlockRef indexBlock = mFileReader->GetOrLoadIndexBlock();
        RETURN_NULLPTR_AS_NULLPTR(indexBlock);
        return indexBlock->GetBuffer();
    }

    ~IndexReader() = default;

    inline uint32_t GetNumBlocks(const ByteBufferRef &buffer)
    {
        uint32_t numBlocks = 0;
        auto ret = buffer->ReadUint32(numBlocks, buffer->Capacity() - NO_4);
        if (UNLIKELY(ret != BSS_OK)) {
            LOG_ERROR("Read numBlock fail:" << ret);
        }
        return numBlocks;
    }

    HashIndexReaderRef GetHashIndexReader() const
    {
        return mHashIndexReader;
    }

    uint32_t GetBlockIndex(const Key &key, const ByteBufferRef &buffer);

    BResult GetDataBlock(const Key &key, BlockHandle &blockHandle);

    BResult GetBlockAt(uint32_t index, BlockHandle &blockHandle, const ByteBufferRef &buffer);

    BlockHandleIteratorRef SubIterator(const Key &startKey, const Key &endKey, bool reverseOrder);

    BlockHandleIteratorRef IteratorInner(bool reverseOrder);

    uint32_t GetFirstBlockIndexNoLessThan(const Key &lookupKey, const ByteBufferRef &buffer)
    {
        uint32_t startBlockIndex = 0;
        uint32_t numBlocks = mNumDataBlocks;

        if (mHashIndexReader != nullptr) {
            uint64_t bucketValue = mHashIndexReader->GetBucket(lookupKey.PriKey().KeyHashCode(), buffer);
            if (bucketValue != UINT64_MAX) {
                startBlockIndex = bucketValue >> 32L;
                numBlocks = bucketValue;
            }
        }

        uint32_t endBlockIndex = startBlockIndex + numBlocks;
        uint32_t index = BinarySearchFirstBlockNoLessThan(lookupKey, startBlockIndex, endBlockIndex, buffer);
        if (index == INVALID_U32_INDEX && endBlockIndex != mNumDataBlocks) {
            index = endBlockIndex;
        }

        return index;
    }

private:
    inline uint32_t GetEndKeyOffsetAt(uint32_t blockIndex, const ByteBufferRef &buffer) const
    {
        return static_cast<uint32_t>(
            FullKeyUtil::ReadValueWithNumberOfBytes(buffer, mEndKeyIndexOffset + blockIndex * mNumBytesForEndKeyIndex,
                                                    mNumBytesForEndKeyIndex));
    }

    uint32_t BinarySearchFirstBlockNoLessThan(const Key &lookupKey, uint32_t startBlockIndex, uint32_t endBlockIndex,
                                              const ByteBufferRef &buffer);

private:
    uint32_t mNumDataBlocks{ 0 };
    uint32_t mNumBytesForEndKeyIndex{ 0 };
    uint32_t mEndKeyIndexOffset{ 0 };
    HashIndexReaderRef mHashIndexReader = nullptr;
    MemManagerRef mMemManager = nullptr;
    FileProcHolder mHolder;
    FileReaderBaseRef mFileReader;
};

class BlockHandleIterator : public Iterator<BlockHandleRef> {
public:
    BlockHandleIterator(uint32_t startIndex, uint32_t endIndex, bool reverseOrder, const IndexReaderRef &indexReader,
                        const ByteBufferRef &indexBlockBuffer)
        : mStartIndex(startIndex),
          mEndIndex(endIndex),
          mReverseOrder(reverseOrder),
          mNextIndex(reverseOrder ? endIndex : startIndex),
          mIndexReader(indexReader),
          mIndexBlockBuffer(indexBlockBuffer)
    {
    }

    bool HasNext() override;

    BlockHandleRef Next() override;

private:
    uint32_t mStartIndex;
    uint32_t mEndIndex;
    bool mReverseOrder;
    uint32_t mNextIndex;
    IndexReaderRef mIndexReader;
    ByteBufferRef mIndexBlockBuffer;
};

}  // namespace bss
}  // namespace ock

#endif  // BOOST_SS_INDEX_READER_H