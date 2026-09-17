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

#ifndef BLOB_DATA_BLOCK_H
#define BLOB_DATA_BLOCK_H

#include <binary/value/value.h>

#include "blob_data_block_meta.h"
#include "blob_structure.h"
#include "block.h"

namespace ock {
namespace bss {
struct BlobValueWrapper {
    BlobValueWrapper() = default;

    BlobValueWrapper(Value &blobValue, uint64_t blobId, uint64_t seqId)
        : mBlobValue(blobValue), mBlobId(blobId), mSeqId(seqId)
    {
    }

    inline void Init(Value &blobValue, uint64_t blobId, uint64_t seqId)
    {
        mBlobValue = blobValue;
        mBlobId = blobId;
        mSeqId = seqId;
    }

    inline uint32_t GetKeyGroup() const
    {
        return mSeqId & 0xFFFFU;
    }

public:
    Value mBlobValue;
    uint64_t mBlobId = 0;
    uint64_t mSeqId = 0;
};
using BlobValueWrapperRef = std::shared_ptr<BlobValueWrapper>;

class BlobDataBlock : public Block {
public:
    explicit BlobDataBlock(ByteBufferRef &buffer) : Block(buffer)
    {
    }

    BResult Init(const BlobDataBlockMetaRef &blobDataBlockMeta, uint32_t length);

    BResult Init(const BlobDataBlockMetaRef &blobDataBlockMeta, uint32_t indexOffset, uint32_t length);

    BlockType GetBlockType() override
    {
        return BlockType::BLOB;
    }

    using BufferAllocator = std::function<BufferRef(uint32_t size)>;

    BufferRef CopyNewBufferFromBuffer(const BufferAllocator &allocator, uint32_t offset, uint32_t length);

    BResult BinarySearchBlobValue(uint64_t blobId, BufferAllocator allocator, Value &value);

    BResult GetBlobValueWrapper(BlobValueWrapper &blobValueWrapper, uint32_t index, BufferAllocator allocator);

    inline BlobDataBlockMetaRef &GetBlockMeta()
    {
        return mBlobDataBlockMeta;
    }

private:
    BlobDataBlockMetaRef mBlobDataBlockMeta = nullptr;
    uint32_t mLength = 0;
    uint32_t mBlobBlockNum = 0;
    uint32_t mIndexOffset = 0;
};
using BlobDataBlockRef = std::shared_ptr<BlobDataBlock>;
}  // namespace bss
}  // namespace ock

#endif