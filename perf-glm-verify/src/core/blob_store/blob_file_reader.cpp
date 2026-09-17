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

#include "blob_file_reader.h"

#include "compressor_utils.h"

namespace ock {
namespace bss {
BlockRef BlobFileReader::GetOrLoadBlock(const BlockHandle &blockHandle, BlockType blockType,
                                        BlockBuilderFunc dataBuilder)
{
    // 1. 首先从block cache缓存中查询Block.
    uint64_t blockId = GetBlockId(blockHandle);
    RETURN_NULLPTR_AS_NULLPTR(mBlockCache);
    BlockRef block = mBlockCache->Get(blockId, blockType);
    if (block == nullptr) {
        // 2. 缓存中不存在则从文件中读取Block.
        ByteBufferRef byteBuffer;
        BResult result = ReadBlock(blockHandle, byteBuffer);
        if (UNLIKELY(result != BSS_OK)) {
            LOG_ERROR("Read block failed, ret:" << result << ", block offset:" << blockHandle.GetOffset()
                                                << ", size:" << blockHandle.GetSize());
            return nullptr;
        }
        // 3. 将Block加入到block cache缓存中.
        block = dataBuilder(byteBuffer);
        if (UNLIKELY(block == nullptr)) {
            LOG_ERROR("Build block failed, block offset:" << blockHandle.GetOffset()
                                                          << ", size:" << blockHandle.GetSize());
            return nullptr;
        }
        mBlockCache->Put(blockId, block, blockType);
    }
    return block;
}

BlockRef BlobFileReader::FetchBlock(const BlockHandle &blockHandle, BlockType blockType, BlockBuilderFunc dataBuilder)
{
    ByteBufferRef byteBuffer;
    BResult result = ReadBlock(blockHandle, byteBuffer);
    if (UNLIKELY(result != BSS_OK)) {
        LOG_ERROR("Read block failed, ret:" << result << ", block offset:" << blockHandle.GetOffset()
                                            << ", size:" << blockHandle.GetSize());
        return nullptr;
    }

    BlockRef block = dataBuilder(byteBuffer);
    if (UNLIKELY(block == nullptr)) {
        LOG_ERROR("Build block failed, block offset:" << blockHandle.GetOffset() << ", size:" << blockHandle.GetSize());
        return nullptr;
    }
    return block;
}

BResult BlobFileReader::ReadBlock(const BlockHandle &blockHandle, ByteBufferRef &byteBuffer)
{
    if (!mInit.load()) {
        LOG_ERROR("Blob file reader not init.");
        return BSS_INNER_ERR;
    }
    // 1. 从BlockHandler中获取Block的offset和size信息.
    uint32_t blockOffset = GetBlockHandleOffset(blockHandle);
    uint32_t rawSize = blockHandle.GetSize();

    // 2. 读取Block的head信息与value信息.
    uint32_t totalBufferSize = HEAD_BLOCK_SIZE + rawSize;
    byteBuffer = CreateBuffer(totalBufferSize);
    if (UNLIKELY(byteBuffer == nullptr)) {
        LOG_ERROR("Make byte buffer ref failed.");
        return BSS_ALLOC_FAIL;
    }
    RETURN_ERROR_AS_NULLPTR(mInputView);
    BResult result = mInputView->ReadByteBuffer(0, byteBuffer, blockOffset, totalBufferSize);
    if (UNLIKELY(result != BSS_OK)) {
        byteBuffer = nullptr;
        LOG_ERROR("Read block failed, totalBufferSize:" << totalBufferSize);
        return result;
    }

    // 3. 读取Head信息.
    auto compressAlgo = static_cast<CompressAlgo>(*byteBuffer->Data());
    uint32_t originLength = *reinterpret_cast<uint32_t *>(byteBuffer->Data() + sizeof(CompressAlgo));
    uint32_t crc = *reinterpret_cast<uint32_t *>(byteBuffer->Data() + sizeof(CompressAlgo) + sizeof(uint32_t));
    if (UNLIKELY(crc != NO_1024)) {
        LOG_ERROR("Data crc check failed, crc:" << crc << ", except crc:" << NO_1024);
        return BSS_INNER_ERR;
    }

    // 4. 抛弃head信息，保留Block的value信息.
    RETURN_NOT_OK(byteBuffer->UpdateDataWithFreeOffset(HEAD_BLOCK_SIZE));

    // 5. 解压block
    RETURN_NOT_OK(DecompressBlock(byteBuffer, originLength, compressAlgo));
    return BSS_OK;
}

BResult BlobFileReader::DecompressBlock(ByteBufferRef &buffer, uint32_t originLength, CompressAlgo compressAlgo)
{
    if (compressAlgo == CompressAlgo::NONE || !CompressorUtils::IsSupportCodec(compressAlgo)) {
        return BSS_OK;
    }
    CompressorRef compressor = CompressorUtils::InitCompressor(compressAlgo);
    RETURN_ALLOC_FAIL_AS_NULLPTR(compressor);
    ByteBufferRef originBuffer = CreateBuffer(originLength);
    if (UNLIKELY(originBuffer == nullptr)) {
        buffer = nullptr;
        LOG_ERROR("Make origin buffer ref failed.");
        return BSS_ALLOC_FAIL;
    }
    uint32_t decompressedSize = compressor->Decompress(originBuffer->Data(), originLength, buffer->Data(),
                                                       buffer->Capacity());
    if (UNLIKELY(decompressedSize == 0 || decompressedSize != originLength)) {
        LOG_ERROR("Decompress failed, originLength: " << originLength << ", decompressedSize: " << decompressedSize);
        buffer = nullptr;
        return BSS_INNER_ERR;
    }
    buffer = originBuffer;
    return BSS_OK;
}

ByteBufferRef BlobFileReader::CreateBuffer(uint32_t size)
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

BResult BlobFileReader::ReadBuffer(uint32_t offset, uint32_t length, ByteBufferRef &byteBuffer)
{
    if (!mInit.load()) {
        LOG_ERROR("Blob file reader not init.");
        return BSS_INNER_ERR;
    }
    byteBuffer = CreateBuffer(length);
    if (UNLIKELY(byteBuffer == nullptr)) {
        LOG_ERROR("Make byte buffer ref failed.");
        return BSS_ALLOC_FAIL;
    }
    RETURN_ERROR_AS_NULLPTR(mInputView);
    BResult result = mInputView->ReadByteBuffer(0, byteBuffer, offset, length);
    if (UNLIKELY(result != BSS_OK)) {
        byteBuffer = nullptr;
        LOG_ERROR("Read byteBuffer failed, offset:" << offset << ", length:" << length);
    }
    return result;
}
}  // namespace bss
}  // namespace ock