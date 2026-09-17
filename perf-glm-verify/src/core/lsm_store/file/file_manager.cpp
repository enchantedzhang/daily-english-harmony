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

#include "file_manager.h"

#include "common/path_transform.h"

namespace ock {
namespace bss {
std::atomic<uint64_t> FileManager::mPrefix = { 0 };

FileInfoRef FileManager::AllocateFile(const FileDirectoryRef &fileDirectory,
                                      const std::function<std::string(std::string)> &fileNameGenerator)
{
    FileIdRef fileId = mFileIdGenerator->Generate();
    if (UNLIKELY(fileId == nullptr)) {
        return nullptr;
    }
    std::ostringstream suffixOss;
    suffixOss << mBackendUid << "_" << mFileSuffix.fetch_add(1);
    std::string prefixName = suffixOss.str();
    Uri uri(fileNameGenerator(prefixName));
    PathRef path = std::make_shared<Path>(fileDirectory->GetDirectoryPath(), std::make_shared<Path>(uri));
    return AllocateFile(fileId, path, true);
}

FileInfoRef FileManager::AllocatePrefixFile(const std::string &prefix, const FileDirectoryRef &fileDirectory,
                                            const std::function<std::string(std::string)> &fileNameGenerator)
{
    FileIdRef fileId = mFileIdGenerator->Generate();
    if (UNLIKELY(fileId == nullptr)) {
        return nullptr;
    }
    std::ostringstream suffixOss;
    suffixOss << prefix << mBackendUid << "_" << mFileSuffix.fetch_add(1);
    std::string prefixName = suffixOss.str();
    Uri uri(fileNameGenerator(prefixName));
    PathRef path = std::make_shared<Path>(fileDirectory->GetDirectoryPath(), std::make_shared<Path>(uri));
    return AllocateFile(fileId, path, true);
}

FileInfoRef FileManager::AllocateFile(const FileIdRef &fileId, const PathRef &path, bool canDelete)
{
    if (UNLIKELY(fileId == nullptr)) {
        LOG_ERROR("fileId is nullptr.");
        return nullptr;
    }
    // 1. 查找文件是否存在.
    WriteLocker<ReadWriteLock> lk(&mRwLock);
    auto item = mFileMapping.find(fileId->Get());
    if (UNLIKELY(item != mFileMapping.end())) {
        LOG_ERROR("Allocate file is existed, fileId:" << fileId->Get() << ".");
        return nullptr;
    }

    // 2. 创建新的文件.
    FileMetaRef fileMeta = std::make_shared<FileMeta>(path, fileId, canDelete);
    fileMeta->AddDbRef(1);
    LOG_DEBUG("Add new fileId to file mapping:" << fileId->Get() << ", path:" << path->ExtractFileName());
    mFileMapping.emplace(fileId->Get(), fileMeta);  // 将创建出来的文件放到fileMapping中进行管理.
    return std::make_shared<FileInfo>(path, fileId);
}

void FileManager::IncDbRef(uint32_t fileId, uint32_t dataSize)
{
    ReadLocker<ReadWriteLock> lk(&mRwLock);
    auto it = mFileMapping.find(fileId);
    if (UNLIKELY(it == mFileMapping.end())) {
        LOG_ERROR("Increase reference to not exist file, fileId:" << fileId << ", snapshot:" << mSnapshotStorage);
        return;
    }

    FileMetaRef fileMeta = it->second;
    if (UNLIKELY(fileMeta == nullptr)) {
        LOG_ERROR("Increase reference to an unknown file, fileId:" << fileId);
        return;
    }
    int32_t ref = fileMeta->AddDbRef(NO_1);
    if (ref < 0) {  // 因为我们用的fetch_add，所以返回的是加之前的计数
        LOG_ERROR("Reference should be positive or 0 before increased.");
        return;
    }
    if (mRestoreState != RestoreState::RESTORING) {
        fileMeta->AddFileSize(dataSize);
    }
    fileMeta->AddDataSize(dataSize);
}

void FileManager::DecDbRef(uint32_t fileId, int64_t epoch, int64_t timestamp, uint32_t dataSize)
{
    ReadLocker<ReadWriteLock> lk(&mRwLock);
    auto fileMetaIt = mFileMapping.find(fileId);
    if (UNLIKELY(fileMetaIt == mFileMapping.end())) {
        LOG_ERROR("Decrease reference to an unknown file, fileId:" << fileId);
        return;
    }
    FileMetaRef fileMeta = fileMetaIt->second;
    if (UNLIKELY(fileMeta == nullptr)) {
        LOG_ERROR("Increase reference to an unknown file, fileId:" << fileId);
        return;
    }
    fileMeta->SetLastEpochToDecDbRef(epoch);
    fileMeta->SetLastTimestampToDecDbRef(timestamp);
    int32_t ref = fileMeta->AddDbRef(-1);
    if (UNLIKELY(ref <= 0)) {
        LOG_ERROR("Reference should be non-negative after decreased, fileId:" << fileId);
        return;
    }
    fileMeta->SubDataSize(dataSize);
    if (IsRestoring()) {
        return;
    }
    // 因为我们用的fetch_add，所以返回的是减之前的计数
    if (ref > 1) {
        return;
    }
    int64_t minEpoch = GetMinSnapshotEpoch();
    if (fileMeta->GetLastEpochToDecDbRef() >= minEpoch) {
        mWaitingDbDeletionFiles.emplace(fileMeta);
        return;
    }
    if (fileMeta->AddSnapshotRef(0) > 0) {
        mWaitingSnapshotDeletionFiles.emplace(fileId, fileMeta);
        return;
    }
    RemoveFile(fileMeta);
    std::vector<FileMetaRef> deleteFiles;
    deleteFiles.emplace_back(fileMeta);
    DeleteFiles(deleteFiles);
}

void FileManager::RemoveFile(const FileMetaRef &fileMeta)
{
    LOG_DEBUG("Remove form file manager, file:" << fileMeta->ToString());
    auto it = mFileMapping.find(fileMeta->GetFileId()->Get());
    if (it != mFileMapping.end()) {
        LOG_DEBUG("remove fileId from file mapping, fileId:" << fileMeta->GetFileId()->Get());
        mFileMapping.erase(it);
    }
    mFileIdGenerator->Recycle(fileMeta->GetFileId());
}

void FileManager::DeleteFiles(const std::vector<FileMetaRef> &deleteFiles)
{
    for (auto &fileMeta : deleteFiles) {
        if (!fileMeta->IsCanDelete()) {
            continue;
        }
        PathRef path = fileMeta->GetFilePath();
        auto ret = unlink(path->Name().c_str());
        if (UNLIKELY(ret != 0)) {
            LOG_ERROR("Delete file failed, fileId:" << fileMeta->GetFileId()->Get()
                                                    << ", filePath:" << path->ExtractFileName());
        } else {
            LOG_DEBUG("Delete file success, fileId:" << fileMeta->GetFileId()->Get()
                                                     << ", filePath:" << path->ExtractFileName());
        }
    }
}

BResult FileManager::StartRestore(const std::vector<SnapshotFileMappingRef> &restoredFileMappings,
                                  bool isExcludeSSTFiles)
{
    WriteLocker<ReadWriteLock> lk(&mRwLock);
    RETURN_NOT_OK(CheckCloseStatus());
    if (UNLIKELY(mRestoreState != RestoreState::NONE)) {
        LOG_ERROR("Unexpected restore state:" << static_cast<uint32_t>(mRestoreState));
        return BSS_ERR;
    }

    if (!mFileMapping.empty()) {
        LOG_ERROR("File mapping should be empty before restore.");
        return BSS_ERR;
    }

    RETURN_NOT_OK(RestoreFileMapping(restoredFileMappings, isExcludeSSTFiles));
    LOG_DEBUG("Restore file manager success, working path:" << mWorkingBasePath->ExtractFileName());
    return BSS_OK;
}

BResult FileManager::RestoreFileMapping(const std::vector<SnapshotFileMappingRef> &restoredFileMappings,
                                        bool isExcludeSSTFiles)
{
    mRestoreState = RestoreState::RESTORING;
    for (const SnapshotFileMappingRef &mapping : restoredFileMappings) {
        PathRef restoredBasePath = mapping->GetBasePath();
        PathRef currentBlobPath = std::make_shared<Path>(restoredBasePath->Name() + "/blobFile");
        bool canDeleted = mWorkingBasePath->Name() == restoredBasePath->Name();
        for (SnapshotFileInfoRef &fileInfo : mapping->GetFileMapping()) {
            uint32_t fileId = fileInfo->GetFileId();
            if (UNLIKELY(fileId == 0)) {  // fileId是0说明不是sst文件.
                continue;
            }

            std::string fileName = fileInfo->GetFileName();
            PathRef targetPath;
            if (isExcludeSSTFiles && fileName.find(BLOB_FILE_NAME_PREFIX) == std::string::npos) {
                continue;
            }
            if (fileName.find(BLOB_FILE_NAME_PREFIX) != std::string::npos) {
                targetPath = currentBlobPath;
            } else {
                targetPath = restoredBasePath;
            }
            PathRef filePath = std::make_shared<Path>(targetPath, fileName);
            FileMetaRef fileMeta = nullptr;
            auto it = mFileMapping.find(fileId);
            if (it == mFileMapping.end()) {  // 新建一个fileMeta插入到fileMapping中.
                FileIdRef fileIdObj = std::make_shared<FileId>();
                RETURN_NOT_OK(fileIdObj->Init(fileId));
                fileMeta = std::make_shared<FileMeta>(filePath, fileIdObj, canDeleted);
                mFileMapping.emplace(fileId, fileMeta);
                LOG_DEBUG("add fileId to file mapping, fileId:" << fileId);
            } else {
                //  检查是否是同一个文件, 若文件名相同则打印日志.
                fileMeta = it->second;
                if (filePath->Name() != fileMeta->GetFilePath()->Name()) {
                    LOG_ERROR("Multiple files are found under a fileId:" << fileId << ", "
                                                                         << fileMeta->GetFilePath()->ExtractFileName()
                                                                         << ", " << filePath->ExtractFileName());
                    return BSS_ERR;
                }
            }
            // 更新文件的大小.
            if (fileMeta->GetFileSize() < fileInfo->GetFileSize()) {
                fileMeta->AddFileSize(fileInfo->GetFileSize() - fileMeta->GetFileSize());
            }
            if (mSnapshotStorage) {  // 如果是本地fileManager则为false, 如果是远端fileManager的则为true.
                fileMeta->AddSnapshotRef(1);
            }
        }
    }
    return BSS_OK;
}

BResult FileManager::EndRestore()
{
    WriteLocker<ReadWriteLock> lk(&mRwLock);
    if (mRestoreState != RestoreState::RESTORING) {
        LOG_ERROR("Unexpected restore state: " << static_cast<uint32_t>(mRestoreState));
        return BSS_ERR;
    }
    mRestoreState = RestoreState::RESTORED;

    for (auto &entry : mFileMapping) {
        auto fileMeta = entry.second;
        CONTINUE_LOOP_AS_NULLPTR(fileMeta);
        if (fileMeta->AddDbRef(0) > 0) {
            continue;
        }
        if (fileMeta->AddSnapshotRef(0) > 0) {
            mWaitingSnapshotDeletionFiles.emplace(fileMeta->GetFileId()->Get(), fileMeta);
            continue;
        }
        mFileIdGenerator->Recycle(fileMeta->GetFileId());
        auto ret = unlink(fileMeta->GetFilePath()->Name().c_str());
        if (UNLIKELY(ret != 0)) {
            LOG_ERROR("Unlink failed, filePath:" << fileMeta->GetFilePath()->ExtractFileName());
        } else {
            LOG_INFO("Unlink success, filePath:" << fileMeta->GetFilePath()->ExtractFileName());
        }
    }
    LOG_DEBUG("File manager restore success.");
    return BSS_OK;
}

FileInfoRef FileManager::AllocateFile()
{
    FileIdRef fileId = mFileIdGenerator->Generate();
    uint64_t prefix = mPrefix.fetch_add(1);
    std::ostringstream suffixOss;
    suffixOss << prefix << "-" << mBackendUid << "-" << mFileSuffix.fetch_add(1);
    PathRef filePath = std::make_shared<Path>(mWorkingBasePath, suffixOss.str());
    return AllocateFile(fileId, filePath, true);
}

FileInfoRef FileManager::AllocateFile(const std::function<std::string()> &filePathGenerator)
{
    FileIdRef fileId = mFileIdGenerator->Generate();
    PathRef filePath = std::make_shared<Path>(filePathGenerator());
    return AllocateFile(fileId, filePath, true);
}

}  // namespace bss
}  // namespace ock