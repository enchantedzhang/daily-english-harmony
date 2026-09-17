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

#ifndef SLICE_COMPACTION_TRIGGER_H
#define SLICE_COMPACTION_TRIGGER_H

#include "memory/evict_manager.h"
#include "slice_table/compaction/slice_compactor.h"
#include "slice_table/compaction/try_compaction_task.h"

namespace ock {
namespace bss {
class SliceCompactionTrigger {
public:
    BResult Init(const ConfigRef &config, const SliceBucketIndexRef &bucketIndex, const MemManagerRef &memManager,
                 const StateFilterManagerRef &stateFilterManager)
    {
        // create compaction helper.
        mSliceCompactor = std::make_shared<SliceCompactor>();
        BResult ret = mSliceCompactor->Init(config, bucketIndex, memManager, stateFilterManager);
        if (ret != BSS_OK) {
            LOG_ERROR("Failed to init slice compaction helper. ret: " << ret);
            return BSS_ERR;
        }

        // create and initialize compaction executor.
        mCompactionEventExecutor = std::make_shared<ExecutorService>(NO_1, NO_1024);
        mCompactionEventExecutor->SetThreadName("SliceCompactionExecutor");
        if (!mCompactionEventExecutor->Start()) {
            LOG_ERROR("Compaction event executor start failed.");
            mCompactionEventExecutor = nullptr;
            return BSS_ERR;
        }
        LOG_INFO("Slice compaction event executor start.");

        return BSS_OK;
    }

    void Exit()
    {
        if (mCompactionEventExecutor != nullptr) {
            mCompactionEventExecutor->Stop();
        }
    }

    void RegisterTombstoneService(const TombstoneServiceRef &tombstoneService) const
    {
        mSliceCompactor->RegisterTombstoneService(tombstoneService);
    }

    void RegisterSliceCompactionMetric(BoostNativeMetricPtr metricPtr)
    {
        mBoostNativeMetric = metricPtr;
    }

    BResult AsyncCompactSlice(const SliceIndexContextRef &sliceIndexContext,
                              const CompactCompletedNotify &compactCompletedNotify)
    {
        RETURN_NOT_OK_AS_FALSE(sliceIndexContext == nullptr, BSS_INVALID_PARAM);
        RETURN_NOT_OK_AS_FALSE(compactCompletedNotify == nullptr, BSS_INVALID_PARAM);
        LogicalSliceChainRef currentLogicalSliceChain = sliceIndexContext->GetLogicalSliceChain();
        RETURN_NOT_OK_AS_FALSE(currentLogicalSliceChain == nullptr, BSS_ERR);

        // 生成异步执行的compact任务.
        const uint32_t bucketIndex = sliceIndexContext->GetSliceIndexSlot();
        const RunnablePtr task = std::make_shared<TryCompactionTask>(mSliceCompactor, bucketIndex,
                                                                     compactCompletedNotify, mBoostNativeMetric);
        bool ret = mCompactionEventExecutor->Execute(task);
        if (UNLIKELY(!ret)) {
            LOG_ERROR("Execute async slice compaction task failed.");
            return BSS_ERR;
        }
        return BSS_OK;
    }

    inline void AddSnapshotSyncTask(SnapshotSyncTaskRef &task)
    {
        mCompactionEventExecutor->Execute(task, true);
    }

private:
    ExecutorServicePtr mCompactionEventExecutor;
    SliceCompactorRef mSliceCompactor;
    BoostNativeMetricPtr mBoostNativeMetric = nullptr;
};
using SliceCompactionTriggerRef = std::shared_ptr<SliceCompactionTrigger>;
}  // namespace bss
}  // namespace ock

#endif  // SLICE_COMPACTION_TRIGGER_H