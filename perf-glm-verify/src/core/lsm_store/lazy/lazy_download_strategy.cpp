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

#include "lazy_download_strategy.h"

#include <memory>

#include "lazy_download_task.h"
#include "lsm_store/file/file_address_util.h"

namespace ock {
namespace bss {

void LazyDownloadRestore::ToWaitingList(const RestoreFileInfo &fileInfo, const PathRef &path)
{
    LOG_DEBUG("ToWaitingList:" << path->ExtractFileName() << ", size:" << mWaitingDownload.size()
                               << ", address:" << fileInfo.remoteFileAddress << ",version:" << mVersion);
    mStopped.store(false);
    mWaitingDownload.emplace(fileInfo.remoteFileAddress, std::make_tuple(fileInfo, path));
}

void LazyDownloadRestore::StartDownload(const std::vector<FileHolderRef> &fileHolders)
{
    mLazyDownloadStartTime =
        std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch())
            .count();
    ScheduleNextDownload(fileHolders);
}

void LazyDownloadRestore::NotifyRead(uint64_t fileAddress)
{
}

void LazyDownloadRestore::AddAddressRef(uint64_t fileAddress, uint32_t refCount)
{
}

void LazyDownloadRestore::ScheduleNextDownload(const std::vector<FileHolderRef> &fileHolders)
{
    auto self = std::static_pointer_cast<LazyDownloadRestore>(shared_from_this());
    auto function = [self](std::vector<FileHolderRef> &innerFileHolders) {
        self->BatchDownloadThenMigrate(innerFileHolders);
    };
    LazyDownloadTaskRef task = std::make_shared<LazyDownloadTask>(fileHolders, function);
    mIoExecutor->Execute(task);
}

void LazyDownloadRestore::BatchDownloadThenMigrate(std::vector<FileHolderRef> &fileHolders)
{
    if (mStopped.load()) {
        LOG_INFO("No need to lazy download files.");
        NotifyStop(fileHolders);
        UpdateMetric();
        return;
    }
    {
        std::lock_guard<std::mutex> lock(mEvictLock);
        std::vector<std::tuple<FileInfoRef, RestoreFileInfo>> fileInfos;
        if (mWaitingDownload.empty()) {
            mStopped.store(true, std::memory_order_relaxed);
            NotifyStop(fileHolders);
            UpdateMetric();
            LOG_INFO("Finish lazy download all files.");
            return;
        }
        LOG_INFO("Start to run lazy download");
        bool downloaded = false;
        DoDownloadFile(fileInfos, downloaded);
        if (!downloaded) {
            LOG_ERROR("Download remote file fail");
            mStopped.store(true, std::memory_order_relaxed);
            return;
        }
        std::unordered_map<std::string, RestoreFileInfo> toAddMapping;
        std::unordered_map<std::string, uint32_t> refCountMap;
        for (auto &fileInfoTuple : fileInfos) {
            auto fileInfo = std::get<0>(fileInfoTuple);
            auto restoreInfo = std::get<1>(fileInfoTuple);
            auto localFileAddress = FileAddressUtil::GetFileAddressWithZeroOffset(fileInfo->GetFileId()->Get());
            restoreInfo.remoteFileAddress = localFileAddress;
            toAddMapping.emplace(restoreInfo.remoteFileName, restoreInfo);
            refCountMap.emplace(restoreInfo.remoteFileName, restoreInfo.refCount);
        }
        std::unordered_map<std::string, std::tuple<uint64_t, uint32_t>> addedMapping;
        MigrateFileMetasWithLocalMapping(toAddMapping, addedMapping, refCountMap, fileHolders);
    }
    ScheduleNextDownload(fileHolders);
}

FileInfoRef LazyDownloadStrategy::Download(RestoreFileInfo &restoreFileInfo, const PathRef &localPath)
{
    std::function<std::string()> function = [&localPath]() -> std::string { return localPath->Name(); };
    FileInfoRef fileInfo = mLocalFileManager->AllocateFile(function);
    if (fileInfo == nullptr) {
        LOG_ERROR("Allocate local file fail!");
        return fileInfo;
    }
    std::string restoreLocalFileName = restoreFileInfo.restoreLocalFileName;
    std::string shortName = PathTransform::ExtractFileName(restoreLocalFileName);
    PathRef targetFile = std::make_shared<Path>(mLocalFileManager->GetBasePath(), std::make_shared<Path>(shortName));

    restoreFileInfo.restoreLocalFileName = targetFile->Name();
    BResult ret = SnapshotDownloader::DownloadFile(restoreFileInfo.remoteFileName, targetFile->Name());
    if (ret != BSS_OK) {
        LOG_ERROR("Download remote file fail, remote path:"
                  << PathTransform::ExtractFileName(restoreFileInfo.remoteFileName)
                  << ",local path:" << PathTransform::ExtractFileName(restoreLocalFileName));
        return nullptr;
    }
    return fileInfo;
}

void LazyDownloadRestore::DoDownloadFile(std::vector<std::tuple<FileInfoRef, RestoreFileInfo>> &fileInfos,
                                         bool &downloaded)
{
    std::lock_guard<std::mutex> qLock(mWaitingLock);
    uint32_t index = 0;
    for (auto item = mWaitingDownload.begin(); item != mWaitingDownload.end();) {
        if (index++ == NO_10) {
            break;
        }
        auto tuple = item->second;
        auto restoreInfo = std::get<0>(tuple);
        if (mIsFileInCompaction(restoreInfo.remoteFileAddress)) {
            item = mWaitingDownload.erase(item);
            continue;
        }
        FileInfoRef fileInfo = Download(restoreInfo, std::get<1>(tuple));
        if (fileInfo != nullptr) {
            fileInfos.emplace_back(fileInfo, restoreInfo);
            item = mWaitingDownload.erase(item);
            downloaded = true;
            continue;
        }
        LOG_ERROR("Download remote file fail:" << PathTransform::ExtractFileName(restoreInfo.remoteFileName));
        ++item;
    }
}

void LazyDownloadRestore::UpdateMetric()
{
    if (UNLIKELY(mMetricPtrAddr == nullptr)) {
        LOG_WARN("Metric pointer address is nullptr.");
        return;
    }
    if ((*mMetricPtrAddr) != nullptr && (*mMetricPtrAddr)->IsRestoreMetricEnabled()) {
        mLazyDownloadEndTime =
            std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch())
                .count();
        if (mLazyDownloadEndTime > mLazyDownloadStartTime) {
            (*mMetricPtrAddr)->SetLazyDownloadTime(mLazyDownloadEndTime - mLazyDownloadStartTime);
        } else {
            (*mMetricPtrAddr)->SetLazyDownloadTime(0);
        }
    }
}

BResult LazyDownloadStrategy::MigrateFileMetasWithLocalMapping(
    std::unordered_map<std::string, RestoreFileInfo> &toAddMapping,
    std::unordered_map<std::string, std::tuple<uint64_t, uint32_t>> &addedMapping,
    std::unordered_map<std::string, uint32_t> &refCountMap, std::vector<FileHolderRef> &fileHolders)
{
    if (toAddMapping.empty()) {
        return BSS_ERR;
    }
    uint64_t startTime = TimeStampUtil::GetCurrentTime();
    uint32_t successMigration = 0;
    bool success = true;
    auto self = shared_from_this();
    auto function = [self, &refCountMap](std::unordered_map<std::string, std::tuple<uint64_t, uint32_t>> entry) {
        for (auto &item : entry) {
            auto primaryAddress = std::make_shared<PrimaryAddress>(std::get<0>(item.second), std::get<1>(item.second),
                                                                   item.first, FileStatus::LOCAL);

            if (refCountMap.find(item.first) == refCountMap.end()) {
                LOG_ERROR(item.first << " does not exsit in refCountMap.");
                return BSS_ERR;
            }
            self->mPrimaryAddressManager->AddPrimaryAddressRef(primaryAddress, refCountMap[item.first]);
        }

        return BSS_OK;
    };

    for (auto &item : fileHolders) {
        CONTINUE_LOOP_AS_NULLPTR(item);
        auto ret = item->MigrateFileMetas(toAddMapping, addedMapping, function);
        if (!ret) {
            LOG_ERROR("Fail to migrate fileMeta after downloading restored file to local");
            success = false;
            continue;
        }
        successMigration++;
    }
    for (auto &item : toAddMapping) {
        uint32_t fileId = FileAddressUtil::GetFileId(item.second.fileAddress);
        if (addedMapping.find(item.first) == addedMapping.end()) {
            mLocalFileManager->DecDbRef(fileId, 0, TimeStampUtil::GetCurrentTime(), item.second.fileLength);
        }
    }
    std::ostringstream printfOss;
    printfOss << "Migrate success :" << successMigration << " file stores after downloading restored file to local:[";
    for (auto &item : addedMapping) {
        printfOss << item.first << ",";
    }
    printfOss << "] use time:" << (TimeStampUtil::GetCurrentTime() - startTime) << "ms.";
    LOG_DEBUG(printfOss.str());
    return success ? BSS_OK : BSS_ERR;
}
}  // namespace bss
}  // namespace ock