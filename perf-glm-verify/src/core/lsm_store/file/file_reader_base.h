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

#ifndef BOOST_SS_FILE_READER_BASE_H
#define BOOST_SS_FILE_READER_BASE_H

#include "block/data_block.h"
#include "block/filter_block_writer.h"
#include "block/index_block_writer.h"
#include "bss_metric.h"
#include "file_address_util.h"
#include "file_meta_data.h"
#include "include/compress_algo.h"
#include "lsm_store/block/block_cache.h"
#include "lsm_store/block/block_handle.h"
#include "lsm_store/key/full_key_filter.h"

namespace ock {
namespace bss {
class FileReaderBase {
public:
    FileReaderBase(const FileMetaDataRef &fileMetaData, const ConfigRef &config, const PathRef &path,
                   const BlockCacheRef &blockCache, const MemManagerRef &memManager, FileProcHolder holder)
        : mFileMetaData(fileMetaData), mPath(path), mBlockCache(blockCache), mConfig(config), mMemManager(memManager)
    {
        RETURN_AS_NULLPTR(config);
        mCacheIndexAndFilter = config->GetCacheIndexAndFilterSwitch();
        mInitOffset = FileAddressUtil::GetFileOffset(mFileMetaData->GetFileAddress());
        mHolder = holder;
    }

    virtual ~FileReaderBase()
    {
        LOG_DEBUG("Delete FileReaderBase success.");
    }

    virtual BResult Get(const Key &key, Value &value) = 0;

    virtual BlockRef BuildFilterBlock(ByteBufferRef byteBuffer) = 0;

    virtual BlockRef BuildDataBlock(ByteBufferRef byteBuffer) = 0;

    virtual BlockRef BuildIndexBlock(ByteBufferRef byteBuffer) = 0;

    virtual BResult Initialize() = 0;

    virtual void Close() = 0;

    BResult Open();

    uint64_t GetBlockId(const BlockHandle &blockHandle);

    BResult FetchBlock(const BlockHandle &blockHandle, ByteBufferRef &byteBuffer, BlockType blockType);

    DataBlockRef FetchDataBlock(const BlockHandle &blockHandle);

    BResult ReadBlock(const BlockHandle &blockHandle, ByteBufferRef &byteBuffer);

    BResult DecompressBlock(ByteBufferRef &outputBuffer, uint32_t originLength, CompressAlgo compressAlgo);

    uint32_t GetBlockHandleOffset(const BlockHandle &blockHandle) const;

    virtual KeyValueIteratorRef IteratorAll(FullKeyFilterRef paramInternalKeyFilter, bool shareReader) = 0;

    virtual KeyValueIteratorRef PrefixIterator(FullKeyFilterRef paramInternalKeyFilter, const Key &paramLK,
                                               bool paramBoolean) = 0;

    virtual FilterBlockRef GetOrLoadFilterBlock() = 0;

    virtual IndexBlockRef GetOrLoadIndexBlock() = 0;

    virtual BlockRef GetOrLoadDataBlock(const BlockHandle &blockHandle) = 0;

    inline void SetFileStatus(FileStatus fileStatus)
    {
        mFileStatus = fileStatus;
    }

    inline void RegisterMetric(BoostNativeMetricPtr metricPtr)
    {
        mBoostNativeMetric = metricPtr;
    }

    inline void AddHitStat(BlockType type, uint64_t size, bool isNeedToRecord)
    {
        if (isNeedToRecord && IsFileCacheEnabled()) {
            mBoostNativeMetric->AddHitStat(type, size);
        }
    }

    inline void AddMissStat(BlockType type, uint64_t size, bool isNeedToRecord)
    {
        if (isNeedToRecord && IsFileCacheEnabled()) {
            mBoostNativeMetric->AddMissStat(type, size);
        }
    }

    inline void AddFilterSuccessCount()
    {
        if (IsFileCacheEnabled()) {
            mBoostNativeMetric->AddFilterSuccessCount();
        }
    }

    inline void AddFilterExistSuccessCount()
    {
        if (IsFileCacheEnabled()) {
            mBoostNativeMetric->AddFilterExistSuccessCount();
        }
    }

    inline void AddFilterExistFailCount()
    {
        if (IsFileCacheEnabled()) {
            mBoostNativeMetric->AddFilterExistFailCount();
        }
    }

    inline bool CheckNeedToRecord()
    {
        return mHolder == FileProcHolder::FILE_STORE_GET || mHolder == FileProcHolder::FILE_STORE_ITERATOR;
    }

    inline void SetFileProcHolder(FileProcHolder holder)
    {
        mHolder = holder;
    }

    inline FileMetaDataRef &GetFileMeta()
    {
        return mFileMetaData;
    }

protected:
    inline bool IsFileCacheEnabled()
    {
        return mBoostNativeMetric != nullptr && mBoostNativeMetric->IsFileCacheMetricEnabled();
    }

    FileMetaDataRef mFileMetaData = nullptr;
    PathRef mPath = nullptr;
    BlockCacheRef mBlockCache = nullptr;
    bool mCacheIndexAndFilter = true;
    uint32_t mInitOffset = 0;
    FileInputViewRef mInputView = nullptr;
    bool mInitialized = false;
    FileStatus mFileStatus = LOCAL;
    ConfigRef mConfig = nullptr;
    MemManagerRef mMemManager = nullptr;
    static thread_local FileProcHolder mHolder;
    BoostNativeMetricPtr mBoostNativeMetric = nullptr;
};
using FileReaderBaseRef = std::shared_ptr<FileReaderBase>;

}  // namespace bss
}  // namespace ock
#endif  // BOOST_SS_FILE_READER_BASE_H