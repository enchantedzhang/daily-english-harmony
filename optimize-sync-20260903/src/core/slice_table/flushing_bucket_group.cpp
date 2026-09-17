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

#include "flushing_bucket_group.h"

namespace ock {
namespace bss {
BResult FlushingBucketGroup::Initialize(std::vector<SliceScore> &list, uint32_t bucketGroupId,
                                        const FullSortEvictorRef &evictor,
                                        const FlushQueueForBucketGroupRef &flushQueueForBucketGroup)
{
    if (evictor == nullptr) {
        LOG_ERROR("evictor is nullptr");
        return BSS_ERR;
    }
    if (flushQueueForBucketGroup == nullptr) {
        LOG_ERROR("flushQueueForBucketGroup is nullptr");
        return BSS_ERR;
    }
    int32_t queue = -1;
    std::vector<DataSliceRef> current;
    // 前面已经按照bucketIndex进行了排序，这里相同的bucket放在同一个current里面
    for (auto iter = list.begin(); iter != list.end();) {
        SliceScore entry = *iter;
        if (queue != static_cast<int32_t>(entry.mQueue)) {
            if (!current.empty()) {
                mDataSlices.push_back(current);
            }
            queue = static_cast<int32_t>(entry.mQueue);
            current.clear();
            current.reserve(NO_10);
        }
        RETURN_ERROR_AS_NULLPTR(entry.mSliceAddress);
        auto dataSlice = entry.mSliceAddress->GetDataSlice();
        if (dataSlice != nullptr) {
            current.push_back(dataSlice);
        }
        iter++;
    }
    if (!current.empty()) {
        mDataSlices.push_back(current);
    }
    mFlushQueueForBucketGroup = flushQueueForBucketGroup;
    mEvictor = evictor;
    mList = list;
    mBucketGroupId = bucketGroupId;
    return BSS_OK;
}

Ref<FlushingBucketGroupIterator> FlushingBucketGroup::Iterator()
{
    Ref<FlushingBucketGroupIterator> iterator = MakeRef<FlushingBucketGroupIterator>();
    iterator->Initialize(mDataSlices);
    return iterator;
}

BResult FlushingBucketGroup::Complete(int32_t flushResult)
{
    if (LIKELY(flushResult == BSS_OK)) {
        for (const SliceScore &scoreEntry : mList) {
            RETURN_ALLOC_FAIL_AS_NULLPTR(scoreEntry.mSliceAddress);
            mEvictor->mReadyToEvictQueue.PushBack(scoreEntry);
            RETURN_ALLOC_FAIL_AS_NULLPTR(mEvictor->mWaterMarkManager);
            mEvictor->mWaterMarkManager->SubEvictingMemory(scoreEntry.mSliceAddress->GetDataLen());
        }
        mEvictor->FlushedSuccessCallBack();
    } else {
        LOG_WARN("Flush to level0 table failed, require to internal retry, ret:" << flushResult);
        for (const SliceScore &scoreEntry : mList) {
            RETURN_ALLOC_FAIL_AS_NULLPTR(scoreEntry.mSliceAddress);
            RETURN_ALLOC_FAIL_AS_NULLPTR(mEvictor->mWaterMarkManager);
            if (!scoreEntry.mSliceAddress->SetStatus(SliceEvent::FLUSH_BACK)) {
                LOG_ERROR("Rollback slice status from compaction to normal failed, slice address status:"
                          << static_cast<uint32_t>(scoreEntry.mSliceAddress->GetSliceStatus()));
            }
            mEvictor->mWaterMarkManager->SubEvictingMemory(scoreEntry.mSliceAddress->GetDataLen());
        }
    }
    return BSS_OK;
}

BResult FlushingBucketGroupIterator::Initialize(std::vector<std::vector<DataSliceRef>> &dataSlices)
{
    mDataSlices = std::move(dataSlices);
    mIter = mDataSlices.begin();
    return BSS_OK;
}

bool FlushingBucketGroupIterator::HasNext()
{
    return mIter != mDataSlices.end();
}

std::vector<DataSliceRef> FlushingBucketGroupIterator::Next()
{
    auto dataSliceList = *mIter;
    mIter++;
    return dataSliceList;
}
}  // namespace bss
}  // namespace ock