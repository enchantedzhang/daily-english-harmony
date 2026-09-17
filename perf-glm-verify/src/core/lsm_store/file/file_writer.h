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

#ifndef BOOST_SS_FILE_WRITER_H
#define BOOST_SS_FILE_WRITER_H

#include <functional>

#include "binary/slice_binary.h"
#include "common/io/file_output_view.h"
#include "data_slice_flush_iterator.h"
#include "file_block_meta.h"
#include "file_mem_allocator.h"
#include "file_meta_index_block_writer.h"
#include "include/compress_algo.h"
#include "include/config.h"
#include "lsm_store/block/block_handle.h"
#include "lsm_store/block/block_meta.h"
#include "lsm_store/block/data_block_writer.h"
#include "lsm_store/block/filter_block_writer.h"
#include "lsm_store/block/index_block_writer.h"

namespace ock {
namespace bss {
class FileWriter {
public:
    FileWriter(const PathRef &filePath, const ConfigRef &config, CompressAlgo compressAlgorithm,
               const MemManagerRef &memManager, FileProcHolder holder, bool maintainRecordMeta)
        : mFilePath(filePath),
          mCompressAlgorithm(compressAlgorithm),
          mMemManager(memManager),
          mMaintainRecordMeta(maintainRecordMeta),
          mHolder(holder)
    {
        mDataBlockWriter = std::make_shared<DataBlockWriter>(config->GetHashIndexLoadRatio(), memManager, holder);
        mIndexBlockWriter = CreateIndexBlockWriter(config, memManager, holder);
        mFilterBlockWriter = std::make_shared<HashFilterBlockWriter>();
        mBlockSize = config->GetDataBlockSize();
        mFileOutputView = std::make_shared<FileOutputView>();
        BResult ret = mFileOutputView->Init(filePath, config);
        if (UNLIKELY(ret != BSS_OK)) {
            LOG_ERROR("Init file output view failed, ret:" << ret);
            mFileOutputView = nullptr;
        }
        mBufferOutputView = std::make_shared<OutputView>(mBlockSize, mMemManager, holder);
        mDataBlockStat = std::make_shared<DataBlockStat>();
        mReusableBlockMeta = std::make_shared<BlockMeta>();
        mMetaIndexBlockWriter = std::make_shared<FileMetaIndexBlockWriter>(mBufferOutputView);
        mFinished = false;
        mClosed = false;
        mIndexBlockStat = std::make_shared<IndexBlockStat>();
    }

    BResult Add(const KeyValueRef &keyValue);

    BResult Finish(FileBlockMetaRef &fileMeta);

    BResult BuildDataBlock();

    BResult BuildMetaBlocks();

    BResult BuildFilterBlock();

    BResult BuildIndexBlock();

    BResult BuildFooter();

    BResult BuildMetaIndexBlock();

    BResult WriteBlock(const ByteBufferRef &buffer, BlockHandleRef &blockHandle, bool needCompress = true);

    BResult CompressBlock(ByteBufferRef &buffer, uint32_t &bufferSize, bool needCompress);

    inline uint64_t GetMagicNumber() const
    {
        return BLOCK_COMMON_MAGIC_NUM;
    }

    inline uint32_t CurrentEstimateSize() const
    {
        // 文件已知大小 + DataBlock的大小 + IndexBlock的大小.
        auto currentSize = mDataBlockWriter->CurrentEstimateSize();
        if (UNLIKELY(currentSize == UINT32_MAX)) {
            return UINT32_MAX;
        }
        return mFileOutputSize + currentSize + mIndexBlockWriter->CurrentEstimateSize();
    }

    static IndexBlockWriterRef CreateIndexBlockWriter(const ConfigRef &config, const MemManagerRef &memManager,
                                                      FileProcHolder holder)
    {
        return std::make_shared<IndexBlockWriter>(config->GetHashIndexLoadRatio(), memManager, holder);
    }

    inline bool IsStateChange(const KeyValueRef &keyValue) const
    {
        RETURN_FALSE_AS_NULLPTR(keyValue);
        RETURN_FALSE_AS_NULLPTR(mDataBlockWriter);
        RETURN_FALSE_AS_NULLPTR(mDataBlockWriter->GetEndKeyValue());
        Key key = mDataBlockWriter->GetEndKeyValue()->key;
        if (StateId::GetStateType(key.StateId()) == PQ && StateId::GetStateType(keyValue->key.StateId()) != PQ) {
            return true;
        }
        return false;
    }

private:
    PathRef mFilePath = nullptr;
    uint32_t mBlockSize = 0;  // 在配置项中设置data block size的大小为16KB.
    CompressAlgo mCompressAlgorithm = CompressAlgo::NONE;
    FileOutputViewRef mFileOutputView = nullptr;
    uint32_t mFileOutputSize = 0;
    DataBlockWriterRef mDataBlockWriter = nullptr;
    BlockMetaRef mReusableBlockMeta = nullptr;
    IndexBlockWriterRef mIndexBlockWriter = nullptr;
    FilterBlockWriterRef mFilterBlockWriter = nullptr;
    BlockHandleRef mFilterBlockHandle = nullptr;
    uint32_t mFilterBlockRawSize = 0;
    BlockHandleRef mIndexBlockHandle = nullptr;
    FileMetaIndexBlockWriterRef mMetaIndexBlockWriter = nullptr;
    BlockHandleRef mMetaIndexBlockHandle = nullptr;
    OutputViewRef mBufferOutputView = nullptr;
    DataBlockStatRef mDataBlockStat = nullptr;
    IndexBlockStatRef mIndexBlockStat = nullptr;
    MemManagerRef mMemManager = nullptr;
    StateIdInterval mStateIdInterval;
    FileRecordMeta mRecordMeta;
    bool mFinished = false;
    bool mClosed = false;
    bool mMaintainRecordMeta = false;
    bool mHasTrackedRecord = false;
    FileProcHolder mHolder;
};
using FileWriterRef = std::shared_ptr<FileWriter>;

}  // namespace bss
}  // namespace ock
#endif  // BOOST_SS_FILE_WRITER_H
