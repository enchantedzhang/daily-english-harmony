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

#ifndef BOOST_SS_EVICTMANAGER_H
#define BOOST_SS_EVICTMANAGER_H

#include <cstdint>
#include <functional>
#include <memory>

#include "full_sort_evictor.h"
#include "slice_evict_manager.h"
#include "slice_table/access_recorder.h"

namespace ock {
namespace bss {
class EvictManager : public std::enable_shared_from_this<EvictManager> {
public:
    EvictManager()
    {
        LOG_INFO("Evict manager construct.");
    }
    ~EvictManager()
    {
        LOG_INFO("Evict manager destroy.");
    }

    BResult Initialize(const ConfigRef &config, const BucketGroupManagerRef &bucketGroupManager,
                       const AccessRecorderRef &accessRecord);

    void Exit();

    BResult TryEvict(bool isSync, bool force, uint32_t minSize);

    void RegisterEvictMetric(BoostNativeMetricPtr metricPtr)
    {
        mFullSortEvictor->RegisterEvictMetric(metricPtr);
    }

    /**
     * for testing.
     */
    void ForceEvict();

    inline void AddSliceUsedMemory(int64_t addedSize) const
    {
        mSliceEvictManager->AddSliceUsedMemory(addedSize);
    }

    inline FullSortEvictorRef GetEvictorHandle()
    {
        return mFullSortEvictor;
    }

    inline int64_t GetMemHighMark()
    {
        return mSliceEvictManager->GetMemHighMark();
    }

    inline void SetMemHighMark(int64_t highMark)
    {
        mSliceEvictManager->SetMemHighMark(highMark);
    }

    inline void AddSnapshotSyncTask(SnapshotSyncTaskRef &task)
    {
        mFullSortEvictor->AddSnapshotSyncTask(task);
    }

private:
    SliceEvictManagerRef mSliceEvictManager;
    FullSortEvictorRef mFullSortEvictor;
};
using EvictManagerRef = std::shared_ptr<EvictManager>;
}  // namespace bss
}  // namespace ock

#endif  // BOOST_SS_EVICTMANAGER_H
