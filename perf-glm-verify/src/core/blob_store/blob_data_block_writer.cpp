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

#include "blob_data_block_writer.h"

namespace ock {
namespace bss {
BResult BlobDataBlockWriter::Write(uint64_t blobId, const uint8_t *data, uint32_t length, uint64_t expireTime,
                                   uint32_t keyGroup)
{
    uint32_t metaSize = BLOB_INDEX_ENTRY_STRUCT_SIZE + BLOB_DATA_BLOCK_HEADER_SIZE;
    RETURN_NOT_OK_AS_FALSE(UINT32_MAX - length < metaSize, BSS_ERR);
    RETURN_NOT_OK_AS_FALSE(!CheckBlobSize(length + metaSize), BSS_ERR);
    RETURN_INVALID_PARAM_AS_NULLPTR(mBuffer);
    RETURN_NOT_OK(mBuffer->Write(data, length));
    BlobIndexEntry entry(blobId, GenerateSeqId(expireTime, keyGroup), mPosition);
    AddIndexEntry(entry);
    mPosition += length;
    RETURN_INVALID_PARAM_AS_NULLPTR(mBlobDataBlockMeta);
    if (blobId < mBlobDataBlockMeta->mMinBlobId) {
        mBlobDataBlockMeta->mMinBlobId = blobId;
    }
    if (UNLIKELY(blobId <= mBlobDataBlockMeta->mMaxBlobId)) {
        LOG_ERROR("The blobId is too small, max id: " << mBlobDataBlockMeta->mMaxBlobId << ", current id: " << blobId);
        return BSS_ERR;
    }
    mBlobDataBlockMeta->mMaxBlobId = blobId;
    mBlobDataBlockMeta->mBlobBlockNum++;
    mBlobDataBlockMeta->UpdateExpireTime(expireTime);
    return BSS_OK;
}

BResult BlobDataBlockWriter::Write(uint64_t blobId, const uint8_t *data, uint32_t length, uint64_t seqId)
{
    RETURN_NOT_OK_AS_FALSE(!CheckBlobSize(length + BLOB_INDEX_ENTRY_STRUCT_SIZE + BLOB_DATA_BLOCK_HEADER_SIZE),
                           BSS_ERR);
    RETURN_INVALID_PARAM_AS_NULLPTR(mBuffer);
    RETURN_NOT_OK(mBuffer->Write(data, length));
    BlobIndexEntry entry(blobId, seqId, mPosition);
    AddIndexEntry(entry);
    mPosition += length;
    RETURN_INVALID_PARAM_AS_NULLPTR(mBlobDataBlockMeta);
    if (blobId < mBlobDataBlockMeta->mMinBlobId) {
        mBlobDataBlockMeta->mMinBlobId = blobId;
    }
    if (UNLIKELY(blobId <= mBlobDataBlockMeta->mMaxBlobId)) {
        LOG_ERROR("The blobId is too small, max id: " << mBlobDataBlockMeta->mMaxBlobId << ", current id: " << blobId);
        return BSS_ERR;
    }
    mBlobDataBlockMeta->mMaxBlobId = blobId;
    mBlobDataBlockMeta->mBlobBlockNum++;
    uint64_t expireTime = seqId >> NO_16;
    mBlobDataBlockMeta->UpdateExpireTime(expireTime);
    return BSS_OK;
}

BlobDataBlockRef BlobDataBlockWriter::WriteBlobDataBlock()
{
    std::vector<BlobIndexEntry> indexEntryList = GetIndexEntryList();
    RETURN_NULLPTR_AS_NULLPTR(mBuffer);
    for (const BlobIndexEntry &indexEntry : indexEntryList) {
        RETURN_NULLPTR_AS_NOT_OK(
            mBuffer->Write(reinterpret_cast<const uint8_t *>(&indexEntry.mBlobId), sizeof(indexEntry.mBlobId)));
        RETURN_NULLPTR_AS_NOT_OK(
            mBuffer->Write(reinterpret_cast<const uint8_t *>(&indexEntry.mOffset), sizeof(indexEntry.mOffset)));
        RETURN_NULLPTR_AS_NOT_OK(
            mBuffer->Write(reinterpret_cast<const uint8_t *>(&indexEntry.mSeqId), sizeof(indexEntry.mSeqId)));
    }
    RETURN_NULLPTR_AS_NOT_OK(mBuffer->Write(reinterpret_cast<const uint8_t *>(&mPosition), sizeof(mPosition)));
    RETURN_NULLPTR_AS_NULLPTR(mBlobDataBlockMeta);
    RETURN_NULLPTR_AS_NOT_OK(mBuffer->Write(reinterpret_cast<const uint8_t *>(&mBlobDataBlockMeta->mBlobBlockNum),
                                            sizeof(mBlobDataBlockMeta->mBlobBlockNum)));
    if (UNLIKELY(UINT32_MAX - mPosition - BLOB_DATA_BLOCK_HEADER_SIZE <
                 indexEntryList.size() * BLOB_INDEX_ENTRY_STRUCT_SIZE)) {
        LOG_ERROR("The value crosses the boundary and exceeds the UINT32_MAX maximum, mPosition: "
                  << mPosition << ", index vec size: " << indexEntryList.size()
                  << ", blob block header size: " << BLOB_DATA_BLOCK_HEADER_SIZE);
        return nullptr;
    }
    uint32_t realBufferLen = mPosition + indexEntryList.size() * BLOB_INDEX_ENTRY_STRUCT_SIZE +
                             BLOB_DATA_BLOCK_HEADER_SIZE;
    BlobDataBlockRef blobDataBlock = std::make_shared<BlobDataBlock>(mBuffer);
    BResult result = blobDataBlock->Init(mBlobDataBlockMeta, mPosition, realBufferLen);
    RETURN_NULLPTR_AS_NOT_OK(result);
    mBuffer->UpdateCapacity(realBufferLen);
    return blobDataBlock;
}

BufferRef BlobDataBlockWriter::CopyNewBufferFromBuffer(BufferAllocator &allocator, uint32_t offset, uint32_t length)
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

BResult BlobDataBlockWriter::SelectBlobValue(uint64_t blobId, BufferAllocator allocator, Value &value)
{
    RETURN_ERROR_AS_NULLPTR(mBlobDataBlockMeta);
    if (blobId < mBlobDataBlockMeta->mMinBlobId || blobId > mBlobDataBlockMeta->mMaxBlobId) {
        return BSS_NOT_FOUND;
    }
    std::vector<BlobIndexEntry> indexEntryList = GetIndexEntryList();
    RETURN_NOT_OK_AS_FALSE(indexEntryList.empty(), BSS_INNER_ERR);
    uint32_t low = 0;
    uint32_t high = indexEntryList.size() - NO_1;
    while (low <= high) {
        uint32_t mid = low + ((high - low) >> NO_1);
        if (UNLIKELY(mid >= indexEntryList.size())) {
            LOG_ERROR("Index out of bound, mid: " << mid << ", size: " << indexEntryList.size());
            return BSS_INNER_ERR;
        }
        const auto &blobIndexEntry = indexEntryList[mid];
        uint64_t midBlobId = blobIndexEntry.mBlobId;
        if (blobId > midBlobId) {
            low = mid + NO_1;
            continue;
        }
        if (blobId < midBlobId) {
            high = mid - NO_1;
            continue;
        }
        uint32_t offset = blobIndexEntry.mOffset;
        uint32_t nextOffset = 0;
        if (mid == indexEntryList.size() - NO_1) {
            nextOffset = mPosition;
        } else {
            if (UNLIKELY(mid + NO_1 >= indexEntryList.size())) {
                LOG_ERROR("Index out of bound, index: " << mid + NO_1 << ", size: " << indexEntryList.size());
                return BSS_INNER_ERR;
            }
            nextOffset = indexEntryList[mid + NO_1].mOffset;
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
}  // namespace bss
}  // namespace ock
