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

#include "blob_immutable_file.h"

#include "blob_index_block.h"

namespace ock {
namespace bss {

BResult BlobImmutableFile::SelectBlobValue(uint64_t blobId, uint32_t keyGroup, Value &value)
{
    RETURN_ERROR_AS_NULLPTR(mBlobFileMeta);
    auto groupRange = mBlobFileMeta->GetValidGroupRange();
    RETURN_NOT_OK_AS_FALSE(keyGroup > INT32_MAX, BSS_INVALID_PARAM);
    RETURN_ERROR_AS_NULLPTR(groupRange);
    if (blobId < mBlobFileMeta->GetMinBlobId() || blobId > mBlobFileMeta->GetMaxBlobId() ||
        !groupRange->ContainsGroup((int32_t)keyGroup)) {
        return BSS_NOT_FOUND;
    }
    auto fileReader = GetFileReader();
    RETURN_ERROR_AS_NULLPTR(fileReader);
    uint32_t blockSize = mBlobFileMeta->GetDataBlockSize();
    auto blockBuilder = [blockSize](ByteBufferRef &byteBuffer) -> BlockRef {
        return std::make_shared<BlobIndexBlock>(byteBuffer, blockSize);
    };
    auto block = fileReader->GetOrLoadBlock(mBlockHandle, BlockType::BLOB, blockBuilder);
    BlobIndexBlockRef indexBlock = std::dynamic_pointer_cast<BlobIndexBlock>(block);
    RETURN_ERROR_AS_NULLPTR(indexBlock);
    BlobDataBlockMetaRef dataBlockMeta = indexBlock->SelectDataBlockMeta(blobId);
    RETURN_ERROR_AS_NULLPTR(dataBlockMeta);
    auto blockBuilder2 = [dataBlockMeta](ByteBufferRef &byteBuffer) -> BlockRef {
        auto blobDataBlock = std::make_shared<BlobDataBlock>(byteBuffer);
        RETURN_NULLPTR_AS_NOT_OK(blobDataBlock->Init(dataBlockMeta, byteBuffer->Capacity()));
        return blobDataBlock;
    };
    auto blockHandle = dataBlockMeta->GetBlockHandle();
    RETURN_ERROR_AS_NULLPTR(mBlobFileReader);
    BlockRef block2 = mBlobFileReader->GetOrLoadBlock(blockHandle, BlockType::BLOB, blockBuilder2);
    BlobDataBlockRef blobDataBlock = std::dynamic_pointer_cast<BlobDataBlock>(block2);
    RETURN_ERROR_AS_NULLPTR(blobDataBlock);
    auto self = shared_from_this();
    auto allocator = [self](uint32_t size) -> ByteBufferRef { return self->CreateBuffer(size); };
    return blobDataBlock->BinarySearchBlobValue(blobId, allocator, value);
}

ByteBufferRef BlobImmutableFile::CreateBuffer(uint32_t size)
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
}  // namespace bss
}  // namespace ock
