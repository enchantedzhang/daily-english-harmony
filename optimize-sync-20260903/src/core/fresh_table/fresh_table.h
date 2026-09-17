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
#ifndef BOOST_SS_FRESHTABLE_H
#define BOOST_SS_FRESHTABLE_H

#include <cstdint>
#include <functional>

#include "common/bss_metric.h"
#include "common/concurrent_deque.h"
#include "common/util/key_group_util.h"
#include "executor/executor_service.h"
#include "fresh_table/boost/boost_segment.h"
#include "fresh_table/handle/slice_handle.h"
#include "fresh_table/memory/memory_segment.h"
#include "include/bss_err.h"
#include "include/bss_types.h"
#include "snapshot/snapshot_restore_utils.h"
#include "snapshot_operator_coordinator.h"

namespace ock {
namespace bss {

class FreshTable {
public:
    FreshTable() = default;
    ~FreshTable();

    /**
     * Open fresh table.
     */
    void Open();

    /**
     * Initialize fresh table.
     * @param config configuration.
     * @param memManager memory manager.
     * @return return BSS_OK if success, else return BSS_ERR.
     */
    BResult Initialize(const ConfigRef &config, const MemManagerRef &memManager, const KeyGroupUtilRef &keyGroupUtil);

    /**
     * Exit fresh table.
     */
    void Exit()
    {
        if (mBoostNativeMetric != nullptr && mBoostNativeMetric->IsFreshMetricEnabled()) {
            // 防止fresh已经销毁但Java仍尝试获取值时导致访问已销毁的对象
            mBoostNativeMetric->SetFreshRecordCount(nullptr);
            mBoostNativeMetric->SetFreshFlushingRecordCount(nullptr);
            mBoostNativeMetric->SetFreshFlushingSegmentCount(nullptr);
            mBoostNativeMetric = nullptr;
        }
    }

    /**
     * Obtain the value by key
     * @param key key.
     * @param value value, nullptr if it is not exist.
     * @return Returns true if it exists or has already been deleted, false otherwise.
     */
    bool Get(const Key &key, Value &value);

    /**
     * Obtain the value by using the keys for key map
     * @param key key.
     * @param value value, nullptr if it is not exist.
     * @return Returns true if it exists or has already been deleted, false otherwise.
     */
    bool GetKMap(const Key &key, Value &value);

    /**
     * Obtain the list value by key. todo: refactor me.
     * @param key key.
     * @param deque list value.
     * @return Returns true if it exists or has already been deleted, false otherwise.
     */
    bool GetList(const Key &key, std::deque<Value> &deque);

    /**
     * Get all value entries from fresh table, just for value.
     * NOTICE: Only traverse one layer!!!
     * @return entries.
     */
    KeyValueIteratorRef EntryIterator(const PriKeyFilter &priKeyFilter, const SecKeyFilter &secKeyFilter = nullptr);

    /**
     * Get all entries of map found by key.
     * @param key first level key.
     * @return all entries of the specified map.
     */
    KeyValueIteratorRef GetMapIterator(const Key &key);

    /**
     * Put key value to fresh table.
     * @param key key.
     * @param value value.
     * @return return BSS_OK if success, else return BSS_ERR.
     */
    BResult Put(const Key &key, const Value &value);

    /**
     * Add key value to fresh table, if key already existed, return error.
     * @param key first key.
     * @param mKey second key.
     * @param value value.
     * @return return BSS_OK if success, else return BSS_ERR.
     */
    BResult Add(const KeyValue &keyValue);

    /**
     * Append value to existed key.
     * @param key existed key.
     * @param value append value.
     * @return return BSS_OK if success, else return BSS_ERR.
     */
    BResult Append(const Key &key, const Value &value);

    /**
     * Resore fresh table by metas.
     * @param restoredDbMetas // todo: refactor me
     * @return return BSS_OK if success, else return OTHERS.
     */
    BResult Restore(const std::vector<RestoredDbMetaRef> &restoredDbMetas);

    /**
     * Get active segment.
     * @return active segment.
     */
    const BoostSegmentRef &GetActiveSegment()
    {
        return mActive;
    }

    /**
     * Is snap shot queue empty or not?
     * @return return true if it is empty, else return false.
     */
    inline bool IsSnapshotQueueEmpty()
    {
        return mSnapshotQueue.Empty();
    }

    /**
     * Get snapshot queue front segment.
     * @param item front segment.
     * @return return true if it has front segment, else return false.
     */
    bool QueueFront(BoostSegmentRef &item)
    {
        return mSnapshotQueue.Front(item);
    }

    /**
     * Set transform trigger, when fresh table want transformer to flush data to slice table, call trigger.
     * @param trigger transform trigger.
     */
    inline void SetTransformTrigger(std::function<void()> trigger)
    {
        mTransformTrigger = trigger;
    }

    /**
     * Trigger segment flush to slice table.
     * @return return BSS_OK if success, else return OTHERS.
     */
    BResult TriggerSegmentFlush();

    /**
     * When flush transformer finished to flush, calls it to end flush.
     */
    void EndSegmentFlush();

    /**
     * Is already trigger flush or not.
     * @return return true if already trigger, else return false.
     */
    inline bool IsAlreadyTriggerFlush() const
    {
        return mIsAlreadyTriggerTransform;
    }

    /**
     * Just for testing, force trigger flush to slice.
     */
    inline void ForceAddToQueue()
    {
        AddFlushingSegment();
        NewActiveSegment(NO_1024 * NO_1024, MapLayerType::SINGLE_LAYER);
    }

    inline BResult ForceFlushToSlice()
    {
        BResult ret = TriggerSegmentFlush();
        if (UNLIKELY(ret != BSS_OK)) {
            LOG_ERROR("Failed to trigger segment flush to slice table. ret: " << ret);
            return ret;
        }
        uint32_t count = 0;
        while (!IsSnapshotQueueEmpty() && count < NO_300) {  // 等待所有MemorySegment Flush完成，最多等30s.
            usleep(NO_100000);                               // 100ms
            count++;
        }
        if (IsSnapshotQueueEmpty()) {
            return BSS_OK;
        }
        LOG_WARN("Wait fresh flush timeout.");
        return BSS_ERR;
    }

    inline void RegisterFreshMetric(BoostNativeMetricPtr metricPtr)
    {
        mBoostNativeMetric = metricPtr;
        if (mBoostNativeMetric == nullptr || !mBoostNativeMetric->IsFreshMetricEnabled()) {
            return;
        }
        mBoostNativeMetric->SetFreshRecordCount([this]() -> uint64_t {
            ReadLocker<ReadWriteLock> lock(&mRwLockActive);
            if (UNLIKELY(mActive == nullptr)) {
                LOG_ERROR("Query fresh record count, active boost segment is null, return 0.");
                return 0;
            }
            BoostHashMapRef boostHashMap = mActive->GetBinaryData();
            if (UNLIKELY(boostHashMap == nullptr)) {
                LOG_ERROR("Query fresh record count, boost hash map is null, return 0.");
                return 0;
            }
            return boostHashMap->Size();
        });
        mBoostNativeMetric->SetFreshFlushingRecordCount([this]() -> uint64_t {
            ReadLocker<ReadWriteLock> lock(&mRwLock);
            uint64_t records = 0;
            for (auto it = mSnapshotQueue.rbegin(); it != mSnapshotQueue.rend(); it++) {
                records += (*it)->Size();
            }
            return records;
        });
        mBoostNativeMetric->SetFreshFlushingSegmentCount([this]() -> uint64_t { return mSnapshotQueue.Size(); });
    }

    inline void IncrementFreshHit()
    {
        if (mBoostNativeMetric == nullptr || !mBoostNativeMetric->IsFreshMetricEnabled()) {
            return;
        }
        mBoostNativeMetric->AddFreshHitCount();
    }

    inline void IncrementFreshListHitMiss(std::deque<Value> &valueInFreshTable)
    {
        if (mBoostNativeMetric == nullptr || !mBoostNativeMetric->IsFreshMetricEnabled()) {
            return;
        }
        if (valueInFreshTable.empty()) {
            mBoostNativeMetric->AddFreshMissCount();
        } else {
            mBoostNativeMetric->AddFreshHitCount();
        }
    }

    inline void IncrementFreshMiss()
    {
        if (mBoostNativeMetric == nullptr || !mBoostNativeMetric->IsFreshMetricEnabled()) {
            return;
        }
        mBoostNativeMetric->AddFreshMissCount();
    }

    inline void IncrementFreshFlush(uint64_t flushedRecords)
    {
        if (mBoostNativeMetric == nullptr || !mBoostNativeMetric->IsFreshMetricEnabled()) {
            return;
        }
        mBoostNativeMetric->AddFreshFlushCount();
        mBoostNativeMetric->AddFreshFlushedSegmentCount();
        mBoostNativeMetric->AddFreshFlushedRecordCount(flushedRecords);
    }

    inline void IncrementFreshSegmentCreateFail()
    {
        if (mBoostNativeMetric == nullptr || !mBoostNativeMetric->IsFreshMetricEnabled()) {
            return;
        }
        mBoostNativeMetric->AddFreshSegmentCreateFailCount();
    }

    inline void IncrementFreshWastedSize(BoostSegmentRef &segment)
    {
        if (mBoostNativeMetric == nullptr || !mBoostNativeMetric->IsFreshMetricEnabled()) {
            return;
        }
        mBoostNativeMetric->AddFreshWastedSize(segment->GetMemorySegment()->GetLen() -
                                               segment->GetMemorySegment()->GetCurPos());
    }

    inline void IncrementFreshKeyValueSize(const Key &key, const Value &value)
    {
        if (mBoostNativeMetric == nullptr || !mBoostNativeMetric->IsFreshMetricEnabled()) {
            return;
        }
        uint32_t keySize = BoostHashMap::GetPriKeyLength(key);
        uint32_t valueSize = BoostHashMap::GetValueLength(value);
        mBoostNativeMetric->AddFreshBinaryKeySize(keySize);
        mBoostNativeMetric->AddFreshBinaryValueSize(valueSize);
    }

    inline void IncrementFreshKeyMapSize(const KeyValue &keyValue)
    {
        if (mBoostNativeMetric == nullptr || !mBoostNativeMetric->IsFreshMetricEnabled()) {
            return;
        }
        uint32_t keySize = BoostHashMap::GetPriKeyLength(keyValue.key);
        uint32_t mapSize = BoostHashMap::GetSecKeyLength(keyValue.key) + BoostHashMap::GetValueLength(keyValue.value);
        mBoostNativeMetric->AddFreshBinaryKeySize(keySize);
        mBoostNativeMetric->AddFreshBinaryMapNodeSize(mapSize);
    }

    inline void DecrementFreshKeyValueSize()
    {
        if (mBoostNativeMetric == nullptr || !mBoostNativeMetric->IsFreshMetricEnabled()) {
            return;
        }
        mBoostNativeMetric->ClearFreshBinaryKeyValue();
    }

    /**
     * only for test.
     */
    inline void SetActiveEmpty()
    {
        mActive = nullptr;
    }

private:
    enum class MapLayerType { SINGLE_LAYER, DUAL_LAYER };

    // 初始化新的活跃二进制段
    // @return 初始化结果，成功返回 BResult::SUCCESS，失败返回相应的错误码
    BResult InitNewActiveBinarySegment();

    void RequireMemoryUntilSuccess(const std::function<BResult()> &function);

    BResult RunWithForceRequireMemory(const std::function<BResult()> &function, uint32_t size, MapLayerType layerType);

    BResult TriggerSegmentFlushWithForceInit(uint32_t size, MapLayerType layerType);

    BResult RestoreOpen();

    BResult FillDataByMemorySegment(const BoostSegmentRef &boostSegment, GroupRange &validKeyGroups);

    bool VisitBinarySegmentMap(const BoostSegmentRef &segment, std::deque<Value> &result, const Key &key);

    BResult NewActiveSegment(uint32_t size, MapLayerType layerType);

    inline bool ShouldForceRequireMemory(uint32_t kvSize)
    {
        if (UNLIKELY(mActive == nullptr || mActive->GetMemorySegment() == nullptr)) {
            return true;
        }
        return (kvSize > mActive->GetMemorySegment()->GetLen() / NO_2);
    }

    inline BResult CreateAndAssignBinarySegment(const MemorySegmentRef &memorySegment)
    {
        WriteLocker<ReadWriteLock> lock(&mRwLockActive);
        mActive = std::make_shared<BoostSegment>();
        RETURN_ALLOC_FAIL_AS_NULLPTR(mActive);
        return mActive->Init(mSegmentId++, memorySegment, 0);
    }

    inline void AddFlushingSegment()
    {
        IncrementFreshWastedSize(mActive);
        DecrementFreshKeyValueSize();
        mSnapshotQueue.PushBack(mActive);
    }

    inline bool PollFlushingSegment(std::shared_ptr<BoostSegment> &item)
    {
        return mSnapshotQueue.TryPopFront(item);
    }

private:
    std::atomic<bool> mIsOpened{ false };

    ReadWriteLock mRwLockActive;
    BoostSegmentRef mActive;

    ReadWriteLock mRwLock;
    ConcurrentDeque<BoostSegmentRef> mSnapshotQueue;

    uint32_t mLinkedListCapacity = 0;
    uint64_t mSegmentId = 0;
    ConfigRef mConfig = nullptr;
    MemManagerRef mMemManager = nullptr;

    SnapshotOperatorCoordinatorRef mSnapshotCoordinator;

    bool mIsAlreadyTriggerTransform = false;
    std::function<void()> mTransformTrigger;

    BoostNativeMetricPtr mBoostNativeMetric = nullptr;
    KeyGroupUtilRef mKeyGroupUtil = nullptr;
};
using FreshTableRef = std::shared_ptr<FreshTable>;

}  // namespace bss
}  // namespace ock
#endif  // BOOST_SS_FRESHTABLE_H
