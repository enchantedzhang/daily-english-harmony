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

#include "version.h"

#include "include/config.h"
#include "version_set.h"

namespace ock {
namespace bss {
ConfigRef Version::GetConf()
{
    return static_cast<VersionSet *>(mParentVersionSet)->GetConf();
}

template <typename K> void BuilderBase<K>::FinalizeVersion()
{
    for (uint32_t idx = 0; idx < mLevels.size(); ++idx) {
        auto groups = mLevels[idx].GetFileMetaDataGroups();
        double score = 0.0;
        for (auto &fileMetaDataGroup : groups) {
            if (!fileMetaDataGroup->GetGroupRange()->Equals(mCurGroupRange)) {
                mCompactionScore = DOUBLE_MAX_VALUE;
                mCompactionLevel = mLevels[idx].GetLevelId();
                mCompactionReason = Reason::KEY_GROUP_RESCALED;
                break;
            }
            for (auto &file : fileMetaDataGroup->GetFiles()) {
                if (file->GetOrderRange()->HasRedundantData()) {
                    mCompactionReason = Reason::ORDER_RANGE_REDUNDANT;
                    break;
                }
            }
            if (mCompactionReason == Reason::ORDER_RANGE_REDUNDANT) {
                mCompactionScore = DOUBLE_MAX_VALUE;
                mCompactionLevel = mLevels[idx].GetLevelId();
                break;
            }

            if (mLevels[idx].GetLevelId() == 0 &&
                static_cast<VersionSet *>(mParentVersionSet)->GetConf()->GetFileStoreL0NumTrigger() != 0) {
                score += 1.0 * fileMetaDataGroup->GetNumberOfFiles() /
                         static_cast<VersionSet *>(mParentVersionSet)->GetConf()->GetFileStoreL0NumTrigger();
            } else {
                uint64_t levelBytes = 0L;
                for (auto &file : fileMetaDataGroup->GetFiles()) {
                    levelBytes += file->GetFileSize();
                }
                if (static_cast<VersionSet *>(mParentVersionSet)->MaxBytesForLevel(mLevels[idx].GetLevelId()) == 0) {
                    LOG_ERROR("Max bytes is 0, levelId: " << mLevels[idx].GetLevelId() << ".");
                    break;
                }
                score += 1.0 * levelBytes /
                         static_cast<VersionSet *>(mParentVersionSet)->MaxBytesForLevel(mLevels[idx].GetLevelId());
            }
            if (score > mCompactionScore) {
                mCompactionScore = score;
                mCompactionLevel = mLevels[idx].GetLevelId();
            }
        }
        if (mCompactionScore == DOUBLE_MAX_VALUE) {
            break;
        }
    }

    if (mCompactionScore < 1.0) {
        mCompactionReason = Reason::NO_NEED_COMPACTION;
    } else if (mCompactionScore != DOUBLE_MAX_VALUE) {
        mCompactionReason = (mCompactionLevel == 0) ? Reason::LEVEL0_NUM_TRIGGERED : Reason::LEVEL_SIZE_TRIGGERED;
    }
}

void VersionBuilder::AddLevel(uint32_t levelId, Level level)
{
    if (mLevels.empty()) {
        mLevels =
            Version::InitEmptyLevels(static_cast<VersionSet *>(mParentVersionSet)->GetConf()->GetFileStoreNumLevels());
    }
    mLevels[levelId] = level;
}

VersionPtr VersionBuilder::Build()
{
    if (mLevels.empty()) {
        mLevels =
            Version::InitEmptyLevels(static_cast<VersionSet *>(mParentVersionSet)->GetConf()->GetFileStoreNumLevels());
    }
    FinalizeVersion();
    return std::make_shared<Version>(mParentVersionSet, mLevels, mCompactionLevel, mCurGroupRange, mCompactionScore,
                                     mCompactionReason);
}

VersionPtr Version::Release()
{
    // 只有当version的引用计数为0才会去释放version.
    if (mReferenceCount.fetch_sub(NO_1) == NO_1) {
        if (static_cast<VersionSet *>(mParentVersionSet)->RemoveVersion(shared_from_this())) {
            return shared_from_this();
        }
        LOG_WARN("Released version " << LevelSummary() << " not included in parent version set.");
    }
    return nullptr;
}

VersionPtr Version::MigrateVersion(
    VersionPtr currentVersion, std::unordered_map<std::string, RestoreFileInfo> &toRelocateFileMapping,
    std::unordered_map<std::string, std::tuple<uint64_t, uint32_t>> &relocatedFileMapping)
{
    // 标记是否有文件被迁移
    bool relocated = false;
    RestoreBuilderRef versionBuilder = RestoreBuilder::NewRestoredBuilder(currentVersion->mParentVersionSet);
    versionBuilder->SetCurGroupRange(currentVersion->GetGroupRange())
        ->SetCurOrderRange(static_cast<VersionSet *>(currentVersion->mParentVersionSet)->GetOrderRange());
    for (auto level : currentVersion->mLevels) {
        for (auto &fileGroup : level.GetFileMetaDataGroups()) {
            if (fileGroup->GetNumberOfFiles() == 0) {
                continue;
            }
            FileDataGroupBuilderRef groupBuilder = FileDataGroupBuilder::NewBuilder();
            groupBuilder->SetGroupRange(fileGroup->GetGroupRange());
            for (auto &file : fileGroup->GetFiles()) {
                LOG_INFO(" MigrateVersion file name:" << file->ToString());
                if (toRelocateFileMapping.find(file->GetIdentifier()) == toRelocateFileMapping.end()) {
                    // 如果文件不需要被迁移，直接添加到文件元数据组构建器中
                    groupBuilder->AddFileMeta(file);
                    continue;
                }

                // 获取文件的迁移信息
                auto fileInfo = toRelocateFileMapping[file->GetIdentifier()];
                // 更新已迁移文件的映射
                relocatedFileMapping.emplace(fileInfo.remoteFileName,
                                             std::make_tuple(fileInfo.remoteFileAddress, fileInfo.fileLength));
                // 创建一个新的文件元数据，并设置相应的属性
                auto newFileBuilder = FileMetaData::NewBuilder();
                newFileBuilder->Fill(file->GetSmallest(), file->GetLargest(), fileInfo.fileLength,
                                     fileInfo.remoteFileAddress, file->GetSeqId(), file->GetGroupRange(),
                                     file->GetOrderRange(), fileInfo.restoreLocalFileName, file->GetStateIdInterval(),
                                     FileStatus::LOCAL, file->GetRecordMeta());
                auto newFile = newFileBuilder->Build();
                RETURN_NULLPTR_AS_NULLPTR(newFile);
                groupBuilder->AddFileMeta(newFile);
                LOG_INFO("Migrate version success, file:"
                         << PathTransform::ExtractFileName(file->GetIdentifier())
                         << ", local path:" << PathTransform::ExtractFileName(fileInfo.restoreLocalFileName));
                relocated = true;
            }
            versionBuilder->AddLevel(level.GetLevelId(), groupBuilder->Build());
        }
    }
    if (relocated) {
        return versionBuilder->Build();
    }
    return currentVersion;
}

ConfigRef RestoreBuilder::GetConf()
{
    return static_cast<VersionSet *>(mParentVersionSet)->GetConf();
}

FileMetaData::FileMetaDataComparator RestoreBuilder::GetFileMetaDataComparator()
{
    return static_cast<VersionSet *>(mParentVersionSet)->GetFileMetaDataComparator();
}

VersionPtr RestoreBuilder::Build()
{
    mLevels = std::vector<Level>();
    for (LevelBuilderRef &levelBuilder : mLevelBuilders) {
        CONTINUE_LOOP_AS_NULLPTR(levelBuilder);
        mLevels.push_back(levelBuilder->Build());
    }
    FinalizeVersion();
    auto result = std::make_shared<Version>(mParentVersionSet, mLevels, mCompactionLevel, mCurGroupRange,
                                            mCompactionScore, mCompactionReason);
    result->SetVersionSeqId();
    return result;
}

}  // namespace bss
}  // namespace ock
