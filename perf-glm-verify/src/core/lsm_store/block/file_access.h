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

#ifndef BOOST_SS_FILE_ACCESS_H
#define BOOST_SS_FILE_ACCESS_H

#include <memory>

#include "lsm_store/file/file_cache_manager.h"
#include "lsm_store/file/file_factory.h"
#include "lsm_store/file/file_mem_allocator.h"
#include "lsm_store/file/file_meta_data.h"
#include "lsm_store/file/file_reader.h"

namespace ock {
namespace bss {
class FileAccess {
public:
    explicit FileAccess(const FileMetaDataRef &fileMetaData, const FileFactoryRef &fileFactory,
                        const FileCacheManagerRef &fileCache, const MemManagerRef &memManager)
        : mFileMetaData(fileMetaData), mFileFactory(fileFactory), mFileCache(fileCache), mMemManager(memManager)
    {
        LOG_DEBUG("create FileAccess for file:" << fileMetaData->GetFileAddress());
    }

    ~FileAccess()
    {
        std::lock_guard<std::mutex> lock(mMutex);
        for (const auto &fileReader : mFileReader) {
            if (fileReader != nullptr) {
                fileReader->Close();
            }
        }
        LOG_DEBUG("delete FileAccess success.");
    }

    inline FileReaderBaseRef GetFileReader(FileProcHolder holder, BoostNativeMetricPtr boostNativeMetric)
    {
        auto holderIndex = FileMemAllocator::IsForceType(holder) ? NO_1 : NO_0;
        std::lock_guard<std::mutex> lock(mMutex);
        if (UNLIKELY(mFileReader[holderIndex] == nullptr)) {
            mFileReader[holderIndex] = CreateFileReader(holder, boostNativeMetric);
        }
        if (LIKELY(mFileReader[holderIndex] != nullptr)) {
            mFileReader[holderIndex]->SetFileProcHolder(holder);
        }
        return mFileReader[holderIndex];
    }

    inline FileReaderBaseRef CreateFileReader(FileProcHolder holder, BoostNativeMetricPtr boostNativeMetric)
    {
        FileInfoRef fileInfo = mFileCache->GetPrimaryFileInfo(mFileMetaData->GetFileAddress());
        FileReaderBaseRef fileReader = mFileFactory->CreateFileReader(mFileMetaData, fileInfo, mMemManager, holder);
        if (UNLIKELY(fileReader == nullptr)) {
            LOG_ERROR("MakeRef failed, fileReader is nullptr");
            return nullptr;
        }
        fileReader->SetFileStatus(fileInfo->GetFileStatus());
        fileReader->RegisterMetric(boostNativeMetric);
        auto ret = fileReader->Open();
        if (UNLIKELY(ret != BSS_OK)) {
            if (FileMemAllocator::IsForceType(holder)) {
                LOG_ERROR("Open file reader failed, ret:" << ret);
            } else {
                LOG_WARN("Open file reader failed, ret:" << ret);
            }
            return nullptr;
        }
        LOG_DEBUG("Create and open file reader success, fileAddress:" << mFileMetaData->GetFileAddress());
        return fileReader;
    }

private:
    FileMetaDataRef mFileMetaData = nullptr;
    FileReaderBaseRef mFileReader[NO_2] = { nullptr };
    mutable std::mutex mMutex;
    FileFactoryRef mFileFactory = nullptr;
    FileCacheManagerRef mFileCache = nullptr;
    MemManagerRef mMemManager = nullptr;
};
using FileAccessRef = std::shared_ptr<FileAccess>;

}  // namespace bss
}  // namespace ock
#endif  // BOOST_SS_FILE_ACCESS_H