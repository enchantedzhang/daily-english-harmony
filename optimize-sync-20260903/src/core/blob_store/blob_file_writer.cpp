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

#include "blob_file_writer.h"

#include <iostream>

#include "compressor_utils.h"

namespace ock {
namespace bss {
BResult BlobFileWriter::Write(const BlobDataBlockRef &dataBlock)
{
    RETURN_ERROR_AS_NULLPTR(dataBlock);
    BlockHandle blockHandle;
    BResult result = WriteBlock(dataBlock->GetBuffer(), blockHandle);
    RETURN_NOT_OK(result);
    RETURN_NOT_OK(Flush());
    auto blockMeta = dataBlock->GetBlockMeta();
    RETURN_ERROR_AS_NULLPTR(blockMeta);
    blockMeta->SetBlockHandle(blockHandle);
    mBlobIndexBlockWriter->AddBlob(blockMeta);
    RETURN_INVALID_PARAM_AS_NULLPTR(mBlobFileMeta);
    mBlobFileMeta->UpdateBlobId(blockMeta->mMinBlobId, blockMeta->mMaxBlobId);
    mBlobFileMeta->UpdateExpireTime(blockMeta->mMinExpireTime, blockMeta->mMaxExpireTime);
    mBlobFileMeta->IncDataBlockSize();
    mBlobFileMeta->IncBlobNum(blockMeta->mBlobBlockNum);
    return BSS_OK;
}

BResult BlobFileWriter::WriteBlock(const ByteBufferRef &buffer, BlockHandle &blockHandle)
{
    if (UNLIKELY(buffer == nullptr || buffer->Data() == nullptr || mFileOutputView == nullptr)) {
        LOG_ERROR("Write buffer or file output view is nullptr.");
        return BSS_ERR;
    }
    // 文件当前读写偏移量
    uint32_t offset = mFileOutputSize;

    // 1、压缩block.
    ByteBufferRef outputBuffer = buffer;
    uint32_t outputSize = buffer->Capacity();
    BResult ret = CompressBlock(outputBuffer, outputSize);

    // 2. 写入Block head数据.
    uint8_t headData[HEAD_BLOCK_SIZE] = { 0 };  // 其长度为sizeof(NONE)+sizeof(buffer->Capacity())+sizeof(crc).
    headData[0] = (ret == BSS_OK) ? mCompressAlgorithm : CompressAlgo::NONE;
    auto tmpOriginLength = reinterpret_cast<uint32_t *>(headData + sizeof(CompressAlgo));
    *tmpOriginLength = buffer->Capacity();
    auto crc = reinterpret_cast<uint32_t *>(headData + sizeof(CompressAlgo) + sizeof(uint32_t));
    *crc = NO_1024;

    auto sizeToCheck = mFileOutputView->Size();
    if (UNLIKELY(sizeToCheck > INT64_MAX)) {
        LOG_ERROR("File output view size cannot to static cast.");
        return BSS_ERR;
    }
    ret = mFileOutputView->WriteBuffer(headData, static_cast<int64_t>(sizeToCheck), HEAD_BLOCK_SIZE);
    if (UNLIKELY(ret != BSS_OK)) {
        LOG_ERROR("Write block head failed, ret:" << ret << ", writeSize:" << HEAD_BLOCK_SIZE);
        return ret;
    }

    // 3. 写入Block value数据.
    sizeToCheck = mFileOutputView->Size();
    if (UNLIKELY(sizeToCheck > INT64_MAX)) {
        LOG_ERROR("File output view size cannot to static cast.");
        return BSS_ERR;
    }
    ret = mFileOutputView->WriteByteBuffer(outputBuffer, static_cast<int64_t>(sizeToCheck), outputSize);
    if (UNLIKELY(ret != BSS_OK)) {
        LOG_ERROR("Write block value failed, ret:" << ret << ", writeSize:" << outputSize);
        return ret;
    }

    // 4. 更新文件长度.
    blockHandle = { offset, outputSize };  // 描述该block在file中的位置.
    mFileOutputSize += HEAD_BLOCK_SIZE + outputSize;
    return BSS_OK;
}

BResult BlobFileWriter::CompressBlock(ByteBufferRef &buffer, uint32_t &bufferSize)
{
    if (mCompressAlgorithm == CompressAlgo::NONE || !CompressorUtils::IsSupportCodec(mCompressAlgorithm)) {
        return BSS_NO_COMPRESS;
    }

    CompressorRef compressor = CompressorUtils::InitCompressor(mCompressAlgorithm);
    RETURN_ALLOC_FAIL_AS_NULLPTR(compressor);
    int64_t maxCompressedLength = CompressorUtils::MaxCompressedLength(buffer->Capacity());
    ByteBufferRef outputBuffer = CreateBuffer(maxCompressedLength);
    if (UNLIKELY(outputBuffer == nullptr)) {
        LOG_ERROR("Make new buffer failed, because of out of memory.");
        return BSS_ALLOC_FAIL;
    }
    uint32_t outputSize = compressor->Compress(outputBuffer->Data(), maxCompressedLength, buffer->Data(),
                                               buffer->Capacity(), NO_4);
    // 假如数据已无法压缩，即不压缩，主动失败
    if (UNLIKELY(outputSize == 0 || outputSize > bufferSize)) {
        return BSS_INNER_ERR;
    }

    buffer = outputBuffer;
    bufferSize = outputSize;
    return BSS_OK;
}

BlobImmutableFileRef BlobFileWriter::WriteImmutableFile(uint64_t currentVersion)
{
    RETURN_NULLPTR_AS_NULLPTR(mBlobIndexBlockWriter);
    uint32_t indexBlockSize = mBlobIndexBlockWriter->EstimateSize();
    ByteBufferRef indexBlockBuffer = CreateBuffer(indexBlockSize);
    if (UNLIKELY(indexBlockBuffer == nullptr)) {
        LOG_ERROR("Make new buffer failed, because of out of memory.");
        return nullptr;
    }
    mBlobIndexBlockWriter->WriteIndexBlock(indexBlockBuffer);
    BlockHandle blockHandle;
    BResult result = WriteBlock(indexBlockBuffer, blockHandle);
    RETURN_NULLPTR_AS_NOT_OK(result);
    RETURN_NULLPTR_AS_NULLPTR(mFileOutputView);
    RETURN_NULLPTR_AS_NOT_OK(mFileOutputView->WriteUint32(blockHandle.GetOffset()));
    RETURN_NULLPTR_AS_NOT_OK(mFileOutputView->WriteUint32(blockHandle.GetSize()));
    // 占位预留
    RETURN_NULLPTR_AS_NOT_OK(mFileOutputView->WriteUint32(NO_1));
    RETURN_NULLPTR_AS_NOT_OK(mFileOutputView->WriteUint32(BLOB_FILE_MAGIC_NUM));
    mBlobFileMeta->SetFileSize(mFileOutputView->Size());
    mBlobFileMeta->SetVersion(currentVersion);
    RETURN_NULLPTR_AS_NOT_OK(mFileOutputView->Flush());
    RETURN_NULLPTR_AS_NULLPTR(mBlobFileReader);
    return std::make_shared<BlobImmutableFile>(mFilePath, mConfig, mMemManager, mBlockCache, mFileCacheManager,
                                               blockHandle, mBlobFileMeta, mBlobFileReader);
}

ByteBufferRef BlobFileWriter::CreateBuffer(uint32_t size)
{
    uintptr_t dataAddress;
    RETURN_NULLPTR_AS_NULLPTR(mMemManager);
    auto retVal = mMemManager->GetMemory(MemoryType::FILE_STORE, size, dataAddress);
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

BResult BlobFileWriter::SelectBlobValue(uint64_t blobId, uint32_t keyGroup, Value &value)
{
    RETURN_ERROR_AS_NULLPTR(mBlobFileMeta);
    auto group = mBlobFileMeta->GetValidGroupRange();
    RETURN_ERROR_AS_NULLPTR(group);
    if (blobId < mBlobFileMeta->GetMinBlobId() || blobId > mBlobFileMeta->GetMaxBlobId() ||
        !group->ContainsGroup(static_cast<int32_t>(keyGroup))) {
        return BSS_NOT_FOUND;
    }
    RETURN_ERROR_AS_NULLPTR(mBlobIndexBlockWriter);
    auto dataBlockMeta = mBlobIndexBlockWriter->SelectDataBlockMeta(blobId);
    RETURN_ERROR_AS_NULLPTR(dataBlockMeta);
    auto blockHandle = dataBlockMeta->GetBlockHandle();
    auto blockBuilder = [dataBlockMeta](ByteBufferRef &byteBuffer) -> BlockRef {
        auto blobDataBlock = std::make_shared<BlobDataBlock>(byteBuffer);
        RETURN_NULLPTR_AS_NOT_OK(blobDataBlock->Init(dataBlockMeta, byteBuffer->Capacity()));
        return blobDataBlock;
    };
    RETURN_ERROR_AS_NULLPTR(mBlobFileReader);
    BlockRef block = mBlobFileReader->GetOrLoadBlock(blockHandle, BlockType::BLOB, blockBuilder);
    BlobDataBlockRef blobDataBlock = std::dynamic_pointer_cast<BlobDataBlock>(block);
    RETURN_ERROR_AS_NULLPTR(blobDataBlock);
    auto self = shared_from_this();
    auto allocator = [self](uint32_t size) -> ByteBufferRef { return self->CreateBuffer(size); };
    return blobDataBlock->BinarySearchBlobValue(blobId, allocator, value);
}
}  // namespace bss
}  // namespace ock