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

#ifndef BOOST_SS_LAZY_DOWNLOAD_STRATEGY_H
#define BOOST_SS_LAZY_DOWNLOAD_STRATEGY_H

#include "common/bss_metric.h"
#include "file_holder.h"
#include "lsm_store/file/primary_address_manager.h"
#include "restore_file_info.h"

namespace ock {
namespace bss {
class LazyDownloadStrategy : public std::enable_shared_from_this<LazyDownloadStrategy> {
public:
    LazyDownloadStrategy(FileManagerRef &fileManager, ExecutorServiceRef &executorService,
                         PrimaryAddressManagerRef &primaryAddressManager,
                         std::function<bool(uint64_t)> &isFileInCompaction)
        : mIoExecutor(executorService),
          mIsFileInCompaction(isFileInCompaction),
          mPrimaryAddressManager(primaryAddressManager),
          mLocalFileManager(fileManager)
    {
    }

    virtual void ToWaitingList(const RestoreFileInfo &fileInfo, const PathRef &path)
    {
    }

    virtual void StartDownload(const std::vector<FileHolderRef> &fileHolders)
    {
    }

    virtual void NotifyRead(uint64_t fileAddress)
    {
    }

    virtual void AddAddressRef(uint64_t fileAddress, uint32_t refCount)
    {
    }

    FileInfoRef Download(RestoreFileInfo &restoreFileInfo, const PathRef &localPath);

    BResult MigrateFileMetasWithLocalMapping(
        std::unordered_map<std::string, RestoreFileInfo> &toAddMapping,
        std::unordered_map<std::string, std::tuple<uint64_t, uint32_t>> &addedMapping,
        std::unordered_map<std::string, uint32_t> &refCountMap, std::vector<FileHolderRef> &fileHolders);

    inline bool IsStop()
    {
        return mStopped.load();
    }

    static inline void NotifyStop(const std::vector<FileHolderRef> &fileHolders)
    {
        for (auto &fileHolder : fileHolders) {
            fileHolder->NotifyStop();
        }
    }

    void RegisterLazyDownloadMetric(BoostNativeMetricPtr *metricPtrAddr)
    {
        mMetricPtrAddr = metricPtrAddr;
    }

protected:
    ExecutorServiceRef mIoExecutor;
    std::mutex mEvictLock;
    std::function<bool(uint64_t)> mIsFileInCompaction;
    PrimaryAddressManagerRef mPrimaryAddressManager;
    FileManagerRef mLocalFileManager;
    std::atomic<bool> mStopped{ true };
    BoostNativeMetricPtr *mMetricPtrAddr = nullptr;
};
using LazyDownloadStrategyRef = std::shared_ptr<LazyDownloadStrategy>;

class LazyDownloadRestore : public LazyDownloadStrategy {
public:
    LazyDownloadRestore(FileManagerRef &fileManager, ExecutorServiceRef &executorService,
                        PrimaryAddressManagerRef &primaryAddressManager,
                        std::function<bool(uint64_t)> isFileInCompaction)
        : LazyDownloadStrategy(fileManager, executorService, primaryAddressManager, isFileInCompaction), mVersion(0)
    {
    }

    void ToWaitingList(const RestoreFileInfo &fileInfo, const PathRef &path) override;

    void StartDownload(const std::vector<FileHolderRef> &fileHolders) override;

    void NotifyRead(uint64_t fileAddress) override;

    void AddAddressRef(uint64_t fileAddress, uint32_t refCount) override;

private:
    void ScheduleNextDownload(const std::vector<FileHolderRef> &fileHolders);

    void BatchDownloadThenMigrate(std::vector<FileHolderRef> &fileHolders);

    void DoDownloadFile(std::vector<std::tuple<FileInfoRef, RestoreFileInfo>> &fileInfos, bool &downloaded);

    void UpdateMetric();

private:
    std::unordered_map<uint64_t, std::tuple<RestoreFileInfo, PathRef>> mWaitingDownload;
    std::mutex mWaitingLock;
    uint32_t mVersion;
    long mLazyDownloadStartTime = 0L;
    long mLazyDownloadEndTime = 0L;
};

}  // namespace bss
}  // namespace ock
#endif  // BOOST_SS_LAZY_DOWNLOAD_STRATEGY_H