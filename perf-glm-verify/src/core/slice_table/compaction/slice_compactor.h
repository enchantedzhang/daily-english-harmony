/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */

#ifndef BOOST_SS_SLICE_COMPACTOR_H
#define BOOST_SS_SLICE_COMPACTOR_H

#include <memory>

#include "memory/evict_manager.h"
#include "slice_compaction_policy.h"
#include "slice_table/index/slice_bucket_index.h"

namespace ock {
namespace bss {
using CompactCompletedNotify =
    std::function<void(LogicalSliceChainRef logicalSliceChain, uint32_t bucketIndex, uint32_t compactionStartChainIndex,
                       uint32_t compactionEndChainIndex, DataSliceRef compactedDataSlice,
                       std::vector<SliceAddressRef> invalidSliceAddressList, uint32_t finalOldSliceSize,
                       bool fromRestore)>;

class SliceCompactor {
public:
    ~SliceCompactor()
    {
        LOG_INFO("Delete SliceCompactor success");
    }
    BResult Init(const ConfigRef &config, const SliceBucketIndexRef &bucketIndex, const MemManagerRef &memManager,
                 const StateFilterManagerRef &stateFilterManager);
    void TryCompaction(uint32_t bucketIndex, const CompactCompletedNotify &compactCompletedNotify,
                       BoostNativeMetricPtr metricPtr);
    BResult DoCompaction(const SliceIndexContextRef &sliceIndexContext,
                         const SelectedSliceContextRef &selectedSliceContext,
                         const CompactCompletedNotify &compactCompletedNotify, BoostNativeMetricPtr metricPtr);
    BResult DoCompactSlice(const std::vector<DataSliceRef> &canCompactSliceListReversed,
                           DataSliceRef &compactedDataSlice, bool forceFilter, uint32_t bucketIndex,
                           bool reserveDeleteMarker);

    void RegisterTombstoneService(const TombstoneServiceRef &tombstoneService)
    {
        mTombstoneService = tombstoneService;
    }

private:
    uint32_t mInMemoryCompactionThreshold = 0;
    SliceCompactionPolicyRef mSliceCompactionPolicy;
    SliceBucketIndexRef mBucketIndex;
    MemManagerRef mMemManager;
    StateFilterManagerRef mStateFilterManager;
    TombstoneServiceRef mTombstoneService = nullptr;
};
using SliceCompactorRef = std::shared_ptr<SliceCompactor>;
}  // namespace bss
}  // namespace ock
#endif  // BOOST_SS_SLICE_COMPACTOR_H
