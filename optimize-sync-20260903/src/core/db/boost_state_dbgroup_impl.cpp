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

#include "boost_state_dbgroup_impl.h"

#include <utility>

#include "boost_state_db_impl.h"
#include "boost_state_dbgroup.h"

namespace ock {
namespace bss {
BResult BoostStateDbGroupImpl::Add(ock::bss::BoostStateDB *boostStateDb)
{
    WriteLocker<ReadWriteLock> lock(&mRwLock);
    for (auto &iter : mBoostStateDb) {
        if (iter == boostStateDb) {
            LOG_ERROR("Add boost state db already exit");
            return BSS_EXISTS;
        }
    }

    mBoostStateDb.push_back(boostStateDb);
    return BSS_OK;
}

BResult BoostStateDbGroupImpl::Remove(ock::bss::BoostStateDB *boostStateDb)
{
    WriteLocker<ReadWriteLock> lock(&mRwLock);
    int32_t pos = 0;
    for (auto &iter : mBoostStateDb) {
        if (iter == boostStateDb) {
            mBoostStateDb.erase(mBoostStateDb.begin() + pos);
            break;
        }
        pos++;
    }

    return BSS_OK;
}

BResult BoostStateDbGroupImpl::GetEvictFlushInfo(int64_t &useTotal, int64_t &flushingTotal)
{
    int64_t useTotalTmp = 0ULL;
    int64_t flushingTmp = 0ULL;
    for (auto &iter : mBoostStateDb) {
        CONTINUE_LOOP_AS_NULLPTR(iter);
        auto dbImpl = dynamic_cast<BoostStateDBImpl *>(iter);
        // 获取当前等待淘汰的数据大小.
        const auto &sliceTable = dbImpl->GetSliceTable();
        RETURN_ALLOC_FAIL_AS_NULLPTR(sliceTable);
        const auto &fullSortEvictor = sliceTable->GetFullSortEvictor();
        RETURN_ALLOC_FAIL_AS_NULLPTR(fullSortEvictor);
        RETURN_ALLOC_FAIL_AS_NULLPTR(fullSortEvictor->mWaterMarkManager);
        useTotalTmp += fullSortEvictor->mWaterMarkManager->GetUsedMemorySize();
        // 获取当前正在淘汰的数据大小.
        flushingTmp += fullSortEvictor->mWaterMarkManager->GetEvictingMemorySize();
    }
    useTotal = useTotalTmp;
    flushingTotal = flushingTmp;
    return BSS_OK;
}

void BoostStateDbGroupImpl::SelectEvictBoostDb(uint32_t fileSize, BoostStateDB *&boostDb, BucketGroupRef &minGroup)
{
    float minScore = FLOAT_MAX_VALUE;
    float score = 0.0;
    BoostStateDBImpl *dbImpl = nullptr;
    BucketGroupRef currentGroup = nullptr;
    boostDb = nullptr;
    minGroup = nullptr;
    for (auto &iter : mBoostStateDb) {
        CONTINUE_LOOP_AS_NULLPTR(iter);
        dbImpl = dynamic_cast<BoostStateDBImpl *>(iter);
        const auto &sliceTable = dbImpl->GetSliceTable();
        CONTINUE_LOOP_AS_NULLPTR(sliceTable);
        const auto &fullSortEvictor = sliceTable->GetFullSortEvictor();
        CONTINUE_LOOP_AS_NULLPTR(fullSortEvictor);
        fullSortEvictor->SelectMinScoreGroup(fileSize, currentGroup, score);
        if (score < minScore) {
            minScore = score;
            minGroup = currentGroup;
            boostDb = iter;
        }
    }
}

BResult BoostStateDbGroupImpl::TryEvict(bool isSync, bool isForce, uint32_t minSize)
{
    int64_t useTotal = 0ULL;
    int64_t flushingTotal = 0ULL;
    WriteLocker<ReadWriteLock> lock(&mRwLock);
    GetEvictFlushInfo(useTotal, flushingTotal);

    BoostStateDB *boostDb = nullptr;
    BucketGroupRef minGroup = nullptr;
    // 内存不足，启用低水位提前淘汰
    int64_t evictWaterMark = isForce ? mEvictLowWaterMark : mEvictHighWaterMark;
    while (isForce || (useTotal - evictWaterMark) > flushingTotal) {
        SelectEvictBoostDb(minSize != UINT32_MAX ? minSize : mEvictMinSize, boostDb, minGroup);
        if (UNLIKELY(boostDb == nullptr)) {
            LOG_DEBUG("Select boost db for evict process failed.");
            return BSS_OK;
        }
        if (UNLIKELY(minGroup == nullptr)) {
            LOG_INFO("Select minimal group for evict process failed.");
            return BSS_OK;
        }

        auto dbImpl = dynamic_cast<BoostStateDBImpl *>(boostDb);
        const auto &sliceTable = dbImpl->GetSliceTable();
        RETURN_ALLOC_FAIL_AS_NULLPTR(sliceTable);
        const auto &fullSortEvictor = sliceTable->GetFullSortEvictor();
        RETURN_ALLOC_FAIL_AS_NULLPTR(fullSortEvictor);
        // 淘汰刷盘
        uint64_t selectedSize = fullSortEvictor->Flush(minSize != UINT32_MAX ? minSize : mEvictMinSize, minGroup,
                                                       isSync);
        if (selectedSize == 0) {
            break;
        }
        LOG_INFO("Evict select bucketGroupId:"
                 << minGroup->GetBucketGroupId() << ", useTotal:" << useTotal << ", evictWaterMark:" << evictWaterMark
                 << ", flushTotal:" << flushingTotal << ", selectSize:" << selectedSize << ", isForce:" << isForce);
        RETURN_NOT_OK(GetEvictFlushInfo(useTotal, flushingTotal));
        if (isForce) {
            // 内存不足强制淘汰，至少淘汰一次
            isForce = false;
        }
    }
    return BSS_OK;
}

std::unordered_map<uint32_t, BoostStateDbGroupPtr> BoostStateDbGroupMgr::mBoostStateDbGroup;
std::unordered_map<uint32_t, std::unique_ptr<ReadWriteLock>> BoostStateDbGroupMgr::mSlotLockMap;
ReadWriteLock BoostStateDbGroupMgr::mRwLock;

BResult BoostStateDbGroupMgr::Add(uint32_t dbGroupId, BoostStateDB *boostStateDb)
{
    WriteLocker<ReadWriteLock> lock(&mRwLock);
    auto iter = mBoostStateDbGroup.find(dbGroupId);
    if (iter != mBoostStateDbGroup.end()) {
        LOG_INFO("Add boost state db to group success, groupId:" << dbGroupId);
        if (UNLIKELY(iter->second == nullptr)) {
            LOG_ERROR("iter->second is nullptr");
            return BSS_ERR;
        }
        auto ret = iter->second->Add(boostStateDb);
        iter->second->UpdateSliceEvictWaterMark();
        return ret;
    }
    LOG_ERROR("Not found db group, groupId:" << dbGroupId);
    return BSS_NOT_EXISTS;
}

BResult BoostStateDbGroupMgr::Remove(uint32_t dbGroupId, BoostStateDB *boostStateDb)
{
    WriteLocker<ReadWriteLock> lock(&mRwLock);
    auto iter = mBoostStateDbGroup.find(dbGroupId);
    if (iter != mBoostStateDbGroup.end()) {
        auto dbGroupPtr = iter->second;
        auto ret = dbGroupPtr->Remove(boostStateDb);
        if (UNLIKELY(ret != BSS_OK)) {
            return ret;
        }
        LOG_INFO("Remove boost state db, groupId:" << dbGroupId);

        MemManagerRef localMemManager = iter->second->GetMemManager();
        localMemManager->DecDbRefCount();
    }
    return BSS_OK;
}

void BoostStateDbGroupMgr::DeleteDbGroupPtr(uint32_t dbGroupId)
{
    WriteLocker<ReadWriteLock> lock(&mRwLock);
    auto iter = mBoostStateDbGroup.find(dbGroupId);
    if (iter != mBoostStateDbGroup.end()) {
        auto dbGroupPtr = iter->second;
        if (dbGroupPtr->IsEmpty()) {
            mBoostStateDbGroup.erase(iter);
            LOG_INFO("Delete db group success, groupId:" << dbGroupId);
            delete dbGroupPtr;
            dbGroupPtr = nullptr;
        }
    }
}

BoostStateDbGroupPtr BoostStateDbGroupMgr::GetBoostStateDbGroup(uint32_t dbGroupId)
{
    WriteLocker<ReadWriteLock> lock(&mRwLock);
    auto iter = mBoostStateDbGroup.find(dbGroupId);
    if (iter != mBoostStateDbGroup.end()) {
        return iter->second;
    }

    return nullptr;
}

BResult BoostStateDbGroupMgr::Create(uint32_t dbGroupId, ConfigRef config)
{
    WriteLocker<ReadWriteLock> lock(&mRwLock);
    auto iter = mBoostStateDbGroup.find(dbGroupId);
    if (iter != mBoostStateDbGroup.end()) {
        if (UNLIKELY(iter->second == nullptr)) {
            LOG_ERROR("iter->second is nullptr.");
            return BSS_ERR;
        }
        MemManagerRef localMemManager = iter->second->GetMemManager();
        RETURN_NOT_OK(localMemManager->AddDbRefCount());
        return BSS_OK;
    }

    auto memManager = std::make_shared<MemManager>(AllocatorType::MEM_MULTI);
    auto ret = memManager->Initialize(config);
    if (UNLIKELY(ret != BSS_OK)) {
        LOG_ERROR("Failed to initialize memory manager. ret:" << ret);
        return ret;
    }

    auto dbGroupPtr = new (std::nothrow) BoostStateDbGroupImpl(dbGroupId, config, memManager);
    if (UNLIKELY(dbGroupPtr == nullptr)) {
        return BSS_ALLOC_FAIL;
    }
    mBoostStateDbGroup.insert(std::make_pair(dbGroupId, dbGroupPtr));
    LOG_INFO("Create db group success, groupId:" << dbGroupId);
    return BSS_OK;
}

BResult BoostStateDbGroupMgr::GetMemManager(uint32_t dbGroupId, MemManagerRef &memManager)
{
    memManager = nullptr;
    WriteLocker<ReadWriteLock> lock(&mRwLock);
    auto iter = mBoostStateDbGroup.find(dbGroupId);
    if (LIKELY(iter != mBoostStateDbGroup.end() && iter->second != nullptr)) {
        memManager = iter->second->GetMemManager();
        return BSS_OK;
    }
    LOG_ERROR("Invalid db group, groupId:" << dbGroupId);
    return BSS_INNER_ERR;
}

ReadWriteLock &BoostStateDbGroupMgr::GetOrCreateLock(uint32_t dbGroupId)
{
    WriteLocker<ReadWriteLock> lock(&mRwLock);
    auto &lockPtr = mSlotLockMap[dbGroupId];
    if (!lockPtr) {
        lockPtr = std::make_unique<ReadWriteLock>();
    }
    return *lockPtr;
}

}  // namespace bss
}  // namespace ock