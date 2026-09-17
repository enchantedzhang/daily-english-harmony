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

#include "file_store_impl.h"

#include <algorithm>
#include <vector>

#include "binary/query_binary.h"
#include "common/path_transform.h"
#include "common/util/seq_generator.h"
#include "compaction/list_state_file_reuse_planner.h"
#include "file_address_util.h"
#include "file_info.h"
#include "lsm_store/version/version_meta_serializer.h"

namespace ock {
namespace bss {
static uint64_t GeneratedFileSeqId(const std::vector<FileMetaDataRef> &v1, const std::vector<FileMetaDataRef> &v2)
{
    static uint64_t gFileSeqId = 1;
    if (v1.empty() && v2.empty()) {
        return gFileSeqId++;
    }

    int64_t maxSeqId = 0;
    for (const auto &fileMetaData : v1) {
        CONTINUE_LOOP_AS_NULLPTR(fileMetaData);
        if (maxSeqId < fileMetaData->GetSeqId()) {
            maxSeqId = fileMetaData->GetSeqId();
        }
    }
    for (const auto &fileMetaData : v2) {
        CONTINUE_LOOP_AS_NULLPTR(fileMetaData);
        if (maxSeqId < fileMetaData->GetSeqId()) {
            maxSeqId = fileMetaData->GetSeqId();
        }
    }
    return static_cast<uint64_t>(maxSeqId);
}

BResult LsmStore::Initialize()
{
    // 1. 初始化GroupRange.
    mOrderRange = std::make_shared<HashCodeOrderRange>(mFileStoreID->GetHashCodeRange()->GetStartHashCode(),
                                                       mFileStoreID->GetHashCodeRange()->GetEndHashCode());
    RETURN_ALLOC_FAIL_AS_NULLPTR(mOrderRange);
    mGroupRange = mFileStoreID->GetGroupRange();
    RETURN_INVALID_PARAM_AS_NULLPTR(mGroupRange);

    // 2. 初始化FileCache.
    mFileCache = std::make_shared<FileCache>(mConf, mFileFactory, mFileCacheManager, mMemManager);
    RETURN_ALLOC_FAIL_AS_NULLPTR(mFileCache);
    mFileCache->SetFileStoreSeqId(mSeqId);
    mFileDirectory = mFileCacheManager->CreateFileSubDirectory();
    RETURN_ALLOC_FAIL_AS_NULLPTR(mFileDirectory);

    // 3. 创建versionSet, 设置compaction迭代器构造builder.
    mVersionSet = std::make_shared<VersionSet>(mConf, mGroupRange, mOrderRange, mFileCache);
    RETURN_ALLOC_FAIL_AS_NULLPTR(mVersionSet);

    auto buildFunc = [this](const FileMetaDataRef &fileMetaData) -> KeyValueIteratorRef {
        FileProcHolder holder = FileProcHolder::FILE_STORE_COMPACTION;
        FullKeyFilterRef keyFilter = IsGroupAndOrderRangeAligned() ?
                                         std::static_pointer_cast<FullKeyFilter>(mStateFilterManager) :
                                         std::make_shared<FileMetaStateFilter>(fileMetaData->GetGroupRange(),
                                                                               fileMetaData->GetOrderRange(),
                                                                               mStateFilterManager);
        return mFileCache->IteratorAll(keyFilter, fileMetaData->GetFileAddress(), holder);
    };
    mVersionSet->SetFileIteratorBuilder(InputSortedRun::FileIteratorWriter::Of(buildFunc));

    // 4. 初始化Compaction调度器.
    auto ret = InitializeCompaction();
    RETURN_NOT_OK_NO_LOG(ret);

    LOG_INFO("Initialize file store success, fileStoreId:" << mSeqId << ", directory path:"
                                                           << mFileDirectory->GetDirectoryPath()->ExtractFileName());
    FileHolderRef fileHolder = shared_from_this();
    mFileCacheManager->RegisterFileHolder(fileHolder);
    return BSS_OK;
}

BResult LsmStore::InitializeCompaction()
{
    if (mCompactionExecutor != nullptr) {
        return BSS_OK;
    }
    mBackgroundCompactions.store(0, std::memory_order_seq_cst);
    mCompactionExecutor = std::make_shared<ExecutorService>(NO_1, NO_128);
    if (mCompactionExecutor == nullptr) {
        LOG_ERROR("Make compaction executor memory failed.");
        return BSS_ALLOC_FAIL;
    }
    mCompactionExecutor->SetThreadName("LsmCompactionExecutor");
    if (!mCompactionExecutor->Start()) {
        LOG_ERROR("Start compaction executor failed.");
        return BSS_ALLOC_FAIL;
    }
    auto uSwitch = mConf->GetLsmStoreCompactionSwitch();
    LOG_INFO("Initialize file store compaction success, fileStoreId:" << mSeqId << ", switch:" << uSwitch);
    return BSS_OK;
}

BResult LsmStore::InitNewFileIfNecessary(const CompactionProcessorRef &processor, FileProcHolder holder) const
{
    if (processor->mFileBuilder == nullptr) {
        FileInfoRef fileInfo = mFileCacheManager->AllocateFile(mFileDirectory, FileName::CreateFileName);
        RETURN_ALLOC_FAIL_AS_NULLPTR(fileInfo);
        processor->mCurrentFileId = fileInfo->GetFileId()->Get();
        processor->mCurrentFileName = fileInfo->GetFilePath()->Name();
        processor->mCurrentFileSeqId = GeneratedFileSeqId(processor->mCompaction->GetLevelInputs(),
                                                          processor->mCompaction->GetOutputLevelInputs());
        processor->mFileBuilder = mFileCache->CreateBuilder(fileInfo->GetFilePath(),
                                                            processor->mCompaction->GetOutputLevelId(), holder);
        LOG_DEBUG("Compaction processor create file builder success, fileStoreSeqId:" << mSeqId << ".");
    }
    return BSS_OK;
}

BResult LsmStore::FinishCompactionOutputFile(const CompactionProcessorRef &processor) const
{
    if (processor->mFileBuilder != nullptr) {
        FileBlockMetaRef fileBlockMeta;
        auto ret = processor->mFileBuilder->Finish(fileBlockMeta);
        RETURN_NOT_OK_NO_LOG(ret);
        RETURN_ERROR_AS_NULLPTR(fileBlockMeta->GetStartKey());
        FullKeyRef smallest = FullKeyUtil::CopyInternalKey(fileBlockMeta->GetStartKey(), mMemManager,
                                                           FileProcHolder::FILE_STORE_COMPACTION);
        RETURN_ERROR_AS_NULLPTR(fileBlockMeta->GetEndKey());
        FullKeyRef largest = FullKeyUtil::CopyInternalKey(fileBlockMeta->GetEndKey(), mMemManager,
                                                          FileProcHolder::FILE_STORE_COMPACTION);
        if (UNLIKELY(smallest == nullptr || largest == nullptr)) {
            mFileCacheManager->DiscardFile(FileAddressUtil::GetFileAddressWithZeroOffset(processor->mCurrentFileId));
            return BSS_ALLOC_FAIL;
        }
        FileMetaData::BuilderRef builder = FileMetaData::NewBuilder();
        builder->Fill(smallest, largest, fileBlockMeta->GetFileSize(),
                      FileAddressUtil::GetFileAddressWithZeroOffset(processor->mCurrentFileId),
                      static_cast<int64_t>(processor->mCurrentFileSeqId), mGroupRange, mOrderRange,
                      processor->mCurrentFileName, fileBlockMeta->GetStateIdInterval(), FileStatus::LOCAL,
                      fileBlockMeta->GetRecordMeta());
        FileMetaDataRef fileMetaData = builder->Build();
        RETURN_ERROR_AS_NULLPTR(fileMetaData);
        mFileCache->FinishBuilder(processor->mCurrentFileId, fileMetaData);
        processor->mOutputs.emplace_back(fileMetaData);
        processor->mFileBuilder = nullptr;
        LOG_DEBUG("Finish compaction output file success, fileStoreSeqId:"
                  << mSeqId << ", filename: " << PathTransform::ExtractFileName(processor->mCurrentFileName)
                  << ", smallestKey:" << smallest->ToString() << ", largestKey:" << largest->ToString()
                  << ", fileAddress:" << fileMetaData->GetFileAddress() << ", fileId:" << fileMetaData->GetFileId()
                  << ", fileSize:" << fileMetaData->GetFileSize());
    }
    return BSS_OK;
}

void LsmStore::FinalizeVersionEdit(const CompactionProcessorRef &processor, std::vector<FileMetaDataRef> &outputs) const
{
    VersionEdit::Builder *editBuilder = processor->mCompaction->GetEditBuilder();
    for (auto &lvlInput : processor->mCompaction->GetLevelInputs()) {
        editBuilder->DeleteFile(lvlInput->GetGroupRange(), processor->mCompaction->GetInputLevelId(),
                                lvlInput->GetIdentifier());
    }

    for (auto &outputLvlInput : processor->mCompaction->GetOutputLevelInputs()) {
        editBuilder->DeleteFile(outputLvlInput->GetGroupRange(), processor->mCompaction->GetOutputLevelId(),
                                outputLvlInput->GetIdentifier());
    }
    processor->mOutputSize.clear();
    for (auto &output : processor->mOutputs) {
        if (!output->GetGroupRange()->Equals(mGroupRange)) {
            LOG_ERROR("New File should be current group range.");
        }
        processor->mOutputSize.emplace_back(output->GetFileSize());
        editBuilder->AddFile(mGroupRange, processor->mCompaction->GetOutputLevelId(), output);
    }
    for (const auto &adoptedFile : processor->mCompaction->GetAdoptedFiles()) {
        if (!adoptedFile->GetGroupRange()->Equals(mGroupRange)) {
            LOG_ERROR("Adopted file should be current group range.");
        }
        editBuilder->AddFile(adoptedFile->GetGroupRange(), processor->mCompaction->GetOutputLevelId(), adoptedFile);
    }

    outputs.swap(processor->mOutputs);
}

BResult LsmStore::AddEntry(const CompactionProcessorRef &compactionProcessor, const KeyValueRef &keyValue,
                           FileProcHolder holder) const
{
    if (compactionProcessor->mFileBuilder != nullptr && compactionProcessor->mFileBuilder->IsStateChange(keyValue)) {
        RETURN_NOT_OK_NO_LOG(FinishCompactionOutputFile(compactionProcessor));
    }
    BResult ret = InitNewFileIfNecessary(compactionProcessor, holder);
    RETURN_NOT_OK_NO_LOG(ret);
    ret = compactionProcessor->mFileBuilder->Add(keyValue);
    RETURN_NOT_OK_NO_LOG(ret);

    // 文件达到目标大小时完成输出；大 ListState APPEND 继续单独分段，避免合并链占用过多内存。
    auto currentSize = compactionProcessor->mFileBuilder->CurrentEstimateSize();
    if (UNLIKELY(currentSize == UINT32_MAX)) {
        LOG_WARN("Compact failed, output file is too large.");
        return BSS_INNER_ERR;
    }
    if (currentSize > mConf->GetFileBaseSize() ||
        (StateId::IsList(keyValue->key.StateId()) && keyValue->value.ValueType() == ValueType::APPEND &&
         keyValue->value.ValueLen() > IO_SIZE_4M)) {
        return FinishCompactionOutputFile(compactionProcessor);
    }
    return BSS_OK;
}

bool LsmStore::CheckCompactionCompleted() const
{
    return (mBackgroundCompactions.load(std::memory_order_seq_cst) == 0);
}

void LsmStore::ScheduleLsmStoreCompaction(const RunnablePtr &task) const
{
    if (mConf->GetLsmStoreCompactionSwitch() == 0) {  // compaction功能关闭则直接返回.
        LOG_LIMIT_INFO("Lsm store compaction switch is off, no need to perform a compaction.");
        return;
    }
    if (UNLIKELY(mCompactionExecutor == nullptr)) {
        return;
    }

    // 如果当前正在执行compaction则直接返回.
    if (mBackgroundCompactions.load(std::memory_order_seq_cst) > 0) {
        return;
    }

    if (!mVersionSet->NeedCompaction()) {
        LOG_DEBUG("Not need schedule compaction, because compaction score not enough, score:"
                  << mVersionSet->GetCompactionScore() << ".");
        return;
    }

    // 后台执行compaction.
    if (!mCompactionExecutor->Execute(task)) {
        LOG_ERROR("Execute compaction task failed, waiting for the next trigger.");
    }
}

void LsmStore::Compaction()
{
    mBackgroundCompactions.fetch_add(1, std::memory_order_seq_cst);
    Compaction::Result result = BackgroundCompaction();
    mBackgroundCompactions.fetch_sub(1, std::memory_order_seq_cst);
    if (result == Compaction::Result::ABORT) {  // 中断compaction任务，等待下次触发.
        return;
    }
    if (result != Compaction::Result::NON_COMPACTION) {
        std::lock_guard<std::mutex> lock(mMutex);
        RunnablePtr compactionTask = std::make_shared<LsmStoreCompactionTask>(shared_from_this());
        ScheduleLsmStoreCompaction(compactionTask);
    }
}

Compaction::Result LsmStore::BackgroundCompaction()
{
    if (mCompactionAbort.load() || !mVersionSet->NeedCompaction()) {
        return Compaction::Result::NON_COMPACTION;
    }

    bool concurrentCompaction = false;
    bool isTrivialMove = false;
    CompactionRef compaction = nullptr;
    // 从versionSet中选择需要做compaction的version时需要加锁互斥.
    {
        std::lock_guard<std::mutex> lock(mMutex);
        if (UNLIKELY(mBackgroundCompactions.load(std::memory_order_seq_cst) > 1)) {  // 判断是否存在并发压缩操作执行
            LOG_WARN("Exists compaction task is running, need exit background compaction.");
            concurrentCompaction = true;
        } else {
            // 1. 选择compaction对象.
            if ((compaction = mVersionSet->PickCompaction(mGroupRange)) == nullptr) {
                return Compaction::Result::NON_COMPACTION;
            }

            // 2. 根据选择的compaction的groupRange判断是否为简单移动.
            if ((isTrivialMove =
                     compaction->IsTrivialMove())) {  // 移动流程则只将文件移动到下一层，不需要做具体的压缩操作.
                FileMetaDataRef fileMetaData = compaction->GetLevelInputs().at(0);
                VersionEditRef versionEdit = compaction->GetEditBuilder()
                                                 ->DeleteFile(mGroupRange, compaction->GetInputLevelId(),
                                                              fileMetaData->GetIdentifier())
                                                 ->AddFile(mGroupRange, compaction->GetOutputLevelId(), fileMetaData)
                                                 ->Build();
                VersionInnerBuilderRef innerVersionBuilder =
                    VersionInnerBuilder::NewVersionInnerBuilder(mVersionSet.get(), mVersionSet->GetCurrent(),
                                                                mVersionSet->GetGroupRange(),
                                                                mVersionSet->GetOrderRange());
                if (innerVersionBuilder != nullptr) {
                    innerVersionBuilder->Apply(versionEdit);
                    VersionPtr newVersion = innerVersionBuilder->Build();
                    AppendNewVersion(newVersion);
                    innerVersionBuilder->InternalRelease();
                }
            } else {
                if (mZeroCopySwitch && mTombstoneService == nullptr && !mConf->GetEnableKVSeparate()) {
                    auto adoptableFilter = [this](uint16_t stateId, uint64_t seqId) {
                        return mStateFilterManager == nullptr || !mStateFilterManager->StateFilter(stateId, seqId);
                    };
                    ListStateFileReusePlanner::Build(compaction, adoptableFilter);
                }
                mFileCacheManager->RegisterFilesForCompaction(compaction->GetLevelInputs());
                mFileCacheManager->RegisterFilesForCompaction(compaction->GetOutputLevelInputs());
            }
        }
    }

    // 如果compaction对象存在且为简单移动则关闭compaction.
    if (compaction != nullptr && isTrivialMove) {
        CloseCompactionWithMutex(compaction);
    }

    if (concurrentCompaction) {  // 如果存在并发compaction则返回中止状态.
        LOG_WARN("Concurrent compactions exist, avoid current compaction.");
        return Compaction::Result::CONCURRENT_ABORT;
    }

    // 3.1. 如果为简单移动，则更新文件存储的简单移动状态，并返回简单移动状态. 进行统计
    if (isTrivialMove) {
        LOG_INFO("Background compaction is trivial move, need update file store state.");
        return Compaction::Result::TRIVIAL_MOVE;
    }

    // 3.2. 执行compaction.
    CompactionProcessorRef processor = std::make_shared<CompactionProcessor>(compaction);
    auto ret = DoCompaction(processor);
    Compaction::Result retStatus = (ret == BSS_OK) ? Compaction::Result::NORMAL : Compaction::Result::ABORT;
    if (ret == BSS_OK) {
        CountCompaction(processor);
    }
    if (mTombstoneService != nullptr) {
        mTombstoneService->Commit(ret == BSS_OK);
    }
    // 清理并关闭compaction.
    CleanupCompaction(processor);
    CloseCompactionWithMutex(compaction);
    return retStatus;
}

void LsmStore::CloseCompactionWithMutex(const CompactionRef &compaction)
{
    // 清理旧的version需要加锁互斥.
    std::unique_lock<std::mutex> locker(mMutex);
    CleanOldVersion(compaction->Dispose());
}

void LsmStore::CleanupCompaction(const CompactionProcessorRef &processor) const
{
    if (processor->mFileBuilder != nullptr) {
        processor->mFileBuilder = nullptr;
    }
    mFileCacheManager->ReleaseFilesForCompaction(processor->mCompaction->GetOutputLevelInputs());
    mFileCacheManager->ReleaseFilesForCompaction(processor->mCompaction->GetLevelInputs());
    for (auto &output : processor->mOutputs) {
        mFileCacheManager->DiscardFile(output->GetFileAddress());
    }
}

BResult LsmStore::DoCompaction(const CompactionProcessorRef &processor)
{
    if (UNLIKELY(mCompactionAbort.load())) {
        return BSS_ALREADY_DONE;
    }

    FileProcHolder holder = FileProcHolder::FILE_STORE_COMPACTION;  // 标记该流程是Compaction流程
    // 1. 获取Merge iterator.
    MergingIteratorRef iterator;
    {
        std::lock_guard<std::mutex> lock(mMutex);  // 从versionSet中获取merge迭代器时需要加锁互斥.
        iterator = mVersionSet->MakeInputIterator(processor->mCompaction, mMemManager, holder, mTombstoneService);
        RETURN_ERROR_AS_NULLPTR(iterator);
    }

    const std::vector<FileMetaDataRef> &adoptedFiles = processor->mCompaction->GetAdoptedFiles();
    size_t adoptedFileIndex = 0;
    // 2. 遍历迭代器添加KVPair.
    while (iterator->HasNext()) {
        if (UNLIKELY(mCompactionAbort.load())) {
            return BSS_ALREADY_DONE;
        }

        auto current = iterator->Next();
        RETURN_ALLOC_FAIL_AS_NULLPTR(current);

        bool isAdoptedKey = false;
        while (adoptedFileIndex < adoptedFiles.size()) {
            int32_t cmp = adoptedFiles[adoptedFileIndex]->GetSmallest()->CompareKey(current->key);
            if (cmp > 0) {
                break;
            }
            RETURN_NOT_OK_NO_LOG(FinishCompactionOutputFile(processor));
            if (cmp == 0) {
                isAdoptedKey = true;
                break;
            }
            ++adoptedFileIndex;
        }
        if (isAdoptedKey) {
            continue;
        }

        if (UNLIKELY(processor->mCompaction->ShouldStopBefore(current))) {
            LOG_INFO("Should stop do compaction, fileStoreSeqId:" << mSeqId << ".");
            RETURN_NOT_OK_NO_LOG(FinishCompactionOutputFile(processor));
        }

        // 如果当前键值对的值类型为删除并且底层也不存在，则跳过该键值对.
        if ((current->value.ValueType() == ValueType::DELETE ||
             mStateFilterManager->StateFilter(current->key.StateId(), current->value.SeqId())) &&
            processor->mCompaction->KeyNotExistBeyondOutputLevels(current->key)) {
            continue;
        }

        RETURN_NOT_OK_NO_LOG(AddEntry(processor, current, FileProcHolder::FILE_STORE_COMPACTION));
    }
    RETURN_NOT_OK_NO_LOG(FinishCompactionOutputFile(processor));  // 输出compactions的文件.
    // 3. 创建新的version并替换versionSet中的current version.
    std::vector<FileMetaDataRef> outputs;
    FinalizeVersionEdit(processor, outputs);
    std::lock_guard<std::mutex> lock(mMutex);  // 生成新的version并加入到versionSet中需要加锁互斥.
    VersionInnerBuilderRef innerVersionBuilder =
        VersionInnerBuilder::NewVersionInnerBuilder(mVersionSet.get(), mVersionSet->GetCurrent(),
                                                    mVersionSet->GetGroupRange(), mVersionSet->GetOrderRange());
    innerVersionBuilder->Apply(processor->mCompaction->GetEditBuilder()->Build());
    VersionPtr newVersion = innerVersionBuilder->Build();
    AppendNewVersion(newVersion);
    mFileCacheManager->ConfirmAllocationOnFlushOrCompaction(outputs);
    innerVersionBuilder->InternalRelease();
    iterator->Close();

    LOG_INFO("Compaction success, fileStoreIdSeqId:" << mSeqId << ", versionSeqId:" << newVersion->GetVersionSeqId()
                                                     << ", fileStore:" << mFileStoreID->ToString()
                                                     << ", outLevel:" << processor->mCompaction->GetOutputLevelId());
    return BSS_OK;
}

BResult LsmStore::BuildLsmStoreFlushFile(const IteratorRef<std::vector<DataSliceRef>> &dataSliceVectorIterator,
                                         FileMetaDataRef &fileMetaData, bool &flag)
{
    FileInfoRef fileInfo = mFileCacheManager->AllocateFile(mFileDirectory, FileName::CreateFileName);
    RETURN_ERROR_AS_NULLPTR(fileInfo);
    FileProcHolder holder = FileProcHolder::FILE_STORE_FLUSH;  // 标记该流程是Flush写流程
    auto fileBuilder = mFileCache->CreateBuilder(fileInfo->GetFilePath(), 0, holder);
    SliceKVIterator iterator(dataSliceVectorIterator, mMemManager, mStateFilterManager->GetKeyGroupUtil(),
                             mStateFilterManager->GetStartKeyGroup(), mStateFilterManager->GetEndKeyGroup(),
                             mTombstoneService);
    Iterator_Result result;
    uint32_t keyCount = 0;
    while ((result = iterator.HasNext()) == Iterator_Result_Continue) {
        auto keyValue = iterator.Next();
        if (mStateFilterManager->StateFilter(keyValue->key.StateId(), keyValue->value.SeqId())) {
            continue;
        }
        auto ret = fileBuilder->Add(keyValue);
        if (UNLIKELY(ret != BSS_OK)) {
            mFileCacheManager->DiscardFile(FileAddressUtil::GetFileAddressWithZeroOffset(fileInfo->GetFileId()->Get()));
            return BSS_INNER_ERR;
        }
        keyCount++;
        flag = true;
    }
    if (UNLIKELY(result == Iterator_Result_Failed)) {
        mFileCacheManager->DiscardFile(FileAddressUtil::GetFileAddressWithZeroOffset(fileInfo->GetFileId()->Get()));
        return BSS_INNER_ERR;
    }

    if (!flag) {
        LOG_WARN("All data expires and is skipped for current flush.");
        mFileCacheManager->DiscardFile(FileAddressUtil::GetFileAddressWithZeroOffset(fileInfo->GetFileId()->Get()));
        return BSS_OK;
    }

    FileBlockMetaRef fileMeta;
    auto retVal = fileBuilder->Finish(fileMeta);
    if (UNLIKELY(retVal != BSS_OK)) {
        mFileCacheManager->DiscardFile(FileAddressUtil::GetFileAddressWithZeroOffset(fileInfo->GetFileId()->Get()));
        return BSS_INNER_ERR;
    }

    // create file cache by file metaData.
    uint64_t fileSeqId = GetNextSeqNumber();
    FullKeyRef smallest = FullKeyUtil::CopyInternalKey(fileMeta->GetStartKey(), mMemManager, holder);
    FullKeyRef largest = FullKeyUtil::CopyInternalKey(fileMeta->GetEndKey(), mMemManager, holder);
    if (UNLIKELY(smallest == nullptr || largest == nullptr)) {
        mFileCacheManager->DiscardFile(FileAddressUtil::GetFileAddressWithZeroOffset(fileInfo->GetFileId()->Get()));
        return BSS_ALLOC_FAIL;
    }
    fileMetaData =
        std::make_shared<FileMetaData>(FileAddressUtil::GetFileAddressWithZeroOffset(fileInfo->GetFileId()->Get()),
                                       fileSeqId, static_cast<uint64_t>(fileMeta->GetFileSize()), smallest, largest,
                                       mGroupRange, mOrderRange, fileInfo->GetFilePath()->Name(),
                                       fileMeta->GetStateIdInterval(), FileStatus::LOCAL, fileMeta->GetRecordMeta());
    mFileCache->FinishBuilder(fileInfo->GetFileId()->Get(), fileMetaData);
    UpdateLevelFileMetric(fileMetaData->GetFileSize(), 0, true);
    return BSS_OK;
}

// 判断targetKey是否在lsm level文件中不存在.
bool LsmStore::KeyNotExistBeyondOutputLevels(const Key &key, const VersionPtr &currentVersion, uint32_t numLevels,
                                             std::vector<int32_t> &levelPointers) const
{
    for (uint32_t level = 0; level < numLevels; level++) {
        if (UNLIKELY(level >= currentVersion->GetLevels().size())) {
            LOG_ERROR("Invalid level form currentVersion, levelId:" << level << ".");
            return false;
        }

        auto fileMetaDataGroups = currentVersion->GetLevels().at(level).GetFileMetaDataGroups();
        auto isFindKey = FindKeyInFileMetaDataGroups(key, level, levelPointers, fileMetaDataGroups);
        if (isFindKey) {
            return false;
        }
    }
    return true;
}

bool LsmStore::FindKeyInFileMetaDataGroups(const Key &key, uint32_t level, std::vector<int32_t> &levelPointers,
                                           const std::vector<FileMetaDataGroupRef> &fileMetaDataGroups) const
{
    for (auto &fileMetaDataGroup : fileMetaDataGroups) {
        if (UNLIKELY(!fileMetaDataGroup->GetGroupRange()->Equals(mGroupRange))) {
            continue;
        }

        std::vector<FileMetaDataRef> files = fileMetaDataGroup->GetFiles();
        if (level == 0) {
            if (FindKeyInLevel0(key, files)) {
                return true;
            }
        } else {
            if (FindKeyInOtherLevels(key, level, levelPointers, files)) {
                return true;
            }
        }
    }
    return false;
}

bool LsmStore::FindKeyInLevel0(const Key &key, const std::vector<FileMetaDataRef> &files) const
{
    for (const auto &fileMetaData : files) {
        CONTINUE_LOOP_AS_NULLPTR(fileMetaData);
        auto cmpLargest = fileMetaData->GetLargest()->CompareKey(key);
        if (cmpLargest >= 0 && fileMetaData->GetSmallest()->CompareKey(key) <= 0) {
            return true;
        }
    }
    return false;
}

bool LsmStore::FindKeyInOtherLevels(const Key &key, uint32_t level, std::vector<int32_t> &levelPointers,
                                    const std::vector<FileMetaDataRef> &files) const
{
    auto fileSize = static_cast<int32_t>(files.size());
    while (levelPointers[level] < fileSize) {
        const FileMetaDataRef &fileMetaData = files.at(levelPointers[level]);
        CONTINUE_LOOP_AS_NULLPTR(fileMetaData);
        auto cmpLargest = fileMetaData->GetLargest()->CompareKey(key);
        if (cmpLargest >= 0 && fileMetaData->GetSmallest()->CompareKey(key) <= 0) {
            return true;
        }

        if (cmpLargest > 0) {
            break;
        }
        levelPointers[level] = levelPointers[level] + 1;
    }
    return false;
}

BResult LsmStore::BuildLsmStoreFlushFile(const PQTableIteratorRef &iter, FileMetaDataRef &fileMetaData, bool &flag)
{
    FileInfoRef fileInfo = mFileCacheManager->AllocateFile(mFileDirectory, FileName::CreateFileName);
    RETURN_ERROR_AS_NULLPTR(fileInfo);
    FileProcHolder holder = FileProcHolder::FILE_STORE_FLUSH;  // 标记该流程是Flush写流程
    auto fileBuilder = mFileCache->CreateBuilder(fileInfo->GetFilePath(), 0, holder);
    auto currentVersion = mVersionSet->GetCurrent();
    auto numLevels = mConf->GetFileStoreNumLevels();
    std::vector<int32_t> levelPointers;
    levelPointers.resize(numLevels);
    while (iter->HasNext()) {
        // 如果当前键值对的值类型为删除并且底层也不存在，则跳过该键值对.
        auto keyValue = iter->Next();
        CONTINUE_LOOP_AS_NULLPTR(keyValue);
        if ((keyValue->value.ValueType() == ValueType::DELETE) &&
            KeyNotExistBeyondOutputLevels(keyValue->key, currentVersion, numLevels, levelPointers)) {
            continue;
        }
        auto ret = fileBuilder->Add(keyValue);
        if (UNLIKELY(ret != BSS_OK)) {
            mFileCacheManager->DiscardFile(FileAddressUtil::GetFileAddressWithZeroOffset(fileInfo->GetFileId()->Get()));
            return BSS_INNER_ERR;
        }
        flag = true;
    }

    if (!flag) {
        LOG_WARN("All data type is delete, lower level does not have the same key, skipped for current flush.");
        mFileCacheManager->DiscardFile(FileAddressUtil::GetFileAddressWithZeroOffset(fileInfo->GetFileId()->Get()));
        return BSS_OK;
    }

    FileBlockMetaRef fileMeta;
    auto retVal = fileBuilder->Finish(fileMeta);
    if (UNLIKELY(retVal != BSS_OK)) {
        mFileCacheManager->DiscardFile(FileAddressUtil::GetFileAddressWithZeroOffset(fileInfo->GetFileId()->Get()));
        return BSS_INNER_ERR;
    }

    // create file cache by file meta data.
    uint64_t fileSeqId = GetNextSeqNumber();
    FullKeyRef smallest = FullKeyUtil::CopyInternalKey(fileMeta->GetStartKey(), mMemManager, holder);
    FullKeyRef largest = FullKeyUtil::CopyInternalKey(fileMeta->GetEndKey(), mMemManager, holder);
    if (UNLIKELY(smallest == nullptr || largest == nullptr)) {
        mFileCacheManager->DiscardFile(FileAddressUtil::GetFileAddressWithZeroOffset(fileInfo->GetFileId()->Get()));
        return BSS_ALLOC_FAIL;
    }
    fileMetaData =
        std::make_shared<FileMetaData>(FileAddressUtil::GetFileAddressWithZeroOffset(fileInfo->GetFileId()->Get()),
                                       fileSeqId, static_cast<uint64_t>(fileMeta->GetFileSize()), smallest, largest,
                                       mGroupRange, mOrderRange, fileInfo->GetFilePath()->Name(),
                                       fileMeta->GetStateIdInterval(), FileStatus::LOCAL, fileMeta->GetRecordMeta());
    mFileCache->FinishBuilder(fileInfo->GetFileId()->Get(), fileMetaData);
    return BSS_OK;
}

BResult LsmStore::Put(const PQTableIteratorRef &iterator)
{
    // create file builder.
    FileMetaDataRef fileMetaData = nullptr;
    bool flag = false;
    auto result = BuildLsmStoreFlushFile(iterator, fileMetaData, flag);
    RETURN_NOT_OK_NO_LOG(result);
    // 当前淘汰的数据全为Delete，且下层没有相同重复key，无需继续淘汰
    if (!flag) {
        return BSS_OK;
    }
    CreateVersion(fileMetaData);
    return BSS_OK;
}

BResult LsmStore::Put(const IteratorRef<std::vector<DataSliceRef>> &dataSliceVectorIterator)
{
    // create file builder.
    FileMetaDataRef fileMetaData = nullptr;
    bool flag = false;
    auto result = BuildLsmStoreFlushFile(dataSliceVectorIterator, fileMetaData, flag);
    if (mTombstoneService != nullptr) {
        mTombstoneService->Commit(result == BSS_OK);
    }
    RETURN_NOT_OK_NO_LOG(result);
    // 当前淘汰的数据已全部过期，无需继续淘汰
    if (!flag) {
        return BSS_OK;
    }
    CreateVersion(fileMetaData);
    return BSS_OK;
}

void LsmStore::CreateVersion(const FileMetaDataRef &fileMetaData)
{  // create version and add it to version set.
    VersionEdit::Builder editBuilder = VersionEdit::Builder();
    editBuilder.AddFile(mGroupRange, 0, fileMetaData);
    VersionEditRef edit = editBuilder.Build();
    std::lock_guard<std::mutex> lock(mMutex);  // 向versionSet添加version时需要加锁互斥.
    VersionInnerBuilderRef innerVersionBuilder =
        VersionInnerBuilder::NewVersionInnerBuilder(mVersionSet.get(), mVersionSet->GetCurrent(),
                                                    mVersionSet->GetGroupRange(), mVersionSet->GetOrderRange());
    innerVersionBuilder->Apply(edit);
    VersionPtr newVersion = innerVersionBuilder->Build();
    AppendNewVersion(newVersion);
    std::vector<FileMetaDataRef> vec;
    vec.emplace_back(fileMetaData);
    mFileCacheManager->ConfirmAllocationOnFlushOrCompaction(vec);

    // trigger compaction.
    RunnablePtr compactionTask = std::make_shared<LsmStoreCompactionTask>(shared_from_this());
    ScheduleLsmStoreCompaction(compactionTask);
    // version build release.
    innerVersionBuilder->InternalRelease();
    LOG_INFO("Flush file store success, fileStoreSeqId:"
             << mSeqId << ", versionId:" << newVersion->GetVersionSeqId()
             << ", fileName:" << PathTransform::ExtractFileName(fileMetaData->GetIdentifier()) << ", smallestKey:"
             << fileMetaData->GetSmallest()->ToString() << ", largestKey:" << fileMetaData->GetLargest()->ToString()
             << ", fileAddress:" << fileMetaData->GetFileAddress() << ", fileId:" << fileMetaData->GetFileId()
             << ", fileSize:" << fileMetaData->GetFileSize());
}

uint64_t LsmStore::GetNextSeqNumber()
{
    return mNextFileSeqNumber.fetch_add(1);
}

void LsmStore::AppendNewVersion(const VersionPtr &version)
{
    VersionPtr oldVersion = mVersionSet->AppendVersion(version);
    CleanOldVersion(oldVersion);  // 直接清除旧的version.
}

void LsmStore::CleanOldVersion(const VersionPtr &oldVersion)
{
    if (!mGroupAndOrderRangeAligned && mVersionSet->GetActiveVersions().size() == 1 &&
        mVersionSet->GetCurrent()->IsVersionAligned()) {
        mGroupAndOrderRangeAligned = true;
    }

    if (oldVersion == nullptr) {
        return;
    }

    std::vector<FileMetaDataRef> liveFiles = mVersionSet->GetLiveFileMetaDatas();
    std::unordered_set<uint64_t> primaryFileAddress;
    for (uint32_t idx = 0; idx < liveFiles.size(); idx++) {
        auto fileAddr = liveFiles[idx]->GetFileAddress();
        primaryFileAddress.insert(fileAddr);
    }

    std::unordered_map<uint64_t, std::vector<FileMetaDataRef>> toRemoveFiles;
    for (auto &fileMetaData : oldVersion->GetFileMetaDatas()) {
        uint64_t address = fileMetaData->GetFileAddress();
        auto iter = primaryFileAddress.find(address);
        if (iter != primaryFileAddress.end()) {
            continue;
        }

        if (toRemoveFiles.find(address) == toRemoveFiles.end()) {
            toRemoveFiles.insert({ address, std::vector<FileMetaDataRef>() });
        }
        if (!toRemoveFiles[address].empty() &&
            FileAddressUtil::GetFileId(fileMetaData->GetFileAddress()) !=
                FileAddressUtil::GetFileId((toRemoveFiles[address].at(0))->GetFileAddress())) {
            LOG_ERROR("Prepare to clean file metas who are using the same file, but with different file ids.");
            return;
        }
        toRemoveFiles[address].push_back(fileMetaData);
    }

    auto iter = toRemoveFiles.begin();
    while (iter != toRemoveFiles.end()) {
        uint64_t fileAddress = iter->first;
        mFileCache->RemoveFile(fileAddress);
        mFileCacheManager->DiscardFile(fileAddress);
        iter++;
    }
}

BResult LsmStore::Get(const Key &key, std::deque<Value> &values, SectionsReadMetaRef &sectionsReadMeta, bool flag)
{
    CompositeValue value;
    BResult ret = InternalGet(key, value, sectionsReadMeta, flag);
    if (LIKELY(ret == BSS_OK && !value.IsEmpty())) {
        value.GetValues(values);
        return BSS_OK;
    }
    return BSS_NOT_EXISTS;
}

BResult LsmStore::InternalGet(const Key &key, Value &value)
{
    // 1. 获取当前的version.
    VersionPtr current = GetCurrentVersion();

    // 2. 计算当前key的keyGroup.
    uint32_t keyGroup = static_cast<uint32_t>(mStateFilterManager->GetGroup(key.KeyHashCode(), key.StateId()));
    std::vector<FileMetaDataRef> filesForKey;

    // 3. 在当前version中逐层遍历.
    for (auto &level : current->GetLevels()) {
        filesForKey.clear();
        level.GetFilesForKey(key, static_cast<int32_t>(keyGroup), !current->IsVersionAligned(), filesForKey);
        for (auto &fileMetaData : filesForKey) {
            if (UNLIKELY(mFileCache->Get(fileMetaData->GetFileAddress(), key, value) != BSS_OK)) {
                continue;
            }
            LOG_DEBUG("File get success, keyHashCode:" << key.KeyHashCode() << ", valueLen:" << value.ValueLen()
                                                       << ", valueType:" << static_cast<uint32_t>(value.ValueType()));
            AddLevelHitCount(level.GetLevelId());
            AddHitCount();
            if (UNLIKELY(value.ValueType() == DELETE || mStateFilterManager->Filter(key, value))) {
                value.Init(DELETE, 0, nullptr, value.SeqId(), nullptr);
                ReleaseVersionFinally(current);
                return BSS_NOT_EXISTS;
            } else {
                ReleaseVersionFinally(current);
                return BSS_OK;
            }
        }
        AddLevelHitMissCount(level.GetLevelId());
    }
    AddHitMissCount();
    ReleaseVersionFinally(current);
    return BSS_NOT_EXISTS;
}

BResult LsmStore::InternalGet(const Key &key, CompositeValue &value, SectionsReadMetaRef &sectionsReadMeta, bool flag)
{
    // 1. 获取当前的version.
    VersionPtr current = GetCurrentVersion();

    // 2. 计算当前key的keyGroup.
    auto keyGroup = static_cast<uint32_t>(mStateFilterManager->GetGroup(key.KeyHashCode(), key.StateId()));

    // 3. 在当前version中逐层遍历.
    std::vector<FileMetaDataRef> filesForKey;
    Value result;
    bool find = false;
    bool startPartRead = flag;
    for (auto &level : current->GetLevels()) {
        filesForKey.clear();
        level.GetFilesForKey(key, static_cast<int32_t>(keyGroup), !current->IsVersionAligned(), filesForKey);
        bool levelFind = false;
        for (auto &fileMetaData : filesForKey) {
            if (startPartRead) {
                sectionsReadMeta->mFilesForKey.emplace_back(fileMetaData);
                continue;
            }
            if (UNLIKELY(mFileCache->Get(fileMetaData->GetFileAddress(), key, result) != BSS_OK)) {
                continue;
            }
            find = true;
            levelFind = true;
            if (StateId::IsList(key.StateId()) && result.ValueLen() > IO_SIZE_4M) {
                startPartRead = true;
            }
            if (LIKELY(result.ValueType() != DELETE && !mStateFilterManager->Filter(key, result))) {
                value.AddValue(result);
                if (result.ValueType() == APPEND) {
                    continue;
                }
                AddHitCount();
                AddLevelHitCount(level.GetLevelId());
                ReleaseVersionFinally(current);
                return BSS_OK;
            } else {
                ReleaseVersionFinally(current);
                return value.IsEmpty() ? BSS_NOT_EXISTS : BSS_OK;
            }
        }
        if (!levelFind) {
            AddLevelHitMissCount(level.GetLevelId());
        }
    }
    if (startPartRead && !sectionsReadMeta->mFilesForKey.empty()) {
        sectionsReadMeta->mCurrent = current;
        return BSS_OK;
    }

    ReleaseVersionFinally(current);
    if (find) {
        AddHitCount();
        return BSS_OK;
    }
    AddHitMissCount();
    return BSS_NOT_EXISTS;
}

BResult LsmStore::GetByReadSection(uint64_t fileAddress, Key &key, Value &value)
{
    auto ret = mFileCache->Get(fileAddress, key, value);
    if (UNLIKELY(ret != BSS_OK)) {
        return ret;
    }
    if (mStateFilterManager->Filter(key, value)) {
        return BSS_NOT_EXISTS;
    }
    return BSS_OK;
}

FileFactoryRef LsmStore::GetFileFactory()
{
    return mFileCache->GetFileFactory();
}

KeyValueIteratorRef LsmStore::CreateMergingIterator(
    VersionPtr &version, InputSortedRun::FileIteratorWriterRef fileIteratorBuilder,
    std::function<std::vector<FileMetaDataRef>(Level level)> filesGetter, bool reverseOrder, bool sectionRead,
    FileProcHolder holder)
{
    std::vector<KeyValueIteratorRef> iterators;

    for (const Level &level : version->GetLevels()) {
        std::vector<FileMetaDataRef> filesForKey = filesGetter(level);
        if (level.GetLevelId() == 0 && !filesForKey.empty()) {
            for (auto &fileMetaData : filesForKey) {
                iterators.emplace_back(InputSortedRun::BuildInputSortedRunIterator(fileMetaData, fileIteratorBuilder));
            }
            continue;
        }
        std::vector<InputSortedRunRef> inputSortedRunList =
            InputSortedRun::BuildInputSortedRun(filesForKey, reverseOrder ?
                                                                 mVersionSet->GetFileMetaDataReverseComparator() :
                                                                 mVersionSet->GetFileMetaDataComparator());
        for (auto &inputSortedRun : inputSortedRunList) {
            iterators.emplace_back(inputSortedRun->GetIterator(fileIteratorBuilder));
        }
    }

    if (iterators.empty()) {
        ReleaseVersionFinally(version);
        return nullptr;
    }

    auto cleaner = [this](VersionPtr &versionPtr) { this->ReleaseVersionFinally(versionPtr); };
    auto result = std::make_shared<MergingIterator>(iterators, mMemManager, cleaner, version, reverseOrder, holder,
                                                    sectionRead, nullptr);
    ReleaseVersionFinally(version);
    return result;
}

void LsmStore::ReleaseVersionFinally(VersionPtr &version)
{
    if (UNLIKELY(version == nullptr)) {
        return;
    }
    // 释放version需要加锁互斥.
    std::lock_guard<std::mutex> lock(mMutex);
    CleanOldVersion(version->Release());
}

void LsmStore::Close()
{
    mFileCacheManager->CleanResource();
    mCompactionAbort.store(true);
    ExitCompaction();
}

SnapshotMetaRef LsmStore::WriteMeta(const FileOutputViewRef &localFileOutputView, FileOutputViewRef &remoteOutView,
                                    uint64_t snapshotId)
{
    if (UNLIKELY(localFileOutputView == nullptr)) {
        LOG_ERROR("Local file output view is nullptr");
        return nullptr;
    }
    // fileStore的元数据内容: versionMeta.
    VersionPtr version = GetVersionForSnapshot(snapshotId);
    RETURN_NULLPTR_AS_NULLPTR(version);
    // 1. 序列化version meta.
    auto localBufferOutputView = std::make_shared<OutputView>(mMemManager, FileProcHolder::FILE_STORE_SNAPSHOT);
    auto snapshotMeta = VersionMetaSerializer::Serialize(version, mFileFactory, localBufferOutputView,
                                                         GetCurrentFileSeqNumber());
    if (UNLIKELY(snapshotMeta == nullptr)) {
        LOG_ERROR("Serialize version meta failed.");
        return nullptr;
    }

    // 2. 将序列化的versionMeta写入文件.
    auto sizeToCheck = localFileOutputView->Size();
    if (UNLIKELY(sizeToCheck > INT64_MAX)) {
        LOG_ERROR("Local file output view size cannot static cast.");
        return nullptr;
    }
    auto ret = localFileOutputView->WriteBuffer(localBufferOutputView->Data(), static_cast<int64_t>(sizeToCheck),
                                                localBufferOutputView->GetOffset());
    if (UNLIKELY(ret != BSS_OK)) {
        LOG_ERROR("Write serialize version meta to snapshot file failed, ret:" << ret);
        return nullptr;
    }
    return snapshotMeta;
}

BResult LsmStore::RestoreData()
{
    if (mRestoredFileMapping.empty()) {
        return BSS_OK;
    }

    std::unordered_map<std::string, std::pair<uint64_t, uint32_t>> relocateFileMapping;
    auto ret = mFileCacheManager->RestoreFiles(relocateFileMapping, mRestoredFileMapping, mFileDirectory,
                                               mRescaledOnRestored);
    if (UNLIKELY(ret != BSS_OK)) {
        LOG_ERROR("Restore files failed, ret:" << ret);
        return ret;
    }
    VersionPtr current = mVersionSet->GetCurrent();
    mFileCache->Restore(current);
    mRestoredFileMapping.clear();
    LOG_DEBUG("Restored file store from previous local snapshot success.");
    return BSS_OK;
}

BResult LsmStore::RestoreVersionInfo(const std::vector<std::pair<FileInputViewRef, uint64_t>> &metaList,
                                     const std::unordered_map<std::string, std::string> &lazyPathMapping,
                                     std::unordered_map<std::string, uint32_t> &restorePathFileIdMap,
                                     bool isLazyDownload)
{
    std::vector<VersionPtr> restoredVersions;
    std::map<std::string, RestoreFileInfo> restoredFileMappings;
    bool groupRangeChanged = false;
    int64_t epoch = 0;
    uint64_t fileSeqNumber = 0;

    for (const auto &pair : metaList) {
        FileInputViewRef inputView = pair.first;
        if (UNLIKELY(inputView == nullptr)) {
            LOG_ERROR("Restore version failed, inputView is null.");
            return BSS_ERR;
        }
        inputView->Seek((pair.second));
        // 读取version并反序列化.
        VersionPtr version = nullptr;
        uint64_t fileSeqId = 0;
        std::unordered_map<std::string, RestoreFileInfo> fileMapping;
        auto basePath = mFileCacheManager->GetLocalFileManager()->GetBasePath();
        VersionMetaSerializer::Deserialize(inputView, mVersionSet, mOrderRange, version, fileSeqId, fileMapping,
                                           GetFileFactory()->GetFileFactoryType(), mMemManager, lazyPathMapping,
                                           restorePathFileIdMap, basePath, isLazyDownload);
        if (UNLIKELY(version == nullptr)) {
            LOG_ERROR("Deserialize version failed, orderRange: " << mOrderRange->ToString());
            return BSS_ERR;
        }

        groupRangeChanged = !mGroupRange->RangeEquals(version->GetGroupRange());
        epoch = std::max(epoch, version->GetGroupRange()->GetEpoch());
        restoredVersions.push_back(version);
        fileSeqNumber = std::max(fileSeqNumber, fileSeqId);
        restoredFileMappings.insert(fileMapping.begin(), fileMapping.end());
    }

    mRescaledOnRestored = (groupRangeChanged || metaList.size() > 1);
    if (mRescaledOnRestored) {
        epoch++;
    }
    if (epoch != 0L) {
        mGroupRange->SetEpoch(epoch);
        mVersionSet->ClearInitVersion();
    }

    auto restoredVersion = VersionSet::RestoreVersions(mVersionSet.get(), restoredVersions, mGroupRange, mOrderRange);
    if (UNLIKELY(restoredVersion == nullptr)) {
        LOG_ERROR("Restore version failed, groupRange:" << mGroupRange->ToString()
                                                        << ", orderRange:" << mOrderRange->ToString());
        return BSS_ERR;
    }
    LOG_INFO("restore version:" << restoredVersion->LevelSummary());
    SetRestoredFileSeqNumber(fileSeqNumber + 1);
    mVersionSet->AppendVersion(restoredVersion);

    uint64_t totalSize = 0;
    std::unordered_set<std::string> restoredFileIdentifiers;
    std::vector<FileMetaDataRef> restoredFileMetas = restoredVersion->GetFileMetaDatas();
    for (const auto &fileMeta : restoredFileMetas) {
        restoredFileIdentifiers.emplace(fileMeta->GetIdentifier());
        totalSize += fileMeta->GetFileSize();
    }
    auto it = restoredFileMappings.begin();
    while (it != restoredFileMappings.end()) {
        if (restoredFileIdentifiers.count(it->first) < NO_1) {
            it = restoredFileMappings.erase(it);  // 返回删除元素后下一个元素的迭代器
        } else {
            ++it;  // 递增迭代器以检查下一个元素
        }
    }
    mRestoredFileMapping.insert(restoredFileMappings.begin(), restoredFileMappings.end());

    // restore期间不做compaction.
    mBackgroundCompactions.fetch_add(1, std::memory_order_seq_cst);
    LOG_DEBUG("FileStore restore version info success, fileDataTotalSize:" << totalSize << ", version:"
                                                                           << restoredVersion->ToString());
    return BSS_OK;
}

void LsmStore::ReleaseSnapshot(uint64_t snapshotId)
{
    if (mSnapshotVersions.find(snapshotId) == mSnapshotVersions.end()) {
        LOG_INFO("Version not found, snapshotId:" << snapshotId);
        return;
    }
    auto version = mSnapshotVersions.at(snapshotId);
    auto delCnt = this->mSnapshotVersions.erase(snapshotId);
    if (delCnt > 0) {
        LOG_DEBUG("File store release snapshot success.");
        ReleaseVersionFinally(version);
    }
}

KeyValueIteratorRef LsmStore::PrefixIterator(const Key &prefixKey, bool reverseOrder)

{
    VersionPtr current = GetCurrentVersion();
    uint32_t keyGroup =
        static_cast<uint32_t>(mStateFilterManager->GetGroup(prefixKey.KeyHashCode(), prefixKey.StateId()));
    auto buildFunc = [this, current, prefixKey,
                      reverseOrder](const FileMetaDataRef &fileMetaData) -> KeyValueIteratorRef {
        FullKeyFilterRef keyFilter = current->IsVersionAligned() ?
                                         std::static_pointer_cast<FullKeyFilter>(mStateFilterManager) :
                                         std::make_shared<FileMetaStateFilter>(fileMetaData->GetGroupRange(),
                                                                               fileMetaData->GetOrderRange(),
                                                                               mStateFilterManager);
        return mFileCache->PrefixIterator(keyFilter, fileMetaData->GetFileAddress(), prefixKey, reverseOrder,
                                          FileProcHolder::FILE_STORE_ITERATOR);
    };
    auto fileIteratorBuilder = InputSortedRun::FileIteratorWriter::Of(buildFunc);

    auto filesGetter = [prefixKey, keyGroup, current](Level level) -> std::vector<FileMetaDataRef> {
        return level.GetFilesContainingPrefixKey(prefixKey, keyGroup, !current->IsVersionAligned());
    };
    return CreateMergingIterator(current, fileIteratorBuilder, filesGetter, reverseOrder);
}

bool LsmStore::MigrateFileMetas(
    std::unordered_map<std::string, RestoreFileInfo> &toMigrateFileMapping,
    std::unordered_map<std::string, std::tuple<uint64_t, uint32_t>> &migratedFileMapping,
    std::function<BResult(std::unordered_map<std::string, std::tuple<uint64_t, uint32_t>>)> consumer)
{
    if (toMigrateFileMapping.empty()) {
        return true;
    }
    std::unordered_map<std::string, std::tuple<uint64_t, uint32_t>> tempMapping;
    VersionPtr current;
    {
        std::lock_guard<std::mutex> lk(mMutex);
        current = mVersionSet->GetCurrent();
    }
    LOG_DEBUG("Migrate file metas start, current version:" << current->LevelSummary());
    VersionPtr newVersion = Version::MigrateVersion(current, toMigrateFileMapping, tempMapping);
    if (newVersion == nullptr) {
        LOG_ERROR("Migrate version failed.");
        return false;
    }
    LOG_DEBUG("Migrate file metas finish, new version:" << newVersion->LevelSummary());
    if (newVersion != current) {
        AppendNewVersion(newVersion);
        auto ret = consumer(tempMapping);
        if (ret != BSS_OK) {
            LOG_ERROR("Execute consumer failed, ret: " << ret << ".");
            return false;
        }
        mFileCache->Restore(newVersion);
        CleanOldVersion(current);
        migratedFileMapping.insert(tempMapping.begin(), tempMapping.end());
    }
    LOG_INFO("Migrate file metas success.");
    return true;
}

}  // namespace bss
}  // namespace ock
