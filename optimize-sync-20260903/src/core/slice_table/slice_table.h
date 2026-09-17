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

#ifndef BOOST_SS_SLICETABLEMANAGER_H
#define BOOST_SS_SLICETABLEMANAGER_H

#include <cstdint>
#include <memory>
#include <stack>
#include <vector>

#include "access_recorder.h"
#include "binary/fresh_binary.h"
#include "binary/key_value.h"
#include "blob_store/blob_store.h"
#include "blob_store_snapshot_operator.h"
#include "common/bss_metric.h"
#include "compaction/slice_compaction_trigger.h"
#include "compaction/slice_compactor.h"
#include "flushing_bucket_group.h"
#include "include/bss_err.h"
#include "include/config.h"
#include "memory/evict_manager.h"
#include "slice_table/bucket_group_manager.h"
#include "slice_table/slice/raw_data_slice.h"
#include "snapshot/file_store_snapshot_operator.h"
#include "snapshot/slice_table_snapshot.h"

namespace ock {
namespace bss {

class SliceTable : public std::enable_shared_from_this<SliceTable> {
public:
    ~SliceTable()
    {
        mBoostNativeMetric = nullptr;
        if (mNeedReleaseBlockCache) {
            RETURN_AS_NULLPTR(mConfig);
            BlockCacheManager::Instance()->DeleteBlockCache(mConfig->GetTaskSlotFlag());
        }
        LOG_INFO("Delete sliceTableManager object success.");
    }

    /**
     * Initialize slice table
     * @param config configuration
     * @param fileCache file cache.
     * @param memManager memory manager.
     * @return if success return BSS_OK, else return others.
     */
    BResult Initialize(const ConfigRef &config, const FileCacheManagerRef &fileCache, const MemManagerRef &memManager,
                       const StateFilterManagerRef &stateFilterManager);

    /**
     * Exist slice table.
     */
    void Exit();

    /**
     *  Open slice table.
     */
    BResult Open();

    /**
     * Close slice table.
     */
    void Close();

    /**
     * Try to evict slice to LMS file for current db.
     * @param addedSize already added to slice size.
     * @param isSync sync execute evict task
     * @param isForce force to execute evict
     * @param minSize evict file size
     * @return if successfully returns BSS_OK, otherwise returns other
     */
    BResult TryCurrentDBEvict(int64_t addedSize, bool isSync, bool isForce, uint32_t minSize = UINT32_MAX);

    /**
     * Try to evict slice to LMS file for current slot.
     * @param addedSize already added to slice size.
     * @param isSync sync execute evict task
     * @param isForce force to execute evict
     * @param minSize evict file size
     * @return if successfully returns BSS_OK, otherwise returns other
     */
    BResult TryCurrentSlotEvict(int64_t addedSize, bool isSync, bool isForce, uint32_t minSize = UINT32_MAX);

    /**
     * Try to compact one slice chain.
     * @param sliceIndexContext slice index context, include slice chain info.
     * @param compactCompletedNotify callback.
     */
    BResult TryCompact(const SliceIndexContextRef &sliceIndexContext,
                       const CompactCompletedNotify &compactCompletedNotify);

    /**
     * Obtain list by key.
     * @param key key
     * @param result BinaryValue list
     * @return if success return BSS_OK, else return others.
     */
    inline BResult GetList(const Key &key, std::deque<Value> &result, std::vector<SectionsReadContextRef> &readMetas)
    {
        return InternalGetList(key, result, readMetas);
    }

    /**
     * Obtain the value by using the keys from slice table.
     * @param Key key, it is SglKey or DualKey.
     * @param value value, nullptr if it is not exist.
     * @return Returns true if it exists or has already been deleted, false otherwise.
     */
    inline bool Get(const Key &key, Value &value)
    {
        // record access number.
        mAccessRecorder->Record();

        // select slice chain
        auto sliceChain = mSliceBucketIndex->GetLogicalSliceChain(key);
        if (UNLIKELY(sliceChain == nullptr)) {
            return false;
        }
        if (UNLIKELY(sliceChain->IsEmpty() && !sliceChain->HasFilePage())) {
            AddSliceMiss();
            AddSliceReadCount();
            return false;
        }

        AddSliceReadCount();

        // get value from slice chain
        auto self = shared_from_this();
        auto getFromBlobFunc = [self](uint64_t blobId, uint32_t keyHashCode, uint64_t seqId,
                                      Value &originalValue) -> BResult {
            return self->GetValueFromBlobStore(blobId, keyHashCode, seqId, originalValue);
        };
        auto ioRes = sliceChain->Get(key, value, getFromBlobFunc, mBoostNativeMetric);
        AddSliceListHitMiss(ioRes);
        return ioRes > 0 ? true : false;
    }

    inline uint64_t GetTick()
    {
        return mAccessRecorder->AccessCount();
    }

    inline int64_t GetMemHighMark()
    {
        return mEvictManager->GetMemHighMark();
    }

    inline void SetMemHighMark(int64_t highMark)
    {
        mEvictManager->SetMemHighMark(highMark);
    }

    /**
     * Add kv pairs to slice chain.
     * @param curSliceIndexContext the slice chain to be added
     * @param dataList the kv pairs to be added.
     * @param version fresh table version
     * @return the data size of the currently added slice.
     */
    int32_t AddSlice(const SliceIndexContextRef &curSliceIndexContext,
                     std::vector<std::pair<SliceKey, Value>> &dataList, uint64_t version);

    BResult AddSlice(const SliceIndexContextRef &curSliceIndexContext, RawDataSlice &rawDataSlice, uint32_t &addSize,
                     bool &forceEvict);

    BResult WriteValueToBlobStore(FreshValueNodePtr &curVal, uint32_t keyHash, uint16_t stateId, uint64_t &blobId);

    BResult GetValueFromBlobStore(uint64_t blobId, uint32_t keyGroup, uint64_t seqId, Value &value);

    /**
     * Iterator all, for savepoint, with kv-separate.
     * @param keyFilter key filter
     * @param stateId state id
     * @param blobValueTransformFunc kv-separate conversion to normal kv func
     * @return iterator of all kv.
     */
    KeyValueIteratorRef EntryIterator(const KeyFilter &keyFilter, uint16_t stateId,
                                      BlobValueTransformFunc &blobValueTransformFunc);

    /**
     * Get iterator for prefix, with kv-separate.
     * @param prefixKey prefix key.
     * @param blobValueTransformFunc kv-separate conversion to normal kv func
     * @return iterator of having the same prefix.
     */
    KeyValueIteratorRef PrefixIterator(const Key &prefixKey, BlobValueTransformFunc &blobValueTransformFunc);

    /**
     * Iterator all, for savepoint.
     * @param keyFilter key filter
     * @param stateId state id
     * @return iterator of all kv.
     */
    KeyValueIteratorRef EntryIterator(const KeyFilter &keyFilter, uint16_t stateId);

    /**
     * Get iterator for prefix.
     * @param prefixKey prefix key.
     * @return iterator of having the same prefix.
     */
    KeyValueIteratorRef PrefixIterator(const Key &prefixKey);

    /**
     * Obtain memory manager.
     * @return memory manager instance.
     */
    inline MemManagerRef GetMemManager()
    {
        return mMemManager;
    }

    /**
     * Obtain slice bucket index
     * @return slice bucket index instance.
     */
    inline SliceBucketIndexRef GetSliceBucketIndex()
    {
        return mSliceBucketIndex;
    }

    /**
     * Obtain bucket group manager
     * @return bucket group manager instance.
     */
    inline BucketGroupManagerRef GetBucketGroupManager()
    {
        return mBucketGroupManager;
    }

    inline StateFilterManagerRef GetStateFilterManager()
    {
        return mStateFilterManager;
    }

    inline const KeyGroupUtilRef &GetKeyGroupUtil() const
    {
        return mKeyGroupUtil;
    }
    inline uint32_t GetStartKeyGroup() const
    {
        return mConfig->GetStartGroup();
    }
    inline uint32_t GetEndKeyGroup() const
    {
        return mConfig->GetEndGroup();
    }

    inline FullSortEvictorRef GetFullSortEvictor()
    {
        return mEvictManager->GetEvictorHandle();
    }

    /**
     * For snapshot, prepare file store snapshot.
     * @param operatorId operator id.
     * @param snapshotId snapshot id.
     * @return return operator instance.
     */
    FileStoreSnapshotOperatorRef PrepareFileStoreSnapshot(uint64_t operatorId, uint64_t snapshotId);

    /**
     * For snapshot, prepare blob store snapshot.
     * @param operatorId operator id.
     * @param snapshotId snapshot id.
     * @return return operator instance.
     */
    BlobStoreSnapshotOperatorRef PrepareBlobStoreSnapshot(uint64_t operatorId, uint64_t snapshotId);

    /**
     * Add one slice table snapshot to slice table.
     * @param snapshotId snapshot id.
     * @param sliceTableSnapshot slice table snapshot.
     */
    void AddSliceTableSnapshot(uint64_t snapshotId, const SliceTableSnapshotRef &sliceTableSnapshot);

    /**
     * Judge snapshot exist.
     * @param snapshotId snapshot id.
     * @return return true if exist, else return false.
     */
    bool IsSnapshotIdExist(uint64_t snapshotId);

    /**
     * Delete one slice table snapshot.
     * @param snapshotId snapshot id to be deleted.
     */
    void EraseSnapshotId(uint64_t snapshotId);

    /**
     * For testing, force trigger evict slice table to lsm store.
     */
    inline void ForceTriggerEvict()
    {
        mEvictManager->ForceEvict();
    }

    /**
     * For testing, force trigger compact lsm store.
     */
    void ForceTriggerCompaction()
    {
        // todo: implement me.
    }

    /**
     * For testing, force clean current version.
     */
    void ForceCleanCurrentVersion()
    {
        mBucketGroupManager->ForceCleanCurrentVersion();
    }

    inline void AddSnapshotSyncTask(SnapshotSyncTaskRef &compactionTask, SnapshotSyncTaskRef &evictTask)
    {
        mSnapshotVersion.fetch_add(1, std::memory_order_release);
        mEvictManager->AddSnapshotSyncTask(compactionTask);
        mCompactManager->AddSnapshotSyncTask(evictTask);
    }

    inline uint32_t GetSnapshotVersion() const
    {
        return mSnapshotVersion.load(std::memory_order_acquire);
    }

    inline void SetCompactionStatus(const SliceTableCompaction &compactionStatus)
    {
        mIsCompaction.store(compactionStatus, std::memory_order_release);
    }

    BResult TryCleanUpEvictedSlices(const LogicalSliceChainRef &logicalSliceChain, uint32_t bucketIndex)
    {
        if (UNLIKELY(logicalSliceChain == nullptr)) {
            LOG_ERROR("Logic slice chain is nullptr.");
            return BSS_INNER_ERR;
        }
        mSliceBucketIndex->LockWrite(bucketIndex);
        if (UNLIKELY(logicalSliceChain != mSliceBucketIndex->GetLogicChainedSlice(bucketIndex))) {
            LOG_ERROR("Slice chain not matched.");
            mSliceBucketIndex->Unlock(bucketIndex);
            return BSS_INNER_ERR;
        }

        if (logicalSliceChain->GetSliceStatus() == SliceStatus::COMPACTING) {
            mSliceBucketIndex->Unlock(bucketIndex);
            return BSS_OK;
        }

        int32_t curIndex = 0;
        while (curIndex <= logicalSliceChain->GetSliceChainTailIndex()) {
            SliceAddressRef sliceAddress = logicalSliceChain->GetSliceAddress(curIndex);
            if (UNLIKELY(sliceAddress == nullptr)) {
                LOG_ERROR("sliceAddress is nullptr, index:" << curIndex);
                mSliceBucketIndex->Unlock(bucketIndex);
                return BSS_INNER_ERR;
            }
            if (!(sliceAddress->IsEvicted())) {
                break;
            }
            curIndex++;
        }
        if (curIndex < static_cast<int32_t>(NO_5)) {
            mSliceBucketIndex->Unlock(bucketIndex);
            return BSS_OK;
        }

        LogicalSliceChainRef newLogicalSliceChain = mSliceBucketIndex->CreateLogicalChainedSlice();
        if (UNLIKELY(newLogicalSliceChain == nullptr)) {
            LOG_ERROR("create new slice chain failed, it is null.");
            mSliceBucketIndex->Unlock(bucketIndex);
            return BSS_INNER_ERR;
        }
        for (int32_t i = curIndex; i < static_cast<int32_t>(logicalSliceChain->GetCurrentSliceChainLen()); i++) {
            newLogicalSliceChain->InsertSlice(logicalSliceChain->GetSliceAddress(i));
        }
        mBucketGroupManager->MarkLogicalSliceChainFlushed(newLogicalSliceChain,
                                                          mBucketGroupManager->GetBucketGroupVector()[0]);
        newLogicalSliceChain->SetBaseSliceIndex(logicalSliceChain->GetBaseSliceIndex() - curIndex);
        auto invalidSliceAddress = std::vector<SliceAddressRef>();
        mSliceBucketIndex->UpdateLogicalSliceChain(bucketIndex, logicalSliceChain, newLogicalSliceChain);
        mSliceBucketIndex->Unlock(bucketIndex);
        return BSS_OK;
    }

    inline void AddSliceUsedMemory(int64_t size) const
    {
        mEvictManager->AddSliceUsedMemory(size);
    }

    inline void AddSliceHit()
    {
        if (mBoostNativeMetric == nullptr || !mBoostNativeMetric->IsSliceMetricEnabled()) {
            return;
        }
        mBoostNativeMetric->AddSliceHitCount();
    }

    inline void AddSliceMiss()
    {
        if (mBoostNativeMetric == nullptr || !mBoostNativeMetric->IsSliceMetricEnabled()) {
            return;
        }
        mBoostNativeMetric->AddSliceMissCount();
    }

    inline void AddSliceListHitMiss(std::stack<Value> &mergingValues, bool &isFound)
    {
        if (mBoostNativeMetric == nullptr || !mBoostNativeMetric->IsSliceMetricEnabled()) {
            return;
        }
        if (mergingValues.empty() && !isFound) {
            mBoostNativeMetric->AddSliceMissCount();
        } else {
            mBoostNativeMetric->AddSliceHitCount();
        }
    }

    inline void AddSliceListHitMiss(IOResult &res)
    {
        if (mBoostNativeMetric == nullptr || !mBoostNativeMetric->IsSliceMetricEnabled()) {
            return;
        }
        if (res == SLICE_FOUND || res == SLICE_FOUND_DELETE) {
            mBoostNativeMetric->AddSliceHitCount();
            return;
        }
        if (res == NOT_FOUND || res == LSM_FOUND) {
            mBoostNativeMetric->AddSliceMissCount();
        }
    }

    inline void AddSliceReadCount()
    {
        if (mBoostNativeMetric == nullptr || !mBoostNativeMetric->IsSliceMetricEnabled()) {
            return;
        }
        mBoostNativeMetric->AddSliceReadCount();
    }

    inline void AddSliceReadMetric(uint64_t size)
    {
        if (mBoostNativeMetric == nullptr || !mBoostNativeMetric->IsSliceMetricEnabled()) {
            return;
        }
        mBoostNativeMetric->AddSliceReadSize(size);
    }

    void RegisterSliceMetric(BoostNativeMetricPtr metricPtr)
    {
        mBoostNativeMetric = metricPtr;
        mBucketGroupManager->RegisterMetric(metricPtr);
        mEvictManager->RegisterEvictMetric(metricPtr);
        mCompactManager->RegisterSliceCompactionMetric(metricPtr);
        if (mBoostNativeMetric != nullptr && mBoostNativeMetric->IsSliceMetricEnabled()) {
            mBoostNativeMetric->SetSliceChainAvgSize(
                [this]() -> uint64_t { return mSliceBucketIndex->GetAvgSliceChainSize(); });
            mBoostNativeMetric->SetSliceAvgSize([this]() -> uint64_t { return mSliceBucketIndex->GetAvgSliceSize(); });
        }
    }

    inline bool NeedKvSeparate(uint32_t valueSize) const
    {
        return mIsKVSeparate && valueSize > mKvThreshold;
    }

    BResult RestoreBlobStore(const std::vector<SliceTableRestoreMetaRef> &metaList,
                             std::unordered_map<std::string, uint32_t> &restorePathFileIdMap);

    TombstoneServiceRef GetTombstoneService()
    {
        return mTombstoneService;
    }

    void StopSliceTable()
    {
        mIsRunning.store(false);
    }

    bool IsRunning()
    {
        return mIsRunning;
    }

private:
    static uint32_t ComputeIndexBucketNum(uint64_t totalMem, const ConfigRef &config);
    static uint32_t ComputeBucketGroupNum(uint32_t bucketNum, const ConfigRef &config);
    BResult InitializeBlobStore();

    inline SliceKVIteratorPtr GetIterator()
    {
        std::vector<std::vector<DataSliceRef>> dataSlices;
        uint32_t curSlot = 0;
        while (curSlot < mSliceBucketIndex->GetIndexCapacity()) {
            auto curChain = mSliceBucketIndex->GetLogicChainedSlice(static_cast<uint32_t>(curSlot));
            if (curChain == nullptr) {
                LOG_ERROR("curChain is nullptr");
                return nullptr;
            }
            if (curChain->IsNone()) {
                curSlot++;
                continue;
            }
            int32_t curChainIndex = curChain->GetSliceChainTailIndex();
            std::vector<DataSliceRef> curChainSlice;
            while (curChainIndex >= 0) {
                SliceAddressRef sliceAddress = curChain->GetSliceAddress(curChainIndex);
                if (sliceAddress == nullptr) {
                    LOG_ERROR("sliceAddress is nullptr");
                    return nullptr;
                }
                if (sliceAddress->IsEvicted()) {
                    break;
                }
                DataSliceRef dataSlice = sliceAddress->GetDataSlice();
                if (dataSlice == nullptr) {
                    break;
                }
                curChainSlice.emplace(curChainSlice.begin(), dataSlice);
                curChainIndex--;
            }
            dataSlices.emplace_back(curChainSlice);
            curSlot++;
        }
        Ref<FlushingBucketGroupIterator> dataSliceVectorIterator = MakeRef<FlushingBucketGroupIterator>();
        dataSliceVectorIterator->Initialize(dataSlices);
        return std::make_shared<SliceKVIterator>(dataSliceVectorIterator, mMemManager, mKeyGroupUtil,
                                                 mConfig->GetStartGroup(), mConfig->GetEndGroup());
    }

    BResult GetFromFile(const Key &key, LogicalSliceChainRef &logicalSliceChain, Value &finalResult,
                        std::stack<Value> &mergingValues, std::vector<SectionsReadContextRef> &readMetas) const;

    BResult MergeList(std::deque<Value> &result, Value finalResult, std::stack<Value> &mergingValues) const;

    BResult InternalGetList(const Key &key, std::deque<Value> &result, std::vector<SectionsReadContextRef> &readMetas);

    void GetFromSliceChain(const Key &key, LogicalSliceChainRef &logicalSliceChain, int32_t curIndex,
                           uint32_t readLength, std::stack<Value> &mergingValues, Value &finalResult, bool &isFound);
    bool GetFromSlice(const Key &key, std::stack<Value> &mergingValues, Value &finalResult,
                      SliceAddressRef &sliceAddress, DataSliceRef &dataSlice, bool &isFound) const;

    void RegisterTombstoneService();

private:
    std::atomic<bool> mIsRunning{ true };
    mutable std::mutex mMutex;
    SliceBucketIndexRef mSliceBucketIndex;
    ConfigRef mConfig;
    BucketGroupManagerRef mBucketGroupManager = std::make_shared<BucketGroupManager>();
    FileCacheManagerRef mFileCache;
    MemManagerRef mMemManager;
    std::mutex mRunningSnapshotMutex;
    std::unordered_map<uint64_t, SliceTableSnapshotRef> mRunningSnapshot;
    EvictManagerRef mEvictManager;
    SliceCompactionTriggerRef mCompactManager;
    TombstoneServiceRef mTombstoneService = nullptr;
    KeyGroupUtilRef mKeyGroupUtil = nullptr;
    AccessRecorderRef mAccessRecorder;
    std::atomic<uint32_t> mSnapshotVersion{ 0 };
    std::atomic<SliceTableCompaction> mIsCompaction{ SliceTableCompaction::OPEN };
    StateFilterManagerRef mStateFilterManager;
    BoostNativeMetricPtr mBoostNativeMetric = nullptr;
    bool mIsKVSeparate = false;
    uint32_t mKvThreshold = 0;
    bool mNeedReleaseBlockCache = false;
    BlobStoreRef mBlobStore = nullptr;
};
using SliceTableManagerRef = std::shared_ptr<SliceTable>;

class SliceTablePrefixIterator : public Iterator<KeyValueRef> {
public:
    ~SliceTablePrefixIterator() override = default;
    BResult Init(const LogicalSliceChainRef &logicalSliceChain, const Key &prefixKey);

    bool HasNext() override;

    KeyValueRef Next() override;

private:
    void Advance();
    bool mClosed = false;
    uint32_t mCurIndex = 0;
    Key mPrefixKey;
    KeyValueRef mCurrent;
    DataSliceRef mLastDataSlice;
    SecKeyValueMap mVisited;
    std::shared_ptr<SliceTable> mSliceTable;
    std::vector<std::pair<SliceAddressRef, DataSliceRef>> mSliceAddressAndDataSlices;
    KeyValueIteratorRef mCurrentSliceTableKvIterator;

    LogicalSliceChainRef mLogicalSliceChain = nullptr;
    uint32_t mFileIndex = 0;
    KeyValueIteratorRef mCurrentFilePageMap = nullptr;
    void SetSliceIter();
    void SetFileIter(const std::vector<FilePageRef> &filePages);
};

}  // namespace bss
}  // namespace ock
#endif  // BOOST_SS_SLICETABLEMANAGER_H
