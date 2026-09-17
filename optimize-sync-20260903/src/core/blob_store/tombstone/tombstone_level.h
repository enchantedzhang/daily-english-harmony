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

#ifndef BOOST_SS_TOMBSTONE_LEVEL_H
#define BOOST_SS_TOMBSTONE_LEVEL_H

#include <vector>

#include "include/config.h"
#include "tombstone.h"
#include "tombstone_compaction_processor.h"
#include "tombstone_file_group.h"
#include "tombstone_file_manager.h"

namespace ock {
namespace bss {
struct TombstoneFileGroupComparator {
    bool operator()(const TombstoneFileGroupRef t1, const TombstoneFileGroupRef t2)
    {
        if (UNLIKELY(t1 == nullptr || t2 == nullptr)) {
            LOG_ERROR("Tombstone file group is null");
            return false;
        }
        return t1->GetFileSize() < t2->GetFileSize();
    }
};

class TombstoneLevel {
public:
    TombstoneLevel(const ConfigRef &config, uint32_t levelId, const TombstoneFileManagerRef &tombstoneFileManager,
                   bool isTopLevel, const MemManagerRef &memManager)
        : mConfig(config),
          mLevel(levelId),
          mFileManager(tombstoneFileManager),
          mIsTopLevel(isTopLevel),
          mMemManager(memManager)
    {
    }

    void InsertFileGroup(TombstoneFileGroupRef newGroup)
    {
        WriteLocker<ReadWriteLock> lock(&mRwLock);
        mFileGroup.emplace_back(newGroup);
    }

    void InsertFileGroups(std::vector<TombstoneFileGroupRef> newGroups)
    {
        WriteLocker<ReadWriteLock> lock(&mRwLock);
        mFileGroup.insert(mFileGroup.end(), newGroups.begin(), newGroups.end());
    }

    void TriggerCompaction(const BlobFileGroupManagerRef &blobFileGroupManager)
    {
        if (mIsTopLevel) {
            return;
        }
        if (LevelFileSize() < mConfig->GetTombstoneLevelMaxSize(mLevel)) {
            if (mFileGroup.size() >= mConfig->GetTombstoneCompactionGroupSize()) {
                auto newGroup = MergeWithinLevel(false, false);
                InsertFileGroup(newGroup);
            }
        } else if (!mNextLevel->IsTopLevel() || mNextLevel->GetFileGroup().empty()) {  // 不是top level一定有next level
            auto newGroup = MergeWithinLevel(true, mNextLevel->IsTopLevel(), blobFileGroupManager);
            mNextLevel->InsertFileGroup(newGroup);
        } else {
            CompactionOutSideLevel(blobFileGroupManager);
        }
    }

    uint64_t LevelFileSize()
    {
        uint64_t totalSize = 0;
        for (const auto &item : mFileGroup) {
            totalSize += item->GetFileSize();
        }
        return totalSize;
    }

    inline std::vector<TombstoneFileGroupRef> &GetFileGroup()
    {
        return mFileGroup;
    }

    inline std::vector<TombstoneFileRef> GetAndClearFistGroupFiles()
    {
        WriteLocker<ReadWriteLock> lock(&mRwLock);
        if (!mFileGroup.empty()) {
            auto firstGroup = mFileGroup.at(0);
            if (firstGroup == nullptr) {
                LOG_ERROR("tombstone file manager compaction all file to top level, but top group is null");
                return {};
            }
            mFileGroup.erase(mFileGroup.begin());
            auto files = firstGroup->GetFiles();
            return files;
        }
        return {};
    }

    void CleanExpireTombstoneFile(uint64_t minBlobId, std::vector<TombstoneFileRef> &pendingDeleteFiles)
    {
        WriteLocker<ReadWriteLock> lock(&mRwLock);
        auto group = mFileGroup.begin();
        while (group != mFileGroup.end()) {
            TombstoneFileGroupRef tombstoneFileGroup = *group;
            CONTINUE_LOOP_AS_NULLPTR(tombstoneFileGroup);
            tombstoneFileGroup->CleanExpireTombstoneFile(minBlobId, pendingDeleteFiles);
            if (tombstoneFileGroup->Empty()) {
                group = mFileGroup.erase(group);
                continue;
            }
            ++group;
        }
    }

    TombstoneFileGroupRef MergeWithinLevel(bool force, bool nextLevelTop,
                                           BlobFileGroupManagerRef blobFileGroupManager = nullptr)
    {
        std::vector<TombstoneFileGroupRef> selectedFileGroup = SelectCompactionGroup(force);
        if (selectedFileGroup.empty()) {
            return nullptr;
        }
        std::vector<TombstoneFileRef> compactionFiles;
        for (const auto &item : selectedFileGroup) {
            CONTINUE_LOOP_AS_NULLPTR(item);
            auto files = item->GetFiles();
            compactionFiles.insert(compactionFiles.end(), files.begin(), files.end());
        }
        std::vector<TombstoneFileRef> nextLevelFiles;
        TombstoneCompactionProcessorRef compactionProcessor =
            std::make_shared<TombstoneCompactionProcessor>(compactionFiles, nextLevelFiles, mConfig, mFileManager,
                                                           nextLevelTop, mMemManager, blobFileGroupManager);
        std::vector<TombstoneFileRef> newFiles;
        auto ret = compactionProcessor->Process(newFiles);
        if (UNLIKELY(ret != BSS_OK)) {
            LOG_ERROR("compaction process fail, ret: " << ret);
            return nullptr;
        }
        TombstoneFileGroupRef newGroup = std::make_shared<TombstoneFileGroup>(newFiles);
        WriteLocker<ReadWriteLock> lock(&mRwLock);
        mFileGroup.erase(std::remove_if(mFileGroup.begin(), mFileGroup.end(),
                                        [&selectedFileGroup](const TombstoneFileGroupRef &group) {
                                            return std::find(selectedFileGroup.begin(), selectedFileGroup.end(),
                                                             group) != selectedFileGroup.end();
                                        }),
                         mFileGroup.end());
        return newGroup;
    }

    void CompactionOutSideLevel(const BlobFileGroupManagerRef &blobFileGroupManager)
    {
        if (mNextLevel == nullptr || !mNextLevel->IsTopLevel()) {
            return;
        }
        auto largestSizeFile = FindLargestFile();
        if (UNLIKELY(largestSizeFile == nullptr)) {
            return;
        }
        TombstoneFileMetaRef largestFileMeta = largestSizeFile->GetFileMeta();
        RETURN_AS_NULLPTR(largestFileMeta);

        std::vector<TombstoneFileRef> selectedFiles;
        uint64_t minBlobId = UINT64_MAX;
        uint64_t maxBlobId = 0;
        SelectRangeFile(selectedFiles, minBlobId, maxBlobId, largestFileMeta);
        auto topLevelGroup = mNextLevel->GetFileGroup();
        auto firstGroup = !topLevelGroup.empty() ? topLevelGroup.at(0) : nullptr;
        RETURN_AS_NULLPTR(firstGroup);
        auto fileSubVec = firstGroup->FindOverLapFile(minBlobId, maxBlobId);
        RETURN_AS_NULLPTR(fileSubVec);
        TombstoneCompactionProcessorRef compactionProcessor =
            std::make_shared<TombstoneCompactionProcessor>(selectedFiles, fileSubVec->GetFileVec(), mConfig,
                                                           mFileManager, true, mMemManager, blobFileGroupManager);
        std::vector<TombstoneFileRef> newFiles;
        auto ret = compactionProcessor->Process(newFiles);
        if (UNLIKELY(ret != BSS_OK)) {
            LOG_ERROR("compaction process fail, ret: " << ret);
            return;
        }
        topLevelGroup[0]->MigrateFile(fileSubVec, newFiles);
    }

    TombstoneFileRef FindLargestFile()
    {
        TombstoneFileRef largestSizeFile = nullptr;
        uint64_t largestSize = 0;
        ReadLocker<ReadWriteLock> lock(&mRwLock);
        for (const auto &item : mFileGroup) {
            for (auto &file : item->GetFiles()) {
                if (file->GetFileSize() > largestSize) {
                    largestSize = file->GetFileSize();
                    largestSizeFile = file;
                }
            }
        }
        return largestSizeFile;
    }

    void SelectRangeFile(std::vector<TombstoneFileRef> &selectedVec, uint64_t &minBlobId, uint64_t &maxBlobId,
                         TombstoneFileMetaRef &largestFileMeta)
    {
        WriteLocker<ReadWriteLock> lock(&mRwLock);
        auto group = mFileGroup.begin();
        while (group != mFileGroup.end()) {
            (*group)->SelectRangeFile(selectedVec, minBlobId, maxBlobId, largestFileMeta);
            auto fileVec = (*group)->GetFiles();
            if (fileVec.empty()) {
                group = mFileGroup.erase(group);
            }
        }
    }

    std::vector<TombstoneFileGroupRef> SelectCompactionGroup(bool force)
    {
        uint32_t compactionSize = mConfig->GetTombstoneCompactionGroupSize();
        std::vector<TombstoneFileGroupRef> ret;
        std::vector<TombstoneFileGroupRef> sortedFileGroup;
        {
            ReadLocker<ReadWriteLock> lock(&mRwLock);
            if (mFileGroup.size() < compactionSize && !force) {
                return {};
            }
            sortedFileGroup.insert(sortedFileGroup.begin(), mFileGroup.begin(), mFileGroup.end());
        }
        std::sort(sortedFileGroup.begin(), sortedFileGroup.end(), TombstoneFileGroupComparator());
        uint32_t index = 0;
        auto selectSize = std::min(compactionSize, static_cast<uint32_t>(sortedFileGroup.size()));
        while (index < selectSize) {
            ret.push_back(sortedFileGroup.at(index));
            index++;
        }
        return ret;
    }

    inline bool IsTopLevel() const
    {
        return mIsTopLevel;
    }

    inline void SetTopLevel(bool isTopLevel)
    {
        mIsTopLevel = isTopLevel;
    }

    inline bool IsFull()
    {
        return LevelFileSize() >= mConfig->GetTombstoneLevelMaxSize(mLevel);
    }

    inline uint32_t GetLevelId() const
    {
        return mLevel;
    }

    inline void SetNextLevel(std::shared_ptr<TombstoneLevel> nextLevel)
    {
        mNextLevel = nextLevel;
    }

    inline void Upgrade()
    {
        mLevel++;
    }

    std::shared_ptr<TombstoneLevel> Snapshot()
    {
        std::shared_ptr<TombstoneLevel> newLevel = std::make_shared<TombstoneLevel>(mConfig, mLevel, mFileManager,
                                                                                    mIsTopLevel, mMemManager);
        for (const auto &item : mFileGroup) {
            auto newGroup = item->Snapshot();
            newLevel->InsertFileGroup(newGroup);
        }
        return newLevel;
    }

    std::vector<TombstoneFileMetaRef> GetFileMetasForSnapshot()
    {
        std::vector<TombstoneFileMetaRef> metas;
        ReadLocker<ReadWriteLock> lock(&mRwLock);
        for (const auto &group : mFileGroup) {
            std::vector<TombstoneFileMetaRef> metasInGroup = group->GetTombstoneFileMetas();
            if (metasInGroup.empty()) {
                continue;
            }
            metas.insert(metas.end(), metasInGroup.begin(), metasInGroup.end());
        }
        return metas;
    }

    std::vector<TombstoneFileRef> GetFiles()
    {
        std::vector<TombstoneFileRef> files;
        ReadLocker<ReadWriteLock> lock(&mRwLock);
        for (const auto &group : mFileGroup) {
            CONTINUE_LOOP_AS_NULLPTR(group);
            for (const auto &file : group->GetFiles()) {
                files.emplace_back(file);
            }
        }
        return files;
    }

    std::vector<TombstoneFileRef> GetAndClearFiles()
    {
        std::vector<TombstoneFileRef> files;
        WriteLocker<ReadWriteLock> lock(&mRwLock);
        for (const auto &group : mFileGroup) {
            CONTINUE_LOOP_AS_NULLPTR(group);
            group->CopyAndClearFiles(files);
        }
        mFileGroup.clear();
        return files;
    }

    uint64_t GetTotalFileSize()
    {
        uint64_t totalSize = 0;
        ReadLocker<ReadWriteLock> lock(&mRwLock);
        for (const auto &group : mFileGroup) {
            CONTINUE_LOOP_AS_NULLPTR(group);
            totalSize += group->GetFileSize();
        }
        return totalSize;
    }

    inline uint64_t CalTombstoneNum(uint64_t minBlobId)
    {
        uint64_t count = 0;
        ReadLocker<ReadWriteLock> lock(&mRwLock);
        for (const auto &group : mFileGroup) {
            CONTINUE_LOOP_AS_NULLPTR(group);
            count += group->CalTombstoneNum(minBlobId);
        }
        return count;
    }

private:
    ConfigRef mConfig;
    uint32_t mLevel;
    std::vector<TombstoneFileGroupRef> mFileGroup;
    ReadWriteLock mRwLock;
    std::shared_ptr<TombstoneLevel> mNextLevel;
    TombstoneFileManagerRef mFileManager;
    bool mIsTopLevel;
    MemManagerRef mMemManager;
};
using TombstoneLevelRef = std::shared_ptr<TombstoneLevel>;
}  // namespace bss
}  // namespace ock

#endif  // BOOST_SS_TOMBSTONE_LEVEL_H
