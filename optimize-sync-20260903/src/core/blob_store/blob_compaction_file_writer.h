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

#ifndef BOOST_SS_BLOB_COMPACTION_FILE_WRITER_H
#define BOOST_SS_BLOB_COMPACTION_FILE_WRITER_H

#include "blob_data_block_writer.h"
#include "blob_file_manager.h"
#include "blob_file_writer.h"

namespace ock {
namespace bss {

class BlobCompactionFileWriter {
public:
    BlobCompactionFileWriter(ConfigRef config, MemManagerRef memManager, BlobFileManagerRef blobFileGroupManager,
                             uint64_t version)
        : mConfig(config),
          mMemManager(memManager),
          mBlobFileManager(blobFileGroupManager),
          mVersion(version),
          mDefaultBlockSize(config->GetBlobDefaultBlockSize())
    {
    }

    BResult WriteBlob(BlobValueWrapper blobValueWrapper)
    {
        auto value = blobValueWrapper.mBlobValue;
        uint32_t size = value.ValueLen() + NO_28;
        if (mBlobDataBlockWriter == nullptr) {
            ByteBufferRef buffer = CreateBuffer(size);
            RETURN_ERROR_AS_NULLPTR(buffer);
            mBlobDataBlockWriter = std::make_shared<BlobDataBlockWriter>(buffer);
        }
        if (!mBlobDataBlockWriter->CheckBlobSize(size) && !mBlobDataBlockWriter->IsEmpty()) {
            auto dataBlock = mBlobDataBlockWriter->WriteBlobDataBlock();
            RETURN_NOT_OK(WriteDataBlock(dataBlock));
            ByteBufferRef buffer = CreateBuffer(size);
            RETURN_ERROR_AS_NULLPTR(buffer);
            mBlobDataBlockWriter = std::make_shared<BlobDataBlockWriter>(buffer);
        }
        return mBlobDataBlockWriter->Write(blobValueWrapper.mBlobId, value.ValueData(), value.ValueLen(),
                                           blobValueWrapper.mSeqId);
    }

    BResult WriteDataBlock(BlobDataBlockRef dataBlock)
    {
        if (mBlobFileWriter == nullptr) {
            mBlobFileWriter = mBlobFileManager->NewBlobFileWriter();
            RETURN_ERROR_AS_NULLPTR(mBlobFileWriter);
        }
        if (mBlobFileWriter->GetOutputSize() > mConfig->GetBlobFileSize()) {
            auto newFile = mBlobFileWriter->WriteImmutableFile(mVersion);
            RETURN_ERROR_AS_NULLPTR(newFile);
            mWrittenFiles.emplace_back(newFile);
            mBlobFileWriter = mBlobFileManager->NewBlobFileWriter();
            RETURN_ERROR_AS_NULLPTR(mBlobFileWriter);
        }
        return mBlobFileWriter->Write(dataBlock);
    }

    ByteBufferRef CreateBuffer(uint32_t size)
    {
        size = std::max(size, mDefaultBlockSize);
        uintptr_t dataAddress;
        RETURN_NULLPTR_AS_NULLPTR(mMemManager);
        auto retVal = mMemManager->GetMemory(MemoryType::SLICE_TABLE, size, dataAddress);
        if (UNLIKELY(retVal != 0)) {
            LOG_WARN("Alloc memory for write blob value failed, size:" << size);
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

    BResult Finish(std::vector<BlobImmutableFileRef> &ret)
    {
        if (mBlobDataBlockWriter == nullptr) {
            ret = std::move(mWrittenFiles);
            return BSS_OK;
        }
        auto dataBlock = mBlobDataBlockWriter->WriteBlobDataBlock();
        RETURN_ERROR_AS_NULLPTR(dataBlock);
        RETURN_NOT_OK(WriteDataBlock(dataBlock));
        auto file = mBlobFileWriter->WriteImmutableFile(mVersion);
        RETURN_ERROR_AS_NULLPTR(file);
        mWrittenFiles.emplace_back(file);
        ret = std::move(mWrittenFiles);
        return BSS_OK;
    }

private:
    ConfigRef mConfig;
    BlobDataBlockWriterRef mBlobDataBlockWriter;
    MemManagerRef mMemManager;
    BlobFileWriterRef mBlobFileWriter;
    BlobFileManagerRef mBlobFileManager;
    std::vector<BlobImmutableFileRef> mWrittenFiles;
    uint64_t mVersion;
    uint32_t mDefaultBlockSize;
};
using BlobCompactionFileWriterRef = std::shared_ptr<BlobCompactionFileWriter>;
}  // namespace bss
}  // namespace ock

#endif
