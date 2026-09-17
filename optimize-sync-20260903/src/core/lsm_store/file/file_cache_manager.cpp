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

#include "file_cache_manager.h"

#include <unordered_map>

#include "common/util/file_address_utils.h"
#include "file_cache_type.h"
#include "lazy/lazy_download_strategy.h"
#include "lazy/snapshot_downloader.h"

namespace ock {
namespace bss {

BResult FileCacheManager::RestoreFiles(std::unordered_map<std::string, std::pair<uint64_t, uint32_t>> &result,
                                       std::unordered_map<std::string, RestoreFileInfo> &restoredFileMapping,
                                       FileDirectoryRef &localFileAllocateDir, bool rescaled)
{
    std::unordered_map<uint64_t, std::string> downloadedRemoteAddress;  // 已下载远程文件地址映射
    for (const auto &entry : restoredFileMapping) {
        RestoreFileInfo restoreFileInfo = entry.second;
        std::string fileIdentifier = restoreFileInfo.fileIdentifier;
        if (restoreFileInfo.fileStatus == FileStatus::LOCAL) {
            RETURN_NOT_OK(RestoreFromLocal(fileIdentifier, restoreFileInfo));
        } else {
            RETURN_NOT_OK(RestoreFromDfs(fileIdentifier, restoreFileInfo, downloadedRemoteAddress, localFileAllocateDir,
                                         rescaled));
        }
    }

    result.reserve(mRestoredFileIdentifierMapping.size());
    for (const auto &entry : mRestoredFileIdentifierMapping) {
        result.emplace(entry.first, std::make_pair(entry.second->GetFileAddress(), entry.second->GetFileLength()));
    }
    return BSS_OK;
}

BResult FileCacheManager::RestoreFromLocal(const std::string &fileName, const RestoreFileInfo &restoreInfo)
{
    auto fileId = FileAddressUtils::GetFileId(restoreInfo.fileAddress);
    LOG_INFO("Restore from local:" << PathTransform::ExtractFileName(fileName) << ", fileId:" << fileId
                                   << ", fileStatus:" << static_cast<uint32_t>(restoreInfo.fileStatus));
    auto it = mRestoredFileIdentifierMapping.find(fileName);
    if (it == mRestoredFileIdentifierMapping.end()) {
        mLocalFileManager->IncDbRef(fileId, restoreInfo.fileLength);  // 增加引用计数.
        auto primaryAddress = std::make_shared<PrimaryAddress>(restoreInfo.fileAddress, restoreInfo.fileLength,
                                                               fileName, FileStatus::LOCAL);
        mRestoredFileIdentifierMapping.emplace(fileName, primaryAddress);
    }
    // 增加本地地址引用计数.
    mPrimaryFileManager->AddPrimaryAddressRef(mRestoredFileIdentifierMapping[fileName], restoreInfo.refCount);
    return BSS_OK;
}

BResult FileCacheManager::RestoreFromDfs(const std::string &fileName, const RestoreFileInfo &restoreInfo,
                                         std::unordered_map<uint64_t, std::string> &downloadedRemoteAddress,
                                         const FileDirectoryRef &localFileAllocateDir, bool rescaled)
{
    auto fileId = static_cast<uint32_t>(FileAddressUtils::GetFileId(restoreInfo.fileAddress));
    LOG_INFO("Restore from remote:" << PathTransform::ExtractFileName(fileName) << ", fileId:" << fileId
                                    << ", fileStatus:" << static_cast<uint32_t>(restoreInfo.fileStatus));
    uint64_t remoteAddress = restoreInfo.remoteFileAddress;
    uint64_t remoteFileId = FileAddressUtils::GetFileId(remoteAddress);
    uint64_t fileLength = restoreInfo.fileLength;
    uint32_t refCount = restoreInfo.refCount;
    auto it = mRestoredFileIdentifierMapping.find(fileName);
    if (it != mRestoredFileIdentifierMapping.end()) {
        mPrimaryFileManager->AddPrimaryAddressRef(it->second, refCount);
        mLazyDownloadStrategy->AddAddressRef(it->second->GetFileAddress(), refCount);
        return BSS_OK;
    }
    if (downloadedRemoteAddress.count(remoteAddress)) {
        LOG_ERROR("The remote file has been restore:" << remoteAddress);
        return BSS_ERR;
    }
    downloadedRemoteAddress.emplace(remoteAddress, fileName);

    if (!rescaled) {  // 非扩容场景.
        return ResolveUnRescaledFile(restoreInfo, localFileAllocateDir, fileName);
    }

    // 扩容场景.
    FileInfoRef previousRemoteFileInfo = mDfsFileManager->GetFileInfo(remoteFileId);
    if (previousRemoteFileInfo != nullptr) {
        return ResolveRescaledFile(previousRemoteFileInfo, restoreInfo, localFileAllocateDir, fileName);
    }

    FileInfoRef fileInfo = mDfsFileManager->AllocateFile();
    RETURN_ALLOC_FAIL_AS_NULLPTR(fileInfo);
    auto ret = SnapshotDownloader::DownloadFile(restoreInfo.remoteFileName, restoreInfo.restoreLocalFileName);
    if (ret != BSS_OK) {
        LOG_ERROR("Download from remote failed, remote path:"
                  << PathTransform::ExtractFileName(restoreInfo.remoteFileName)
                  << ", local path:" << PathTransform::ExtractFileName(restoreInfo.restoreLocalFileName));
        return ret;
    }
    PrimaryAddressRef primaryAddress = std::make_shared<PrimaryAddress>(remoteAddress, fileLength, fileName, DFS);
    RETURN_ALLOC_FAIL_AS_NULLPTR(primaryAddress);
    mRestoredFileIdentifierMapping.emplace(fileName, primaryAddress);
    auto persistentAddress = std::make_shared<PersistentAddress>(remoteAddress, fileLength, fileName, false);
    RETURN_ALLOC_FAIL_AS_NULLPTR(persistentAddress);
    mPrimaryFileManager->AddPrimaryAddressWithPersistentAddress(primaryAddress, persistentAddress, refCount);
    mDfsFileManager->IncDbRef(remoteFileId, fileLength);
    return BSS_OK;
}

BResult FileCacheManager::ResolveRescaledFile(const FileInfoRef &previousRemoteFileInfo, RestoreFileInfo restoreInfo,
                                              const FileDirectoryRef &localFileAllocateDir, const std::string &fileName)
{
    RETURN_INVALID_PARAM_AS_NULLPTR(localFileAllocateDir);
    uint64_t remoteAddress = restoreInfo.remoteFileAddress;
    uint64_t fileLength = restoreInfo.fileLength;
    uint32_t refCount = restoreInfo.refCount;
    auto rescaledFileId = mRescaledFilePathMapping.find(previousRemoteFileInfo->GetFilePath());
    // 如果已经进行了缩放
    if (rescaledFileId != mRescaledFilePathMapping.end()) {
        // 如果文件已经被重新缩放，记录下载的文件ID和远程文件地址。
        remoteAddress = FileAddressUtils::GetFileAddressWithNewFileId(rescaledFileId->second, remoteAddress);
        if (mRescaledDownloadAddress.count(remoteAddress)) {
            mDfsFileManager->IncDbRef(rescaledFileId->second, restoreInfo.fileLength);
        }

        auto primaryAddress = mRestoredFileIdentifierMapping.find(fileName);
        if (UNLIKELY(primaryAddress != mRestoredFileIdentifierMapping.end())) {
            mPrimaryFileManager->AddPrimaryAddressRef(primaryAddress->second, refCount);
        }
        mRescaledDownloadAddress.emplace(remoteAddress);
        return BSS_OK;
    } else {
        FileInfoRef allocateFile = mDfsFileManager->AllocateFromReadOnlyFile(previousRemoteFileInfo->GetFilePath());
        RETURN_ALLOC_FAIL_AS_NULLPTR(allocateFile);
        if (mRescaledDownloadAddress.count(remoteAddress)) {
            LOG_ERROR("Rescaled download address already has remote address:" << remoteAddress);
            return BSS_ERR;
        }
        mRescaledFilePathMapping.emplace(previousRemoteFileInfo->GetFilePath(), allocateFile->GetFileId()->Get());
    }
    PrimaryAddressRef primaryAddress = std::make_shared<PrimaryAddress>(remoteAddress, fileLength, fileName,
                                                                        FileStatus::DFS);
    mRestoredFileIdentifierMapping.emplace(fileName, primaryAddress);
    auto persistentAddress = std::make_shared<PersistentAddress>(remoteAddress, fileLength, fileName, false);
    mPrimaryFileManager->AddPrimaryAddressWithPersistentAddress(primaryAddress, persistentAddress, refCount);
    mDfsFileManager->IncDbRef(FileAddressUtils::GetFileId(remoteAddress), fileLength);
    PathRef path = std::make_shared<Path>(localFileAllocateDir->GetDirectoryPath(),
                                          PathTransform::ExtractFileName(fileName));
    mLazyDownloadStrategy->ToWaitingList(restoreInfo, path);
    return BSS_OK;
}

BResult FileCacheManager::ResolveUnRescaledFile(const RestoreFileInfo &restoreInfo,
                                                const FileDirectoryRef &localFileAllocateDir,
                                                const std::string &fileName)
{
    RETURN_INVALID_PARAM_AS_NULLPTR(localFileAllocateDir);
    uint64_t remoteAddress = restoreInfo.remoteFileAddress;
    uint64_t fileLength = restoreInfo.fileLength;
    uint32_t refCount = restoreInfo.refCount;
    auto primaryAddress = std::make_shared<PrimaryAddress>(remoteAddress, fileLength, fileName, FileStatus::DFS);
    mRestoredFileIdentifierMapping.emplace(fileName, primaryAddress);
    auto persistentAddress = std::make_shared<PersistentAddress>(remoteAddress, fileLength, fileName, false);
    mPrimaryFileManager->AddPrimaryAddressWithPersistentAddress(primaryAddress, persistentAddress, refCount);
    mDfsFileManager->IncDbRef(FileAddressUtils::GetFileId(remoteAddress), fileLength);
    PathRef path = std::make_shared<Path>(localFileAllocateDir->GetDirectoryPath(),
                                          PathTransform::ExtractFileName(fileName));
    mLazyDownloadStrategy->ToWaitingList(restoreInfo, path);
    return BSS_OK;
}

}  // namespace bss
}  // namespace ock