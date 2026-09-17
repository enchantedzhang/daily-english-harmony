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

#ifndef BOOST_SS_TOMBSTONE_FILE_MANAGER_SNAPSHOT_H
#define BOOST_SS_TOMBSTONE_FILE_MANAGER_SNAPSHOT_H

#include "snapshot/snapshot_meta.h"
#include "tombstone_level.h"

namespace ock {
namespace bss {
class TombstoneFileManagerSnapshot {
public:
    explicit TombstoneFileManagerSnapshot(std::vector<TombstoneLevelRef> levels)
    {
        TombstoneLevelRef lastLevel = nullptr;
        for (const auto &item : levels) {
            CONTINUE_LOOP_AS_NULLPTR(item);
            TombstoneLevelRef levelSnapshot = item->Snapshot();
            auto fileMetas = item->GetFileMetasForSnapshot();
            mFileMetas.insert(mFileMetas.end(), fileMetas.begin(), fileMetas.end());
            mHighLevels.emplace_back(levelSnapshot);
            if (lastLevel != nullptr) {
                lastLevel->SetNextLevel(levelSnapshot);
            }
            lastLevel = levelSnapshot;
        }
    }

    std::vector<TombstoneFileMetaRef> GetFileMetas() const
    {
        return mFileMetas;
    }

    BResult WriteMeta(const FileOutputViewRef &localFileOutputView, SnapshotMetaRef &snapshotMeta)
    {
        RETURN_NOT_OK(localFileOutputView->WriteUint32(mHighLevels.size()));
        for (const auto &item : mHighLevels) {
            RETURN_NOT_OK(WriteLevel(item, localFileOutputView, snapshotMeta));
        }
        return BSS_OK;
    }

private:
    BResult WriteLevel(TombstoneLevelRef level, const FileOutputViewRef &localFileOutputView,
                       SnapshotMetaRef &snapshotMeta)
    {
        auto fileGroups = level->GetFileGroup();
        RETURN_NOT_OK(localFileOutputView->WriteUint32(fileGroups.size()));
        for (auto &item : fileGroups) {
            RETURN_NOT_OK(WriteGroup(item, localFileOutputView, snapshotMeta));
        }
        return BSS_OK;
    }

    BResult WriteGroup(TombstoneFileGroupRef &group, const FileOutputViewRef &localFileOutputView,
                       SnapshotMetaRef &snapshotMeta)
    {
        auto files = group->GetFiles();
        RETURN_NOT_OK(localFileOutputView->WriteUint32(files.size()));
        for (const auto &item : files) {
            auto fileMeta = item->GetFileMeta();
            RETURN_ERROR_AS_NULLPTR(fileMeta);
            RETURN_NOT_OK(fileMeta->Snapshot(localFileOutputView));
            if (UNLIKELY(fileMeta->GetFileStatus() != LOCAL)) {
                continue;
            }
            snapshotMeta->AddLocalFullSize(fileMeta->GetFileSize());
            snapshotMeta->AddLocalIncrementalSize(fileMeta->GetFileSize());
            snapshotMeta->AddLocalFileId(fileMeta->GetFileId());
            PathRef path = std::make_shared<Path>(fileMeta->GetIdentifier());
            snapshotMeta->AddLocalFilePath(path);
        }
        return BSS_OK;
    }

private:
    std::vector<TombstoneLevelRef> mHighLevels;
    std::vector<TombstoneFileMetaRef> mFileMetas;
    ConfigRef mConfig;
    FileCacheManagerRef mFileCacheManager;
    MemManagerRef mMemManager;
    TombstoneFileManagerRef mFileManager;
};
using TombstoneFileManagerSnapshotRef = std::shared_ptr<TombstoneFileManagerSnapshot>;
}  // namespace bss
}  // namespace ock

#endif  // BOOST_SS_TOMBSTONE_FILE_MANAGER_SNAPSHOT_H
