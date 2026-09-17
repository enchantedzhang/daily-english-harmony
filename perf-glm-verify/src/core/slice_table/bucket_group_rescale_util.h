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

#ifndef BOOST_SS_BUCKET_GROUP_RESCALE_UTIL_H
#define BOOST_SS_BUCKET_GROUP_RESCALE_UTIL_H

#include <algorithm>

#include "slice_table/bucket_group_manager.h"
#include "slice_table/bucket_group_range.h"

namespace ock {
namespace bss {

class BucketGroupRescaleUtil {
public:
    static BResult Rescale(const SliceBucketGroupRangeGroupRef &oldGroup, const SliceBucketGroupRangeGroupRef &newGroup,
                           const BucketGroupManagerRef &bucketGroupManager, const SliceBucketMapperRef &oldMapper,
                           const SliceBucketMapperRef &targetMapper,
                           std::unordered_map<uint32_t, std::vector<uint32_t>> &rescaleRelation)
    {
        if (UNLIKELY(oldGroup == nullptr || newGroup == nullptr || bucketGroupManager == nullptr ||
                     oldMapper == nullptr || targetMapper == nullptr)) {
            LOG_ERROR("Rescale slice buckets failed because input is nullptr.");
            return BSS_INVALID_PARAM;
        }
        if (UNLIKELY(oldGroup->mTotalBucket != newGroup->mTotalBucket ||
                     oldMapper->GetBucketNum() != targetMapper->GetBucketNum() ||
                     oldMapper->GetBucketNum() != oldGroup->mTotalBucket)) {
            LOG_ERROR("Rescale with different slice bucket counts is unsupported, oldGroup:"
                      << oldGroup->mTotalBucket << ", newGroup:" << newGroup->mTotalBucket << ", oldMapper:"
                      << oldMapper->GetBucketNum() << ", targetMapper:" << targetMapper->GetBucketNum());
            return BSS_NOT_SUPPORTED;
        }
        if (UNLIKELY(oldGroup->mSliceSegments.size() != 1 || newGroup->mSliceSegments.size() != 1 ||
                     oldGroup->mSliceSegments[0]->GetBucketGroupId() != 0 ||
                     newGroup->mSliceSegments[0]->GetBucketGroupId() != 0)) {
            LOG_ERROR("Rescale only supports one bucket group with id 0, oldGroupCount:"
                      << oldGroup->mSliceSegments.size() << ", newGroupCount:" << newGroup->mSliceSegments.size());
            return BSS_NOT_SUPPORTED;
        }

        rescaleRelation[0].push_back(0);
        auto targetTaskRange = targetMapper->GetTaskRange();
        RETURN_ERROR_AS_NULLPTR(targetTaskRange);
        for (uint32_t oldBucket = 0; oldBucket < oldGroup->mTotalBucket; ++oldBucket) {
            auto oldChain = oldGroup->mSliceBucketIndex->GetLogicChainedSlice(oldBucket);
            RETURN_ERROR_AS_NULLPTR(oldChain);
            if (oldChain->IsNone()) {
                continue;
            }

            auto oldRange = oldMapper->GetBucketRange(oldBucket);
            RETURN_ERROR_AS_NULLPTR(oldRange);
            const uint32_t overlapStart = std::max(oldRange->GetStartHashCode(), targetTaskRange->GetStartHashCode());
            const uint32_t overlapEnd = std::min(oldRange->GetEndHashCode(), targetTaskRange->GetEndHashCode());
            if (overlapStart > overlapEnd) {
                continue;
            }

            const uint32_t targetStart = targetMapper->MapUnchecked(overlapStart);
            const uint32_t targetEnd = targetMapper->MapUnchecked(overlapEnd);

            const uint32_t fanout = targetEnd - targetStart + 1;
            const bool rangeClipped = overlapStart != oldRange->GetStartHashCode() ||
                                      overlapEnd != oldRange->GetEndHashCode();
            const bool requireForceCompaction = fanout > 1 || rangeClipped;

            for (uint32_t targetBucket = targetStart; targetBucket <= targetEnd; ++targetBucket) {
                LogicalSliceChainRef targetChain = oldChain;
                if (fanout > 1) {
                    targetChain = oldChain->DeepCopy(true);
                    RETURN_ALLOC_FAIL_AS_NULLPTR(targetChain);
                }

                std::vector<FilePageRef> filePages;
                targetChain->GetFilePages(filePages);
                if (!filePages.empty()) {
                    auto lsmStore = bucketGroupManager->GetLsmStoreByBucketIndex(targetBucket);
                    RETURN_ERROR_AS_NULLPTR(lsmStore);
                    targetChain->RestoreFilePage(lsmStore);
                }
                RETURN_NOT_OK(AddLogicalSliceChainIntoMappingTable(newGroup->mSliceBucketIndex, targetBucket,
                                                                   targetChain, requireForceCompaction));
            }
        }
        return BSS_OK;
    }

private:
    static BResult AddLogicalSliceChainIntoMappingTable(SliceBucketIndexRef &sliceBucketIndex, uint32_t indexSlot,
                                                        LogicalSliceChainRef &logicalSliceChain,
                                                        bool requireForceCompaction)
    {
        auto nowChain = sliceBucketIndex->GetLogicChainedSlice(indexSlot);
        RETURN_ERROR_AS_NULLPTR(nowChain);
        if (nowChain->IsNone()) {
            sliceBucketIndex->SetLogicChainedSlice(indexSlot, std::make_shared<CompositeLogicalSliceChain>());
            nowChain = sliceBucketIndex->GetLogicChainedSlice(indexSlot);
        }
        auto composite = std::dynamic_pointer_cast<CompositeLogicalSliceChain>(nowChain);
        RETURN_ERROR_AS_NULLPTR(composite);
        RETURN_NOT_OK(composite->AddLogicalSliceChain(logicalSliceChain, requireForceCompaction));
        return BSS_OK;
    }
};

}  // namespace bss
}  // namespace ock
#endif  // BOOST_SS_BUCKET_GROUP_RESCALE_UTIL_H
