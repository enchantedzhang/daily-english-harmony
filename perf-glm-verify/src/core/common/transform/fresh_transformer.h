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

#ifndef FRESH_TRANSFORMER_H
#define FRESH_TRANSFORMER_H

#include "fresh_table/fresh_table.h"
#include "slice_table/binary_map/binary_key.h"
#include "slice_table/compaction/replace_logical_slice_task.h"
#include "slice_table/slice_table.h"

namespace ock {
namespace bss {
class FreshHandler : public std::enable_shared_from_this<FreshHandler> {
public:
    FreshHandler(const ConfigRef &config, SliceTableManagerRef sliceTable, ExecutorServicePtr transExecutor)
        : mConfig(config), mSliceTable(sliceTable), mTransExecutor(transExecutor)
    {
    }

    ~FreshHandler() = default;

    BResult Handle(const BoostSegmentRef &segment);

    /**
     * divide data of fresh table into groups.
     */
    BResult DivideFreshData(std::unordered_map<SliceIndexContextRef, std::shared_ptr<RawDataSlice>,
                                               SliceIndexContextHash, SliceIndexContextEqual> &organizedData,
                            const BoostSegmentRef &segment);

    /**
     * write data to slice table.
     */
    BResult WriteDataToSlice(std::unordered_map<SliceIndexContextRef, RawDataSliceRef, SliceIndexContextHash,
                                                SliceIndexContextEqual> &organizedData);

    /**
     * compaction callback.
     */
    void CompactCallback(LogicalSliceChainRef &logicalSlicechain, uint32_t sliceIndexSlot,
                         uint32_t compactionStartChainIndex, uint32_t compactionEndChainIndex,
                         DataSliceRef &compactedDataSlice, std::vector<SliceAddressRef> &invalidSliceAddressList);

    BResult DoTrans(FreshKeyNodePtr key, FreshValueNodePtr value,
                    std::vector<std::pair<BinaryKey, FreshValueNodePtr>> &collection, const BoostSegmentRef &segment);

private:
    ConfigRef mConfig;
    SliceTableManagerRef mSliceTable;
    ExecutorServicePtr mTransExecutor;
};
using FreshHandlerRef = std::shared_ptr<FreshHandler>;

class TransformerProcessor : public Runnable {
public:
    TransformerProcessor(FreshTableRef freshTable, SliceTableManagerRef sliceTable, FreshHandlerRef handle)
        : mFreshTable(freshTable), mSliceTable(sliceTable), mHandle(handle)
    {
    }

    ~TransformerProcessor() override
    {
        LOG_DEBUG("Delete TransformerProcessor success");
    }

    void Run() override
    {
        BResult result;
        int64_t times = NO_10;
        do {
            result = Handle();
            if (LIKELY(result == BSS_OK)) {
                break;
            }
            if (UNLIKELY(times <= NO_0)) {
                LOG_ERROR("Fresh table transform retry over than 10 times, ret:" << result);
                return;
            }
            times--;
            LOG_WARN("Fresh table transform handle failed, need to inner retry, times:" << times << ", ret:" << result);
        } while (UNLIKELY(result != BSS_OK) && (usleep(NO_100), 1));
        mFreshTable->EndSegmentFlush();
    }

    BResult Handle() const
    {
        BoostSegmentRef segment;
        bool ret = mFreshTable->QueueFront(segment);
        if (!ret) {
            LOG_DEBUG("fresh snapshot queue is empty.");
            return BSS_OK;
        }
        LOG_INFO("begin to flush fresh segment:" << segment->GetSegmentId());
        auto start = std::chrono::steady_clock::now();
        BResult res = mHandle->Handle(segment);
        auto end = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        if (UNLIKELY(elapsed.count() > TWO_MINUTES_IN_MS)) {
            LOG_WARN("Flush fresh segment: " << segment->GetSegmentId() << " cost time:" << elapsed.count() << " ms.");
        }
        return res;
    }

private:
    FreshTableRef mFreshTable;
    SliceTableManagerRef mSliceTable;
    FreshHandlerRef mHandle;
};

class FreshTransformer : public std::enable_shared_from_this<FreshTransformer> {
public:
    ~FreshTransformer()
    {
        LOG_INFO("Delete FreshTransformer success");
    }

    BResult Init(const ConfigRef &config, const FreshTableRef &freshTable, const SliceTableManagerRef &sliceTable)
    {
        mConfig = config;
        mFreshTable = freshTable;
        mSliceTable = sliceTable;
        mTransformExecutor = std::make_shared<ExecutorService>(NO_1, NO_2048);
        mTransformExecutor->SetThreadName("FreshTransformerExecutor");
        if (!mTransformExecutor->Start()) {
            LOG_ERROR("Failed to start transform executor.");
            return BSS_ERR;
        }
        auto self = shared_from_this();
        mFreshTable->SetTransformTrigger([self]() { self->TriggerTransform(); });
        return BSS_OK;
    }

    void Exit()
    {
        if (mTransformExecutor != nullptr) {
            mTransformExecutor->Stop();
        }
        mFreshTable = nullptr;
        LOG_INFO("Transformer exit.");
    }

    ExecutorServicePtr GetTransformExecutor()
    {
        return mTransformExecutor;
    }

    /**
     * trigger transform data from fresh table to slice table.
     */
    void TriggerTransform();

    /**
     * Just for testing, force trigger transform.
     */
    void ForceTriggerTransform()
    {
        mFreshTable->ForceAddToQueue();
        FreshHandlerRef mHandle = std::make_shared<FreshHandler>(mConfig, mSliceTable, mTransformExecutor);
        auto self = shared_from_this();
        RunnablePtr processor = std::make_shared<TransformerProcessor>(mFreshTable, mSliceTable, mHandle);
        processor->Run();
    }

private:
    ConfigRef mConfig;
    FreshTableRef mFreshTable;
    SliceTableManagerRef mSliceTable;
    ExecutorServicePtr mTransformExecutor;
};
using FreshTransformerRef = std::shared_ptr<FreshTransformer>;
}  // namespace bss
}  // namespace ock

#endif  // FRESH_TRANSFORMER_H
