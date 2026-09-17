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

#include "tombstone_file_manager.h"

#include "blob_file_manager.h"
#include "snapshot/blob_store_snapshot_coordinator.h"
#include "snapshot/blob_store_snapshot_operator.h"
#include "tombstone_compaction_processor.h"
#include "tombstone_file_manager_snapshot.h"
#include "tombstone_level.h"
#include "tombstone_service.h"

namespace ock {
namespace bss {
TombstoneFileRef TombstoneFileManager::AllocateFile(uint64_t version)
{
    FileInfoRef fileInfo = mFileCacheManager->AllocateFile(mBasePath, TombstoneFileManager::TombstoneFileName);
    RETURN_NULLPTR_AS_NULLPTR(fileInfo);
    auto fileMeta = std::make_shared<TombstoneFileMeta>(fileInfo, mGroupRange, version);
    TombstoneFileRef tombstoneFile = std::make_shared<TombstoneFile>(mConfig, mFileCacheManager, fileMeta, true);
    BResult ret = tombstoneFile->InitFileOutPutView(fileInfo);
    RETURN_NULLPTR_AS_NOT_OK(ret);
    LOG_INFO("New tombstone file success, file name: " << fileInfo->GetFilePath()->ExtractFileName()
                                                       << ", fileAddress:" << fileMeta->GetFileAddress());
    return tombstoneFile;
}

BResult TombstoneFileManager::TriggerCompaction()
{
    uint32_t versionSmallThanSnapshot = CalLevel0CompactionFileNum(mSnapshotMinVersion);
    while (versionSmallThanSnapshot >= mConfig->mTombstoneLevel0CompactionFileNum) {
        RETURN_NOT_OK(DoCompaction(mSnapshotMinVersion, false));
        versionSmallThanSnapshot = CalLevel0CompactionFileNum(mSnapshotMinVersion);
    }

    // 触发其它Level compaction
    for (const auto &level : mHighLevels) {
        level->TriggerCompaction(mBlobFileManager->GetBlobFileGroupManager());
    }
    CheckTopLevelIsFull();
    return BSS_OK;
}

BResult TombstoneFileManager::DoCompaction(uint64_t maxVersion, bool snapshot)
{
    std::vector<TombstoneFileRef> compactionFiles = FindLevel0CompactionFiles(maxVersion);
    if (compactionFiles.empty()) {
        return BSS_OK;
    }
    TombstoneFileGroupRef newFileGroup;
    std::vector<TombstoneFileRef> nextLevel;
    std::vector<TombstoneFileRef> newFiles;

    if (compactionFiles.size() == 1) {
        newFileGroup = std::make_shared<TombstoneFileGroup>(compactionFiles);
    } else {
        auto self = shared_from_this();
        TombstoneCompactionProcessorRef processor =
            std::make_shared<TombstoneCompactionProcessor>(compactionFiles, nextLevel, mConfig, self, false,
                                                           mMemManager, mBlobFileManager->GetBlobFileGroupManager());
        auto ret = processor->Process(newFiles);
        if (UNLIKELY(ret != BSS_OK)) {
            LOG_ERROR("Do compaction fail, ret:" << ret);
            return ret;
        }
        newFileGroup = std::make_shared<TombstoneFileGroup>(newFiles);
    }
    mHighLevels.at(0)->InsertFileGroup(newFileGroup);
    if (snapshot) {
        return BSS_OK;
    }
    // 触发其它level compaction
    for (const auto &level : mHighLevels) {
        level->TriggerCompaction(mBlobFileManager->GetBlobFileGroupManager());
    }
    CheckTopLevelIsFull();
    return BSS_OK;
}

uint32_t TombstoneFileManager::CalLevel0CompactionFileNum(uint64_t maxVersion)
{
    uint32_t canMergeNum = 0;
    for (const auto &item : mLevel0) {
        for (const auto &file : item.second->GetWrittenFiles()) {
            CONTINUE_LOOP_AS_NULLPTR(file);
            if (file->GetVersion() <= maxVersion) {
                canMergeNum++;
            }
        }
    }
    return canMergeNum;
}

void TombstoneFileManager::DiscardFile(const std::vector<TombstoneFileRef> &files)
{
    for (const auto &item : files) {
        mFiles.erase(item);
        auto ref = item->DecRef();
        if (UNLIKELY(ref == 0)) {
            LOG_DEBUG("tombstone file ref is 0, file:" << item->GetFileAddress());
            mFileCacheManager->DiscardFile(item->GetFileAddress());
        }
    }
}

std::vector<TombstoneFileRef> TombstoneFileManager::FindLevel0CompactionFiles(uint64_t maxVersion)
{
    std::vector<TombstoneFileRef> ret;
    for (const auto &item : mLevel0) {
        item.second->GetLessVersionFiles(ret, maxVersion);
    }
    return ret;
}

void TombstoneFileManager::InitLevel()
{
    TombstoneFileManagerRef self = shared_from_this();
    TombstoneLevelRef level1 = std::make_shared<TombstoneLevel>(mConfig, NO_1, self, false, mMemManager);
    TombstoneLevelRef level2 = std::make_shared<TombstoneLevel>(mConfig, NO_2, self, true, mMemManager);
    level1->SetNextLevel(level2);
    mHighLevels.emplace_back(level1);
    mHighLevels.emplace_back(level2);
}

void TombstoneFileManager::CheckTopLevelIsFull()
{
    uint32_t topLevelId = mHighLevels.size() - NO_1;
    auto topLevel = mHighLevels.at(topLevelId);
    if (!(topLevel->IsFull())) {
        return;
    }
    auto preLevel = mHighLevels.at(topLevelId - NO_1);
    auto oldTopLevelId = topLevel->GetLevelId();
    auto newLevel = std::make_shared<TombstoneLevel>(mConfig, oldTopLevelId, shared_from_this(), false, mMemManager);
    preLevel->SetNextLevel(newLevel);
    newLevel->SetNextLevel(topLevel);
    mHighLevels.insert(mHighLevels.begin() + topLevelId, newLevel);
}

TombstoneFileManagerSnapshotRef TombstoneFileManager::CreateSnapshot(uint64_t version)
{
    uint32_t versionSmallThanSnapshot = CalLevel0CompactionFileNum(version);
    while (versionSmallThanSnapshot > 0) {
        DoCompaction(version, true);
        versionSmallThanSnapshot = CalLevel0CompactionFileNum(version);
    }
    return std::make_shared<TombstoneFileManagerSnapshot>(mHighLevels);
}

BResult TombstoneFileManager::Restore(const FileInputViewRef &fileInputView,
                                      std::unordered_map<std::string, uint32_t> &restorePathFileIdMap,
                                      uint64_t restoreVersion, bool rescale)
{
    for (const auto &item : mLevel0) {
        item.second->RestoreVersion(restoreVersion);
    }
    return RestoreLevel(fileInputView, restorePathFileIdMap, rescale);
}

void TombstoneFileManager::FinishRestore()
{
    uint32_t topLevelId = mHighLevels.size() - NO_1;
    auto topLevel = mHighLevels.at(topLevelId);
    RETURN_AS_NULLPTR(topLevel);
    topLevel->SetTopLevel(true);
}

BResult TombstoneFileManager::RestoreLevel(const FileInputViewRef &inputView,
                                           std::unordered_map<std::string, uint32_t> &restorePathFileIdMap,
                                           bool rescale)
{
    uint32_t levelSize = 0;
    RETURN_NOT_OK(inputView->Read(levelSize));
    std::vector<TombstoneLevelRef> restoreLevels;
    restoreLevels.reserve(levelSize);
    std::unordered_map<std::string, RestoreFileInfo> restoreFileMapping;
    // 1、读取levels信息
    for (uint32_t i = 1; i <= levelSize; ++i) {
        TombstoneLevelRef restoreLevel = std::make_shared<TombstoneLevel>(mConfig, i, shared_from_this(),
                                                                          i == levelSize, mMemManager);
        std::vector<TombstoneFileGroupRef> groupVec;
        RETURN_NOT_OK(RestoreFileGroup(inputView, restorePathFileIdMap, groupVec, restoreFileMapping));
        restoreLevel->InsertFileGroups(groupVec);

        if (i > 1) {
            auto preLevel = restoreLevels[i - NO_2];
            preLevel->SetNextLevel(restoreLevel);
        }
        restoreLevels.emplace_back(restoreLevel);
    }
    // 2、恢复mHighLevels
    auto restoreLevelSize = static_cast<int32_t>(restoreLevels.size());
    int32_t levelGap = restoreLevelSize - static_cast<int32_t>(mHighLevels.size());
    while (levelGap > 0) {
        TombstoneLevelRef lastLevel = mHighLevels.at(mHighLevels.size() - NO_1);
        CONTINUE_LOOP_AS_NULLPTR(lastLevel);
        TombstoneLevelRef newLevel = std::make_shared<TombstoneLevel>(mConfig, lastLevel->GetLevelId() + NO_1,
                                                                      shared_from_this(), false, mMemManager);
        lastLevel->SetNextLevel(newLevel);
        mHighLevels.push_back(newLevel);
        lastLevel->SetTopLevel(false);
        --levelGap;
    }
    for (int index = 0; index < restoreLevelSize; ++index) {
        TombstoneLevelRef curLevel = mHighLevels.at(index);
        curLevel->InsertFileGroups(restoreLevels.at(index)->GetFileGroup());
    }
    // 3、恢复文件
    std::unordered_map<std::string, std::pair<uint64_t, uint32_t>> result;
    RETURN_NOT_OK(mFileCacheManager->RestoreFiles(result, restoreFileMapping, mBasePath, rescale));
    return BSS_OK;
}

BResult TombstoneFileManager::RestoreFileGroup(const FileInputViewRef &inputView,
                                               std::unordered_map<std::string, uint32_t> &restorePathFileIdMap,
                                               std::vector<TombstoneFileGroupRef> &groupVec,
                                               std::unordered_map<std::string, RestoreFileInfo> &restoreFileMapping)
{
    uint32_t groupSize = 0;
    RETURN_NOT_OK(inputView->Read(groupSize));
    for (uint32_t i = 0; i < groupSize; ++i) {
        TombstoneFileGroupRef fileGroup;
        RETURN_NOT_OK(RestoreFile(inputView, restorePathFileIdMap, fileGroup, restoreFileMapping));
        groupVec.emplace_back(fileGroup);
    }
    return BSS_OK;
}

BResult TombstoneFileManager::RestoreFile(const FileInputViewRef &inputView,
                                          std::unordered_map<std::string, uint32_t> &restorePathFileIdMap,
                                          TombstoneFileGroupRef &fileGroup,
                                          std::unordered_map<std::string, RestoreFileInfo> &restoreFileMapping)
{
    uint32_t fileCount = 0;
    std::vector<TombstoneFileRef> fileVec;
    RETURN_NOT_OK(inputView->Read(fileCount));
    for (uint32_t i = 0; i < fileCount; ++i) {
        TombstoneFileMetaRef fileMeta = std::make_shared<TombstoneFileMeta>();
        RestoreFileInfo restoreFileInfo;
        RETURN_NOT_OK(fileMeta->Restore(inputView, mFileCacheManager->GetPrimaryDataBasePath(), restorePathFileIdMap,
                                        restoreFileInfo));
        TombstoneFileRef file = std::make_shared<TombstoneFile>(mConfig, mFileCacheManager, fileMeta, false);
        auto it = restoreFileMapping.find(restoreFileInfo.fileIdentifier);
        if (it == restoreFileMapping.end()) {
            restoreFileMapping.emplace(restoreFileInfo.fileIdentifier, restoreFileInfo);
        } else {
            it->second.refCount += 1;
        }
        fileVec.emplace_back(file);
    }
    fileGroup = std::make_shared<TombstoneFileGroup>(fileVec);
    return BSS_OK;
}

void TombstoneFileManager::TriggerSnapshot(uint64_t snapshotId, uint64_t blobStoreVersion, uint64_t seqId,
                                           BlobStoreSnapshotOperatorRef &blobStoreSnapshotOperator)
{
    std::vector<TombstoneServiceRef> level0;
    for (const auto &item : mLevel0) {
        level0.emplace_back(item.second);
    }
    TombstoneFileManagerRef self = shared_from_this();
    BlobStoreSnapshotCoordinatorRef coordinator =
        std::make_shared<BlobStoreSnapshotCoordinator>(mMemManager, level0, blobStoreVersion, seqId, mBlobFileManager,
                                                       self, blobStoreSnapshotOperator, mFileCacheManager);
    blobStoreSnapshotOperator->SetSnapshotCoordinator(coordinator);
    mSnapshotMap.emplace(snapshotId, coordinator);
    UpdateSnapshotMinVersion();
}

bool TombstoneFileManager::ShouldCompactionAllFileToTop()
{
    if (UNLIKELY(mHighLevels.size() < NO_2)) {
        LOG_ERROR("tombstone file manager high level size is unexpect :" << mHighLevels.size());
        return false;
    }
    auto topLevel = mHighLevels.at(mHighLevels.size() - NO_1);
    auto preLevel = mHighLevels.at(mHighLevels.size() - NO_2);
    bool should = topLevel->GetFileGroup().empty() ||
                  topLevel->GetTotalFileSize() < (preLevel->GetTotalFileSize() / NO_2);
    if (should) {
        LOG_INFO("tombstone file manager, top level group size:"
                 << topLevel->GetFileGroup().size() << ",top level file size: " << topLevel->GetTotalFileSize()
                 << ",pre level file size: " << preLevel->GetTotalFileSize());
    }
    return should;
}

void TombstoneFileManager::CompactionAllFileToTop()
{
    if (UNLIKELY(mHighLevels.size() < NO_2)) {
        LOG_INFO("tombstone file manager high level size is unexpect :" << mHighLevels.size());
        return;
    }
    std::vector<TombstoneFileRef> selectFiles;
    for (const auto &item : mLevel0) {
        auto tombstoneService = item.second;
        CONTINUE_LOOP_AS_NULLPTR(tombstoneService);
        auto files = tombstoneService->GetAndClearWrittenFiles();
        for (const auto &file : files) {
            CONTINUE_LOOP_AS_NULLPTR(file);
            selectFiles.emplace_back(file);
        }
    }
    for (uint32_t i = 0; i < mHighLevels.size() - 1; ++i) {
        std::vector<TombstoneFileRef> files = mHighLevels.at(i)->GetAndClearFiles();
        selectFiles.insert(selectFiles.end(), files.begin(), files.end());
    }
    if (selectFiles.empty()) {
        LOG_DEBUG("tombstone file manager compaction all file to top level, but select file is empty");
        return;
    }
    std::vector<TombstoneFileRef> topGroupFiles;
    auto topLevel = mHighLevels.at(mHighLevels.size() - 1);
    RETURN_AS_NULLPTR(topLevel);
    auto topFileGroups = topLevel->GetFileGroup();
    auto files = topLevel->GetAndClearFistGroupFiles();
    if (!files.empty()) {
        topGroupFiles.insert(topGroupFiles.end(), files.begin(), files.end());
    }
    auto self = shared_from_this();
    TombstoneCompactionProcessorRef processor =
        std::make_shared<TombstoneCompactionProcessor>(selectFiles, topGroupFiles, mConfig, self, true, mMemManager,
                                                       mBlobFileManager->GetBlobFileGroupManager());
    std::vector<TombstoneFileRef> newFiles;
    auto ret = processor->Process(newFiles);
    if (UNLIKELY(ret != BSS_OK)) {
        LOG_ERROR("tombstone file manager compaction all file to top level, but process fail, ret: " << ret);
        return;
    }
    auto newFileGroup = std::make_shared<TombstoneFileGroup>(newFiles);
    topLevel->InsertFileGroup(newFileGroup);
    LOG_INFO("tombstone file manager compaction all file to top level success, new file size: " << newFiles.size());
}

TombstoneLevelRef TombstoneFileManager::GetTopLevel() const
{
    return mHighLevels.back();
}

void TombstoneFileManager::CleanExpireTombstoneFile(uint64_t minBlobId)
{
    std::vector<TombstoneFileRef> pendingDeleteFiles;
    for (const auto &level : mHighLevels) {
        CONTINUE_LOOP_AS_NULLPTR(level);
        level->CleanExpireTombstoneFile(minBlobId, pendingDeleteFiles);
    }
    if (pendingDeleteFiles.empty()) {
        return;
    }
    DiscardFile(pendingDeleteFiles);
}

std::shared_ptr<TombstoneService> TombstoneFileManager::AddLevel0(const std::string &name,
                                                                  const KeyGroupUtilRef &keyGroupUtil)
{
    auto service = std::make_shared<TombstoneService>(mConfig, mGroupRange, mInitVersion, mTombstoneFlushExecutor,
                                                      shared_from_this(), mMemManager, keyGroupUtil);
    mLevel0.emplace(name, service);
    return service;
}

void TombstoneFileManager::UpdateSnapshotMinVersion()
{
    uint64_t minVersion = UINT64_MAX;
    for (const auto &item : mSnapshotMap) {
        minVersion = std::min(minVersion, item.first);
    }
    mSnapshotMinVersion = minVersion;
}

void TombstoneFileManager::ReleaseSnapshot(uint64_t snapshotId)
{
    auto it = mSnapshotMap.find(snapshotId);
    if (it == mSnapshotMap.end()) {
        return;
    }
    auto coordinator = it->second;
    mSnapshotMap.erase(it);
    UpdateSnapshotMinVersion();
    if (coordinator == nullptr) {
        LOG_ERROR("tombstone file manager release snapshot, but coordinator is null, snapshotId: " << snapshotId);
        return;
    }
    coordinator->ReleaseResource();
}

uint64_t TombstoneFileManager::GetSnapshotVersion(uint64_t snapshotId)
{
    auto it = mSnapshotMap.find(snapshotId);
    if (it == mSnapshotMap.end()) {
        LOG_INFO("Tombstone file manager snapshot id is not exist, snapshotId: " << snapshotId);
        return UINT64_MAX;
    }
    return it->second->GetVersion();
}

uint64_t TombstoneFileManager::CalTombstoneNum(uint64_t minBlobId)
{
    uint64_t tombstoneNum = 0;
    for (const auto &pair : mLevel0) {
        tombstoneNum += pair.second->CalTombstoneNum(minBlobId);
    }

    for (const auto &level : mHighLevels) {
        tombstoneNum += level->CalTombstoneNum(minBlobId);
    }
    return tombstoneNum;
}

}  // namespace bss
}  // namespace ock
