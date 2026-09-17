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

#ifndef BLOB_FILE_WRITER_H
#define BLOB_FILE_WRITER_H
#include <bss_err.h>

#include "blob_data_block.h"
#include "blob_file_meta.h"
#include "blob_file_reader.h"
#include "blob_immutable_file.h"
#include "blob_index_block_writer.h"
#include "block/block_handle.h"
#include "file/file_address_util.h"
#include "file/file_id.h"
#include "io/file_output_view.h"
#include "lsm_store/block/block_cache.h"
#include "lsm_store/file/file_cache_manager.h"

namespace ock {
namespace bss {

class BlobFileWriter : public std::enable_shared_from_this<BlobFileWriter> {
public:
    ~BlobFileWriter()
    {
        if (mFileOutputView != nullptr) {
            mFileOutputView->Close();
        }
    }

    BResult Init(const PathRef &filePath, const ConfigRef &config, const FileIdRef &blobFileId,
                 const MemManagerRef &memManager, const BlockCacheRef &blockCache,
                 const FileCacheManagerRef &fileCacheManager)
    {
        RETURN_INVALID_PARAM_AS_NULLPTR(filePath);
        RETURN_INVALID_PARAM_AS_NULLPTR(config);
        RETURN_INVALID_PARAM_AS_NULLPTR(blockCache);
        RETURN_INVALID_PARAM_AS_NULLPTR(fileCacheManager);
        mFileOutputView = std::make_shared<FileOutputView>();
        mFilePath = filePath;
        RETURN_NOT_OK(mFileOutputView->Init(filePath));
        mMemManager = memManager;
        mBlockCache = blockCache;
        mConfig = config;
        mFileCacheManager = fileCacheManager;
        mBlobIndexBlockWriter = std::make_shared<BlobIndexBlockWriter>();
        GroupRangeRef groupRange = std::make_shared<GroupRange>(mConfig->GetStartGroup(), mConfig->GetEndGroup());
        RETURN_INVALID_PARAM_AS_NULLPTR(blobFileId);
        mBlobFileMeta = std::make_shared<BlobFileMeta>(filePath->Name(), groupRange, groupRange, blobFileId->Get());
        mBlobFileReader = std::make_shared<BlobFileReader>();
        RETURN_NOT_OK(mBlobFileReader->Init(mBlobFileMeta, mConfig, filePath, blockCache, memManager));
        return BSS_OK;
    }

    /**
     * Write data blocks to a file
     * @param dataBlock dataBlock
     * @return BResult
     */
    BResult Write(const BlobDataBlockRef &dataBlock);

    BResult WriteBlock(const ByteBufferRef &buffer, BlockHandle &blockHandle);

    BResult CompressBlock(ByteBufferRef &buffer, uint32_t &bufferSize);

    inline BResult Flush() const
    {
        RETURN_ERROR_AS_NULLPTR(mFileOutputView);
        return mFileOutputView->Flush();
    }

    uint32_t GetOutputSize() const
    {
        if (UNLIKELY(mBlobIndexBlockWriter == nullptr)) {
            LOG_ERROR("BlobIndexBlockWriter is nullptr.");
            return UINT32_MAX;
        }
        if (UNLIKELY(UINT32_MAX - mBlobIndexBlockWriter->EstimateSize() < mFileOutputSize)) {
            LOG_ERROR("The parameters are not legal, fileOutputSize: " << mFileOutputSize << ", index block size:"
                                                                       << mBlobIndexBlockWriter->EstimateSize());
            return UINT32_MAX;
        }
        return mFileOutputSize + mBlobIndexBlockWriter->EstimateSize();
    }

    const BlobFileMetaRef &GetBlobFileMeta()
    {
        return mBlobFileMeta;
    }

    BlobImmutableFileRef WriteImmutableFile(uint64_t currentVersion);

    BResult SelectBlobValue(uint64_t blobId, uint32_t keyGroup, Value &value);

    ByteBufferRef CreateBuffer(uint32_t size);

private:
    FileOutputViewRef mFileOutputView = nullptr;
    PathRef mFilePath = nullptr;
    uint32_t mFileOutputSize = 0;
    CompressAlgo mCompressAlgorithm = CompressAlgo::LZ4;
    MemManagerRef mMemManager = nullptr;
    BlobIndexBlockWriterRef mBlobIndexBlockWriter = nullptr;
    BlockCacheRef mBlockCache = nullptr;
    BlobFileMetaRef mBlobFileMeta = nullptr;
    ConfigRef mConfig = nullptr;
    FileCacheManagerRef mFileCacheManager = nullptr;
    BlobFileReaderRef mBlobFileReader = nullptr;
};
using BlobFileWriterRef = std::shared_ptr<BlobFileWriter>;
}  // namespace bss
}  // namespace ock

#endif
