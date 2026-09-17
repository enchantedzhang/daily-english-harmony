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

#include "blob_cleaner.h"

#include "blob_compaction_file_writer.h"
#include "blob_file_group_manager.h"
#include "blob_file_merging_iterator.h"
#include "blob_file_meta_iterator.h"
#include "blob_store.h"
#include "tombstone/compaction_tombstone_file_runable.h"
#include "tombstone/tombstone_file_manager.h"
#include "tombstone/tombstone_file_merging_iterator.h"
#include "tombstone/tombstone_file_writer.h"
#include "tombstone/tombstone_level.h"

namespace ock {
namespace bss {
BResult BlobCleaner::Restore(const FileInputViewRef &fileInputView,
                             std::unordered_map<std::string, uint32_t> &restorePathFileIdMap, uint64_t restoreVersion,
                             bool rescale)
{
    return mTombstoneFileManager->Restore(fileInputView, restorePathFileIdMap, restoreVersion, rescale);
}

void BlobCleaner::FinishRestore()
{
    mTombstoneFileManager->FinishRestore();
}

void BlobCleaner::TriggerSnapshot(uint64_t snapshotId, uint64_t blobStoreVersion, uint64_t seqId,
                                  BlobStoreSnapshotOperatorRef &blobStoreSnapshotOperator)
{
    if (mEnableTombstone) {
        // 快照互斥，如果当前有墓碑文件正在compaction，需要等待compaction完成并关闭，待快照完成重新开启，ReleaseTombstoneSnapshot入口
        StopTombstoneCompaction();
    }
    mTombstoneFileManager->TriggerSnapshot(snapshotId, blobStoreVersion, seqId, blobStoreSnapshotOperator);
}

BResult BlobCleaner::Init(const ConfigRef &config, const BlobFileGroupManagerRef &blobFileGroupManager,
                          const FileCacheManagerRef &fileCacheManager, const BlobFileManagerRef &blobFileManager,
                          const MemManagerRef &memManager, uint64_t version)
{
    mConfig = config;
    mEnableTombstone = mConfig->GetEnableTombstone();
    mBlobFileGroupManager = blobFileGroupManager;
    mFileCacheManager = fileCacheManager;
    mBlobFileManager = blobFileManager;
    // 1个定时compaction常驻线程
    mCompactionExecutor = std::make_shared<ExecutorService>(NO_1, NO_1);
    mCompactionExecutor->SetThreadName("BlobCompactionExecutor");
    if (UNLIKELY(!mCompactionExecutor->Start())) {
        LOG_ERROR("Failed to start blob compaction executor.");
        return BSS_ERR;
    }
    mMemManager = memManager;
    GroupRangeRef range = std::make_shared<GroupRange>(mConfig->GetStartGroup(), mConfig->GetEndGroup());
    // 1个墓碑文件Flush线程
    ExecutorServicePtr executorService = std::make_shared<ExecutorService>(NO_1, NO_1024);
    executorService->SetThreadName("BlobTombstoneFlushExecutor");
    if (UNLIKELY(!executorService->Start())) {
        LOG_ERROR("Failed to start blob tombstone flush executor.");
        return BSS_ERR;
    }
    mTombstoneFileManager = std::make_shared<TombstoneFileManager>(mConfig, mFileCacheManager, range, version,
                                                                   mMemManager, executorService, mBlobFileManager);
    mTombstoneFileManager->InitLevel();
    if (mEnableTombstone) {
        StartScheduleCompaction();
    }
    LOG_INFO("Blob cleaner is start.");
    return BSS_OK;
}

bool BlobCleaner::TriggerCompaction()
{
    LOG_DEBUG("Blob cleaner trigger compaction start.");
    mBlobFileManager->CleanExpireFile();
    mBlobFileManager->CleanDeleteFiles();
    auto minBlobId = mBlobFileGroupManager->GetMinBlobId();
    if (minBlobId != UINT64_MAX) {
        mTombstoneFileManager->CleanExpireTombstoneFile(minBlobId);
    }
    mTombstoneFileManager->TriggerCompaction();
    double blobSpaceWasteRate = CalBlobSpaceWasteRate(minBlobId);
    if (mBlobFileManager->GetFileSize() > mConfig->GetBlobMinCompactionThreshold() &&
        blobSpaceWasteRate > mConfig->GetBlobMaxSpaceAmplificationRatio()) {
        DoCompaction();
        mBlobFileManager->CleanDeleteFiles();
    }
    return true;
}

void BlobCleaner::DoCompaction()
{
    if (mTombstoneFileManager->ShouldCompactionAllFileToTop()) {
        mTombstoneFileManager->CompactionAllFileToTop();
    }
    uint32_t startIndex = 0;
    BlobFileGroupRef maxDeleteRatioGroup = nullptr;
    auto selectBlobFiles = mBlobFileManager->SelectMaxCompactionRateFiles(startIndex, maxDeleteRatioGroup);
    if (selectBlobFiles.empty() || maxDeleteRatioGroup == nullptr) {
        LOG_INFO("BlobCompaction DoCompaction blob file is empty");
        return;
    }
    uint32_t selectFileSize = selectBlobFiles.size();
    auto compactionFiles = maxDeleteRatioGroup->RegisterCompaction(startIndex, selectFileSize);
    if (UNLIKELY(compactionFiles.empty())) {
        LOG_ERROR("Register compaction file is empty.");
        return;
    }
    uint64_t minBlobId = compactionFiles.at(0)->GetBlobFileMeta()->GetMinBlobId();
    uint64_t maxBlobId = compactionFiles.at(compactionFiles.size() - 1)->GetBlobFileMeta()->GetMaxBlobId();
    auto tombstoneTopLevel = mTombstoneFileManager->GetTopLevel();
    if (tombstoneTopLevel->GetFileGroup().empty()) {
        LOG_INFO("BlobCompaction DoCompaction tombstone top level is empty");
        return;
    }
    auto tombstoneFileGroup = tombstoneTopLevel->GetFileGroup().front();
    if (UNLIKELY(tombstoneFileGroup == nullptr)) {
        LOG_INFO("BlobCompaction DoCompaction tombstone file group is empty");
        return;
    }
    auto tombstoneFileSubVec = tombstoneFileGroup->FindOverLapFile(minBlobId, maxBlobId);
    if (UNLIKELY(tombstoneFileSubVec == nullptr || tombstoneFileSubVec->Empty())) {
        LOG_INFO("BlobCompaction DoCompaction tombstone file sub vec is empty");
        return;
    }
    std::vector<FileMetaBaseRef> compactionFileMetas;
    for (const auto &item : compactionFiles) {
        compactionFileMetas.emplace_back(item->GetBlobFileMeta());
    }
    for (const auto &item : tombstoneFileSubVec->GetFileVec()) {
        compactionFileMetas.emplace_back(item->GetFileMeta());
    }
    mFileCacheManager->RegisterFilesForCompaction(compactionFileMetas);
    CompactionResult result;
    auto ret = ProcessCompaction(compactionFiles, tombstoneFileSubVec->GetFileVec(), result);
    if (UNLIKELY(ret != BSS_OK)) {
        LOG_ERROR("Process compaction fail, ret:" << ret);
        mFileCacheManager->ReleaseFilesForCompaction(compactionFileMetas);
        return;
    }
    std::vector<FileMetaBaseRef> newBlobFileMetas;
    for (const auto &item : result.first) {
        newBlobFileMetas.emplace_back(item->GetBlobFileMeta());
    }
    mFileCacheManager->ConfirmAllocationOnFlushOrCompaction(newBlobFileMetas);
    maxDeleteRatioGroup->MigrateFiles(startIndex, startIndex + selectFileSize, result.first);
    maxDeleteRatioGroup->UnregisterCompaction(compactionFiles);
    tombstoneFileGroup->MigrateFile(tombstoneFileSubVec, result.second);
    mBlobFileManager->DeleteBlobFiles(compactionFiles);
    mFileCacheManager->ReleaseFilesForCompaction(compactionFileMetas);
    mTombstoneFileManager->DiscardFile(tombstoneFileSubVec->GetFileVec());
    std::ostringstream oss;
    oss << "Compaction success, input files:";
    for (const auto &item : compactionFiles) {
        oss << item->GetBlobFileMeta()->ToString();
    }
    oss << ", input tombstone files:";
    for (const auto &item : tombstoneFileSubVec->GetFileVec()) {
        oss << item->GetFileMeta()->ToString();
    }
    oss << ", new blob files:";
    for (const auto &item : result.first) {
        oss << item->GetBlobFileMeta()->ToString();
    }
    oss << ", new tombstone files:";
    for (const auto &item : result.second) {
        oss << item->GetFileMeta()->ToString();
    }
    std::string basicString = oss.str();
    LOG_INFO(basicString);
}

BResult BlobCleaner::ProcessCompaction(const std::vector<BlobImmutableFileRef> &blobFiles,
                                       const std::vector<TombstoneFileRef> &tombstoneFiles, CompactionResult &result)
{
    if (blobFiles.empty() || tombstoneFiles.empty()) {
        return BSS_INVALID_PARAM;
    }
    uint64_t tombFileMaxVersion = 0;
    std::vector<TombstoneFileIteratorRef> tombstoneFileIterators;
    for (const auto &item : tombstoneFiles) {
        CONTINUE_LOOP_AS_NULLPTR(item);
        auto iterator = std::make_shared<TombstoneFileIterator>(item, mMemManager);
        RETURN_NOT_OK(iterator->Init());
        tombstoneFileIterators.emplace_back(iterator);
        tombFileMaxVersion = std::max(tombFileMaxVersion, item->GetFileMeta()->GetVersion());
    }
    uint64_t blobFileMaxVersion = blobFiles.back()->GetBlobFileMeta()->GetVersion();
    auto blobFileWriter = std::make_shared<BlobCompactionFileWriter>(mConfig, mMemManager, mBlobFileManager,
                                                                     blobFileMaxVersion);
    auto tombstoneFileWriter = std::make_shared<TombstoneFileWriter>(mConfig, mTombstoneFileManager, tombFileMaxVersion,
                                                                     mMemManager);
    auto tombstoneMergeIterator = std::make_shared<TombstoneFileMergingIterator>(tombstoneFileIterators);
    BResult ret = tombstoneMergeIterator->Init();
    if (UNLIKELY(ret != BSS_OK)) {
        tombstoneMergeIterator->Close();
        return ret;
    }
    auto blobMergeIterator = std::make_shared<BlobFileMergingIterator>(blobFiles, mMemManager);
    ret = blobMergeIterator->Init();
    if (UNLIKELY(ret != BSS_OK)) {
        tombstoneMergeIterator->Close();
        blobMergeIterator->Close();
        return ret;
    }
    RETURN_NOT_OK(DoCompactionForBlobAndTombstone(blobMergeIterator, tombstoneMergeIterator, blobFileWriter,
                                                  tombstoneFileWriter));
    std::vector<TombstoneFileRef> newTombstoneFiles;
    RETURN_NOT_OK(tombstoneFileWriter->FinishWrite(newTombstoneFiles));
    std::vector<BlobImmutableFileRef> newBlobFiles;
    RETURN_NOT_OK(blobFileWriter->Finish(newBlobFiles));
    result = { newBlobFiles, newTombstoneFiles };
    return BSS_OK;
}

BResult BlobCleaner::DoCompactionForBlobAndTombstone(
    const std::shared_ptr<BlobFileMergingIterator> &blobMergeIterator,
    const std::shared_ptr<TombstoneFileMergingIterator> &tombstoneMergeIterator,
    const std::shared_ptr<BlobCompactionFileWriter> &blobFileWriter,
    const std::shared_ptr<TombstoneFileWriter> &tombstoneFileWriter)
{
    BResult ret = BSS_OK;
    while (blobMergeIterator->HasNext() && tombstoneMergeIterator->HasNext()) {
        auto blobValue = blobMergeIterator->PeekNext();
        auto tombstone = tombstoneMergeIterator->PeekNext();
        int32_t cmp = CompareBlobAndTombstone(blobValue, tombstone);
        if (cmp > 0) {  // tombstone小，写入文件，tombstone向后移动
            ret = tombstoneFileWriter->WriteTombstone(tombstoneMergeIterator->Next());
            if (UNLIKELY(ret != BSS_OK)) {
                tombstoneMergeIterator->Close();
                blobMergeIterator->Close();
                return ret;
            }
            continue;
        }
        if (cmp < 0) {  // blob小，写入文件，blob向后移动
            auto fileMeta = blobMergeIterator->GetCurFileMeta();
            blobValue = blobMergeIterator->Next();
            if (!(IsBlobNotExpire(blobValue) &&
                  fileMeta->GetValidGroupRange()->ContainsGroup(blobValue->GetKeyGroup()))) {
                continue;
            }
            ret = blobFileWriter->WriteBlob(*blobValue);
            if (UNLIKELY(ret != BSS_OK)) {
                tombstoneMergeIterator->Close();
                blobMergeIterator->Close();
                return ret;
            }
            continue;
        }
        // 相等，直接删除，继续下一个
        blobMergeIterator->Next();
        tombstoneMergeIterator->Next();
    }
    while (blobMergeIterator->HasNext()) {
        auto fileMeta = blobMergeIterator->GetCurFileMeta();
        auto blobValue = blobMergeIterator->Next();
        if (IsBlobNotExpire(blobValue) && fileMeta->GetValidGroupRange()->ContainsGroup(blobValue->GetKeyGroup())) {
            blobFileWriter->WriteBlob(*blobValue);
        }
    }

    while (tombstoneMergeIterator->HasNext()) {
        tombstoneFileWriter->WriteTombstone(tombstoneMergeIterator->Next());
    }
    tombstoneMergeIterator->Close();
    blobMergeIterator->Close();
    return ret;
}

int32_t BlobCleaner::CompareBlobAndTombstone(BlobValueWrapperRef blobValueWrapper, TombstoneRef tombstone)
{
    auto b1 = blobValueWrapper->mBlobId;
    auto b2 = tombstone->GetBlobId();
    if (b1 == b2) {
        return 0;
    }
    return b1 < b2 ? -1 : 1;
}

bool BlobCleaner::IsBlobNotExpire(BlobValueWrapperRef &blobValueWrapper)
{
    int64_t expireTime = blobValueWrapper->mSeqId >> NO_16;
    if (expireTime <= 0) {
        return true;
    }
    uint64_t current = TimeStampUtil::GetCurrentTime();
    uint64_t expireTimeU = static_cast<uint64_t>(expireTime);
    uint64_t retainTimeInMill = mConfig->GetBlobFileRetainTimeInMill();
    if (UNLIKELY(UINT64_MAX - expireTimeU < retainTimeInMill)) {
        LOG_ERROR("Integer overflow, expireTime: " << expireTime << ", retainTime: " << retainTimeInMill);
        return true;
    }
    return current <= expireTimeU + retainTimeInMill;
}

void BlobCleaner::StartScheduleCompaction()
{
    RunnablePtr processor = std::make_shared<TombstoneFileCompactionRunable>(shared_from_this());
    mCompactionExecutor->Execute(processor);
    LOG_INFO("Blob cleaner compaction processor start.");
}

TombstoneServiceRef BlobCleaner::RegisterTombstoneService(const std::string &name, const KeyGroupUtilRef &keyGroupUtil)
{
    if (mEnableTombstone) {
        return mTombstoneFileManager->AddLevel0(name, keyGroupUtil);
    }
    return nullptr;
}

double BlobCleaner::CalBlobSpaceWasteRate(uint64_t minBlobId)
{
    uint64_t totalTombstoneNum = mTombstoneFileManager->CalTombstoneNum(minBlobId);
    uint64_t totalValidBlobNum = 0;
    uint64_t totalBlobNumBelongKeyGroupRange = 0;
    uint64_t totalBlobNum = 0;
    auto blobFileIterator = std::make_shared<BlobFileIterator>();
    BResult result = blobFileIterator->Init(mBlobFileManager->GetBlobFileGroupManager());
    if (result != BSS_OK) {
        return 1.0F;
    }
    while (blobFileIterator->HasNext()) {
        float expiredRatio = 0.0F;
        BlobImmutableFileRef blobFile = blobFileIterator->Next();
        CONTINUE_LOOP_AS_NULLPTR(blobFile);
        auto blobFileMeta = blobFile->GetBlobFileMeta();
        CONTINUE_LOOP_AS_NULLPTR(blobFileMeta);
        uint64_t minExpireTime = blobFileMeta->GetMinExpireTime();
        uint64_t maxExpireTime = blobFileMeta->GetMaxExpireTime();
        if (minExpireTime != 0 && maxExpireTime != 0) {
            uint64_t currentTime = TimeStampUtil::GetCurrentTime();
            if (currentTime < minExpireTime) {
                expiredRatio = 0.0F;
            } else if (currentTime > maxExpireTime) {
                expiredRatio = 1.0F;
            } else if (minExpireTime == maxExpireTime) {
                expiredRatio = 0.0F;
            } else {
                expiredRatio = static_cast<float>(currentTime - minExpireTime) /
                               static_cast<float>(maxExpireTime - minExpireTime);
            }
        }
        auto validGroupRange = blobFileMeta->GetValidGroupRange();
        CONTINUE_LOOP_AS_NULLPTR(validGroupRange);
        auto coveredGroupRange = blobFileMeta->GetCoveredGroupRange();
        CONTINUE_LOOP_AS_NULLPTR(coveredGroupRange);
        float keyGroupValidRatio = static_cast<float>(validGroupRange->GetKeyGroupRangeSize()) /
                                   static_cast<float>(coveredGroupRange->GetKeyGroupRangeSize());
        uint32_t blobNum = blobFileMeta->GetBlobNum();
        totalValidBlobNum +=
            static_cast<uint64_t>(static_cast<float>(blobNum) * keyGroupValidRatio * (1 - expiredRatio));
        totalBlobNumBelongKeyGroupRange += static_cast<uint64_t>(static_cast<float>(blobNum) * keyGroupValidRatio);
        totalBlobNum += blobNum;
    }
    if (totalBlobNumBelongKeyGroupRange == 0) {
        totalTombstoneNum = 0;
    } else {
        totalTombstoneNum = std::min(totalTombstoneNum, totalBlobNumBelongKeyGroupRange - 1);
    }
    uint64_t tmp = 1;
    totalValidBlobNum = std::max(tmp, totalValidBlobNum);
    double blobSpaceWasteRate =
        (static_cast<float>(totalBlobNum) / static_cast<float>(totalValidBlobNum) *
         (1.0F - static_cast<float>(totalTombstoneNum) / static_cast<float>(totalBlobNumBelongKeyGroupRange)));
    LOG_DEBUG("Blob cleaner cal blob space waste rate, totalBlobNum: "
              << totalBlobNum << ", totalBlobNumBelongKeyGroupRange: " << totalBlobNumBelongKeyGroupRange
              << ", totalTombstoneNum: " << totalTombstoneNum << ", totalValidBlobNum: " << totalValidBlobNum
              << ", blobSpaceWasteRate: " << blobSpaceWasteRate);
    return blobSpaceWasteRate;
}

void BlobCleaner::ReleaseTombstoneSnapshot(uint64_t snapshotId)
{
    if (UNLIKELY(mTombstoneFileManager == nullptr)) {
        return;
    }
    mTombstoneFileManager->ReleaseSnapshot(snapshotId);
    if (mEnableTombstone) {
        StartScheduleCompaction();
    }
}

}  // namespace bss
}  // namespace ock
