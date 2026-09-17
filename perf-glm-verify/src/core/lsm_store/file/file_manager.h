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

#ifndef BOOST_SS_FILE_MANAGER_H
#define BOOST_SS_FILE_MANAGER_H

#include <atomic>
#include <functional>
#include <map>
#include <mutex>
#include <set>
#include <unordered_map>

#include "common/path.h"
#include "executor/executor_service.h"
#include "file_directory.h"
#include "file_id_generator.h"
#include "file_info.h"
#include "file_meta.h"
#include "snapshot/restore_db_meta.h"
#include "util/bss_lock.h"

namespace ock {
namespace bss {
class FileManager {
public:
    enum RestoreState : uint8_t { NONE = 0, RESTORING = 1, RESTORED = 2 };

    FileManager() = default;

    FileManager(const PathRef &workingBasePath, std::string identifier, const FileIdGeneratorRef &fileIdGenerator,
                const ExecutorServiceRef &executorService, bool snapshotStorage)
        : mWorkingBasePath(workingBasePath),
          mBackendUid(identifier),
          mSnapshotStorage(snapshotStorage),
          mFileIdGenerator(fileIdGenerator),
          mIoExecutor(executorService),
          mRestoreState(RestoreState::NONE)
    {
    }

    ~FileManager()
    {
        std::unordered_map<uint32_t, FileMetaRef>().swap(mFileMapping);
        std::set<FileMetaRef>().swap(mWaitingDbDeletionFiles);
        std::unordered_map<uint32_t, FileMetaRef>().swap(mWaitingSnapshotDeletionFiles);
    }

    FileInfoRef AllocateFile(const FileDirectoryRef &fileDirectory,
                             const std::function<std::string(std::string)> &fileNameGenerator);

    FileInfoRef AllocatePrefixFile(const std::string &prefix, const FileDirectoryRef &fileDirectory,
                                   const std::function<std::string(std::string)> &fileNameGenerator);

    FileInfoRef AllocateFile(const FileIdRef &fileId, const PathRef &path, bool canDelete);

    FileInfoRef AllocateFile();

    FileInfoRef AllocateFile(const std::function<std::string()> &filePathGenerator);

    void IncDbRef(uint32_t fileId, uint32_t dataSize);

    void DecDbRef(uint32_t fileId, int64_t epoch, int64_t timestamp, uint32_t dataSize);

    inline bool IsRestoring() const
    {
        return mRestoreState == RESTORING;
    }

    inline int64_t GetMinSnapshotEpoch() const
    {
        return mEpochOfRunningSnapshots.empty() ? INT64_MAX : mEpochOfRunningSnapshots.begin()->second;
    }

    void RemoveFile(const FileMetaRef &fileMeta);

    void DeleteFiles(const std::vector<FileMetaRef> &deleteFiles);

    inline FileInfoRef GetFileInfo(uint32_t fileId)
    {
        ReadLocker<ReadWriteLock> lk(&mRwLock);
        auto it = mFileMapping.find(fileId);
        if (it == mFileMapping.end()) {
            LOG_WARN("file mapping not contains fileId:" << fileId);
            return nullptr;
        }
        return std::make_shared<FileInfo>(it->second->GetFilePath(), it->second->GetFileId());
    }

    inline PathRef GetBasePath() const
    {
        return mWorkingBasePath;
    }

    BResult StartRestore(const std::vector<SnapshotFileMappingRef> &restoredFileMappings, bool isExcludeSSTFiles);

    BResult EndRestore();

    inline FileInfoRef AllocateFromReadOnlyFile(const PathRef &path)
    {
        FileIdRef fileId = mFileIdGenerator->Generate();
        return AllocateFile(fileId, path, false);
    }

    inline BResult CheckCloseStatus() const
    {
        if (UNLIKELY(mClosed)) {
            LOG_ERROR("File manager has been closed.");
            return BSS_ERR;
        }
        return BSS_OK;
    }

    static std::atomic<uint64_t> mPrefix;

private:
    BResult RestoreFileMapping(const std::vector<SnapshotFileMappingRef> &restoredFileMappings, bool isExcludeSSTFiles);

private:
    PathRef mWorkingBasePath = nullptr;
    std::string mBackendUid;
    bool mSnapshotStorage = false;
    mutable std::mutex mMutex;
    std::atomic<uint64_t> mFileSuffix = { 0 };
    FileIdGeneratorRef mFileIdGenerator = nullptr;
    std::unordered_map<uint32_t, FileMetaRef> mFileMapping;
    std::set<FileMetaRef> mWaitingDbDeletionFiles;
    std::unordered_map<uint32_t, FileMetaRef> mWaitingSnapshotDeletionFiles;
    ExecutorServiceRef mIoExecutor = nullptr;  // 用于异步删除文件, 当前流程未使用.
    std::map<int64_t, int64_t> mEpochOfRunningSnapshots;
    bool mClosed = false;
    ReadWriteLock mRwLock;
    RestoreState mRestoreState = RestoreState::NONE;
};
using FileManagerRef = std::shared_ptr<FileManager>;

}  // namespace bss
}  // namespace ock
#endif  // BOOST_SS_FILE_MANAGER_H