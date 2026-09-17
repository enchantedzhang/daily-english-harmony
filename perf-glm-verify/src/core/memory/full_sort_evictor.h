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

#ifndef BOOST_SS_FULL_SORT_EVICTOR_H
#define BOOST_SS_FULL_SORT_EVICTOR_H
#include <cstdint>
#include <deque>
#include <memory>
#include <mutex>
#include <queue>

#include "common/concurrent_deque.h"
#include "executor/executor_service.h"
#include "slice_evict_manager.h"
#include "slice_table/access_recorder.h"
#include "slice_table/bucket_group.h"
#include "slice_table/bucket_group_manager.h"
#include "slice_table/slice/slice_address.h"
#include "snapshot/snapshot_sync_task.h"

namespace ock {
namespace bss {

struct SliceScore {
    SliceScore() = default;
    SliceScore(SliceAddressRef &sliceAddress, uint64_t queue, SliceIndexContextRef &indexContext, uint64_t tick)
        : mScore(sliceAddress->Score(tick)), mQueue(queue), mSliceAddress(sliceAddress), mIndexContext(indexContext)
    {
    }
    float mScore;
    uint32_t mQueue;  // identification slice chain
    SliceAddressRef mSliceAddress;
    SliceIndexContextRef mIndexContext;
};

struct BucketGroupHash {
    uint32_t operator()(const BucketGroupRef &bucketGroup) const
    {
        return std::hash<BucketGroupRange *>()(bucketGroup->GetBucketGroupRange().get());
    }
};

struct BucketGroupEqual {
    bool operator()(const BucketGroupRef &a, const BucketGroupRef &b) const
    {
        return a->GetBucketGroupId() == b->GetBucketGroupId();
    }
};

using EvictNotify = std::function<void(uint32_t, const LogicalSliceChainRef &, const SliceAddressRef &)>;
class FullSortEvictor;
using FullSortEvictorRef = std::shared_ptr<FullSortEvictor>;
class FullSortEvictor : public std::enable_shared_from_this<FullSortEvictor> {
public:
    ~FullSortEvictor()
    {
        LOG_INFO("Delete FullSortEvictor success");
    }

    BResult Initialize(uint32_t evictMinSize, const BucketGroupManagerRef &bucketGroupManager,
                       const SliceEvictManagerRef &waterMarkManager, const AccessRecorderRef &accessRecord);

    void Exit();

    BResult TryEvict(bool isSync, bool force, uint32_t minSize);

    /**
     * For testing.
     */
    void ForceEvict();

    void FlushedSuccessCallBack();

    void RegisterEvictMetric(BoostNativeMetricPtr metricPtr)
    {
        mBoostNativeMetric = metricPtr;
        if (mBoostNativeMetric != nullptr && mBoostNativeMetric->IsSliceMetricEnabled()) {
            mBoostNativeMetric->SetSliceEvictWaitingCount([this]() -> uint64_t { return mService->QueueSize(); });
        }
    }

    struct CompareScoreEntry {
        bool operator()(const SliceScore &a, const SliceScore &b) const
        {
            return a.mScore > b.mScore;  // 降序排列，实现最小堆
        }
    };

    inline void SetVictMinSize(uint32_t size)
    {
        mEvictMinSize = size;
    }

    enum class QueueStatus { NORMAL, RUNNING, FAILED };

    class FlushQueueForBucketGroup : public std::enable_shared_from_this<FlushQueueForBucketGroup> {
    public:
        BResult Initialize(const FullSortEvictorRef &fullSortEvictor, const ExecutorServicePtr &executorService,
                           const BucketGroupRef &bucketGroup, BoostNativeMetricPtr *metricPtr);
        bool SubmitJob(std::vector<SliceScore> &entryList, const FullSortEvictorRef &fullSortEvictor, bool isSync);

        ExecutorServicePtr mService;
        std::weak_ptr<FullSortEvictor> mFullSortEvictor;
        BucketGroupRef mBucketGroup;
        BoostNativeMetricPtr *mMetricPtrAddr = nullptr;
    };
    using FlushQueueForBucketGroupRef = std::shared_ptr<FlushQueueForBucketGroup>;
    ExecutorServicePtr mService;
    AccessRecorderRef mAccessRecord;
    BucketGroupManagerRef mBucketGroupManager;
    ConcurrentDeque<SliceScore> mReadyToEvictQueue;
    ConcurrentDeque<FlushQueueForBucketGroupRef> mFailedToEvictQueue;
    SliceEvictManagerRef mWaterMarkManager;
    std::unordered_map<BucketGroupRef, FlushQueueForBucketGroupRef, BucketGroupHash, BucketGroupEqual>
        mFlushQueueForBucketGroupMap;
    uint64_t SortAndFlush(uint32_t fileSize, bool isSync);
    uint64_t Flush(uint32_t fileSize, BucketGroupRef minGroup, bool isSync);
    uint64_t TriggerFlush(std::vector<SliceScore> &entryList, BucketGroupRef &bucketGroup, bool isSync);
    BResult InitFlushQueues();
    void CalculateScore(LogicalSliceChainRef &chain, float &score, uint64_t &size, uint64_t &baseSize) const;
    void SelectMinScoreGroup(uint32_t fileSize, BucketGroupRef &minGroup, float &minScore) const;
    std::vector<SliceScore> SelectEvictSlices(uint32_t fileSize, BucketGroupRef &minGroup, uint32_t chainCount);

    inline void AddSnapshotSyncTask(const SnapshotSyncTaskRef &task)
    {
        mService->Execute(task, true);
    }

private:
    uint32_t mEvictMinSize = 0;  // eliminate the minimum size required as the file size.
    BoostNativeMetricPtr mBoostNativeMetric = nullptr;
};
using FlushQueueForBucketGroupRef = std::shared_ptr<FullSortEvictor::FlushQueueForBucketGroup>;
}  // namespace bss
}  // namespace ock

#endif  // BOOST_SS_FULL_SORT_EVICTOR_H
