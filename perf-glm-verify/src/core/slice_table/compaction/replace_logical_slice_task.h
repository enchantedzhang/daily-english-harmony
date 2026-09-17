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
#ifndef REPLACELOGICALSLICETASK_H
#define REPLACELOGICALSLICETASK_H

#include "executor/executor_service.h"
#include "slice_compactor.h"
#include "slice_table.h"
#include "slice_table/slice/logical_slice_chain.h"

namespace ock {
namespace bss {
class ReplaceLogicalSlice {
public:
    ReplaceLogicalSlice(const SliceTableManagerRef &sliceTable, ConfigRef &config)
    {
        mSliceTable = sliceTable;
        mConfig = config;
        mBucketIndex = mSliceTable->GetSliceBucketIndex();
    }

    BResult SyncReplaceChainAndSlice(LogicalSliceChainRef &logicalSliceChain, uint32_t sliceIndexSlot,
                                     uint32_t compactionStartChainIndex, uint32_t compactionEndChainIndex,
                                     DataSliceRef &compactedDataSLice,
                                     std::vector<SliceAddressRef> &invalidSliceAddressList);

    BResult SyncUpdateChain(LogicalSliceChainRef &oldLogicalSliceChain, LogicalSliceChainRef &newLogicalSliceChain,
                            uint32_t sliceIndexSlot);

    BucketGroupRef GetBucketGroup() const
    {
        RETURN_NULLPTR_AS_NULLPTR(mSliceTable->GetBucketGroupManager());
        return mSliceTable->GetBucketGroupManager()->GetBucketGroupVector()[0];
    }

private:
    std::shared_ptr<Config> mConfig;
    SliceTableManagerRef mSliceTable;
    SliceBucketIndexRef mBucketIndex;
};
using ReplaceLogicalSliceRef = std::shared_ptr<ReplaceLogicalSlice>;

class ReplaceLogicalSliceTask : public Runnable {
public:
    ReplaceLogicalSliceTask(ReplaceLogicalSliceRef &replaceLogicalSlice, LogicalSliceChainRef &logicalSliceChain,
                            uint32_t sliceIndexSlot, uint32_t compactionStartChainIndex,
                            uint32_t compactionEndChainIndex, DataSliceRef &compactedDataSlice,
                            std::vector<SliceAddressRef> &invalidSliceAddressList)
        : mReplaceLogicalSlice(replaceLogicalSlice),
          mLogicalSliceChain(logicalSliceChain),
          mSliceIndexSlot(sliceIndexSlot),
          mCompactionStartChainIndex(compactionStartChainIndex),
          mCompactionEndChainIndex(compactionEndChainIndex),
          mCompactedDataSlice(compactedDataSlice),
          mInvalidSliceAddressList(invalidSliceAddressList)
    {
    }

    ~ReplaceLogicalSliceTask() override = default;

    void Run() override;

    void Clean();

private:
    ReplaceLogicalSliceRef mReplaceLogicalSlice;
    LogicalSliceChainRef mLogicalSliceChain;
    uint32_t mSliceIndexSlot;
    uint32_t mCompactionStartChainIndex;
    uint32_t mCompactionEndChainIndex;
    DataSliceRef mCompactedDataSlice;
    std::vector<SliceAddressRef> mInvalidSliceAddressList;
};

}  // namespace bss
}  // namespace ock
#endif  // REPLACELOGICALSLICETASK_H
