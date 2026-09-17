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

#ifndef BOOST_SS_BUCKETGROUPFLUSHTASK_H
#define BOOST_SS_BUCKETGROUPFLUSHTASK_H

#include <atomic>

#include "executor/executor_service.h"
#include "flushing_bucket_group.h"

namespace ock {
namespace bss {

class BucketGroupFlushTask : public Runnable {
public:
    BucketGroupFlushTask() = default;
    explicit BucketGroupFlushTask(std::vector<SliceScore> &entryList, uint32_t bucketGroupId,
                                  const FullSortEvictorRef &evictor,
                                  const FlushQueueForBucketGroupRef &flushQueueForBucketGroup,
                                  const LsmStoreRef &lsmStore, BoostNativeMetricPtr metricPtr)
    {
        mFlushingBucketGroup = std::make_shared<FlushingBucketGroup>();
        mFlushingBucketGroup->Initialize(entryList, bucketGroupId, evictor, flushQueueForBucketGroup);
        mLsmStore = lsmStore;
        mBoostNativeMetric = metricPtr;
    }
    void Run() override
    {
        bool expect = false;
        BResult result = BSS_ERR;
        if (mBoostNativeMetric != nullptr && mBoostNativeMetric->IsSliceMetricEnabled()) {
            for (auto sliceCore : mFlushingBucketGroup->GetEvictList()) {
                mEvictSize += sliceCore.mSliceAddress->GetDataLen();
            }
            mBoostNativeMetric->AddSliceEvictSize(mEvictSize);
        }
        do {
            if (!mResourceCleanupOwnershipTaken.compare_exchange_strong(expect, true)) {
                break;
            }
            if (UNLIKELY(mLsmStore == nullptr)) {
                break;
            }
            // write kv pair to lsmStore.
            result = mLsmStore->Put(mFlushingBucketGroup->Iterator());
        } while (false);

        if (UNLIKELY(mFlushingBucketGroup->Complete(result) != BSS_OK)) {
            LOG_ERROR("Complete flush file failed, used slice memory is not updated.");
            if (mBoostNativeMetric != nullptr && mBoostNativeMetric->IsSliceMetricEnabled()) {
                mBoostNativeMetric->SubSliceEvictSize(mEvictSize);
            }
        }
    }

private:
    LsmStoreRef mLsmStore = nullptr;
    std::shared_ptr<FlushingBucketGroup> mFlushingBucketGroup;
    std::atomic<bool> mResourceCleanupOwnershipTaken{ false };
    BoostNativeMetricPtr mBoostNativeMetric = nullptr;
    uint64_t mEvictSize = 0;
};

}  // namespace bss
}  // namespace ock
#endif  // BOOST_SS_BUCKETGROUPFLUSHTASK_H