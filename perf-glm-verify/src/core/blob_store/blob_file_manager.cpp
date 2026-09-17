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

#include "blob_file_manager.h"

#include "blob_cleaner.h"
#include "file/file_name.h"

namespace ock {
namespace bss {

BResult BlobFileManager::Init(const MemManagerRef &memManager, const FileCacheManagerRef &fileCacheManager,
                              const ConfigRef &config, const BlockCacheRef &blockCache, uint64_t version)
{
    mFileCacheManager = fileCacheManager;
    mFileDirectory = mFileCacheManager->CreateFileSubDirectory("blobFile");
    RETURN_ERROR_AS_NULLPTR(mFileDirectory);
    mMemManager = memManager;
    mConfig = config;
    mBlockCache = blockCache;
    GroupRangeRef groupRange = std::make_shared<GroupRange>(mConfig->GetStartGroup(), mConfig->GetEndGroup());
    mBlobFileGroupManager = std::make_shared<BlobFileGroupManager>(groupRange);
    mVersion.store(version);
    mBlobCleaner = std::make_shared<BlobCleaner>();
    mBlobCleaner->Init(mConfig, mBlobFileGroupManager, mFileCacheManager, shared_from_this(), mMemManager, version);
    return BSS_OK;
}

BResult BlobFileManager::WriteBlock(BlobDataBlockRef &dataBlock)
{
    if (mBlobFileWriter == nullptr) {
        RETURN_NOT_OK(InitBlobFileWriter());
    }
    RETURN_ERROR_AS_NULLPTR(mBlobFileWriter);
    RETURN_ERROR_AS_NULLPTR(mConfig);
    if (mBlobFileWriter->GetOutputSize() > mConfig->GetBlobFileSize()) {
        BlobImmutableFileRef blobImmutableFile = mBlobFileWriter->WriteImmutableFile(mVersion.load());
        RETURN_ERROR_AS_NULLPTR(blobImmutableFile);
        auto blobFileMeta = blobImmutableFile->GetBlobFileMeta();
        RETURN_ERROR_AS_NULLPTR(blobFileMeta);
        RETURN_NOT_OK_AS_FALSE(blobFileMeta->GetDataBlockSize() <= 0, BSS_ERR);
        RETURN_NOT_OK(AddBlobImmutableFile(blobImmutableFile));
        RETURN_NOT_OK(InitBlobFileWriter());
    }
    RETURN_NOT_OK(mBlobFileWriter->Write(dataBlock));
    return BSS_OK;
}

BResult BlobFileManager::AddBlobImmutableFile(const BlobImmutableFileRef &blobImmutableFile)
{
    RETURN_INVALID_PARAM_AS_NULLPTR(blobImmutableFile);
    RETURN_ERROR_AS_NULLPTR(mBlobFileWriter);
    auto writerFileMeta = mBlobFileWriter->GetBlobFileMeta();
    RETURN_ERROR_AS_NULLPTR(writerFileMeta);
    RETURN_NOT_OK_AS_FALSE(!blobImmutableFile->IsSame(writerFileMeta), BSS_INNER_ERR);
    auto fileMeta = blobImmutableFile->GetBlobFileMeta();
    RETURN_ERROR_AS_NULLPTR(fileMeta);
    std::vector<BlobFileMetaRef> blobFileMetas;
    blobFileMetas.emplace_back(fileMeta);
    RETURN_INVALID_PARAM_AS_NULLPTR(mFileCacheManager);
    mFileCacheManager->ConfirmAllocationOnFlushOrCompaction(blobFileMetas);
    RETURN_ERROR_AS_NULLPTR(mBlobFileGroupManager);
    auto fileGroup = mBlobFileGroupManager->GetBlobFileGroup();
    RETURN_ERROR_AS_NULLPTR(fileGroup);
    RETURN_NOT_OK(fileGroup->AddLastFile(blobImmutableFile));
    LOG_INFO("Add blob file success, file name: " << PathTransform::ExtractFileName(fileMeta->GetIdentifier()));
    WriteLocker<ReadWriteLock> lock(&mRwLock);
    mBlobFileWriter = nullptr;
    return BSS_OK;
}

BlobFileWriterRef BlobFileManager::NewBlobFileWriter()
{
    RETURN_NULLPTR_AS_NULLPTR(mFileCacheManager);
    FileInfoRef fileInfo = mFileCacheManager->AllocatePrefixFile(BLOB_FILE_NAME_PREFIX, mFileDirectory,
                                                                 FileName::CreateFileName);
    RETURN_NULLPTR_AS_NULLPTR(fileInfo);
    const auto &blobFileWriter = std::make_shared<BlobFileWriter>();
    RETURN_NULLPTR_AS_NOT_OK(blobFileWriter->Init(fileInfo->GetFilePath(), mConfig, fileInfo->GetFileId(), mMemManager,
                                                  mBlockCache, mFileCacheManager));
    RETURN_NULLPTR_AS_NULLPTR(fileInfo->GetFilePath());
    LOG_INFO("New blob file success, file name: " << fileInfo->GetFilePath()->ExtractFileName());
    return blobFileWriter;
}

BResult BlobFileManager::InitBlobFileWriter()
{
    BlobFileWriterRef blobFileWriter = NewBlobFileWriter();
    RETURN_ERROR_AS_NULLPTR(blobFileWriter);
    WriteLocker<ReadWriteLock> lock(&mRwLock);
    mBlobFileWriter = blobFileWriter;
    return BSS_OK;
}

BResult BlobFileManager::Get(uint64_t blobId, uint32_t keyGroup, Value &value)
{
    {
        BResult ret = BSS_OK;
        ReadLocker<ReadWriteLock> lock(&mRwLock);
        if (mBlobFileWriter != nullptr) {
            ret = mBlobFileWriter->SelectBlobValue(blobId, keyGroup, value);
            if (LIKELY(ret == BSS_OK)) {
                return BSS_OK;
            }
            if (UNLIKELY(ret != BSS_NOT_FOUND)) {
                LOG_ERROR("Look up in flush queue failed, ret: " << ret);
                return ret;
            }
        }
    }

    RETURN_ERROR_AS_NULLPTR(mBlobFileGroupManager);
    return mBlobFileGroupManager->BinarySearchBlobValue(blobId, keyGroup, value);
}

BResult BlobFileManager::Restore(const std::vector<std::pair<FileInputViewRef, int64_t>> &fileInputViews,
                                 uint64_t version, std::unordered_map<std::string, uint32_t> &restorePathFileIdMap,
                                 bool rescale)
{
    std::vector<std::pair<FileInputViewRef, uint64_t>> tombstoneFileInputViews;
    for (const auto &item : fileInputViews) {
        auto fileInputView = item.first;
        if (UNLIKELY(item.second == -1)) {
            continue;
        }
        mVersion.store(version);
        RETURN_NOT_OK(RestoreFileGroup(fileInputView, restorePathFileIdMap, rescale));
        RETURN_NOT_OK(mBlobCleaner->Restore(fileInputView, restorePathFileIdMap, version, rescale));
    }
    mBlobCleaner->FinishRestore();
    return BSS_OK;
}

BResult BlobFileManager::FlushCurrentBlobFile(uint64_t version)
{
    RETURN_NOT_OK_AS_FALSE(version < mVersion.load(), BSS_INNER_ERR);
    if (UNLIKELY(mBlobFileWriter == nullptr)) {
        mVersion.fetch_add(NO_1);
        return BSS_OK;
    }
    BlobImmutableFileRef blobImmutableFile = mBlobFileWriter->WriteImmutableFile(mVersion.load());
    RETURN_ERROR_AS_NULLPTR(blobImmutableFile);
    auto blobFileMeta = blobImmutableFile->GetBlobFileMeta();
    RETURN_ERROR_AS_NULLPTR(blobFileMeta);
    RETURN_NOT_OK_AS_FALSE(blobFileMeta->GetDataBlockSize() <= 0, BSS_ERR);
    RETURN_NOT_OK(AddBlobImmutableFile(blobImmutableFile));
    return BSS_OK;
}

BlobFileGroupManagerRef BlobFileManager::CopyBlobFileGroupManager(uint64_t version)
{
    return mBlobFileGroupManager->SyncSnapshot(version);
}

BResult BlobFileManager::RestoreFileGroup(FileInputViewRef &fileInputView,
                                          std::unordered_map<std::string, uint32_t> &restorePathFileIdMap, bool rescale)
{
    uint32_t groupSize = 0;
    RETURN_NOT_OK(fileInputView->Read(groupSize));
    std::unordered_map<std::string, RestoreFileInfo> restoreFileMapping;
    for (uint32_t j = 0; j < groupSize; j++) {
        uint32_t subValidKeyGroupStart = 0;
        GroupRangeRef validGroupRange = std::make_shared<GroupRange>(mConfig->GetStartGroup(), mConfig->GetEndGroup());
        RETURN_NOT_OK(fileInputView->Read(subValidKeyGroupStart));
        uint32_t subValidKeyGroupEnd = 0;
        RETURN_NOT_OK(fileInputView->Read(subValidKeyGroupEnd));
        GroupRangeRef subValidGroupRange = std::make_shared<GroupRange>(subValidKeyGroupStart, subValidKeyGroupEnd);
        subValidGroupRange = std::make_shared<GroupRange>(validGroupRange->Intersection(subValidGroupRange));
        uint32_t fileCount = 0;
        RETURN_NOT_OK(fileInputView->Read(fileCount));
        std::vector<BlobImmutableFileRef> files;
        files.reserve(fileCount);
        for (uint32_t i = 0; i < fileCount; ++i) {
            BlobFileMetaRef blobFileMeta = std::make_shared<BlobFileMeta>();
            RestoreFileInfo restoreFileInfo;
            RETURN_NOT_OK(RestoreFileMeta(fileInputView, restorePathFileIdMap, blobFileMeta, restoreFileInfo));
            GroupRangeRef restoreValidGroupRange =
                std::make_shared<GroupRange>(blobFileMeta->GetValidGroupRange()->Intersection(subValidGroupRange));
            if (restoreValidGroupRange->GetEndGroup() == -1) {
                continue;
            }
            auto it = restoreFileMapping.find(restoreFileInfo.fileIdentifier);
            if (it == restoreFileMapping.end()) {
                restoreFileMapping.emplace(restoreFileInfo.fileIdentifier, restoreFileInfo);
            } else {
                it->second.refCount += 1;
            }
            blobFileMeta->SetValidGroupRange(restoreValidGroupRange);
            BlobImmutableFileRef blobImmutableFile =
                std::make_shared<BlobImmutableFile>(mConfig, blobFileMeta, mMemManager, mBlockCache, mFileCacheManager);
            files.emplace_back(blobImmutableFile);
        }
        if (subValidGroupRange->GetEndGroup() == -1) {
            continue;
        }
        if (!rescale && j == 0 && subValidGroupRange->Equals(validGroupRange)) {
            mBlobFileGroupManager->RestoreFirstGroup(files);
            continue;
        }
        if (!files.empty()) {
            BlobFileGroupRef blobFileGroup = std::make_shared<BlobFileGroup>(subValidGroupRange, files);
            mBlobFileGroupManager->AddGroup(blobFileGroup);
        }
    }
    std::unordered_map<std::string, std::pair<uint64_t, uint32_t>> result;
    RETURN_NOT_OK(mFileCacheManager->RestoreFiles(result, restoreFileMapping, mFileDirectory, rescale));
    return mBlobFileGroupManager->RestoreFileFooterAndIndexBlock();
}

BResult BlobFileManager::RestoreFileMeta(FileInputViewRef &fileInputView,
                                         std::unordered_map<std::string, uint32_t> &restorePathFileIdMap,
                                         BlobFileMetaRef &blobFileMeta, RestoreFileInfo &restoreFileInfo)
{
    RETURN_NOT_OK(blobFileMeta->Restore(fileInputView, restoreFileInfo, mFileCacheManager->GetPrimaryDataBasePath(),
                                        restorePathFileIdMap));
    return BSS_OK;
}

void BlobFileManager::FindMaxDeleteRatioFile(double &maxDeleteRatio, BlobFileGroupRef &maxDeleteRatioGroup,
                                             BlobImmutableFileRef &maxDeleteRatioFile, uint32_t &selectFileIndex)
{
    auto fileGroups = mBlobFileGroupManager->GetFileGroups();
    for (auto &fileGroup : fileGroups) {
        auto blobImmutableFiles = fileGroup->GetFiles();
        uint32_t index = 0;
        for (auto &blobImmutableFile : blobImmutableFiles) {
            CONTINUE_LOOP_AS_NULLPTR(blobImmutableFile);
            auto blobFileMeta = blobImmutableFile->GetBlobFileMeta();
            CONTINUE_LOOP_AS_NULLPTR(blobFileMeta);
            double deleteRatio = blobFileMeta->EstimateDeleteRatio();
            if (deleteRatio > maxDeleteRatio) {
                LOG_DEBUG("Find max delete ratio file, address: " << blobFileMeta->GetFileAddress()
                                                                  << ", deleteRatio: " << deleteRatio
                                                                  << ", maxDeleteRatio: " << maxDeleteRatio);
                maxDeleteRatio = deleteRatio;
                maxDeleteRatioFile = blobImmutableFile;
                maxDeleteRatioGroup = fileGroup;
                selectFileIndex = index;
            }
            index++;
        }
    }
}

std::vector<BlobImmutableFileRef> BlobFileManager::SelectMaxCompactionRateFiles(uint32_t &startIndex,
                                                                                BlobFileGroupRef &maxDeleteRatioGroup)
{
    double maxDeleteRatio = 0;
    BlobImmutableFileRef maxDeleteRatioFile = nullptr;
    uint32_t selectFileIndex = 0;
    FindMaxDeleteRatioFile(maxDeleteRatio, maxDeleteRatioGroup, maxDeleteRatioFile, selectFileIndex);
    if (maxDeleteRatioFile == nullptr) {
        return {};
    }
    return SelectContinuousFilesByMaxDeleteRatioFile(maxDeleteRatio, maxDeleteRatioGroup, maxDeleteRatioFile,
                                                     selectFileIndex, startIndex);
}

std::vector<BlobImmutableFileRef> BlobFileManager::SelectContinuousFilesByMaxDeleteRatioFile(
    double &maxDeleteRatio, BlobFileGroupRef &maxDeleteRatioGroup, BlobImmutableFileRef &maxDeleteRatioFile,
    uint32_t &selectFileIndex, uint32_t &startIndex)
{
    if (UNLIKELY(maxDeleteRatioGroup == nullptr || maxDeleteRatioFile == nullptr)) {
        LOG_ERROR("maxDeleteRatioGroup or maxDeleteRatioFile is nullptr");
        return {};
    }
    uint32_t estimateFileSizeAfterCompaction =
        static_cast<uint32_t>(maxDeleteRatioFile->GetBlobFileMeta()->GetFileSize() * (1.0 - maxDeleteRatio));
    int32_t left = static_cast<int32_t>(selectFileIndex) - 1;
    int32_t right = static_cast<int32_t>(selectFileIndex) + 1;
    startIndex = selectFileIndex;
    std::vector<BlobImmutableFileRef> ret;
    ret.emplace_back(maxDeleteRatioFile);
    auto tombstoneFileTargetSize = mConfig->mTombstoneFileSize;
    double blobFileCompactionMinDeleteRatio = mConfig->GetBlobFileCompactionMinDeleteRatio();
    auto blobImmutableFiles = maxDeleteRatioGroup->GetFiles();
    auto fileCount = (int32_t)blobImmutableFiles.size();
    while (left >= 0 || right < (fileCount - 1)) {
        if ((double)estimateFileSizeAfterCompaction < ((double)tombstoneFileTargetSize * NO_0_8)) {
            return ret;
        }
        double leftDeleteRatio = (left >= 0) ? blobImmutableFiles[left]->EstimateDeleteRatio() : 0.0;
        double rightDeleteRatio = (right < fileCount) ? blobImmutableFiles[right]->EstimateDeleteRatio() : 0.0;
        if (leftDeleteRatio < blobFileCompactionMinDeleteRatio && rightDeleteRatio < blobFileCompactionMinDeleteRatio) {
            return ret;
        }
        if (rightDeleteRatio > leftDeleteRatio) {
            if (right >= fileCount) {
                return ret;
            }
            auto file = blobImmutableFiles[right];
            estimateFileSizeAfterCompaction +=
                (uint32_t)(file->GetBlobFileMeta()->GetFileSize() * (1.0 - rightDeleteRatio));
            ret.emplace_back(file);
            right++;
            continue;
        }
        if (left < 0) {
            return ret;
        }
        auto file = blobImmutableFiles[left];
        if (file->GetBlobFileMeta()->GetMaxExpireTime() < TimeStampUtil::GetCurrentTime()) {
            left = -1;
            continue;
        }
        estimateFileSizeAfterCompaction += (uint32_t)(file->GetBlobFileMeta()->GetFileSize() * (1.0 - leftDeleteRatio));
        ret.insert(ret.begin(), file);
        left--;
        startIndex--;
    }
    return ret;
}

TombstoneServiceRef BlobFileManager::RegisterTombstoneService(const std::string &name,
                                                              const KeyGroupUtilRef &keyGroupUtil)
{
    return mBlobCleaner->RegisterTombstoneService(name, keyGroupUtil);
}

void BlobFileManager::ReleaseTombstoneSnapshot(uint64_t snapshotId)
{
    if (UNLIKELY(mBlobCleaner == nullptr)) {
        return;
    }
    mBlobCleaner->ReleaseTombstoneSnapshot(snapshotId);
}

}  // namespace bss
}  // namespace ock
