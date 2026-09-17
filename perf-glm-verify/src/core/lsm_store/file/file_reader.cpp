/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#include "file_reader.h"

#include "block/index_block_writer.h"
#include "compressor_utils.h"
#include "file_address_util.h"
#include "file_structure.h"
#include "lsm_store/block/data_block.h"
#include "lsm_store/block/filter_block_writer.h"

namespace ock {
namespace bss {
thread_local FileProcHolder FileReaderBase::mHolder;

BResult FileReader::Get(const Key &key, Value &value)
{
    // 1. 读取filter block, 成功则在filter中检查key是否存在.
    FilterBlockRef filterBlock = GetOrLoadFilterBlock();
    if (LIKELY(filterBlock != nullptr)) {
        if (!filterBlock->KeyMayMatch(key.MixedHashCode())) {
            AddFilterSuccessCount();
            return BSS_NOT_EXISTS;
        }
    }

    // 2. 从index block中读取data block handle信息.
    BlockHandle blockHandle;
    RETURN_ERROR_AS_NULLPTR(mIndexReader);
    auto ret = mIndexReader->GetDataBlock(key, blockHandle);
    if (UNLIKELY(ret != BSS_OK)) {
        return ret;
    }

    // 3. 读取data block, 成功即得到kv pair.
    DataBlockRef dataBlock = std::dynamic_pointer_cast<DataBlock>(GetOrLoadDataBlock(blockHandle));
    if (UNLIKELY(dataBlock == nullptr)) {
        LOG_ERROR("Get data block failed, offset:" << blockHandle.GetOffset() << ", size:" << blockHandle.GetSize());
        return BSS_ALLOC_FAIL;
    }
    ret = dataBlock->GetKey(key, value);
    if (ret == BSS_OK) {
        AddFilterExistSuccessCount();
    } else if (ret == BSS_NOT_EXISTS) {
        AddFilterExistFailCount();
    }
    return ret;
}

FilterBlockRef FileReader::GetOrLoadFilterBlock()
{
    RETURN_NULLPTR_AS_NULLPTR_NO_LOG(mFilterBlockHandle);
    if (!mCacheIndexAndFilter) {
        if (UNLIKELY(mFilterBlock == nullptr)) {
            mFilterBlock = GetFilterBlock();
        }
        return mFilterBlock;
    }
    return std::static_pointer_cast<FilterBlock>(GetOrLoadBlock(*mFilterBlockHandle, BlockType::FILTER));
}

IndexBlockRef FileReader::GetOrLoadIndexBlock()
{
    RETURN_NULLPTR_AS_NULLPTR_NO_LOG(mIndexBlockHandle);
    if (!mCacheIndexAndFilter) {
        if (UNLIKELY(mIndexBlock == nullptr)) {
            mIndexBlock = GetIndexBlock();
        }
        return mIndexBlock;
    }
    return std::static_pointer_cast<IndexBlock>(GetOrLoadBlock(*mIndexBlockHandle, BlockType::INDEX));
}

DataBlockRef FileReaderBase::FetchDataBlock(const BlockHandle &blockHandle)
{
    ByteBufferRef byteBuffer = nullptr;
    FetchBlock(blockHandle, byteBuffer, BlockType::DATA);
    RETURN_NULLPTR_AS_NULLPTR_NO_LOG(byteBuffer);
    BlockRef block = BuildDataBlock(byteBuffer);
    return std::dynamic_pointer_cast<DataBlock>(block);
}

BlockRef FileReader::GetOrLoadBlock(const BlockHandle &blockHandle, BlockType blockType)
{
    // 1. 首先从block cache缓存中查询Block.
    uint64_t blockId = GetBlockId(blockHandle);
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
        block = BuildBlock(byteBuffer, blockType);
        if (UNLIKELY(block == nullptr)) {
            LOG_ERROR("Build block failed, block offset:" << blockHandle.GetOffset()
                                                          << ", size:" << blockHandle.GetSize());
            return nullptr;
        }
        mBlockCache->Put(blockId, block, blockType);
        AddMissStat(blockType, block->BufferLen(), CheckNeedToRecord());
        return block;
    }
    AddHitStat(blockType, block->BufferLen(), CheckNeedToRecord());
    return block;
}

uint64_t FileReaderBase::GetBlockId(const BlockHandle &blockHandle)
{
    return static_cast<uint64_t>(FileAddressUtil::GetFileId(mFileMetaData->GetFileAddress())) << NO_32 |
           (mInitOffset + blockHandle.GetOffset());
}

BResult FileReaderBase::FetchBlock(const BlockHandle &blockHandle, ByteBufferRef &byteBuffer, BlockType blockType)
{
    uint64_t blockId = GetBlockId(blockHandle);
    auto cachedBlock = mBlockCache->Get(blockId, blockType);
    if (cachedBlock == nullptr) {
        auto ret = ReadBlock(blockHandle, byteBuffer);
        if (UNLIKELY(ret != BSS_OK && ret != BSS_ALLOC_FAIL)) {
            LOG_ERROR("Read block failed, block:" << blockId << ", size:" << blockHandle.GetSize() << ", ret:" << ret);
        }
        return ret;
    }
    byteBuffer = cachedBlock->GetBuffer();
    return BSS_OK;
}

BResult FileReaderBase::ReadBlock(const BlockHandle &blockHandle, ByteBufferRef &byteBuffer)
{
    // 1. 从BlockHandler中获取Block的offset和size信息.
    uint32_t blockOffset = GetBlockHandleOffset(blockHandle);
    uint32_t rawSize = blockHandle.GetSize();

    // 2. 读取Block的head信息与value信息.
    uint32_t totalBufferSize = HEAD_BLOCK_SIZE + rawSize;
    auto addr = FileMemAllocator::Alloc(mMemManager, mHolder, totalBufferSize, __FUNCTION__);
    if (UNLIKELY(addr == 0)) {
        return BSS_ALLOC_FAIL;
    }
    byteBuffer = MakeRef<ByteBuffer>(reinterpret_cast<uint8_t *>(addr), totalBufferSize, mMemManager);
    if (UNLIKELY(byteBuffer == nullptr)) {
        mMemManager->ReleaseMemory(addr);
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
    uint32_t originLength = *reinterpret_cast<uint32_t *>(byteBuffer->Data() + NO_1);
    uint32_t crc = *reinterpret_cast<uint32_t *>(byteBuffer->Data() + NO_5);
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

BResult FileReaderBase::DecompressBlock(ByteBufferRef &buffer, uint32_t originLength, CompressAlgo compressAlgo)
{
    if (compressAlgo == CompressAlgo::NONE || !CompressorUtils::IsSupportCodec(compressAlgo)) {
        return BSS_OK;
    }
    CompressorRef compressor = CompressorUtils::InitCompressor(compressAlgo);
    RETURN_ALLOC_FAIL_AS_NULLPTR(compressor);
    auto addrOrigin = FileMemAllocator::Alloc(mMemManager, mHolder, originLength, __FUNCTION__);
    if (UNLIKELY(addrOrigin == 0)) {
        buffer = nullptr;
        return BSS_ALLOC_FAIL;
    }
    ByteBufferRef originBuffer = MakeRef<ByteBuffer>(reinterpret_cast<uint8_t *>(addrOrigin), originLength,
                                                     mMemManager);
    if (UNLIKELY(originBuffer == nullptr)) {
        mMemManager->ReleaseMemory(addrOrigin);
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

uint32_t FileReaderBase::GetBlockHandleOffset(const BlockHandle &blockHandle) const
{
    return mInitOffset + blockHandle.GetOffset();
}

BlockRef FileReader::BuildBlock(const ByteBufferRef &buffer, BlockType blockType)
{
    RETURN_NULLPTR_AS_NULLPTR(buffer);
    if (blockType == BlockType::FILTER) {
        return BuildFilterBlock(buffer);
    } else if (blockType == BlockType::DATA) {
        return BuildDataBlock(buffer);
    } else if (blockType == BlockType::INDEX) {
        return BuildIndexBlock(buffer);
    }
    return nullptr;
}

BlockRef FileReader::BuildFilterBlock(ByteBufferRef byteBuffer)
{
    auto filterBlock = std::make_shared<FilterBlock>(byteBuffer);
    RETURN_NULLPTR_AS_NOT_OK(filterBlock->Init());
    return filterBlock;
}

BlockRef FileReader::BuildDataBlock(ByteBufferRef byteBuffer)
{
    auto blockRef = std::make_shared<DataBlock>(byteBuffer);
    auto ret = blockRef->Init(mMemManager, mHolder);
    if (UNLIKELY(ret != BSS_OK)) {
        LOG_ERROR("Failed to init block.");
        blockRef = nullptr;
    }
    return blockRef;
}

BlockRef FileReader::BuildIndexBlock(ByteBufferRef byteBuffer)
{
    return std::make_shared<IndexBlock>(byteBuffer, mMemManager, mHolder);
}

BlockRef FileReader::GetOrLoadDataBlock(const BlockHandle &blockHandle)
{
    return GetOrLoadBlock(blockHandle, BlockType::DATA);
}

BResult FileReaderBase::Open()
{
    if (mInitialized) {
        return BSS_OK;
    }
    // 1. 初始化file inputView.
    mInputView = std::make_shared<FileInputView>();
    RETURN_NOT_OK(mInputView->Init(static_cast<FileSystemType>(mFileStatus), mPath));
    // 2. 初始化file reader.
    auto ret = Initialize();
    RETURN_NOT_OK_NO_LOG(ret);
    mInitialized = true;
    return BSS_OK;
}

BResult FileReader::Initialize()
{
    BResult ret = InitializeFooterAndMetaBlock();
    if (ret != BSS_OK) {
        LOG_WARN("Init footer and block handle failed, ret:" << ret << ", path:" << mPath->ExtractFileName());
        return ret;
    }

    ret = InitializeIndexReader();
    if (ret != BSS_OK) {
        LOG_WARN("Init index reader failed, ret:" << ret << ", path:" << mPath->ExtractFileName());
    }
    return ret;
}

BResult FileReader::InitializeFooterAndMetaBlock()
{
    uint32_t footerOffset = mInitOffset + static_cast<uint32_t>(mFileMetaData->GetFileSize()) - sizeof(FooterStructure);
    FooterStructure footer{};
    auto ret = mInputView->ReadBuffer(reinterpret_cast<uint8_t *>(&footer), sizeof(FooterStructure), footerOffset);
    if (UNLIKELY(ret != BSS_OK)) {
        LOG_ERROR("Read footer from file input view failed, ret:" << ret << ", readSize:" << sizeof(FooterStructure));
        return ret;
    }
    if (UNLIKELY(footer.magicNum != BLOCK_COMMON_MAGIC_NUM)) {
        LOG_ERROR("Inconsistent magic number, expected:" << BLOCK_COMMON_MAGIC_NUM << ", actual:" << footer.magicNum);
        return BSS_INNER_ERR;
    }
    BlockHandle metaIndexBlockHandle(footer.metaIndexBlockHandleOffset, footer.metaIndexBlockHandleSize);
    ByteBufferRef metaIndexBlockBuffer = nullptr;
    ret = ReadBlock(metaIndexBlockHandle, metaIndexBlockBuffer);
    if (UNLIKELY(ret != BSS_OK)) {
        LOG_ERROR("Read meta index block failed, ret:" << ret);
        return ret;
    }
    FileMetaIndexRef fileMetaIndex = FileMetaIndex::CreateFileMetaIndex(metaIndexBlockBuffer);
    // 初始化FilterHandle
    ret = InitializeFilterHandle(fileMetaIndex);
    if (UNLIKELY(ret != BSS_OK)) {
        LOG_WARN("Init filter handle failed, ret:" << ret << ", path:" << mPath->ExtractFileName());
        return ret;
    }
    // 初始化IndexHandle
    ret = InitializeIndexHandle(fileMetaIndex);
    if (UNLIKELY(ret != BSS_OK)) {
        LOG_WARN("Init index handle failed, ret:" << ret << ", path:" << mPath->ExtractFileName());
    }
    return ret;
}

FilterBlockRef FileReader::GetFilterBlock()
{
    RETURN_NULLPTR_AS_NULLPTR(mFilterBlockHandle);
    ByteBufferRef byteBuffer;
    auto ret = ReadBlock(*mFilterBlockHandle, byteBuffer);
    if (UNLIKELY(ret != BSS_OK)) {
        LOG_WARN("Read filter block failed, ret:" << ret << ", block offset:" << mFilterBlockHandle->GetOffset()
                                                  << ", size:" << mFilterBlockHandle->GetSize());
        return nullptr;
    }
    LOG_DEBUG("Read filter block success, block offset:" << mFilterBlockHandle->GetOffset()
                                                         << ", size:" << mFilterBlockHandle->GetSize());
    FilterBlockRef filterBlock = std::make_shared<FilterBlock>(byteBuffer);
    RETURN_NULLPTR_AS_NULLPTR(filterBlock);
    RETURN_NULLPTR_AS_NOT_OK(filterBlock->Init());
    return filterBlock;
}

IndexBlockRef FileReader::GetIndexBlock()
{
    RETURN_NULLPTR_AS_NULLPTR(mIndexBlockHandle);
    ByteBufferRef byteBuffer;
    auto ret = ReadBlock(*mIndexBlockHandle, byteBuffer);
    if (UNLIKELY(ret != BSS_OK)) {
        LOG_WARN("Read index block failed, ret:" << ret << ", block offset:" << mIndexBlockHandle->GetOffset()
                                                 << ", size:" << mIndexBlockHandle->GetSize());
        return nullptr;
    }
    LOG_DEBUG("Read index block success, block offset:" << mIndexBlockHandle->GetOffset()
                                                        << ", size:" << mIndexBlockHandle->GetSize());
    IndexBlockRef indexBlock = std::make_shared<IndexBlock>(byteBuffer, mMemManager, mHolder);
    return indexBlock;
}

BResult FileReader::InitializeFilterHandle(const FileMetaIndexRef &fileMetaIndex)
{
    BlockHandleRef metaBlockMeta = fileMetaIndex->GetMetaBlock(NO_0);
    RETURN_ERROR_AS_NULLPTR(metaBlockMeta);
    mFilterBlockHandle = metaBlockMeta;
    return BSS_OK;
}

BResult FileReader::InitializeIndexHandle(const FileMetaIndexRef &fileMetaIndex)
{
    BlockHandleRef metaBlockMeta = fileMetaIndex->GetMetaBlock(NO_1);
    RETURN_ERROR_AS_NULLPTR(metaBlockMeta);
    mIndexBlockHandle = metaBlockMeta;
    return BSS_OK;
}

BResult FileReader::InitializeIndexReader()
{
    mIndexReader = std::make_shared<IndexReader>(mMemManager, mHolder, shared_from_this());
    return mIndexReader->Init();
}

}  // namespace bss
}  // namespace ock