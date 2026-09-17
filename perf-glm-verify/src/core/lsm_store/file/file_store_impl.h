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

#ifndef BSS_DEV_FILE_STORE_IMPL_H
#define BSS_DEV_FILE_STORE_IMPL_H

#include <atomic>
#include <memory>
#include <stack>

#include "binary/query_binary.h"
#include "bss_metric.h"
#include "common/util/iterator.h"
#include "compaction/compaction.h"
#include "executor/executor_service.h"
#include "file_cache.h"
#include "file_cache_manager.h"
#include "file_meta_state_filter.h"
#include "file_name.h"
#include "file_store_id.h"
#include "file_writer.h"
#include "include/bss_err.h"
#include "include/bss_types.h"
#include "include/config.h"
#include "lsm_store/key/full_key_filter.h"
#include "memory/skiplist.h"
#include "order_range.h"
#include "sections_read_meta.h"
#include "snapshot/abstract_snapshot_operator.h"
#include "tombstone/tombstone_service.h"
#include "transform/pq_iterator.h"
#include "version/version_set.h"

namespace ock {
namespace bss {
class CompactionProcessor {
public:
    explicit CompactionProcessor(const CompactionRef &compaction) : mCompaction(compaction)
    {
    }

    ~CompactionProcessor()
    {
        std::vector<FileMetaDataRef>().swap(mOutputs);
    }

public:
    CompactionRef mCompaction = nullptr;
    std::vector<FileMetaDataRef> mOutputs;
    std::vector<uint64_t> mOutputSize;
    FileWriterRef mFileBuilder = nullptr;
    uint32_t mCurrentFileId = 0;
    std::string mCurrentFileName;
    uint64_t mCurrentFileSeqId = 0;
};
using CompactionProcessorRef = std::shared_ptr<CompactionProcessor>;

class LsmStore;
using LsmStoreRef = std::shared_ptr<LsmStore>;
class LsmStore : public FileHolder, public std::enable_shared_from_this<LsmStore> {
public:
    ~LsmStore() override = default;
    LsmStore(const FileStoreIDRef &fileStoreId, const ConfigRef &config, const FileFactoryRef &fileFactory,
             const FileCacheManagerRef &fileCache, const StateFilterManagerRef &stateFilterManager,
             const MemManagerRef &memManager)
        : mFileStoreID(fileStoreId),
          mConf(config),
          mZeroCopySwitch(config != nullptr && config->GetZeroCopySwitch()),
          mFileCacheManager(fileCache),
          mFileFactory(fileFactory),
          mStateFilterManager(stateFilterManager),
          mMemManager(memManager)
    {
        mSeqId = TimeStampUtil::GetCurrentTime();
    }

    BResult Initialize();

    inline void ExitCompaction()
    {
        if (mCompactionExecutor != nullptr) {
            mCompactionExecutor->Stop();
            mCompactionExecutor = nullptr;
        }
    }

    inline bool IsGroupAndOrderRangeAligned() const
    {
        return mGroupAndOrderRangeAligned;
    }

    inline void SetRestoredFileSeqNumber(uint64_t seqNumber)
    {
        mNextFileSeqNumber.store(seqNumber);
    }

    inline uint64_t GetCurrentFileSeqNumber() const
    {
        return mNextFileSeqNumber.load(std::memory_order_relaxed);
    }

    inline FileStoreIDRef GetFileStoreId()
    {
        return mFileStoreID;
    }

    inline void RegisterMetric(BoostNativeMetricPtr metricPtr)
    {
        mFileCache->RegisterMetric(metricPtr);
        mFileFactory->RegisterMetric(metricPtr);
        mBoostNativeMetric = metricPtr;
    }

    KeyValueIteratorRef PrefixIterator(const Key &prefixKey, bool reverseOrder);

    KeyValueIteratorRef IteratorForSavepoint(VersionPtr &version, bool isPQ)
    {
        version->Retain();

        auto filesGetter = [isPQ](Level level) -> std::vector<FileMetaDataRef> {
            auto groups = level.GetFileMetaDataGroups();
            std::vector<FileMetaDataRef> result;
            for (auto &group : groups) {
                for (auto &fileMetaData : group->GetFiles()) {
                    if (fileMetaData == nullptr || fileMetaData->GetSmallest() == nullptr) {
                        continue;
                    }
                    if (isPQ == fileMetaData->GetSmallest()->IsPqKey()) {
                        result.push_back(fileMetaData);
                    }
                }
            }
            return result;
        };

        auto buildFunc = [this](const FileMetaDataRef &fileMetaData) -> KeyValueIteratorRef {
            FullKeyFilterRef keyFilter = IsGroupAndOrderRangeAligned() ?
                                             std::static_pointer_cast<FullKeyFilter>(mStateFilterManager) :
                                             std::make_shared<FileMetaStateFilter>(fileMetaData->GetGroupRange(),
                                                                                   fileMetaData->GetOrderRange(),
                                                                                   mStateFilterManager);
            return mFileCache->IteratorAll(keyFilter, fileMetaData->GetFileAddress(), false,
                                           FileProcHolder::FILE_STORE_SAVEPOINT);
        };

        auto fileIteratorBuilder = InputSortedRun::FileIteratorWriter::Of(buildFunc);
        return CreateMergingIterator(version, fileIteratorBuilder, filesGetter, false, true,
                                     FileProcHolder::FILE_STORE_SAVEPOINT);
    }

    KeyValueIteratorRef Iterator(uint16_t stateId)
    {
        FileProcHolder holder = FileProcHolder::FILE_STORE_ITERATOR;
        VersionPtr current = CurrentVersion();
        auto filesGetter = [stateId](Level level) -> std::vector<FileMetaDataRef> {
            return level.GetFilesContainingStateId(stateId);
        };
        auto buildFunc = [this, holder](const FileMetaDataRef &fileMetaData) -> KeyValueIteratorRef {
            FullKeyFilterRef keyFilter = IsGroupAndOrderRangeAligned() ?
                                             std::static_pointer_cast<FullKeyFilter>(mStateFilterManager) :
                                             std::make_shared<FileMetaStateFilter>(fileMetaData->GetGroupRange(),
                                                                                   fileMetaData->GetOrderRange(),
                                                                                   mStateFilterManager);
            return mFileCache->IteratorAll(keyFilter, fileMetaData->GetFileAddress(), holder);
        };
        auto fileIteratorBuilder = InputSortedRun::FileIteratorWriter::Of(buildFunc);
        return CreateMergingIterator(current, fileIteratorBuilder, filesGetter, false);
    }

    KeyValueIteratorRef IteratorForPQ(uint32_t stateId, const BinaryData &data)

    {
        VersionPtr current = GetCurrentVersion();
        auto buildFunc = [this, stateId, data](const FileMetaDataRef &fileMetaData) -> KeyValueIteratorRef {
            FileProcHolder holder = FileProcHolder::FILE_STORE_ITERATOR;
            FullKeyFilterRef keyFilter = IsGroupAndOrderRangeAligned() ?
                                             std::static_pointer_cast<FullKeyFilter>(mStateFilterManager) :
                                             std::make_shared<FileMetaStateFilter>(fileMetaData->GetGroupRange(),
                                                                                   fileMetaData->GetOrderRange(),
                                                                                   mStateFilterManager);
            Key prefixKey = QueryKey(stateId, HashCode::Hash(data.Data(), data.Length()), data);
            return mFileCache->PrefixIterator(keyFilter, fileMetaData->GetFileAddress(), prefixKey, false, holder);
        };
        auto fileIteratorBuilder = InputSortedRun::FileIteratorWriter::Of(buildFunc);

        auto filesGetter = [stateId, current](Level level) -> std::vector<FileMetaDataRef> {
            return level.GetFilesContainingStateId(stateId);
        };
        return CreateMergingIterator(current, fileIteratorBuilder, filesGetter, false);
    }

    BResult Put(const IteratorRef<std::vector<DataSliceRef>> &dataSliceVectorIterator);

    BResult Put(const PQTableIteratorRef &iterator);

    BResult Get(const Key &key, std::deque<Value> &values, SectionsReadMetaRef &sectionsReadMeta, bool flag);

    inline bool Get(const Key &key, Value &value)
    {
        return (InternalGet(key, value) == BSS_OK);
    }

    SnapshotMetaRef WriteMeta(const FileOutputViewRef &localFileOutputView, FileOutputViewRef &remoteOutView,
                              uint64_t snapshotId);

    BResult RestoreData();

    BResult RestoreVersionInfo(const std::vector<std::pair<FileInputViewRef, uint64_t>> &metaList,
                               const std::unordered_map<std::string, std::string> &lazyPathMapping,
                               std::unordered_map<std::string, uint32_t> &restorePathFileIdMap, bool isLazyDownload);

    void ReleaseSnapshot(uint64_t snapshotId);

    class LsmStoreCompactionTask : public Runnable {
    public:
        explicit LsmStoreCompactionTask(const LsmStoreRef &lsmStore) : mLsmStore(lsmStore)
        {
        }
        void Run() override
        {
            if (LIKELY(mLsmStore != nullptr)) {
                mLsmStore->Compaction();
            }
        }

    private:
        LsmStoreRef mLsmStore = nullptr;
    };

    inline VersionSetRef GetVersionSet() const
    {
        return mVersionSet;
    }

    FileFactoryRef GetFileFactory();

    inline void StartLsmCompaction()
    {
        std::lock_guard<std::mutex> lock(mMutex);
        RunnablePtr compactionTask = std::make_shared<LsmStoreCompactionTask>(shared_from_this());
        ScheduleLsmStoreCompaction(compactionTask);
    }

    void Close();

    inline VersionPtr CurrentVersion()
    {
        std::lock_guard<std::mutex> lock(mMutex);
        VersionPtr version = mVersionSet->GetCurrent();
        version->Retain();
        return version;
    }

    bool CheckCompactionCompleted() const;

    inline VersionPtr GetVersionForSnapshot(uint64_t snapshotId)
    {
        return mSnapshotVersions[snapshotId];
    }

    std::unordered_map<std::string, std::pair<uint64_t, uint64_t>> ExecuteSnapshot(uint64_t snapshotId)
    {
        if (UNLIKELY(mSnapshotVersions.count(snapshotId) > 0)) {
            LOG_ERROR("Current version of snapshot:" << snapshotId << "should exist.");
            return {};
        }

        // 1. 保存当前version.
        VersionPtr current = GetCurrentVersion();
        mSnapshotVersions.emplace(snapshotId, current);
        LOG_DEBUG("FileStore version, checkpointId:" << snapshotId << ", version info:" << current->ToString());
        if (mTombstoneService != nullptr) {
            mTombstoneService->TriggerSnapshot(snapshotId);
        }
        // 2. 查找当前version下的所有文件信息.
        auto fileMetaDatas = current->GetFileMetaDatas();
        std::unordered_map<std::string, std::pair<uint64_t, uint64_t>> result(fileMetaDatas.size());
        for (auto const &fileMetaData : fileMetaDatas) {
            auto identifier = fileMetaData->GetIdentifier();
            if (result.find(identifier) == result.end()) {
                result.emplace(identifier, std::make_pair(fileMetaData->GetFileAddress(), fileMetaData->GetFileSize()));
            }
        }

        return result;
    }

    bool MigrateFileMetas(
        std::unordered_map<std::string, RestoreFileInfo> &toMigrateFileMapping,
        std::unordered_map<std::string, std::tuple<uint64_t, uint32_t>> &migratedFileMapping,
        std::function<BResult(std::unordered_map<std::string, std::tuple<uint64_t, uint32_t>>)> consumer) override;

    inline std::string GetName() override
    {
        return "FileStore:" + GetFileStoreId()->ToString();
    };

    /**
     * For testing, force clean current version.
     */
    inline void CleanVersion()
    {
        VersionPtr version = mVersionSet->GetCurrent();
        mVersionSet->ClearInitVersion();
        CleanOldVersion(version);
    }

    BResult GetByReadSection(uint64_t fileAddress, Key &key, Value &value);

    void ReleaseVersionFinally(VersionPtr &version);

    void RegisterTombstoneService(TombstoneServiceRef &tombstoneService)
    {
        mTombstoneService = tombstoneService;
        LOG_INFO("Register tombstone service success");
    }

private:
    BResult InitializeCompaction();

    inline VersionPtr GetCurrentVersion()
    {
        std::lock_guard<std::mutex> lock(mMutex);
        VersionPtr current = mVersionSet->GetCurrent();
        current->Retain();
        return current;
    }

    BResult InternalGet(const Key &key, Value &value);
    BResult InternalGet(const Key &key, CompositeValue &value, SectionsReadMetaRef &sectionsReadMeta, bool flag);

    BResult InitNewFileIfNecessary(const CompactionProcessorRef &processor, FileProcHolder holder) const;
    BResult FinishCompactionOutputFile(const CompactionProcessorRef &processor) const;
    void FinalizeVersionEdit(const CompactionProcessorRef &processor, std::vector<FileMetaDataRef> &outputs) const;
    BResult AddEntry(const CompactionProcessorRef &processor, const KeyValueRef &keyValue, FileProcHolder holder) const;

    void CloseCompactionWithMutex(const CompactionRef &compaction);
    void CleanupCompaction(const CompactionProcessorRef &processor) const;
    BResult DoCompaction(const CompactionProcessorRef &processor);
    Compaction::Result BackgroundCompaction();
    void Compaction();
    void ScheduleLsmStoreCompaction(const RunnablePtr &task) const;

    uint64_t GetNextSeqNumber();
    void AppendNewVersion(const VersionPtr &version);
    void CleanOldVersion(const VersionPtr &oldVersion);

    KeyValueIteratorRef CreateMergingIterator(VersionPtr &version,
                                              InputSortedRun::FileIteratorWriterRef fileIteratorBuilder,
                                              std::function<std::vector<FileMetaDataRef>(Level level)> filesGetter,
                                              bool reverseOrder, bool sectionRead = false,
                                              FileProcHolder holder = FileProcHolder::FILE_STORE_ITERATOR);

    BResult BuildLsmStoreFlushFile(const IteratorRef<std::vector<DataSliceRef>> &dataSliceVectorIterator,
                                   FileMetaDataRef &fileMetaData, bool &flag);

    BResult BuildLsmStoreFlushFile(const PQTableIteratorRef &iter, FileMetaDataRef &fileMetaData, bool &flag);

    bool KeyNotExistBeyondOutputLevels(const Key &key, const VersionPtr &currentVersion, uint32_t numLevels,
                                       std::vector<int32_t> &levelPointers) const;

    bool FindKeyInFileMetaDataGroups(const Key &key, uint32_t level, std::vector<int32_t> &levelPointers,
                                     const std::vector<FileMetaDataGroupRef> &fileMetaDataGroups) const;

    bool FindKeyInLevel0(const Key &key, const std::vector<FileMetaDataRef> &files) const;

    bool FindKeyInOtherLevels(const Key &key, uint32_t level, std::vector<int32_t> &levelPointers,
                              const std::vector<FileMetaDataRef> &files) const;

    inline void NotifyStop() override
    {
        mBackgroundCompactions.store(0, std::memory_order_seq_cst);
    }

    void CountFlushSuccess(uint64_t fileSize) const
    {
        if (mBoostNativeMetric != nullptr && mBoostNativeMetric->IsFileStoreMetricEnabled()) {
            mBoostNativeMetric->AddLsmFlushCount();
        }
    }

    inline void AddCompactionCount() const
    {
        if (mBoostNativeMetric != nullptr && mBoostNativeMetric->IsFileStoreMetricEnabled()) {
            mBoostNativeMetric->AddLsmCompactionCount();
        }
    }

    inline void AddHitCount() const
    {
        if (mBoostNativeMetric != nullptr && mBoostNativeMetric->IsFileStoreMetricEnabled()) {
            mBoostNativeMetric->AddLsmHitCount();
        }
    }

    inline void AddHitMissCount() const
    {
        if (mBoostNativeMetric != nullptr && mBoostNativeMetric->IsFileStoreMetricEnabled()) {
            mBoostNativeMetric->AddLsmMissCount();
        }
    }

    inline void AddLevelHitCount(int32_t levelId) const
    {
        if (mBoostNativeMetric != nullptr && mBoostNativeMetric->IsFileStoreMetricEnabled()) {
            mBoostNativeMetric->AddLevelHitCount(levelId);
        }
    }

    inline void AddLevelHitMissCount(int32_t levelId) const
    {
        if (mBoostNativeMetric != nullptr && mBoostNativeMetric->IsFileStoreMetricEnabled()) {
            mBoostNativeMetric->AddLevelMissHitCount(levelId);
        }
    }

    uint64_t GetLevelFileSize(uint32_t levelId) const
    {
        auto level = mVersionSet->GetCurrent()->GetLevel(levelId);
        return level.GetTotalFileSize();
    }

    void CountCompaction(CompactionProcessorRef &processor)
    {
        if (!(mBoostNativeMetric != nullptr && mBoostNativeMetric->IsFileStoreMetricEnabled())) {
            return;
        }
        auto compaction = processor->mCompaction;
        auto inputLevelId = compaction->GetInputLevelId();
        auto outputLevelId = compaction->GetOutputLevelId();
        for (const auto &size : processor->mOutputSize) {
            mBoostNativeMetric->UpdateLevelFileMetric(size, outputLevelId, true);
            mBoostNativeMetric->AddLsmCompactionWriteSize(size, outputLevelId);
        }
        for (const auto &item : compaction->GetLevelInputs()) {
            CONTINUE_LOOP_AS_NULLPTR(item);
            mBoostNativeMetric->UpdateLevelFileMetric(item->GetFileSize(), inputLevelId, false);
        }
        for (const auto &item : compaction->GetOutputLevelInputs()) {
            CONTINUE_LOOP_AS_NULLPTR(item);
            mBoostNativeMetric->UpdateLevelFileMetric(item->GetFileSize(), outputLevelId, false);
        }
        for (const auto &item : compaction->GetMergeLevelInputs()) {
            CONTINUE_LOOP_AS_NULLPTR(item);
            mBoostNativeMetric->AddLsmCompactionReadSize(item->GetFileSize(), inputLevelId);
        }
        for (const auto &item : compaction->GetMergeOutputLevelInputs()) {
            CONTINUE_LOOP_AS_NULLPTR(item);
            mBoostNativeMetric->AddLsmCompactionReadSize(item->GetFileSize(), outputLevelId);
        }
        for (const auto &item : compaction->GetAdoptedFiles()) {
            CONTINUE_LOOP_AS_NULLPTR(item);
            mBoostNativeMetric->UpdateLevelFileMetric(item->GetFileSize(), outputLevelId, true);
        }
        mBoostNativeMetric->AddLsmCompactionCount();
    }

    inline void UpdateLevelFileMetric(uint64_t fileSize, uint32_t levelId, bool increment) const
    {
        if (mBoostNativeMetric != nullptr && mBoostNativeMetric->IsFileStoreMetricEnabled()) {
            mBoostNativeMetric->UpdateLevelFileMetric(fileSize, levelId, increment);
        }
    }

    void CreateVersion(const FileMetaDataRef &fileMetaData);

    static inline void DivideFileMetas(std::vector<FileMetaDataRef> &toDivide,
                                       std::vector<FileMetaDataRef> &pqFileMetas,
                                       std::vector<FileMetaDataRef> &kvFileMetas)
    {
        for (FileMetaDataRef &fileMeta : toDivide) {
            if (fileMeta->GetSmallest()->IsPqKey()) {
                pqFileMetas.emplace_back(fileMeta);
            } else {
                kvFileMetas.emplace_back(fileMeta);
            }
        }
    }

private:
    FileStoreIDRef mFileStoreID = nullptr;
    ConfigRef mConf = nullptr;
    bool mZeroCopySwitch = false;
    HashCodeOrderRangeRef mOrderRange = nullptr;
    std::atomic<int32_t> mBackgroundCompactions{ 0 };
    std::atomic<bool> mCompactionAbort = { false };
    GroupRangeRef mGroupRange = nullptr;
    bool mRescaledOnRestored = false;
    FileCacheRef mFileCache = nullptr;
    VersionSetRef mVersionSet = nullptr;
    std::mutex mMutex;
    ExecutorServiceRef mCompactionExecutor = nullptr;
    FileCacheManagerRef mFileCacheManager = nullptr;
    FileDirectoryRef mFileDirectory = nullptr;
    FileFactoryRef mFileFactory = nullptr;
    std::atomic<uint64_t> mNextFileSeqNumber{ NO_1 };
    volatile bool mGroupAndOrderRangeAligned = false;
    StateFilterManagerRef mStateFilterManager = nullptr;
    std::unordered_map<uint64_t, VersionPtr> mSnapshotVersions;
    MemManagerRef mMemManager = nullptr;
    uint32_t mSeqId = 0;
    std::unordered_map<std::string, RestoreFileInfo> mRestoredFileMapping;
    BoostNativeMetricPtr mBoostNativeMetric = nullptr;
    TombstoneServiceRef mTombstoneService = nullptr;
};

}  // namespace bss
}  // namespace ock
#endif  // BSS_DEV_FILE_STORE_IMPL_H
