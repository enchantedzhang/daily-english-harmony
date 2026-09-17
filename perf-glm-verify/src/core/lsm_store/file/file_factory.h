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

#ifndef FILE_FACTORY_H
#define FILE_FACTORY_H

#include "common/block.h"
#include "data_slice_flush_iterator.h"
#include "file_info.h"
#include "file_reader.h"
#include "file_writer.h"
#include "include/compress_algo.h"
#include "include/config.h"
#include "slice_table/slice/data_slice.h"

namespace ock {
namespace bss {
enum class FileFactoryType { HASH = 1, SORTED = 2, TEST = 3 };

class FileFactory {
public:
    FileFactory(const ConfigRef &config, const MemManagerRef &memManager) : mConfig(config), mMemManager(memManager)
    {
    }

    FileFactory(const ConfigRef &config, const BlockCacheRef &blockCache) : mConfig(config), mBlockCache(blockCache)
    {
        mNeedReleaseBlockCache = false;
    }

    ~FileFactory()
    {
        if (mNeedReleaseBlockCache) {
            BlockCacheManager::Instance()->DeleteBlockCache(mConfig->GetTaskSlotFlag());
        }
        LOG_INFO("Delete FileFactory success.");
    }

    static std::shared_ptr<FileFactory> CreateFileFactory(const ConfigRef &config, const MemManagerRef &memManager);

    inline BResult Initialize()
    {
        uint64_t fileMemLimit = mMemManager->GetMemoryTypeMaxSize(MemoryType::FILE_STORE);
        float indexCacheRatio = 0.0f;
        if (mConfig->GetCacheIndexAndFilterSwitch()) {
            indexCacheRatio = mConfig->GetCacheIndexAndFilterRatio();
        }
        mBlockCache = BlockCacheManager::Instance()->CreateBlockCache(mConfig->GetTaskSlotFlag(), fileMemLimit,
                                                                      indexCacheRatio);
        mNeedReleaseBlockCache = true;
        mMemManager->RegisterLsmStoreRevoke([this](uint64_t revokeSize) {
            if (UNLIKELY(mBlockCache == nullptr)) {
                return;
            }
            mBlockCache->Revoke(revokeSize);
        });
        return BSS_OK;
    }

    inline BResult Initialize(const MemManagerRef &memManager)
    {
        mMemManager = memManager;
        mMemManager->RegisterLsmStoreRevoke([this](uint64_t revokeSize) { mBlockCache->Revoke(revokeSize); });
        return BSS_OK;
    }

    inline FileFactoryType GetFileFactoryType() const
    {
        return FileFactoryType::HASH;
    }

    inline FileReaderBaseRef CreateFileReader(const FileMetaDataRef &fileMetaData, const FileInfoRef &fileInfo,
                                              const MemManagerRef &memManager, FileProcHolder holder)
    {
        if (UNLIKELY(fileInfo == nullptr)) {
            LOG_WARN("input fileInfo is null.");
            return nullptr;
        }
        return std::make_shared<FileReader>(fileMetaData, mConfig, fileInfo->GetFilePath(), mBlockCache, memManager,
                                            holder);
    }

    inline FileWriterRef CreateFileWriter(const PathRef &path, const ConfigRef &config, CompressAlgo compressAlgorithm,
                                          const MemManagerRef &memManager, FileProcHolder holder,
                                          bool maintainRecordMeta)
    {
        return std::make_shared<FileWriter>(path, config, compressAlgorithm, memManager, holder, maintainRecordMeta);
    }

    inline void RegisterMetric(BoostNativeMetricPtr metricPtr)
    {
        if (mBlockCache != nullptr) {
            mBlockCache->RegisterMetric(mConfig, metricPtr);
        }
    }

private:
    ConfigRef mConfig = nullptr;
    bool mNeedReleaseBlockCache = false;
    BlockCacheRef mBlockCache = nullptr;
    MemManagerRef mMemManager = nullptr;
};
using FileFactoryRef = std::shared_ptr<FileFactory>;

}  // namespace bss
}  // namespace ock
#endif  // FILE_FACTORY_H
