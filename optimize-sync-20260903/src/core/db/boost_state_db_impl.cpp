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

#include "boost_state_db_impl.h"

#include <atomic>
#include <iostream>

#include "boost_state_dbgroup.h"
#include "common/bss_log.h"
#include "kv_table/kv_table_factory.h"
#include "snapshot/restore_operator.h"
#include "snapshot/snapshot_manager.h"

namespace ock {
namespace bss {
std::atomic<uint64_t> AutoCloseable::GLOBAL_COUNT(0);

BResult BoostStateDBImpl::Open(const ConfigRef &config)
{
    mConfig = config;
    auto ret = KeyGroupUtil::Create(mConfig->GetMaxNumberOfParallelSubtasks(), mKeyGroupUtil);
    if (UNLIKELY(ret != BSS_OK)) {
        LOG_ERROR("Failed to initialize key group util, ret:" << ret);
        return ret;
    }
    ReadWriteLock &taskSlotLock = BoostStateDbGroupMgr::GetOrCreateLock(config->GetTaskSlotFlag());
    WriteLocker<ReadWriteLock> lock(&taskSlotLock);

    // initialize memory manager.
    ret = BoostStateDbGroupMgr::Create(config->GetTaskSlotFlag(), mConfig);
    if (ret != BSS_OK) {
        LOG_ERROR("Failed create db group, ret:" << ret);
        InnerClose();
        return BSS_ERR;
    }
    ret = BoostStateDbGroupMgr::GetMemManager(config->GetTaskSlotFlag(), mMemManager);
    if (ret != BSS_OK) {
        LOG_ERROR("Failed to get memory manager. ret:" << ret);
        InnerClose();
        return BSS_ERR;
    }

    // start memory usage monitor
    StartMemoryMonitor();
    // create stateIdProvider, filterManager and stateMigration.
    mStateIdProvider = std::make_shared<StateIdProvider>(mConfig->GetStartGroup(), mConfig->GetEndGroup(), mMemManager);
    mStateFilterManager = std::make_shared<StateFilterManager>(mStateIdProvider, config, mConfig->GetStartGroup(),
                                                               mConfig->GetEndGroup(), mKeyGroupUtil);

    // initialize fresh table.
    mFreshTable = std::make_shared<FreshTable>();
    ret = mFreshTable->Initialize(mConfig, mMemManager, mKeyGroupUtil);
    if (ret != BSS_OK) {
        LOG_ERROR("Failed to initialize fresh table. ret:" << ret);
        InnerClose();
        return BSS_ERR;
    }

    mCacheExecutorService = std::make_shared<ExecutorService>(1, NO_1024);
    mCacheExecutorService->SetThreadName("LsmLazyIOExecutor");
    if (!mCacheExecutorService->Start()) {
        LOG_ERROR("Failed to start lazy download IO executor service, ret:" << ret);
        InnerClose();
        return BSS_ERR;
    }

    // create cache and file manager.
    CreateCacheAndFileManager(mConfig);

    // create slice table.
    mSliceTable = std::make_shared<SliceTable>();
    ret = mSliceTable->Initialize(mConfig, mFileCache, mMemManager, mStateFilterManager);
    if (ret != BSS_OK) {
        LOG_ERROR("Failed to initialize slice table. ret:" << ret);
        InnerClose();
        return BSS_ERR;
    }

    // initialize fresh table to slice table transformer.
    mFreshTransformer = std::make_shared<FreshTransformer>();
    ret = mFreshTransformer->Init(mConfig, mFreshTable, mSliceTable);
    if (ret != BSS_OK) {
        LOG_ERROR("Failed to initialize fresh table transformer. ret:" << ret);
        InnerClose();
        return BSS_ERR;
    }

    // create snapshot manager.
    mSnapshotManager = std::make_shared<SnapshotManager>(mRemoteFileManager, mFileCache);
    mSeqGenerator = std::make_shared<SeqGenerator>();

    // open fresh table.
    mFreshTable->Open();

    auto result = BoostStateDbGroupMgr::Add(mConfig->GetTaskSlotFlag(), this);
    if (UNLIKELY(result != BSS_OK)) {
        LOG_ERROR("Add boost state db to db group failed, ret:" << result);
        InnerClose();
        return result;
    }

    LOG_INFO("Boost state db opened, db group:" << mConfig->GetTaskSlotFlag());
    return BSS_OK;
}

void BoostStateDBImpl::StartMemoryMonitor()
{
#if ENABLE_MEMORY_MONITOR
    mExecutor = std::make_shared<ExecutorService>(NO_1, NO_1);
    mExecutor->SetThreadName("MemMonitorExecutor");
    if (!mExecutor->Start()) {
        LOG_ERROR("Failed to start memory monitor executor service.");
        return;
    }
    mMemManagerMonitorTask = std::make_shared<MemManagerMonitorTask>(mMemManager);
    mExecutor->Execute(mMemManagerMonitorTask);
#endif
}

void BoostStateDBImpl::CreateCacheAndFileManager(const ConfigRef &config)
{
    mFileCacheFactory = std::make_shared<FileCacheFactory>(config, mCacheExecutorService, &mBoostNativeMetric);
    mFileCache = mFileCacheFactory->GetFileCache();
    mLocalFileManager = mFileCacheFactory->GetLocalFileManager();
    mRemoteFileManager = mFileCacheFactory->GetDfsFileManager();
}

void BoostStateDBImpl::Close()
{
    ReadWriteLock &taskSlotLock = BoostStateDbGroupMgr::GetOrCreateLock(mConfig->GetTaskSlotFlag());
    WriteLocker<ReadWriteLock> lock(&taskSlotLock);
    InnerClose();
}

void BoostStateDBImpl::InnerClose()
{
    auto result = BoostStateDbGroupMgr::Remove(GetDbGroupId(), this);
    if (UNLIKELY(result != BSS_OK)) {
        LOG_ERROR("Remove boost state db from db group failed, ret:" << result);
    }

    if (mSliceTable != nullptr) {
        mSliceTable->StopSliceTable();
    }
    if (mFreshTransformer != nullptr) {
        mFreshTransformer->Exit();
    }
    if (mSliceTable != nullptr) {
        mSliceTable->Close();
        mSliceTable->Exit();
    }
    if (mFreshTable != nullptr) {
        mFreshTable->Exit();
    }
    if (mMemManager != nullptr) {
        mMemManager->Exit();
        mMemManager->DeregisterMetric(reinterpret_cast<uintptr_t>(this));
    }

    if (mSnapshotManager != nullptr) {
        mSnapshotManager->Close();
    }

    if (mCacheExecutorService != nullptr) {
        mCacheExecutorService->Stop();
    }

    if (!mPQTable.empty()) {
        while (mPQTable.begin() != mPQTable.end()) {
            mPQTable.begin()->second->Close();
            mPQTable.erase(mPQTable.begin());
        }
    }
    BlockCacheRef blockCache = BlockCacheManager::Instance()->GetBlockCache(mConfig->GetTaskSlotFlag());
    if (LIKELY(blockCache != nullptr)) {
        blockCache->DeRegisterMetric(mConfig);
    }

#if ENABLE_MEMORY_MONITOR
    if (mMemManagerMonitorTask != nullptr) {
        mMemManagerMonitorTask->StopTask();
    }

    if (mExecutor != nullptr) {
        mExecutor->Stop();
    }
#endif
    // 释放DbGroup指针
    BoostStateDbGroupMgr::DeleteDbGroupPtr(GetDbGroupId());
    LOG_INFO("Boost state db closed, db group:" << GetDbGroupId());
}

uint64_t BoostStateDB::ChangeHeapAvailableSize(uint64_t newSize)
{
    return MemManager::ChangeHeapOverSize(newSize);
}

TableRef BoostStateDBImpl::GetTableOrCreate(TableDescriptionRef tableDesc)
{
    RETURN_NULLPTR_AS_NULLPTR(tableDesc);
    TableRef table = mTables[tableDesc->GetTableName()];
    if (table == nullptr) {
        table = KVTableFactory::CreateFromDescription(tableDesc);
        RETURN_NULLPTR_AS_NULLPTR(table);
        mTables[tableDesc->GetTableName()] = table;
    }
    AbstractTableRef abstractTable = std::dynamic_pointer_cast<AbstractTable>(table);
    if (tableDesc->GetTableTTL() > 0 && !mConfig->GetTtlFilterSwitch()) {
        LOG_WARN("TTL expiration data compaction is not enabled.");
        tableDesc->SetTableTTL(-1);
    }
    auto ret = abstractTable->Init(mFreshTable, mSliceTable, mStateIdProvider, tableDesc, mSeqGenerator,
                                   tableDesc->GetTableTTL(), mStateFilterManager);
    return ret == BSS_OK ? table : nullptr;
}

PQTableRef BoostStateDBImpl::CreatePQTable(const std::string &str)
{
    PQTableRef table = mPQTable[str];
    if (table == nullptr) {
        TableSerializer tblSerializer;
        TableDescriptionRef des = std::make_shared<TableDescription>(PQ, str, -1, tblSerializer, *mConfig);
        auto bucketGroupManager = mSliceTable->GetBucketGroupManager();
        if (UNLIKELY(bucketGroupManager == nullptr || bucketGroupManager->GetBucketGroupVector().empty())) {
            return nullptr;
        }
        auto lsmStore = bucketGroupManager->GetBucketGroupVector()[0]->GetLsmStore();
        table = std::make_shared<PQTable>(mMemManager, mFreshTransformer->GetTransformExecutor(), lsmStore, str,
                                          mStateIdProvider, des, mKeyGroupUtil);
        auto ret = table->Initialize();
        if (ret != BSS_OK) {
            return nullptr;
        }
        mPQTable[str] = table;
    }
    return mPQTable[str];
}

BResult BoostStateDBImpl::UpdateTtlConfig(TableDescriptionRef tableDesc)
{
    RETURN_INVALID_PARAM_AS_NULLPTR(tableDesc);
    TableRef table = mTables[tableDesc->GetTableName()];
    if (table != nullptr) {
        AbstractTableRef abstractTable = std::dynamic_pointer_cast<AbstractTable>(table);
        if (tableDesc->GetTableTTL() > 0) {
            if (!mConfig->GetTtlFilterSwitch()) {
                LOG_WARN("TTL expiration data compaction is not enabled.");
                return BSS_INNER_ERR;
            }
            auto &stateFilter = abstractTable->UpdateTtl(tableDesc->GetTableTTL());
            mStateFilterManager->RegisterStateFilter(tableDesc, stateFilter);
        }
        return BSS_OK;
    }
    return BSS_NOT_READY;
}

SnapshotOperatorCoordinator *BoostStateDBImpl::CreateSyncCheckpoint(const std::string &checkpointPath,
                                                                    uint64_t checkpointId)
{
    auto start = std::chrono::high_resolution_clock::now();
    PathRef snapshotPath = std::make_shared<Path>(Uri(checkpointPath));
    LOG_INFO("Create sync checkpoint start, checkpointId:" << checkpointId
                                                           << ", snapshotPath:" << snapshotPath->ExtractFileName());
    std::vector<PQTableRef> pqTables;
    for (const auto &item : mPQTable) {
        pqTables.emplace_back(item.second);
    }
    PendingSnapshotOperatorCoordinatorRef snapshotOperatorCoordinator =
        MakeRef<PendingSnapshotOperatorCoordinator>(checkpointId, mConfig->GetStartGroup(), mConfig->GetEndGroup(),
                                                    mSeqGenerator->Next(), mStateIdProvider, snapshotPath,
                                                    mLocalFileManager, mRemoteFileManager, mConfig, mFreshTable,
                                                    mSliceTable, mMemManager, pqTables);
    auto ret = mSnapshotManager->RegisterPendingSnapshot(snapshotOperatorCoordinator);
    if (UNLIKELY(ret != BSS_OK)) {
        return nullptr;
    }

    ret = snapshotOperatorCoordinator->Start();
    if (UNLIKELY(ret != BSS_OK)) {
        LOG_ERROR("Start snapshot operator coordinator failed, ret:" << ret << ", checkpointId:" << checkpointId);
        NotifyDBSnapshotAbort(checkpointId);
        return nullptr;
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    double elapsed = duration.count() / 1e3;  // 转换为ms
    LOG_INFO("Create sync checkpoint success, checkpointId:" << checkpointId << ", cost time:" << elapsed << " ms.");
    return snapshotOperatorCoordinator.Get();
}

BResult BoostStateDBImpl::CreateAsyncCheckpoint(uint64_t checkpointId, bool isIncremental)
{
    LOG_INFO("Create async checkpoint start, checkpointId:" << checkpointId << ", incremental:" << isIncremental);
    auto start = std::chrono::high_resolution_clock::now();
    auto snapshotCoordinator = mSnapshotManager->GetSnapshotCoordinator(checkpointId);
    RETURN_INVALID_PARAM_AS_NULLPTR(snapshotCoordinator);
    PathRef backupPath = std::make_shared<Path>(Uri(mConfig->GetBackupPath()));

    // 1. freshTable, sliceTable和fileStore分别执行async checkpoint操作.
    for (const auto &registerSnapshotOperator : snapshotCoordinator->GetRegisterSnapshotOperator()) {
        AbstractSnapshotOperatorRef snapshotOperator = registerSnapshotOperator.second;
        auto ret = snapshotOperator->AsyncSnapshot(checkpointId, snapshotCoordinator->GetLocalSnapshotPath(),
                                                   isIncremental, mConfig->GetEnableLocalRecovery(), backupPath,
                                                   mSliceRefCounts);
        if (UNLIKELY(ret != BSS_OK)) {
            NotifyDBSnapshotAbort(checkpointId);
            return ret;
        }
    }

    // 2. 将snapshot的元数据信息写入文件中.
    auto ret = snapshotCoordinator->AcknowledgeAsyncSnapshot();
    if (UNLIKELY(ret != BSS_OK)) {
        LOG_WARN("Acknowledge async snapshot failed, ret:" << ret << ", checkpointId:" << checkpointId);
        NotifyDBSnapshotAbort(checkpointId);
        return ret;
    }

    // 3. 完成async checkpoint.
    mSnapshotManager->CompletePendingSnapshot(checkpointId);

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    double elapsed = duration.count() / 1e3;  // 转换为ms
    LOG_INFO("Create async checkpoint success, checkpointId:" << checkpointId << ", cost time:" << elapsed << " ms.");
    return BSS_OK;
}

BResult BoostStateDBImpl::Restore(std::vector<std::string> &restorePath,
                                  std::unordered_map<std::string, std::string> &lazyPathMapping, bool isLazyDownload,
                                  bool isNewJob)
{
    mConfig->SetIsNewJob(isNewJob);
    std::vector<PathRef> restoredMetaPaths;
    for (auto &item : lazyPathMapping) {
        LOG_DEBUG("Restore path mapping :" << PathTransform::ExtractFileName(item.first) << " -> "
                                           << PathTransform::ExtractFileName(item.second));
    }
    for (const auto &path : restorePath) {
        LOG_INFO("Start restore from path:" << PathTransform::ExtractFileName(path));
        restoredMetaPaths.push_back(std::make_shared<Path>(path + "/metadata"));
    }
    auto restoreOperator = std::make_shared<RestoreOperator>(mConfig, mLocalFileManager, mRemoteFileManager,
                                                             mSliceTable, mFreshTable, mStateIdProvider, mTables,
                                                             mFileCacheFactory, mSnapshotManager, mPQTable);
    auto stateIdProviderBackup = mStateIdProvider->Copy();
    uint64_t seqId = 0;
    BResult ret = restoreOperator->Restore(restoredMetaPaths, lazyPathMapping, seqId, isLazyDownload);
    if (UNLIKELY(ret != BSS_OK)) {
        BResult rollbackRet = mStateIdProvider->RestoreFrom(stateIdProviderBackup);
        if (UNLIKELY(rollbackRet != BSS_OK)) {
            LOG_ERROR("Rollback state id provider failed, ret:" << rollbackRet);
        }
        LOG_ERROR("Restore operator failed, ret:" << ret);
        return ret;
    }
    mSeqGenerator->Restore(seqId);
    for (const auto &item : mTables) {
        std::dynamic_pointer_cast<AbstractTable>(item.second)->SetStateIdProvider(mStateIdProvider);
    }
    for (const auto &item : mPQTable) {
        item.second->SetStateIdProvider(mStateIdProvider);
    }
    // restore完成, 调用lsm compaction
    mSliceTable->Open();
    return BSS_OK;
}

SavepointDataView *BoostStateDBImpl::TriggerSavepoint()
{
    auto stateIdProviderSnapshot = mStateIdProvider->Copy();
    std::vector<PQTableRef> pqTables;
    for (const auto &item : mPQTable) {
        pqTables.emplace_back(item.second);
    }
    auto pendingSavepoint = MakeRef<PendingSavepointCoordinator>(mSnapshotManager, mFreshTable, mSliceTable,
                                                                 mSnapshotManager->AllocateSavepointId(),
                                                                 stateIdProviderSnapshot, mMemManager, pqTables);
    auto ret = mSnapshotManager->RegisterPendingSavepoint(pendingSavepoint);
    if (UNLIKELY(ret != BSS_OK)) {
        LOG_ERROR("Failed to register savepoint, ret:" << ret);
        return nullptr;
    }

    ret = pendingSavepoint->Start();
    if (UNLIKELY(ret != BSS_OK)) {
        mSnapshotManager->NotifySavepointAbort(pendingSavepoint->GetSnapshotId());
        return nullptr;
    }

    return new (std::nothrow) SavepointDataView(mSnapshotManager, pendingSavepoint, mKeyGroupUtil, mMemManager);
}

void BoostStateDBImpl::NotifyDBSnapshotAbort(uint64_t checkpointId)
{
    mSnapshotManager->NotifySnapshotAbort(checkpointId);
    // 开启SliceCompaction
    mSliceTable->SetCompactionStatus(SliceTableCompaction::OPEN);
}

void BoostStateDBImpl::NotifyDBSnapshotComplete(uint64_t checkpointId)
{
    // 仅当开启本地恢复时清理backup目录的过期文件
    if (mConfig->GetEnableLocalRecovery()) {
        DecreaseRefCounts(checkpointId);
    }
    LOG_INFO("Complete checkpoint:" << checkpointId << " success!");
}

inline void BoostStateDBImpl::DecreaseRefCounts(uint64_t checkpointId)
{
    PathRef backupPath = std::make_shared<Path>(Uri(mConfig->GetBackupPath()));
    for (auto it = mSliceRefCounts.begin(); it != mSliceRefCounts.end();) {
        auto name = it->first;
        // 等于0时表明在本次cp中已经不再引用，小于0时表明在之前的cp中已经不再引用但是未删除成功
        if (it->second <= 0) {
            BResult ret = SliceTableSnapshotOperator::unlinkUseless(backupPath, it->first);
            if (UNLIKELY(ret != BSS_OK)) {
                // 部分文件未清理并不影响cp成功, 不应返回error, 但应当打印warn日志
                LOG_WARN("Failed to DecreaseRefCounts for checkpoint: " << checkpointId);
            }
            it = mSliceRefCounts.erase(it);
        } else {
            --(it->second);
            ++it;
        }
    }
}

void BoostStateDBImpl::RegisterMetric(BoostNativeMetricPtr metricPtr)
{
    LOG_INFO("Register metric instance for db of slot:" << mConfig->GetTaskSlotFlag());
    mBoostNativeMetric = metricPtr;
    mFreshTable->RegisterFreshMetric(metricPtr);
    mSliceTable->RegisterSliceMetric(metricPtr);
    mMemManager->RegisterMetric(reinterpret_cast<uintptr_t>(this), metricPtr);
}

BoostStateDBPtr BoostStateDBFactory::Create()
{
    return new BoostStateDBImpl();
}

void BoostStateDBFactory::Destroy(BoostStateDBPtr &db)
{
    delete db;
    db = nullptr;
}
}  // namespace bss
}  // namespace ock
