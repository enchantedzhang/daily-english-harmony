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

#ifndef BLOB_IMMUTABLE_FILE_H
#define BLOB_IMMUTABLE_FILE_H

#include <memory>

#include "blob_file_meta.h"
#include "blob_file_reader.h"
#include "block/block_cache.h"
#include "block/block_handle.h"
#include "config.h"
#include "file/file_cache_manager.h"
#include "mem_manager.h"

namespace ock {
namespace bss {
struct Value;

class BlobImmutableFile : public std::enable_shared_from_this<BlobImmutableFile> {
public:
    BlobImmutableFile(const PathRef &filePath, const ConfigRef &config, const MemManagerRef &memManager,
                      const BlockCacheRef &blockCache, const FileCacheManagerRef &fileCacheManager,
                      const BlockHandle &blockHandle, const BlobFileMetaRef &blobFileMeta,
                      const BlobFileReaderRef &blobFileReader)
        : mFilePath(filePath),
          mConfig(config),
          mMemManager(memManager),
          mBlockCache(blockCache),
          mFileCacheManager(fileCacheManager),
          mBlockHandle(blockHandle),
          mBlobFileMeta(blobFileMeta),
          mBlobFileReader(blobFileReader)
    {
        mFilePath = std::make_shared<Path>(mBlobFileMeta->GetIdentifier());
    }

    BlobImmutableFile(const ConfigRef &config, const BlobFileMetaRef &blobFileMeta, const MemManagerRef &memManager,
                      const BlockCacheRef &blockCache, const FileCacheManagerRef &fileCacheManager)
        : mConfig(config),
          mMemManager(memManager),
          mBlockCache(blockCache),
          mFileCacheManager(fileCacheManager),
          mBlobFileMeta(blobFileMeta)
    {
        mFilePath = std::make_shared<Path>(mBlobFileMeta->GetIdentifier());
    }

    void CloseFileReader()
    {
        std::lock_guard<std::mutex> lock(mMutex);
        if (mBlobFileReader != nullptr) {
            mBlobFileReader->Close();
            mBlobFileReader = nullptr;
        }
    }

    const BlobFileMetaRef &GetBlobFileMeta()
    {
        return mBlobFileMeta;
    }

    bool IsSame(const BlobFileMetaRef &fileMeta) const
    {
        RETURN_FALSE_AS_NULLPTR(mBlobFileMeta);
        auto range = mBlobFileMeta->GetValidGroupRange();
        RETURN_FALSE_AS_NULLPTR(range);
        RETURN_FALSE_AS_NULLPTR(fileMeta);
        auto otherRange = fileMeta->GetValidGroupRange();
        RETURN_FALSE_AS_NULLPTR(otherRange);
        return (mBlobFileMeta->GetFileAddress() == fileMeta->GetFileAddress()) && (range->Equals(otherRange));
    }

    BResult SelectBlobValue(uint64_t blobId, uint32_t keyGroup, Value &value);

    ByteBufferRef CreateBuffer(uint32_t size);

    BlobFileReaderRef &GetFileReader()
    {
        std::lock_guard<std::mutex> lock(mMutex);
        if (UNLIKELY(mBlobFileReader == nullptr)) {
            mBlobFileReader = CreateFileReader();
        }
        return mBlobFileReader;
    }

    BlobFileReaderRef CreateFileReader() const
    {
        RETURN_NULLPTR_AS_NULLPTR(mFileCacheManager);
        RETURN_NULLPTR_AS_NULLPTR(mBlobFileMeta);
        auto fileInfo = mFileCacheManager->GetPrimaryFileInfo(mBlobFileMeta->GetFileAddress());
        RETURN_NULLPTR_AS_NULLPTR(fileInfo);
        auto fileReader = std::make_shared<BlobFileReader>();
        fileReader->SetFileStatus(fileInfo->GetFileStatus());
        RETURN_NULLPTR_AS_NOT_OK(fileReader->Init(mBlobFileMeta, mConfig, mFilePath, mBlockCache, mMemManager));
        return fileReader;
    }

    void DiscardFile()
    {
        RETURN_AS_NULLPTR(mBlobFileMeta);
        if (UNLIKELY(mBlobFileMeta->DecRef() == 0)) {
            CloseFileReader();
            mFileCacheManager->DiscardFile(mBlobFileMeta->GetFileAddress());
        }
    }

    void IncFileRef() const
    {
        RETURN_AS_NULLPTR(mBlobFileMeta);
        mBlobFileMeta->IncRef();
    }

    BResult RestoreFileFooterAndIndexBlock()
    {
        mBlobFileReader = CreateFileReader();
        if (UNLIKELY(mBlobFileReader == nullptr)) {
            LOG_ERROR("Restore file footer and index fail!");
            return BSS_ERR;
        }
        ByteBufferRef buffer;
        auto ret = mBlobFileReader->ReadBuffer(mBlobFileMeta->GetFileSize() - NO_16, NO_16, buffer);
        if (UNLIKELY(ret != BSS_OK)) {
            LOG_ERROR("Restore file read footer fail, ret:" << ret);
            return ret;
        }
        uint32_t indexBlockOffset = 0;
        ret = buffer->ReadUint32(indexBlockOffset, 0);
        if (UNLIKELY(ret != BSS_OK)) {
            LOG_ERROR("Restore file read footer index offset fail, ret:" << ret);
            return ret;
        }
        uint32_t indexBlockSize = 0;
        ret = buffer->ReadUint32(indexBlockSize, sizeof(indexBlockOffset));
        if (UNLIKELY(ret != BSS_OK)) {
            LOG_ERROR("Restore file read footer index block size fail, ret:" << ret);
            return ret;
        }
        mBlockHandle = { indexBlockOffset, indexBlockSize };
        return BSS_OK;
    }

    inline double EstimateDeleteRatio() const
    {
        if (UNLIKELY(mBlobFileMeta == nullptr)) {
            return 0;
        }
        return mBlobFileMeta->EstimateDeleteRatio();
    }

    inline BlockHandle GetIndexBlockHandle()
    {
        return mBlockHandle;
    }

private:
    PathRef mFilePath = nullptr;
    ConfigRef mConfig = nullptr;
    MemManagerRef mMemManager = nullptr;
    BlockCacheRef mBlockCache = nullptr;
    FileCacheManagerRef mFileCacheManager = nullptr;
    BlockHandle mBlockHandle;
    BlobFileMetaRef mBlobFileMeta = nullptr;
    BlobFileReaderRef mBlobFileReader = nullptr;
    mutable std::mutex mMutex;
};
using BlobImmutableFileRef = std::shared_ptr<BlobImmutableFile>;
}  // namespace bss
}  // namespace ock

#endif