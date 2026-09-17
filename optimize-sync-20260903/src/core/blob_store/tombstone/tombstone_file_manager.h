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

#ifndef BOOST_SS_TOMBSTONE_FILE_MANAGER_H
#define BOOST_SS_TOMBSTONE_FILE_MANAGER_H

#include <memory>
#include <unordered_map>

#include "file/file_cache_manager.h"
#include "include/config.h"
#include "tombstone_file.h"
#include "tombstone_file_group.h"

namespace ock {
namespace bss {
class TombstoneService;
class TombstoneLevel;
class TombstoneFileManagerSnapshot;
class BlobFileManager;
class BlobStoreSnapshotOperator;
class BlobStoreSnapshotCoordinator;
class TombstoneFileManager : public std::enable_shared_from_this<TombstoneFileManager> {
public:
    ~TombstoneFileManager()
    {
        LOG_INFO("Delete TombstoneFileManager success.");
    }
    TombstoneFileManager(ConfigRef &config, FileCacheManagerRef &fileCacheManager, GroupRangeRef &groupRange,
                         uint64_t version, MemManagerRef &memManager, ExecutorServicePtr &executorService,
                         std::shared_ptr<BlobFileManager> &blobFileManager)
        : mConfig(config),
          mFileCacheManager(fileCacheManager),
          mGroupRange(groupRange),
          mInitVersion(version),
          mMemManager(memManager),
          mTombstoneFlushExecutor(executorService),
          mBlobFileManager(blobFileManager)
    {
        mBasePath = mFileCacheManager->CreateFileSubDirectory("blobFile");
    }

    void Close()
    {
        std::unordered_map<uint64_t, std::shared_ptr<BlobStoreSnapshotCoordinator>>().swap(mSnapshotMap);
        std::set<TombstoneFileRef>().swap(mFiles);
        std::unordered_map<std::string, std::shared_ptr<TombstoneService>>().swap(mLevel0);
        std::vector<std::shared_ptr<TombstoneLevel>>().swap(mHighLevels);
        if (mTombstoneFlushExecutor != nullptr) {
            mTombstoneFlushExecutor->Stop();
            mTombstoneFlushExecutor = nullptr;
        }
    }

    TombstoneFileRef AllocateFile(uint64_t version);

    void InitLevel();

    inline static std::string TombstoneFileName(const std::string &name)
    {
        std::string prefix = BLOB_FILE_NAME_PREFIX;
        return prefix + "tf_" + name + ".sst";
    }

    inline FileCacheManagerRef GetFileCacheManager() const
    {
        return mFileCacheManager;
    }

    BResult TriggerCompaction();

    BResult DoCompaction(uint64_t maxVersion, bool snapshot);

    uint32_t CalLevel0CompactionFileNum(uint64_t maxVersion);

    void DiscardFile(const std::vector<TombstoneFileRef> &files);

    void IncFileRef(const std::vector<TombstoneFileRef> &files);

    void DecFileRef(const std::vector<TombstoneFileRef> &files);

    inline void AddFiles(std::vector<TombstoneFileRef> &files)
    {
        mFiles.insert(files.begin(), files.end());
    }

    std::vector<TombstoneFileRef> FindLevel0CompactionFiles(uint64_t maxVersion);

    void CheckTopLevelIsFull();

    std::shared_ptr<TombstoneFileManagerSnapshot> CreateSnapshot(uint64_t version);

    BResult Restore(const FileInputViewRef &fileInputView,
                    std::unordered_map<std::string, uint32_t> &restorePathFileIdMap, uint64_t restoreVersion,
                    bool rescale);

    BResult RestoreLevel(const FileInputViewRef &inputView,
                         std::unordered_map<std::string, uint32_t> &restorePathFileIdMap, bool rescale);

    void FinishRestore();

    BResult RestoreFileGroup(const FileInputViewRef &inputView,
                             std::unordered_map<std::string, uint32_t> &restorePathFileIdMap,
                             std::vector<TombstoneFileGroupRef> &groupVec,
                             std::unordered_map<std::string, RestoreFileInfo> &restoreFileMapping);

    BResult RestoreFile(const FileInputViewRef &inputView,
                        std::unordered_map<std::string, uint32_t> &restorePathFileIdMap,
                        TombstoneFileGroupRef &fileGroup,
                        std::unordered_map<std::string, RestoreFileInfo> &restoreFileMapping);

    void TriggerSnapshot(uint64_t snapshotId, uint64_t blobStoreVersion, uint64_t seqId,
                         std::shared_ptr<BlobStoreSnapshotOperator> &blobStoreSnapshotOperator);

    bool ShouldCompactionAllFileToTop();

    void CompactionAllFileToTop();

    std::shared_ptr<TombstoneLevel> GetTopLevel() const;

    void CleanExpireTombstoneFile(uint64_t minBlobId);

    std::shared_ptr<TombstoneService> AddLevel0(const std::string &name, const KeyGroupUtilRef &keyGroupUtil);

    void UpdateSnapshotMinVersion();

    void ReleaseSnapshot(uint64_t snapshotId);

    uint64_t GetSnapshotVersion(uint64_t snapshotId);

    uint64_t CalTombstoneNum(uint64_t minBlobId);

private:
    ConfigRef mConfig;
    FileCacheManagerRef mFileCacheManager = nullptr;
    GroupRangeRef mGroupRange = nullptr;
    FileDirectoryRef mBasePath = nullptr;
    std::unordered_map<std::string, std::shared_ptr<TombstoneService>> mLevel0;
    std::vector<std::shared_ptr<TombstoneLevel>> mHighLevels;
    std::set<TombstoneFileRef> mFiles;
    uint64_t mSnapshotMinVersion = UINT64_MAX;
    uint64_t mInitVersion = 0;
    MemManagerRef mMemManager = nullptr;
    ExecutorServicePtr mTombstoneFlushExecutor = nullptr;
    std::shared_ptr<BlobFileManager> mBlobFileManager = nullptr;
    std::unordered_map<uint64_t, std::shared_ptr<BlobStoreSnapshotCoordinator>> mSnapshotMap;
};
using TombstoneFileManagerRef = std::shared_ptr<TombstoneFileManager>;
}  // namespace bss
}  // namespace ock

#endif  // BOOST_SS_TOMBSTONE_FILE_MANAGER_H
