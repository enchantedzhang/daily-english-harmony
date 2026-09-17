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

#ifndef FLUSH_BLOB_DATA_BLOCK_RUNABLE_H
#define FLUSH_BLOB_DATA_BLOCK_RUNABLE_H
#include "blob_file_manager.h"
#include "blob_write_buffer.h"

namespace ock {
namespace bss {

class FlushBlobDataBlockProcessor : public Runnable {
public:
    explicit FlushBlobDataBlockProcessor(const BlobWriteBufferRef &blobWriteBuffer,
                                         const BlobFileManagerRef &blobFileManager)
        : mBlobWriteBuffer(blobWriteBuffer), mBlobFileManager(blobFileManager)
    {
    }

    ~FlushBlobDataBlockProcessor() override
    {
        LOG_DEBUG("Delete FlushBlobDataBlockProcessor success");
    }

    void Run() override
    {
        BlobDataBlockRef dataBlock;
        RETURN_AS_NULLPTR(mBlobWriteBuffer);
        mBlobWriteBuffer->IncBackgroundFlush();
        bool hasDataBlock = true;
        while (LIKELY(hasDataBlock)) {
            hasDataBlock = mBlobWriteBuffer->QueueFront(dataBlock);
            if (!hasDataBlock) {
                LOG_DEBUG("Blob data block flush queue is empty.");
                mBlobWriteBuffer->SubBackgroundFlush();
                return;
            }
            RETURN_AS_NULLPTR(mBlobFileManager);
            BResult result = mBlobFileManager->WriteBlock(dataBlock);
            if (LIKELY(result == BSS_OK)) {
                mBlobWriteBuffer->EndDataBlockFlush();
            } else {
                LOG_WARN("Flush blob data block failed, need to inner retry, ret:" << result);
            }
        }
    }

private:
    BlobWriteBufferRef mBlobWriteBuffer = nullptr;
    BlobFileManagerRef mBlobFileManager = nullptr;
};
}  // namespace bss
}  // namespace ock

#endif