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

#ifndef BOOST_SS_FILE_CACHE_MANAGER_H
#define BOOST_SS_FILE_CACHE_MANAGER_H

#include <functional>

#include "blob_store/blob_file_meta.h"
#include "file_cache_type.h"
#include "file_directory.h"
#include "file_info.h"
#include "file_manager.h"
#include "file_meta_data.h"
#include "include/config.h"
#include "lazy/lazy_download_strategy.h"
#include "lazy/restore_file_info.h"
#include "primary_address.h"
#include "primary_address_manager.h"

namespace ock {
namespace bss {
class FileCacheManager : public std::enable_shared_from_this<FileCacheManager> {
public:
    FileCacheManager() = default;

    FileCacheManager(const ConfigRef &config, const FileManagerRef &localFileManager,
                     const FileManagerRef &dfsFileManager, const ExecutorServiceRef &ioExecutor)
        : mConfig(config),
          mLocalFileManager(localFileManager),
          mDfsFileManager(dfsFileManager),
          mPrimaryBasePath(localFileManager->GetBasePath()),
          mIoExecutor(ioExecutor)
    {
        mPrimaryFileManager = std::make_shared<PrimaryAddressManager>(localFileManager, dfsFileManager);
    }

    ~FileCacheManager()
    {
        std::unordered_map<std::string, PrimaryAddressRef>().swap(mRestoredFileIdentifierMapping);
    }

    void Init(BoostNativeMetricPtr *metricPtrAddr)
    {
        // 此处循环引用,需要手动置空
        auto self = shared_from_this();
        std::function<bool(uint64_t)> function = [self](uint64_t address) { return self->IsFileInCompaction(address); };
        mLazyDownloadStrategy = std::make_shared<LazyDownloadRestore>(mLocalFileManager, mIoExecutor,
                                                                      mPrimaryFileManager, function);
        mLazyDownloadStrategy->RegisterLazyDownloadMetric(metricPtrAddr);
    }

    void Close()
    {
        // 需要手动置空断开循环引用
        mLazyDownloadStrategy = nullptr;
    }

    inline FileInfoRef AllocateFile(const FileDirectoryRef &fileDirectory,
                                    const std::function<std::string(std::string)> &fileNameGenerator) const
    {
        return mLocalFileManager->AllocateFile(fileDirectory, fileNameGenerator);
    }

    inline FileInfoRef AllocatePrefixFile(const std::string &prefix, const FileDirectoryRef &fileDirectory,
                                          const std::function<std::string(std::string)> &fileNameGenerator) const
    {
        return mLocalFileManager->AllocatePrefixFile(prefix, fileDirectory, fileNameGenerator);
    }

    template <class T>
    inline void ConfirmAllocationOnFlushOrCompaction(const std::vector<std::shared_ptr<T>> &fileMetaDatas) const
    {
        for (const auto &item : fileMetaDatas) {
            auto primaryAddress = std::make_shared<PrimaryAddress>(item->GetFileAddress(), item->GetFileSize(),
                                                                   item->GetIdentifier(), FileStatus::LOCAL);
            mPrimaryFileManager->AddPrimaryAddressRef(primaryAddress, NO_1);
        }
    }

    inline PathRef GetPrimaryDataBasePath() const
    {
        return mLocalFileManager->GetBasePath();
    }

    template <class T> inline void ReleaseFilesForCompaction(const std::vector<std::shared_ptr<T>> &fileMetaList)
    {
        std::lock_guard<std::mutex> lk(mCompactionLock);
        for (auto &metaData : fileMetaList) {
            auto it = mFileAddressInCompaction.find(metaData->GetFileAddress());
            if (it == mFileAddressInCompaction.end()) {
                continue;
            }
            auto count = it->second == 0 ? 0 : it->second - NO_1;
            mFileAddressInCompaction[it->first] = count;
        }
    }

    inline void DiscardFile(uint64_t fileAddress) const
    {
        mPrimaryFileManager->DecPrimaryAddressRef(fileAddress);
    }

    template <class T> void RegisterFilesForCompaction(std::vector<std::shared_ptr<T>> &fileMetaList)
    {
        std::lock_guard<std::mutex> lk(mCompactionLock);
        for (auto &metaData : fileMetaList) {
            auto it = mFileAddressInCompaction.find(metaData->GetFileAddress());
            if (it == mFileAddressInCompaction.end()) {
                mFileAddressInCompaction.emplace(metaData->GetFileAddress(), 1);
                continue;
            }
            auto count = it->second == UINT32_MAX ? UINT32_MAX : it->second + NO_1;
            mFileAddressInCompaction[it->first] = count;
        }
    }

    FileInfoRef GetPrimaryFileInfo(uint64_t address) const
    {
        return mPrimaryFileManager->GetPrimaryFileInfo(address);
    }

    inline FileDirectoryRef CreateFileSubDirectory() const
    {
        FileDirectoryRef subDir = std::make_shared<FileDirectory>(GetPrimaryDataBasePath());
        auto ret = subDir->CreateDirInFileSystem();
        if (UNLIKELY(!ret)) {
            return nullptr;
        }
        RETURN_NULLPTR_AS_NULLPTR(subDir->GetDirectoryPath());
        LOG_INFO("Create file directory success, path:" << subDir->GetDirectoryPath()->ExtractFileName());
        return subDir;
    }

    inline FileDirectoryRef CreateFileSubDirectory(const std::string &name) const
    {
        auto basePath = GetPrimaryDataBasePath();
        RETURN_NULLPTR_AS_NULLPTR(basePath);
        auto subFile = basePath->Name() + "/" + name;
        PathRef subPath = std::make_shared<Path>(subFile);
        FileDirectoryRef subDir = std::make_shared<FileDirectory>(subPath);
        auto ret = subDir->CreateDirInFileSystem();
        if (UNLIKELY(!ret)) {
            LOG_ERROR("Create file directory failed, path:" << subPath->ExtractFileName());
            return nullptr;
        }
        LOG_INFO("Create file directory success, path:" << subDir->GetDirectoryPath()->ExtractFileName());
        return subDir;
    }

    inline FileManagerRef GetLocalFileManager() const
    {
        return mLocalFileManager;
    }

    inline void RegisterFileHolder(const FileHolderRef &fileHolder)
    {
        mFileHolders.emplace_back(fileHolder);
    }

    inline void CleanResource()
    {
        std::vector<FileHolderRef>().swap(mFileHolders);
    }

    BResult RestoreFiles(std::unordered_map<std::string, std::pair<uint64_t, uint32_t>> &result,
                         std::unordered_map<std::string, RestoreFileInfo> &restoredFileMapping,
                         FileDirectoryRef &localFileAllocateDir, bool rescaled);

    inline bool IsFileInCompaction(uint64_t fileAddress)
    {
        std::lock_guard<std::mutex> lk(mCompactionLock);
        return mFileAddressInCompaction.find(fileAddress) != mFileAddressInCompaction.end();
    }

    inline bool IsLazyDownStop() const
    {
        return mLazyDownloadStrategy->IsStop();
    }

    inline void NotifyLazyDownload() const
    {
        mLazyDownloadStrategy->StartDownload(mFileHolders);
    }

private:
    BResult RestoreFromLocal(const std::string &fileName, const RestoreFileInfo &restoreInfo);

    BResult RestoreFromDfs(const std::string &fileName, const RestoreFileInfo &restoreInfo,
                           std::unordered_map<uint64_t, std::string> &downloadedRemoteAddress,
                           const FileDirectoryRef &localFileAllocateDir, bool rescaled);

    BResult ResolveRescaledFile(const FileInfoRef &previousRemoteFileInfo, RestoreFileInfo restoreInfo,
                                const FileDirectoryRef &localFileAllocateDir, const std::string &fileName);

    BResult ResolveUnRescaledFile(const RestoreFileInfo &restoreInfo, const FileDirectoryRef &localFileAllocateDir,
                                  const std::string &fileName);

private:
    ConfigRef mConfig = nullptr;
    FileManagerRef mLocalFileManager = nullptr;
    FileManagerRef mDfsFileManager = nullptr;
    PrimaryAddressManagerRef mPrimaryFileManager = nullptr;
    std::vector<FileHolderRef> mFileHolders;
    LazyDownloadStrategyRef mLazyDownloadStrategy = nullptr;
    std::unordered_map<uint64_t, uint32_t> mFileAddressInCompaction;
    std::mutex mCompactionLock;
    std::unordered_map<std::string, PrimaryAddressRef> mRestoredFileIdentifierMapping;
    std::unordered_map<PathRef, uint32_t> mRescaledFilePathMapping;
    std::set<uint64_t> mRescaledDownloadAddress;
    PathRef mPrimaryBasePath = nullptr;
    ExecutorServiceRef mIoExecutor = nullptr;
};
using FileCacheManagerRef = std::shared_ptr<FileCacheManager>;

}  // namespace bss
}  // namespace ock
#endif  // BOOST_SS_FILE_CACHE_MANAGER_H