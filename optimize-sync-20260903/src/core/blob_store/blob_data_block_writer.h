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

#ifndef BLOB_DATA_BLOCK_WRITER_H
#define BLOB_DATA_BLOCK_WRITER_H

#include <cstdint>
#include <vector>

#include "blob_data_block.h"
#include "blob_data_block_meta.h"
#include "buffer.h"
#include "util/bss_lock.h"

namespace ock {
namespace bss {

struct BlobIndexEntry {
    uint64_t mBlobId = 0;
    uint64_t mSeqId = 0;
    uint32_t mOffset = 0;

    BlobIndexEntry(uint64_t blobId, uint64_t seqId, uint32_t offset)
    {
        mBlobId = blobId;
        mSeqId = seqId;
        mOffset = offset;
    }
};

class BlobDataBlockWriter {
public:
    ~BlobDataBlockWriter() = default;
    explicit BlobDataBlockWriter(const ByteBufferRef &buffer) : mBuffer(buffer)
    {
        mCapacity = buffer->Capacity();
        mBlobDataBlockMeta = std::make_shared<BlobDataBlockMeta>();
    }

    inline bool IsEmpty() const
    {
        return mPosition == 0;
    }

    inline bool CheckBlobSize(uint32_t size)
    {
        ReadLocker<ReadWriteLock> lk(&mRwLock);
        if (UNLIKELY(UINT32_MAX - mPosition - size < mIndexEntryList.size() * BLOB_INDEX_ENTRY_STRUCT_SIZE)) {
            LOG_ERROR("The value crosses the boundary and exceeds the UINT32_MAX maximum, mPosition: "
                      << mPosition << ", size: " << size << ", index vec size: " << mIndexEntryList.size()
                      << ", blob block header size: " << BLOB_DATA_BLOCK_HEADER_SIZE);
            return false;
        }
        auto curSize = mPosition + size + mIndexEntryList.size() * BLOB_INDEX_ENTRY_STRUCT_SIZE;
        return curSize <= mCapacity;
    }

    inline uint32_t GetCapacity() const
    {
        return mCapacity;
    }

    static inline uint64_t GenerateSeqId(uint64_t expireTime, uint32_t keyGroup)
    {
        return (expireTime << 16U) | (keyGroup & 0xFFFFU);
    }

    inline void AddIndexEntry(const BlobIndexEntry &entry)
    {
        WriteLocker<ReadWriteLock> lk(&mRwLock);
        mIndexEntryList.emplace_back(entry);
    }

    // 拷贝防止IndexEntryList变化
    inline std::vector<BlobIndexEntry> GetIndexEntryList()
    {
        ReadLocker<ReadWriteLock> lk(&mRwLock);
        return mIndexEntryList;
    }

    BResult Write(uint64_t blobId, const uint8_t *data, uint32_t length, uint64_t expireTime, uint32_t keyGroup);

    BResult Write(uint64_t blobId, const uint8_t *data, uint32_t length, uint64_t seqId);

    BlobDataBlockRef WriteBlobDataBlock();

    using BufferAllocator = std::function<BufferRef(uint32_t size)>;

    BufferRef CopyNewBufferFromBuffer(BufferAllocator &allocator, uint32_t offset, uint32_t length);

    BResult SelectBlobValue(uint64_t blobId, BufferAllocator allocator, Value &value);

private:
    ByteBufferRef mBuffer = nullptr;
    uint32_t mCapacity = 0;
    uint32_t mPosition = 0;
    std::vector<BlobIndexEntry> mIndexEntryList;
    ReadWriteLock mRwLock;
    BlobDataBlockMetaRef mBlobDataBlockMeta = nullptr;
};
using BlobDataBlockWriterRef = std::shared_ptr<BlobDataBlockWriter>;
}  // namespace bss
}  // namespace ock

#endif