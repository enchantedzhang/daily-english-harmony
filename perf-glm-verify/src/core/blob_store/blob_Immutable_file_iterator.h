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

#ifndef BOOST_SS_BLOB_IMMUTABLE_FILE_ITERATOR_H
#define BOOST_SS_BLOB_IMMUTABLE_FILE_ITERATOR_H

#include "blob_data_block.h"
#include "blob_immutable_file.h"
#include "blob_index_block.h"
#include "blob_structure.h"

namespace ock {
namespace bss {
class BlobImmutableFileIterator : public std::enable_shared_from_this<BlobImmutableFileIterator> {
public:
    explicit BlobImmutableFileIterator(BlobImmutableFileRef file, MemManagerRef memManager)
        : mFile(file), mMemManager(memManager)
    {
    }
    BResult Init()
    {
        RETURN_ERROR_AS_NULLPTR(mFile);
        auto fileReader = mFile->GetFileReader();
        RETURN_ERROR_AS_NULLPTR(fileReader);
        mIndexBlockHandle = std::make_shared<BlockHandle>(mFile->GetIndexBlockHandle());
        RETURN_ERROR_AS_NULLPTR(mIndexBlockHandle);
        ByteBufferRef indexBuffer = nullptr;
        auto ret = fileReader->ReadBlock(*mIndexBlockHandle, indexBuffer);
        RETURN_NOT_OK(ret);
        auto fileMeta = mFile->GetBlobFileMeta();
        RETURN_ERROR_AS_NULLPTR(fileMeta);
        mIndexBlock = std::make_shared<BlobIndexBlock>(indexBuffer, fileMeta->GetDataBlockSize());
        return Advance();
    }

    bool HasNext()
    {
        return mNext != nullptr;
    }

    BlobValueWrapperRef Next()
    {
        auto next = mNext;
        mNext = nullptr;
        auto ret = Advance();
        if (ret != BSS_OK) {
            LOG_ERROR("Iterator advance fail, ret:" << ret);
        }
        return next;
    }

    void Close()
    {
        if (mFile != nullptr) {
            mFile->CloseFileReader();
            mFile = nullptr;
        }
    }

private:
    BResult Advance()
    {
        if (mDataBlock != nullptr && mCurSlotInDataBlock >= mDataBlock->GetBlockMeta()->mBlobBlockNum) {
            mDataBlock = nullptr;
        }
        if (mDataBlock == nullptr && mNextSlotInIndexBlock < mIndexBlock->GetDataBlockNum()) {
            auto dataBlockMeta = mIndexBlock->GetDataBlockMeta(mNextSlotInIndexBlock);
            RETURN_ERROR_AS_NULLPTR(dataBlockMeta);
            auto fileReader = mFile->GetFileReader();
            RETURN_ERROR_AS_NULLPTR(fileReader);
            ByteBufferRef buffer;
            auto ret = fileReader->ReadBlock(dataBlockMeta->GetBlockHandle(), buffer);
            RETURN_NOT_OK(ret);
            mDataBlock = std::make_shared<BlobDataBlock>(buffer);
            mDataBlock->Init(dataBlockMeta, buffer->Capacity());
            mNextSlotInIndexBlock++;
            mCurSlotInDataBlock = 0;
        }
        if (mDataBlock != nullptr && mCurSlotInDataBlock < mDataBlock->GetBlockMeta()->mBlobBlockNum) {
            BlobValueWrapper blobValueWrapper;
            auto self = shared_from_this();
            std::function<ByteBufferRef(uint32_t)> allocator = [self](uint32_t size) -> ByteBufferRef {
                return self->CreateBuffer(size);
            };
            auto ret = mDataBlock->GetBlobValueWrapper(blobValueWrapper, mCurSlotInDataBlock, allocator);
            RETURN_NOT_OK(ret);
            mNext = std::make_shared<BlobValueWrapper>(blobValueWrapper);
            mCurSlotInDataBlock++;
        }
        return BSS_OK;
    }

    ByteBufferRef CreateBuffer(uint32_t size)
    {
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

private:
    BlobImmutableFileRef mFile = nullptr;
    BlockHandleRef mIndexBlockHandle = nullptr;
    BlobIndexBlockRef mIndexBlock = nullptr;
    BlobDataBlockRef mDataBlock = nullptr;
    uint32_t mNextSlotInIndexBlock = 0;
    uint32_t mCurSlotInDataBlock = 0;
    BlobValueWrapperRef mNext = nullptr;
    MemManagerRef mMemManager;
};
using BlobImmutableFileIteratorRef = std::shared_ptr<BlobImmutableFileIterator>;
}  // namespace bss
}  // namespace ock

#endif
