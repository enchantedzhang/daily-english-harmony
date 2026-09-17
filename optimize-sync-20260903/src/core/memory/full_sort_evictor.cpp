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

#include "full_sort_evictor.h"

#include <algorithm>
#include <limits>

#include "slice_table/bucket_group_flush_task.h"

namespace ock {
namespace bss {

BResult FullSortEvictor::TryEvict(bool isSync, bool force, uint32_t minSize)
{
    while (force || (mWaterMarkManager->GetOverHighMark() > mWaterMarkManager->GetEvictingMemorySize())) {
        uint64_t selectedSize = SortAndFlush(minSize != UINT32_MAX ? minSize : mEvictMinSize, isSync);
        if (selectedSize == 0) {
            break;
        }
        LOG_INFO("Finish slice data evict, completed evict size:" << selectedSize << ".");
    }
    return BSS_OK;
}

void FullSortEvictor::ForceEvict()
{
    BucketGroupRef minGroup = nullptr;
    float minScore;
    SelectMinScoreGroup(0, minGroup, minScore);
    if (UNLIKELY(minGroup == nullptr)) {
        LOG_INFO("Not select min score bucket group.");
        return;
    }
    // 淘汰刷盘
    Flush(NO_MAX_VALUE32, minGroup, true);
}

BResult FullSortEvictor::Initialize(uint32_t evictMinSize, const BucketGroupManagerRef &bucketGroupManager,
                                    const SliceEvictManagerRef &waterMarkManager, const AccessRecorderRef &accessRecord)
{
    if (bucketGroupManager == nullptr) {
        LOG_ERROR("bucketGroupManager is nullptr");
        return BSS_ERR;
    }

    if (waterMarkManager == nullptr) {
        LOG_ERROR("waterMarkManager is nullptr");
        return BSS_ERR;
    }

    mService = std::make_shared<ExecutorService>(NO_1, NO_100);
    mService->SetThreadName("SliceEvictExecutor");
    if (!mService->Start()) {
        LOG_ERROR("Failed to start slice evict executor!");
        return BSS_ERR;
    }

    mEvictMinSize = evictMinSize;
    mBucketGroupManager = bucketGroupManager;
    mWaterMarkManager = waterMarkManager;
    mAccessRecord = accessRecord;

    return InitFlushQueues();
}

void FullSortEvictor::Exit()
{
    if (mService != nullptr) {
        mService->Stop();
    }
    if (mBoostNativeMetric != nullptr) {
        mBoostNativeMetric->SetSliceEvictWaitingCount(nullptr);
    }
    mBoostNativeMetric = nullptr;
}

uint64_t FullSortEvictor::SortAndFlush(uint32_t fileSize, bool isSync)
{
    // 1. 选择得分最小的BucketGroup.
    BucketGroupRef minGroup = nullptr;
    float minScore;
    SelectMinScoreGroup(fileSize, minGroup, minScore);
    if (UNLIKELY(minGroup == nullptr)) {
        LOG_INFO("Not select min score bucket group.");
        return 0L;
    }
    // 2. 淘汰刷盘
    return Flush(fileSize, minGroup, isSync);
}

uint64_t FullSortEvictor::Flush(uint32_t fileSize, BucketGroupRef minGroup, bool isSync)
{
    // 1. 构建刷盘的候选者.
    uint32_t chainCount = minGroup->GetSliceChainCount();
    std::vector<SliceScore> candidates = SelectEvictSlices(fileSize, minGroup, chainCount);

    // 2. 刷盘.
    return TriggerFlush(candidates, minGroup, isSync);
}

void FullSortEvictor::FlushedSuccessCallBack()
{
    SliceScore entry{};
    while (!mReadyToEvictQueue.Empty()) {
        mReadyToEvictQueue.TryPopFront(entry);

        // set slice evicted.
        SliceAddressRef sliceAddress = entry.mSliceAddress;
        if (UNLIKELY(sliceAddress == nullptr)) {
            break;
        }
        if (!sliceAddress->SetStatus(SliceEvent::EVICT)) {
            LOG_WARN("Flush success set slice address status failed, slice address status:"
                     << static_cast<uint32_t>(sliceAddress->GetSliceStatus()));
        }
        auto sliceBucketIndex = mBucketGroupManager->GetSliceBucketIndex();
        auto bucketIndex = entry.mQueue;
        // 释放sliceChain的资源.
        sliceBucketIndex->ReleaseChainSlice(bucketIndex, sliceAddress);
        mWaterMarkManager->SubSliceUsedMemory(sliceAddress->GetDataLen());
    }
}

/**
 * 选择要淘汰的slice候选
 */
std::vector<SliceScore> FullSortEvictor::SelectEvictSlices(uint32_t fileSize, BucketGroupRef &minGroup,
                                                           uint32_t chainCount)
{
    std::vector<uint32_t> chainIndexArray(chainCount);
    std::vector<std::shared_ptr<LogicalSliceChain>> chainArray(chainCount);
    std::priority_queue<SliceScore, std::vector<SliceScore>, CompareScoreEntry> priorityQueue;
    // 构建当前SliceBucketIndex的sliceChain迭代器.
    SliceChainIterator chainIterator(minGroup->GetSliceBucketIndex(), minGroup->GetBucketGroupRange());
    // 遍历bucketIndex下的每个SliceChain.
    for (uint32_t i = 0; i < chainCount && chainIterator.HasNext(); ++i) {
        SliceIndexContextRef indexContext = chainIterator.Next();
        if (UNLIKELY(indexContext == nullptr || indexContext->GetLogicalSliceChain() == nullptr)) {
            LOG_ERROR("Slice index context is nullptr.");
            continue;
        }
        LogicalSliceChainRef sliceChain = indexContext->GetLogicalSliceChain();
        LOG_DEBUG("Current logic slice chain is " << sliceChain->ToString());
        if (UNLIKELY(!sliceChain->IsSimpleSliceChain())) {  // 复合的sliceChain不参与淘汰.
            continue;
        }

        // 从前往后遍历sliceChain中的每个sliceAddress判断是否满足淘汰条件, 满足则加入待筛选队列.
        uint32_t sliceIndex = 0;
        uint32_t sliceNum = sliceChain->GetCurrentSliceChainLen();
        while (sliceIndex < sliceNum) {
            SliceAddressRef sliceAddress = sliceChain->GetSliceAddress(static_cast<int32_t>(sliceIndex));
            if (UNLIKELY(sliceAddress == nullptr)) {
                LOG_ERROR("Get slice address is nullptr, index:" << static_cast<int32_t>(sliceIndex));
                sliceIndex++;
                continue;
            }
            if (sliceAddress->GetSliceStatus() == SliceStatus::COMPACTING) {
                break;
            }
            if (sliceAddress->GetSliceStatus() == SliceStatus::NORMAL) {  // 选择sliceAddress状态为normal的Slice加到PQ.
                SliceScore scoreEntry(sliceAddress, i, indexContext, mAccessRecord->AccessCount());
                priorityQueue.emplace(scoreEntry);
                sliceIndex++;
                break;
            }
            sliceIndex++;
        }
        chainArray[i] = sliceChain;
        chainIndexArray[i] = sliceIndex;
    }

    std::vector<SliceScore> candidates;
    uint32_t pickedSize = 0;
    while (pickedSize < fileSize && !priorityQueue.empty()) {
        // add to priority queue, add picked size.
        SliceScore entry = priorityQueue.top();
        if (entry.mSliceAddress == nullptr) {
            continue;
        }
        priorityQueue.pop();

        if (!entry.mSliceAddress->SetStatus(SliceEvent::FLUSH)) {  // set flush flag.
            continue;
        }

        candidates.push_back(entry);
        uint32_t sliceDataLen = entry.mSliceAddress->GetDataLen();
        pickedSize += sliceDataLen;
        mWaterMarkManager->AddEvictingMemory(sliceDataLen);  // add evicting memory

        uint32_t queue = entry.mQueue;
        auto &sliceChain = chainArray[queue];
        uint32_t &sliceIndex = chainIndexArray[queue];
        mBucketGroupManager->MarkLogicalSliceChainFlushed(sliceChain, minGroup);
        if (sliceChain->GetBaseSliceIndex() < sliceIndex) {
            sliceChain->SetBaseSliceIndex(sliceIndex);  // 更新slice的baseIndex.
        }

        // pick one new slice, add to priority queue.
        if (sliceIndex < sliceChain->GetCurrentSliceChainLen()) {
            SliceAddressRef toAdd = sliceChain->GetSliceAddress(static_cast<int32_t>(sliceIndex));
            if (toAdd == nullptr) {
                continue;
            }
            SliceScore scoreEntry(toAdd, queue, entry.mIndexContext, mAccessRecord->AccessCount());
            priorityQueue.emplace(scoreEntry);
            sliceIndex++;
        }
    }

    // cleanup.
    std::vector<std::shared_ptr<LogicalSliceChain>>().swap(chainArray);
    std::vector<uint32_t>().swap(chainIndexArray);
    while (!priorityQueue.empty()) {
        priorityQueue.pop();
    }
    return candidates;
}

/**
 * 冷热数据计算得分，选择得分最低的Group
 */
void FullSortEvictor::SelectMinScoreGroup(uint32_t fileSize, BucketGroupRef &minGroup, float &minScore) const
{
    minScore = std::numeric_limits<float>::max();
    std::shared_ptr<BucketGroupIterator> groupIterator = mBucketGroupManager->GetBucketGroups();
    while (groupIterator->HasNext()) {
        BucketGroupRef bucketGroup = groupIterator->Next();
        if (bucketGroup == nullptr) {
            LOG_ERROR("BucketGroupIterator error, bucketGroup is nullptr");
            return;
        }
        float score = 0.0;
        auto sliceChainIterator = std::make_shared<SliceChainIterator>(bucketGroup->GetSliceBucketIndex(),
                                                                       bucketGroup->GetBucketGroupRange());
        uint64_t size = 0;
        uint64_t baseSize = 0;
        while (sliceChainIterator->HasNext()) {
            SliceIndexContextRef context = sliceChainIterator->Next();
            if (UNLIKELY(context == nullptr)) {
                LOG_ERROR("SliceChainIterator error! context is nullptr!");
                return;
            }
            LogicalSliceChainRef chain = context->GetLogicalSliceChain();
            if (UNLIKELY(chain == nullptr)) {
                LOG_ERROR("chain is nullptr!");
                return;
            }
            if (UNLIKELY(!chain->IsSimpleSliceChain())) {  // 复合的sliceChain不参与算分.
                continue;
            }
            CalculateScore(chain, score, size, baseSize);
        }

        // 当选择的totalSize大于等于32M且分数小于最小得分, 则更新minScore和minGroup.
        if (size >= fileSize && baseSize != 0) {
            float thisScore = score / static_cast<float>(baseSize);
            if (thisScore < minScore) {
                minScore = thisScore;
                minGroup = bucketGroup;
            }
        }
    }
}

void FullSortEvictor::CalculateScore(LogicalSliceChainRef &chain, float &score, uint64_t &size,
                                     uint64_t &baseSize) const
{
    bool baseSlice = true;
    for (uint32_t k = 0; k < chain->GetCurrentSliceChainLen(); ++k) {
        SliceAddressRef sliceAddress = chain->GetSliceAddress(k);
        if (UNLIKELY(sliceAddress == nullptr)) {
            LOG_WARN("sliceAddress is nullptr, index:" << k);
            continue;
        }
        if (!sliceAddress->IsTriggerFlush() && !sliceAddress->IsEvicted()) {
            size += sliceAddress->GetDataLen();
            if (baseSlice) {
                baseSlice = false;
                baseSize += sliceAddress->GetDataLen();
                score += static_cast<float>(sliceAddress->GetDataLen()) *
                         sliceAddress->Score(mAccessRecord->AccessCount());
            }
        }
    }
}

uint64_t FullSortEvictor::TriggerFlush(std::vector<SliceScore> &entryList, BucketGroupRef &bucketGroup, bool isSync)
{
    if (UNLIKELY(entryList.empty())) {
        LOG_WARN("Trigger flush data slices is empty.");
        return 0L;
    }

    std::stable_sort(entryList.begin(), entryList.end(), [](const SliceScore &a, const SliceScore &b) {
        return a.mQueue < b.mQueue;  // 按照queue升序排列
    });

    auto iter = mFlushQueueForBucketGroupMap.find(bucketGroup);
    if (UNLIKELY(iter == mFlushQueueForBucketGroupMap.end())) {
        LOG_ERROR("Not found bucket group, not need exec flush.");
        return 0L;
    }
    bool ret = iter->second->SubmitJob(entryList, shared_from_this(), isSync);
    if (UNLIKELY(!ret)) {
        LOG_ERROR("Submit flush job failed, isSync:" << isSync);
        for (SliceScore &entry : entryList) {
            if (!entry.mSliceAddress->SetStatus(SliceEvent::FLUSH_BACK)) {
                LOG_ERROR("Rollback flush status to normal failed, slice address status:"
                          << static_cast<uint32_t>(entry.mSliceAddress->GetSliceStatus()));
            }
            mWaterMarkManager->SubEvictingMemory(entry.mSliceAddress->GetDataLen());
        }
        return 0L;
    }

    uint64_t totalLen = 0;
    for (auto &scoreEntry : entryList) {
        totalLen += scoreEntry.mSliceAddress->GetDataLen();
    }
    return totalLen;
}

BResult FullSortEvictor::InitFlushQueues()
{
    std::shared_ptr<BucketGroupIterator> bucketGroupIterator = mBucketGroupManager->GetBucketGroups();
    while (bucketGroupIterator->HasNext()) {
        BucketGroupRef bucketGroup = bucketGroupIterator->Next();
        if (mFlushQueueForBucketGroupMap.find(bucketGroup) == mFlushQueueForBucketGroupMap.end()) {
            // 新建
            FlushQueueForBucketGroupRef flushQueue = std::make_shared<FlushQueueForBucketGroup>();
            // 此时mBoostNativeMetric还未被注册，需要传递该指针的地址来保证flushQueue调用mBoostNativeMetric时能够调用到对象
            flushQueue->Initialize(shared_from_this(), mService, bucketGroup, &mBoostNativeMetric);
            mFlushQueueForBucketGroupMap[bucketGroup] = flushQueue;
        }
    }
    return BSS_OK;
}

BResult FullSortEvictor::FlushQueueForBucketGroup::Initialize(const FullSortEvictorRef &fullSortEvictor,
                                                              const ExecutorServicePtr &service,
                                                              const BucketGroupRef &bucketGroup,
                                                              BoostNativeMetricPtr *metricPtrAddr)
{
    if (fullSortEvictor == nullptr) {
        LOG_ERROR("fullSortEvictor is nullptr");
        return BSS_ERR;
    }
    if (service == nullptr) {
        LOG_ERROR("service is nullptr");
        return BSS_ERR;
    }
    if (bucketGroup == nullptr) {
        LOG_ERROR("bucketGroup is nullptr");
        return BSS_ERR;
    }
    mBucketGroup = bucketGroup;
    mService = service;
    mFullSortEvictor = fullSortEvictor;
    mMetricPtrAddr = metricPtrAddr;
    return BSS_OK;
}

bool FullSortEvictor::FlushQueueForBucketGroup::SubmitJob(std::vector<SliceScore> &entryList,
                                                          const FullSortEvictorRef &fullSortEvictor, bool isSync)
{
    BoostNativeMetricPtr metricPtr = mMetricPtrAddr == nullptr ? nullptr : *mMetricPtrAddr;
    RunnablePtr bucketGroupFlushTask =
        std::make_shared<BucketGroupFlushTask>(entryList, mBucketGroup->GetBucketGroupId(), fullSortEvictor,
                                               shared_from_this(), mBucketGroup->GetLsmStore(), metricPtr);
    if (isSync) {
        bucketGroupFlushTask->Run();  // 直接在当前线程上下文中同步执行.
        return true;
    }
    return mService->Execute(bucketGroupFlushTask);  // 提交到ExecutorService中异步执行.
}

}  // namespace bss
}  // namespace ock