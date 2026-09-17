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
#ifndef TRYCOMPACTIONTASK_H
#define TRYCOMPACTIONTASK_H

#include "slice_compactor.h"

namespace ock {
namespace bss {
class TryCompactionTask : public Runnable {
public:
    explicit TryCompactionTask(const SliceCompactorRef &sliceCompactor, uint32_t bucketIndex,
                               const CompactCompletedNotify &compactCompletedNotify, BoostNativeMetricPtr metricPtr)
        : mSliceCompactor(sliceCompactor),
          mBucketIndex(bucketIndex),
          mCompactCompletedNotify(compactCompletedNotify),
          mBoostNativeMetric(metricPtr)
    {
    }

    ~TryCompactionTask() override = default;

    void Run() override
    {
        mSliceCompactor->TryCompaction(mBucketIndex, mCompactCompletedNotify, mBoostNativeMetric);
        if (mBoostNativeMetric != nullptr && mBoostNativeMetric->IsSliceMetricEnabled()) {
            mBoostNativeMetric->AddSliceCompactionCount();
        }
    }

private:
    SliceCompactorRef mSliceCompactor;
    uint32_t mBucketIndex = 0;
    CompactCompletedNotify mCompactCompletedNotify;
    BoostNativeMetricPtr mBoostNativeMetric = nullptr;
};

}  // namespace bss
}  // namespace ock
#endif  // TRYCOMPACTIONTASK_H