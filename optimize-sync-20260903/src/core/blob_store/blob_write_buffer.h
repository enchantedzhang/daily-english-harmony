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

#ifndef BLOB_WRITE_BUFFER_H
#define BLOB_WRITE_BUFFER_H
#include <memory>

#include "blob_data_block_writer.h"
#include "blob_file_manager.h"
#include "concurrent_deque.h"
#include "util/seq_generator.h"

namespace ock {
namespace bss {

class BlobWriteBuffer : public std::enable_shared_from_this<BlobWriteBuffer> {
public:
    ~BlobWriteBuffer()
    {
        LOG_INFO("Delete BlobWriteBuffer success.");
    }
    BlobWriteBuffer() = default;

    BResult Init(const BlobFileManagerRef &blobFileManager, const MemManagerRef &memManager,
                 ExecutorServicePtr blobFlushExecutor);

    BResult Write(const uint8_t *value, uint32_t length, uint64_t expireTime, uint32_t keyGroup, uint64_t &blobId);

    BResult Get(uint64_t blobId, uint32_t keyGroup, Value &value);

    ByteBufferRef CreateBuffer(uint32_t size);

    ByteBufferRef CreateWriteBuffer(uint32_t size);

    void TriggerFlush();

    bool QueueFront(BlobDataBlockRef &item)
    {
        return mFlushQueue.Front(item);
    }

    inline bool IsFlushQueueEmpty() const
    {
        return mFlushQueue.Empty();
    }

    void EndDataBlockFlush()
    {
        BlobDataBlockRef dataBlock;
        // poll出待淘汰队列的队首的dataBlock, 即释放该dataBlock内存.
        PollFlushingDataBlock(dataBlock);
    }

    inline void AddFlushingDataBlock(const BlobDataBlockRef &item)
    {
        WriteLocker<ReadWriteLock> lock(&mRwLock);
        uint64_t flushWaiting = mFlushWaiting.fetch_add(NO_1, std::memory_order_relaxed);
        if (UNLIKELY((flushWaiting + NO_1) % NO_1024 == NO_0)) {
            LOG_WARN("Waiting flush data block is too many, flushWaiting: " << flushWaiting);
        }
        if (UNLIKELY(flushWaiting > NO_3000)) {
            // 如果刷新等待的data block数量过多, 则强制淘汰刷新队列, 避免占用过多内存.
            ForceEvictFlushQueue();
        }
        mFlushQueue.PushBack(item);
        mDataBlockWriter = nullptr;
        TriggerFlush();
    }

    void ForceEvictFlushQueue()
    {
        uint32_t times = NO_1;
        auto start = std::chrono::high_resolution_clock::now();
        while (UNLIKELY(!mFlushQueue.Empty())) {
            if ((times++) % NO_100 == 0) {
                auto end = std::chrono::high_resolution_clock::now();
                auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
                double elapsed = duration.count() / 1e3;  // 转换为ms
                LOG_WARN("Force evict flush queue suspend cost time: " << elapsed << "ms.");
            }
            usleep(NO_100000);  // 100ms
        }
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        double elapsed = duration.count() / 1e3;  // 转换为ms
        LOG_INFO("Force evict flush queue cost time: " << elapsed << "ms.");
    }

    inline bool PollFlushingDataBlock(BlobDataBlockRef &item)
    {
        mFlushWaiting.fetch_sub(NO_1, std::memory_order_relaxed);
        return mFlushQueue.TryPopFront(item);
    }

    inline void SetDefaultBlockSize(uint32_t defaultBlockSize)
    {
        mDefaultBlockSize = defaultBlockSize;
    }

    void IncBackgroundFlush()
    {
        mBackgroundFlush.fetch_add(NO_1);
    }

    void SubBackgroundFlush()
    {
        mBackgroundFlush.fetch_sub(NO_1);
    }

    BResult InitDataBlockWriter(uint32_t realValueSize)
    {
        ByteBufferRef buffer = CreateWriteBuffer(realValueSize);
        if (UNLIKELY(buffer == nullptr)) {
            return BSS_ALLOC_FAIL;
        }
        WriteLocker<ReadWriteLock> lock(&mRwLock);
        mDataBlockWriter = std::make_shared<BlobDataBlockWriter>(buffer);
        return BSS_OK;
    }

    BResult FlushCurrentWriteBuffer(uint64_t snapshotId);

    inline uint64_t GetCurrentSeqId() const
    {
        return mSeqGenerator->Next();
    }

    inline void RestoreSeqId(uint64_t seqId) const
    {
        mSeqGenerator->Restore(seqId);
    }

private:
    MemManagerRef mMemManager = nullptr;
    ReadWriteLock mRwLock;
    BlobDataBlockWriterRef mDataBlockWriter = nullptr;
    SeqGeneratorRef mSeqGenerator = nullptr;
    ExecutorServicePtr mBlobFlushExecutor = nullptr;
    ConcurrentDeque<BlobDataBlockRef> mFlushQueue;
    std::atomic<uint64_t> mFlushWaiting{ 0 };
    BlobFileManagerRef mBlobFileManager = nullptr;
    uint32_t mDefaultBlockSize = IO_SIZE_16K;
    std::atomic<int32_t> mBackgroundFlush{ 0 };
};
using BlobWriteBufferRef = std::shared_ptr<BlobWriteBuffer>;
}  // namespace bss
}  // namespace ock

#endif
