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

#ifndef BOOST_STATE_DB_H
#define BOOST_STATE_DB_H

#include <unordered_map>

#include "auto_closeable.h"
#include "bss_err.h"
#include "config.h"
#include "table.h"
#include "table_description.h"

namespace ock {
namespace bss {
class SavepointDataView;
class SnapshotOperatorCoordinator;
class BoostNativeMetric;
class PQTable;
using PQTableRef = std::shared_ptr<PQTable>;

class BoostStateDB : public AutoCloseable {
public:
    ~BoostStateDB() override = default;
    /**
     * Open state store by configuration.
     * @return return BSS_OK if success, else return BSS_ERR.
     */
    virtual BResult Open(const ConfigRef &config) = 0;

    /**
     * Close state store, destroy all resource.
     */
    virtual void Close() = 0;

    /**
     * Get table or create table if it does not exist.
     * @param tableDesc table description
     * @return return table instance if success, else return nullptr.
     */
    virtual TableRef GetTableOrCreate(TableDescriptionRef tableDesc) = 0;

    /**
     * create pqtable if not exist.
     * @return return pqtable instance if success, else return nullptr.
     */
    virtual PQTableRef CreatePQTable(const std::string &str) = 0;

    /**
     * update TTL fiter manager
     * @param tableDesc table description
     * @return return BSS_OK if success, else return BSS_ERR.
     */
    virtual BResult UpdateTtlConfig(TableDescriptionRef tableDesc) = 0;

    /**
     * State store db synchronizes the snapshot process, and does memory snapshot
     * processing of fresh table and slice table
     * @param checkpointPath path of check point
     * @param checkpointId checkpointId
     */
    virtual SnapshotOperatorCoordinator *CreateSyncCheckpoint(const std::string &checkpointPath,
                                                              uint64_t checkpointId) = 0;

    /**
     * State store db asynchronous snapshot process, fresh table and slice
     * table synchronous process record data written to disk
     * @param checkpointPath path of check point
     */
    virtual BResult CreateAsyncCheckpoint(uint64_t checkpointId, bool isIncremental) = 0;

    /**
     * Restore for incremental snapshot.
     * @param restorePath checkpoint path.
     * @return return BSS_OK if success, else return BSS_ERR.
     */
    virtual BResult Restore(std::vector<std::string> &restorePath,
                            std::unordered_map<std::string, std::string> &lazyPathMapping, bool isLazyDownload = false,
                            bool isNewJob = true) = 0;

    /**
     * Trigger save point.
     * @return
     */
    virtual SavepointDataView *TriggerSavepoint() = 0;

    /**
     * 通知DB取消本次checkpoint任务，主要进行状态重置，资源释放
     *
     * @param checkpointId 当前要取消的checkpoint任务Id
     */
    virtual void NotifyDBSnapshotAbort(uint64_t checkpointId) = 0;

    /**
     * 通知DB完成本次checkpoint任务，清理backup目录的过时slice文件
     *
     * @param checkpointId 当前要完成的checkpoint任务Id
     */
    virtual void NotifyDBSnapshotComplete(uint64_t checkpointId) = 0;

    /**
     * Register metric for all modules
     *
     * @param metricPtr BoostNativeMetric*
     */
    virtual void RegisterMetric(BoostNativeMetric *metricPtr) = 0;

    /**
     * Get boost db state group id
     * @return
     */
    virtual uint32_t GetDbGroupId() = 0;

    /**
     * For testing, force trigger transform flush table to slice table.
     */
    virtual void ForceTriggerTransform() = 0;

    /**
     * For testing, force trigger evict slice table to lsm store.
     */
    virtual void ForceTriggerEvict() = 0;

    /**
     * For testing, force trigger compaction lsm store.
     */
    virtual void ForceTriggerCompaction() = 0;

    /**
     * For testing, force clean current version.
     */
    virtual void ForceCleanCurrentVersion() = 0;

    /**
     * Get current config instance.
     * @return Config config instance
     */
    virtual Config &GetConfig() = 0;

    /**
     * Update slice table evict water mark
     * @param highMark 内存淘汰高水位
     */
    virtual void UpdateSliceEvictWaterMark(int64_t highMark) = 0;

    /**
     * Update heap available size.
     * @param newSize new size.
     */
    static uint64_t ChangeHeapAvailableSize(uint64_t newSize);
};
using BoostStateDBPtr = BoostStateDB *;

class BoostStateDBFactory {
public:
    /**
     * Create boost state store.
     * @return boost state store instance.
     */
    static BoostStateDBPtr Create();

    /**
     * Destroy boost state store. db is auto closable, so will destroy by
     * Java_com_huawei_ock_bss_jni_AbstractNativeHandleReference_close
     * @param db boost state store instance.
     */
    static void Destroy(BoostStateDBPtr &db);
};
}  // namespace bss
}  // namespace ock

#endif