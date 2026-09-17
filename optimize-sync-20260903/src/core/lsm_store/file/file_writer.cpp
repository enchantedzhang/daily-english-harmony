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

#include "file_writer.h"

#include <functional>

#include "compressor_utils.h"
#include "file_structure.h"
#include "include/compress_algo.h"
#include "lsm_store/block/index_block_writer.h"

namespace ock {
namespace bss {

BResult FileWriter::Add(const KeyValueRef &keyValue)
{
    if (UNLIKELY(mClosed || mFinished)) {
        return BSS_ALREADY_EXISTS;
    }

    uint32_t estimateSize = mDataBlockWriter->EstimateAfter(keyValue);
    RETURN_NOT_OK_AS_FALSE(estimateSize == UINT32_MAX, BSS_ERR);
    if (!mDataBlockWriter->IsEmpty() && estimateSize > mBlockSize) {
        auto ret = BuildDataBlock();
        RETURN_NOT_OK_NO_LOG(ret);
    }

    auto ret = mDataBlockWriter->Add(keyValue);
    RETURN_NOT_OK_NO_LOG(ret);
    if (mMaintainRecordMeta) {
        mRecordMeta.UpdateMaxSeqId(keyValue->value.SeqId());
        if (!mHasTrackedRecord) {
            mRecordMeta.SetSingleValueLength(keyValue->value.ValueLen());
            mHasTrackedRecord = true;
        }
    }

    if (mFilterBlockWriter != nullptr) {
        mFilterBlockWriter->Add(keyValue->key.MixedHashCode());
    }
    mStateIdInterval.Update(keyValue->key.StateId());
    return BSS_OK;
}

BResult FileWriter::BuildDataBlock()
{
    ByteBufferRef buffer;
    auto ret = mDataBlockWriter->Finish(buffer);
    RETURN_NOT_OK_NO_LOG(ret);
    BlockHandleRef blockHandle;
    ret = WriteBlock(buffer, blockHandle);
    RETURN_NOT_OK_NO_LOG(ret);

    mReusableBlockMeta->Reset(blockHandle, buffer->Capacity(), mDataBlockWriter->NumKeys(),
                              mDataBlockWriter->GetStartKey(), mDataBlockWriter->GetEndKey());
    mDataBlockStat->UpdateDataBlock(mReusableBlockMeta);
    ret = mIndexBlockWriter->Add(mReusableBlockMeta);
    RETURN_NOT_OK_NO_LOG(ret);
    mDataBlockWriter->Reset();
    return BSS_OK;
}

BResult FileWriter::WriteBlock(const ByteBufferRef &buffer, BlockHandleRef &blockHandle, bool needCompress)
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
    BResult ret = CompressBlock(outputBuffer, outputSize, needCompress);

    // 2. 写入Block head数据.
    uint8_t headData[HEAD_BLOCK_SIZE] = { 0 };  // 其长度为sizeof(NONE)+sizeof(buffer->Capacity())+sizeof(crc).
    headData[0] = (ret == BSS_OK) ? mCompressAlgorithm : CompressAlgo::NONE;
    auto tmpOriginLength = reinterpret_cast<uint32_t *>(headData + NO_1);
    *tmpOriginLength = buffer->Capacity();
    auto crc = reinterpret_cast<uint32_t *>(headData + NO_5);
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
    blockHandle = std::make_shared<BlockHandle>(offset, outputSize);  // 描述该block在file中的位置.
    mFileOutputSize += HEAD_BLOCK_SIZE + outputSize;
    return BSS_OK;
}

BResult FileWriter::CompressBlock(ByteBufferRef &buffer, uint32_t &bufferSize, bool needCompress)
{
    if (!needCompress) {
        return BSS_NO_COMPRESS;
    }
    if (bufferSize < NO_4096) {
        return BSS_NO_COMPRESS;
    }
    if (mCompressAlgorithm == CompressAlgo::NONE || !CompressorUtils::IsSupportCodec(mCompressAlgorithm)) {
        return BSS_NO_COMPRESS;
    }

    CompressorRef compressor = CompressorUtils::InitCompressor(mCompressAlgorithm);
    RETURN_ALLOC_FAIL_AS_NULLPTR(compressor);
    int64_t maxCompressedLength = CompressorUtils::MaxCompressedLength(buffer->Capacity());
    auto addr = FileMemAllocator::Alloc(mMemManager, FileProcHolder::FILE_STORE_COMPRESS, maxCompressedLength,
                                        __FUNCTION__);
    if (UNLIKELY(addr == 0)) {
        return BSS_ALLOC_FAIL;
    }
    ByteBufferRef outputBuffer = MakeRef<ByteBuffer>(reinterpret_cast<uint8_t *>(addr), maxCompressedLength,
                                                     mMemManager);
    if (UNLIKELY(outputBuffer == nullptr)) {
        mMemManager->ReleaseMemory(addr);
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

BResult FileWriter::Finish(FileBlockMetaRef &fileMeta)
{
    if (UNLIKELY(mFileOutputView == nullptr)) {
        LOG_ERROR("File output view is nullptr.");
        return BSS_ERR;
    }

    mFinished = true;
    // 1. Write data block.
    if (!mDataBlockWriter->IsEmpty()) {
        BResult retVal = BuildDataBlock();
        if (UNLIKELY(retVal != BSS_OK)) {
            return retVal;
        }
    }

    // 2. Write meta block.
    BResult retVal = BuildMetaBlocks();
    RETURN_NOT_OK_NO_LOG(retVal);

    // 3. Write footer.
    retVal = BuildFooter();
    RETURN_NOT_OK_NO_LOG(retVal);

    if (mFileOutputView != nullptr) {
        retVal = mFileOutputView->Sync();
        RETURN_NOT_OK_NO_LOG(retVal);
    }
    if (mMaintainRecordMeta) {
        mRecordMeta.Finalize(mDataBlockStat->GetTotalNumKeys());
    }
    fileMeta = std::make_shared<FileBlockMeta>(mFilePath, mDataBlockStat, mIndexBlockStat,
                                               (mFilterBlockHandle == nullptr) ? 0 : mFilterBlockHandle->GetSize(),
                                               mFilterBlockRawSize, mFileOutputSize, mStateIdInterval, mRecordMeta);
    return BSS_OK;
}

BResult FileWriter::BuildMetaBlocks()
{
    // 1. Write filter block.
    auto retVal = BuildFilterBlock();
    RETURN_NOT_OK_NO_LOG(retVal);
    // 2. Write index block.
    retVal = BuildIndexBlock();
    RETURN_NOT_OK_NO_LOG(retVal);
    // 3. Write meta index block.
    return BuildMetaIndexBlock();
}

BResult FileWriter::BuildFilterBlock()
{
    if (mFilterBlockWriter != nullptr) {
        auto capacity = mFilterBlockWriter->CalcFilterBlockLen();
        auto addr = FileMemAllocator::Alloc(mMemManager, mHolder, capacity, __FUNCTION__);
        if (UNLIKELY(addr == 0)) {
            return BSS_ALLOC_FAIL;
        }
        ByteBufferRef buffer = MakeRef<ByteBuffer>(reinterpret_cast<uint8_t *>(addr), capacity, mMemManager);
        if (UNLIKELY(buffer == nullptr)) {
            mMemManager->ReleaseMemory(addr);
            LOG_ERROR("Make byte buffer ref failed.");
            return BSS_ALLOC_FAIL;
        }
        auto ret = mFilterBlockWriter->Finish(buffer->Data(), buffer->Capacity());
        if (UNLIKELY(ret != BSS_OK)) {
            LOG_ERROR("Finish filter block builder failed, ret:" << ret);
            return ret;
        }
        ret = WriteBlock(buffer, mFilterBlockHandle, false);
        if (UNLIKELY(ret != BSS_OK)) {
            LOG_ERROR("Write filter block failed, ret:" << ret);
            return ret;
        }
        LOG_DEBUG("Write filter block success, offset:" << mFilterBlockHandle->GetOffset()
                                                        << ", size:" << mFilterBlockHandle->GetSize());
        mFilterBlockRawSize = buffer->Capacity();
        mMetaIndexBlockWriter->AddMetaBlock(mFilterBlockHandle);
    }

    return BSS_OK;
}

BResult FileWriter::BuildIndexBlock()
{
    RETURN_ERROR_AS_NULLPTR(mIndexBlockWriter);
    ByteBufferRef buffer = nullptr;
    BResult ret = mIndexBlockWriter->Finish(buffer);
    if (UNLIKELY(ret != BSS_OK)) {
        LOG_ERROR("Finish index block builder failed, ret:" << ret);
        return ret;
    }
    ret = WriteBlock(buffer, mIndexBlockHandle, false);
    if (UNLIKELY(ret != BSS_OK)) {
        LOG_ERROR("Write index block failed, ret:" << ret);
        return ret;
    }
    mIndexBlockStat->AddIndexBlockSize(buffer->Capacity());
    mIndexBlockStat->AddIndexBlockRawSize(mIndexBlockHandle->GetSize());
    LOG_DEBUG("Write index block success, offset:" << mIndexBlockHandle->GetOffset()
                                                   << ", size:" << mIndexBlockHandle->GetSize());
    mMetaIndexBlockWriter->AddMetaBlock(mIndexBlockHandle);
    return BSS_OK;
}

BResult FileWriter::BuildFooter()
{
    if (UNLIKELY(mFileOutputView == nullptr)) {
        LOG_ERROR("File output view is nullptr.");
        return BSS_ERR;
    }

    FooterStructure footer = { GetMagicNumber(), mMetaIndexBlockHandle->GetOffset(), mMetaIndexBlockHandle->GetSize() };
    auto ret = mFileOutputView->WriteBuffer(reinterpret_cast<uint8_t *>(&footer),
                                            static_cast<int64_t>(mFileOutputView->Size()), sizeof(FooterStructure));
    if (LIKELY(ret == BSS_OK)) {
        mFileOutputSize += sizeof(FooterStructure);
    }
    return ret;
}

BResult FileWriter::BuildMetaIndexBlock()
{
    ByteBufferRef buffer = nullptr;
    auto ret = mMetaIndexBlockWriter->Finish(buffer);
    if (UNLIKELY(ret != BSS_OK)) {
        LOG_ERROR("Finish meta index block failed, ret:" << ret);
        return ret;
    }

    ret = WriteBlock(buffer, mMetaIndexBlockHandle, false);
    if (UNLIKELY(ret != BSS_OK)) {
        LOG_ERROR("Write metaIndex block failed, ret:" << ret);
        return ret;
    }
    LOG_DEBUG("Write meta index block success, offset:" << mMetaIndexBlockHandle->GetOffset()
                                                        << ", size:" << mMetaIndexBlockHandle->GetSize());
    return BSS_OK;
}

}  // namespace bss
}  // namespace ock
