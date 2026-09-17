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

#include "blob_write_buffer.h"

#include <bss_types.h>

#include "flush_blob_data_block_runable.h"

namespace ock {
namespace bss {
BResult BlobWriteBuffer::Init(const BlobFileManagerRef &blobFileManager, const MemManagerRef &memManager,
                              ExecutorServicePtr blobFlushExecutor)
{
    mSeqGenerator = std::make_shared<SeqGenerator>();
    mBlobFlushExecutor = blobFlushExecutor;
    mMemManager = memManager;
    mBlobFileManager = blobFileManager;
    return BSS_OK;
}

BResult BlobWriteBuffer::Write(const uint8_t *value, uint32_t length, uint64_t expireTime, uint32_t keyGroup,
                               uint64_t &blobId)
{
    uint32_t realValueSize = length + BLOB_INDEX_ENTRY_STRUCT_SIZE + BLOB_DATA_BLOCK_HEADER_SIZE;
    if (UNLIKELY(UINT32_MAX - BLOB_INDEX_ENTRY_STRUCT_SIZE - BLOB_DATA_BLOCK_HEADER_SIZE < length)) {
        LOG_ERROR("Length is too long, realValueSize: " << realValueSize);
        return BSS_ERR;
    }
    if (mDataBlockWriter == nullptr) {
        auto ret = InitDataBlockWriter(realValueSize);
        if (UNLIKELY(ret != BSS_OK)) {
            LOG_LIMIT_WARN("Failed to create block writer, ret:" << ret);
            return BSS_ALLOC_FAIL;
        }
    }
    if (!mDataBlockWriter->CheckBlobSize(realValueSize) && !mDataBlockWriter->IsEmpty()) {
        BlobDataBlockRef dataBlock = mDataBlockWriter->WriteBlobDataBlock();
        if (UNLIKELY(dataBlock == nullptr)) {
            LOG_ERROR("Write blob data block failed, block is null.");
            return BSS_ERR;
        }
        AddFlushingDataBlock(dataBlock);
        auto ret = InitDataBlockWriter(realValueSize);
        if (UNLIKELY(ret != BSS_OK)) {
            LOG_LIMIT_WARN("Failed to create block writer, ret:" << ret);
            return BSS_ALLOC_FAIL;
        }
    }
    RETURN_ERROR_AS_NULLPTR(mSeqGenerator);
    blobId = mSeqGenerator->Next();
    auto ret = mDataBlockWriter->Write(blobId, value, length, expireTime, keyGroup);
    if (UNLIKELY(ret != BSS_OK)) {
        LOG_ERROR("Write blob value failed, keyGroup: " << keyGroup << ", ret:" << ret);
        return ret;
    }
    LOG_DEBUG("Transform real value to blobId success, blobId: " << blobId << ", value size: " << length);
    return BSS_OK;
}

BResult BlobWriteBuffer::Get(uint64_t blobId, uint32_t keyGroup, Value &value)
{
    auto self = shared_from_this();
    auto allocator = [self](uint32_t size) -> ByteBufferRef { return self->CreateBuffer(size); };
    {
        ReadLocker<ReadWriteLock> lock(&mRwLock);
        if (mDataBlockWriter != nullptr) {
            auto ret = mDataBlockWriter->SelectBlobValue(blobId, allocator, value);
            if (LIKELY(ret == BSS_OK)) {
                return BSS_OK;
            }
            if (UNLIKELY(ret != BSS_NOT_FOUND)) {
                LOG_ERROR("Look up in write buffer failed, ret: " << ret);
                return ret;
            }
        }
    }
    auto copyFlushQueue = mFlushQueue.GetDeque();
    for (auto it = copyFlushQueue.rbegin(); it != copyFlushQueue.rend(); ++it) {
        auto ret = (*it)->BinarySearchBlobValue(blobId, allocator, value);
        if (LIKELY(ret == BSS_OK)) {
            return BSS_OK;
        }
        if (UNLIKELY(ret != BSS_NOT_FOUND)) {
            LOG_ERROR("Look up in flush queue failed, ret: " << ret);
            return ret;
        }
    }
    return BSS_NOT_FOUND;
}

ByteBufferRef BlobWriteBuffer::CreateBuffer(uint32_t size)
{
    uintptr_t dataAddress;
    RETURN_NULLPTR_AS_NULLPTR(mMemManager);
    auto retVal = mMemManager->GetMemoryDirect(MemoryType::SLICE_TABLE, size, dataAddress);
    if (UNLIKELY(retVal != 0)) {
        LOG_LIMIT_WARN("Alloc memory for write blob value failed, size:" << size);
        return nullptr;
    }
    // reserve some byte buffer for flush.
    auto byteBuffer = MakeRef<ByteBuffer>(reinterpret_cast<uint8_t *>(dataAddress), size, mMemManager);
    if (UNLIKELY(byteBuffer == nullptr)) {
        mMemManager->ReleaseMemory(dataAddress);
        LOG_ERROR("Make ref failed, byteBuffer is null.");
    }
    return byteBuffer;
}

ByteBufferRef BlobWriteBuffer::CreateWriteBuffer(uint32_t size)
{
    if (size <= mDefaultBlockSize) {
        return CreateBuffer(mDefaultBlockSize);
    }
    return CreateBuffer(size);
}

void BlobWriteBuffer::TriggerFlush()
{
    // 如果当前正在执行Flush则直接返回.
    if (mBackgroundFlush.load(std::memory_order_seq_cst) > 0) {
        return;
    }

    // 生成Flush任务并调度执行
    RunnablePtr processor = std::make_shared<FlushBlobDataBlockProcessor>(shared_from_this(), mBlobFileManager);
    RETURN_AS_NULLPTR(mBlobFlushExecutor);
    bool isOk = mBlobFlushExecutor->Execute(processor, false);
    if (UNLIKELY(!isOk)) {
        LOG_ERROR("Execute blob flush failed.");
    }
}

BResult BlobWriteBuffer::FlushCurrentWriteBuffer(uint64_t snapshotId)
{
    if (UNLIKELY(mDataBlockWriter == nullptr)) {
        return BSS_OK;
    }
    BlobDataBlockRef dataBlock = mDataBlockWriter->WriteBlobDataBlock();
    if (UNLIKELY(dataBlock == nullptr)) {
        LOG_ERROR("Write blob data block failed, block is null. snapshotId: " << snapshotId);
        return BSS_ERR;
    }
    AddFlushingDataBlock(dataBlock);
    // 强制淘汰刷新队列到文件
    ForceEvictFlushQueue();
    return BSS_OK;
}
}  // namespace bss
}  // namespace ock