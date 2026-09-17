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

#ifndef BOOST_SS_BOOST_STATE_DB_IMPL_H
#define BOOST_SS_BOOST_STATE_DB_IMPL_H

#include "common/bss_metric.h"
#include "fresh_table/fresh_table.h"
#include "include/boost_state_db.h"
#include "lsm_store/file/file_cache_factory.h"
#include "pq_table.h"
#include "slice_table/slice_table.h"
#include "snapshot/savepoint_data_view.h"
#include "transform/fresh_transformer.h"

namespace ock {
namespace bss {
class BoostStateDBImpl : public BoostStateDB {
public:
    BoostStateDBImpl() = default;

    ~BoostStateDBImpl() override
    {
        std::unordered_map<std::string, TableRef>().swap(mTables);
        LOG_INFO("Delete BoostStateDBImpl success.");
    }

    BResult Open(const ConfigRef &config) override;

    void Close() override;

    TableRef GetTableOrCreate(TableDescriptionRef tableDesc) override;

    PQTableRef CreatePQTable(const std::string &str) override;

    BResult UpdateTtlConfig(TableDescriptionRef tableDesc) override;

    SnapshotOperatorCoordinator *CreateSyncCheckpoint(const std::string &checkpointPath,
                                                      uint64_t checkpointId) override;

    BResult Restore(std::vector<std::string> &restorePath, std::unordered_map<std::string, std::string> &pathMap,
                    bool isLazyDownload, bool isNewJob) override;

    BResult CreateAsyncCheckpoint(uint64_t checkpointId, bool isIncremental) override;

    SavepointDataView *TriggerSavepoint() override;

    inline uint32_t GetDbGroupId() override
    {
        return mConfig->GetTaskSlotFlag();
    }

    void ForceTriggerTransform() override
    {
        mFreshTransformer->ForceTriggerTransform();
    }

    void ForceTriggerEvict() override
    {
        mSliceTable->ForceTriggerEvict();
    }

    void ForceTriggerCompaction() override
    {
        mSliceTable->ForceTriggerCompaction();
    }

    /**
     * For testing, force clean current version.
     */
    void ForceCleanCurrentVersion() override
    {
        mFreshTransformer->Exit();
        mSliceTable->ForceCleanCurrentVersion();
    }

    void NotifyDBSnapshotAbort(uint64_t checkpointId) override;

    void NotifyDBSnapshotComplete(uint64_t checkpointId) override;

    void RegisterMetric(BoostNativeMetricPtr metricPtr) override;

    /*
     * for testing
     */
    inline MemManagerRef &GetMemManager()
    {
        return mMemManager;
    }

    inline SliceTableManagerRef &GetSliceTable()
    {
        return mSliceTable;
    }

    inline FreshTableRef &GetFreshTable()
    {
        return mFreshTable;
    }

    inline Config &GetConfig() override
    {
        return *mConfig;
    }

    void UpdateSliceEvictWaterMark(int64_t highMark) override
    {
        mSliceTable->SetMemHighMark(highMark);
    }

private:
    void StartMemoryMonitor();
    void CreateCacheAndFileManager(const ConfigRef &config);
    void InnerClose();
    void DecreaseRefCounts(uint64_t checkpointId);

private:
    std::unordered_map<std::string, TableRef> mTables{};
    std::unordered_map<std::string, PQTableRef> mPQTable;
    ConfigRef mConfig;
    MemManagerRef mMemManager;
    FileCacheManagerRef mFileCache;
    FileCacheFactoryRef mFileCacheFactory;
    FileManagerRef mLocalFileManager;
    FileManagerRef mRemoteFileManager;
    FreshTableRef mFreshTable;
    SliceTableManagerRef mSliceTable;
    FreshTransformerRef mFreshTransformer;
    SnapshotManagerRef mSnapshotManager;
    StateIdProviderRef mStateIdProvider;
    StateFilterManagerRef mStateFilterManager;
    KeyGroupUtilRef mKeyGroupUtil;
    SeqGeneratorRef mSeqGenerator;
    ExecutorServiceRef mCacheExecutorService;
    std::unordered_map<std::string, uint32_t> mSliceRefCounts;
    BoostNativeMetricPtr mBoostNativeMetric = nullptr;
#if ENABLE_MEMORY_MONITOR
    ExecutorServicePtr mExecutor;
    MemManagerMonitorTaskRef mMemManagerMonitorTask;
#endif
};

}  // namespace bss
}  // namespace ock
#endif  // BOOST_SS_BOOST_STATE_DB_IMPL_H
