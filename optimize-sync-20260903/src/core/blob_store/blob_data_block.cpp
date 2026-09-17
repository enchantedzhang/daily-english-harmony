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

#include "blob_data_block.h"

namespace ock {
namespace bss {
BResult BlobDataBlock::Init(const BlobDataBlockMetaRef &blobDataBlockMeta, uint32_t length)
{
    RETURN_ERROR_AS_NULLPTR(blobDataBlockMeta);
    mBlobDataBlockMeta = blobDataBlockMeta;
    mLength = length;
    RETURN_ERROR_AS_NULLPTR(mBuffer);
    uint32_t capacity = mBuffer->Capacity();
    RETURN_NOT_OK_AS_FALSE(capacity < BLOB_DATA_BLOCK_HEADER_SIZE, BSS_ERR);
    BResult result = mBuffer->ReadUint32(mBlobBlockNum, capacity - BLOB_INDEX_ENTRY_NUMS_OFFSET);
    RETURN_NOT_OK(result);
    blobDataBlockMeta->SetBlobNum(mBlobBlockNum);
    result = mBuffer->ReadUint32(mIndexOffset, capacity - BLOB_INDEX_ENTRY_BUFFER_OFFSET);
    RETURN_NOT_OK(result);
    RETURN_NOT_OK_AS_FALSE(mIndexOffset > mLength, BSS_ERR);
    return BSS_OK;
}

BResult BlobDataBlock::Init(const BlobDataBlockMetaRef &blobDataBlockMeta, uint32_t indexOffset, uint32_t length)
{
    RETURN_ERROR_AS_NULLPTR(blobDataBlockMeta);
    mBlobDataBlockMeta = blobDataBlockMeta;
    mLength = length;
    mBlobBlockNum = mBlobDataBlockMeta->mBlobBlockNum;
    mIndexOffset = indexOffset;
    return BSS_OK;
}

BufferRef BlobDataBlock::CopyNewBufferFromBuffer(const BufferAllocator &allocator, uint32_t offset, uint32_t length)
{
    BufferRef buffer = allocator(length);
    if (UNLIKELY(buffer == nullptr)) {
        LOG_WARN("Failed to allocate buffer, length:" << length);
        return nullptr;
    }
    // copy value to buffer.
    errno_t ret = memcpy_s(buffer->Data(), length, mBuffer->Data() + offset, length);
    if (UNLIKELY(ret != EOK)) {
        LOG_ERROR("memcpy_s failed for data block, ret: " << ret);
        return nullptr;
    }
    return buffer;
}

BResult BlobDataBlock::BinarySearchBlobValue(uint64_t blobId, BufferAllocator allocator, Value &value)
{
    RETURN_ERROR_AS_NULLPTR(mBlobDataBlockMeta);
    if (blobId < mBlobDataBlockMeta->mMinBlobId || blobId > mBlobDataBlockMeta->mMaxBlobId) {
        return BSS_NOT_FOUND;
    }
    RETURN_NOT_OK_AS_FALSE(mBlobBlockNum == NO_0, BSS_INNER_ERR);
    uint32_t low = 0;
    uint32_t high = mBlobBlockNum - NO_1;
    while (low <= high) {
        uint32_t mid = low + ((high - low) >> NO_1);
        uint64_t midBlobId = 0;
        RETURN_ERROR_AS_NULLPTR(mBuffer);
        auto ret = mBuffer->ReadUint64(midBlobId, mIndexOffset + mid * BLOB_INDEX_ENTRY_STRUCT_SIZE);
        RETURN_NOT_OK(ret);
        if (blobId > midBlobId) {
            low = mid + NO_1;
            continue;
        }
        if (blobId < midBlobId) {
            high = mid - NO_1;
            continue;
        }
        uint32_t offset = 0;
        ret = mBuffer->ReadUint32(offset,
                                  mIndexOffset + mid * BLOB_INDEX_ENTRY_STRUCT_SIZE + BLOB_DATA_BLOCK_HEADER_SIZE);
        RETURN_NOT_OK(ret);
        uint32_t nextOffset = 0;
        if (mid == mBlobBlockNum - NO_1) {
            nextOffset = mIndexOffset;
        } else {
            ret = mBuffer->ReadUint32(nextOffset, mIndexOffset + mid * BLOB_INDEX_ENTRY_STRUCT_SIZE +
                                                      BLOB_INDEX_ENTRY_STRUCT_SIZE + BLOB_DATA_BLOCK_HEADER_SIZE);
            RETURN_NOT_OK(ret);
        }
        if (UNLIKELY(nextOffset <= offset)) {
            LOG_ERROR("Invalid numeric value, offset: " << offset << ", nextOffset: " << nextOffset);
            return BSS_INNER_ERR;
        }
        uint32_t valueLen = nextOffset - offset;
        BufferRef buffer = CopyNewBufferFromBuffer(allocator, offset, valueLen);
        RETURN_ERROR_AS_NULLPTR(buffer);
        value.Init(PUT, valueLen, buffer->Data(), NO_0, buffer);
        return BSS_OK;
    }
    return BSS_INNER_ERR;
}

BResult BlobDataBlock::GetBlobValueWrapper(BlobValueWrapper &blobValueWrapper, uint32_t index,
                                           BufferAllocator allocator)
{
    RETURN_NOT_OK_AS_FALSE(UINT32_MAX - index * NO_20 < mIndexOffset, BSS_INNER_ERR);
    uint32_t base = mIndexOffset + index * NO_20;
    RETURN_NOT_OK_AS_FALSE(UINT32_MAX - base < NO_28, BSS_INNER_ERR);
    uint64_t blobId = 0;
    RETURN_INVALID_PARAM_AS_NULLPTR(mBuffer);
    auto ret = mBuffer->ReadUint64(blobId, base);
    RETURN_NOT_OK(ret);
    uint32_t offset = 0;
    ret = mBuffer->ReadUint32(offset, base + NO_8);
    RETURN_NOT_OK(ret);
    uint64_t seqId = 0;
    ret = mBuffer->ReadUint64(seqId, base + NO_12);
    RETURN_NOT_OK(ret);
    uint32_t nextOffset = 0;
    RETURN_NOT_OK_AS_FALSE(mBlobBlockNum == NO_0, BSS_INNER_ERR);
    if (index == mBlobBlockNum - NO_1) {
        nextOffset = mIndexOffset;
    } else {
        ret = mBuffer->ReadUint32(nextOffset, base + NO_28);
        RETURN_NOT_OK(ret);
    }
    if (UNLIKELY(nextOffset <= offset)) {
        LOG_ERROR("Invalid numeric value, offset: " << offset << ", nextOffset: " << nextOffset);
        return BSS_INNER_ERR;
    }
    uint32_t valueLen = nextOffset - offset;
    BufferRef buffer = CopyNewBufferFromBuffer(allocator, offset, valueLen);
    RETURN_ERROR_AS_NULLPTR(buffer);
    Value value;
    value.Init(PUT, valueLen, buffer->Data(), NO_0, buffer);
    blobValueWrapper.Init(value, blobId, seqId);
    return BSS_OK;
}

}  // namespace bss
}  // namespace ock
