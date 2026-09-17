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

#ifndef BOOST_SS_PENDING_SNAPSHOT_OPERATOR_COORDINATOR_H
#define BOOST_SS_PENDING_SNAPSHOT_OPERATOR_COORDINATOR_H

#include "fresh_table/fresh_table.h"
#include "include/config.h"
#include "lsm_store/file/file_manager.h"
#include "pq_table.h"
#include "snapshot_operator_coordinator.h"
#include "snapshot_stat.h"

namespace ock {
namespace bss {

class PendingSnapshotOperatorCoordinator : public SnapshotOperatorCoordinator {
public:
    ~PendingSnapshotOperatorCoordinator() override = default;

    PendingSnapshotOperatorCoordinator(uint64_t snapshotId, uint32_t startKeyGroup, uint32_t endKeyGroup,
                                       uint64_t seqId, const StateIdProviderRef &stateIdProvider,
                                       const PathRef &localSnapshotPath, const FileManagerRef &localFileManager,
                                       const FileManagerRef &remoteFileManager, const ConfigRef &config,
                                       const FreshTableRef &freshTable, const SliceTableManagerRef &sliceTable,
                                       const MemManagerRef &memManager, const std::vector<PQTableRef> &pqTable)

        : mSnapshotId(snapshotId),
          mStartKeyGroup(startKeyGroup),
          mEndKeyGroup(endKeyGroup),
          mSeqId(seqId),
          mStateIdProvider(stateIdProvider),
          mLocalSnapshotPath(localSnapshotPath),
          mLocalFileManager(localFileManager),
          mRemoteFileManager(localFileManager),
          mConfig(config),
          mFreshTable(freshTable),
          mSliceTable(sliceTable),
          mSnapshotStat(std::make_shared<SnapshotStat>()),
          mMemManager(memManager),
          mPqTables(pqTable)
    {
    }

    BResult Start() override;

    BResult DoBlobStoreSnapshot();

    void RegisterSnapshotOperator(const AbstractSnapshotOperatorRef &snapshotOperator) override
    {
        std::lock_guard<std::mutex> lock(mMutex);
        mRegisteredSnapshotOperators.emplace(snapshotOperator->GetOperatorId(), snapshotOperator);
    }

    void UnregisterSnapshotOperator(const AbstractSnapshotOperatorRef &snapshotOperator) override
    {
        std::lock_guard<std::mutex> lock(mMutex);
        mRegisteredSnapshotOperators.erase(snapshotOperator->GetOperatorId());
    }

    std::map<uint32_t, AbstractSnapshotOperatorRef> GetRegisterSnapshotOperator() override
    {
        return mRegisteredSnapshotOperators;
    }

    BResult CreateHardLinkForLocalFiles(const PathRef &basePath, const std::vector<uint32_t> &fileIds);

    BResult AcknowledgeAsyncSnapshot() override
    {
        if (mState.load(std::memory_order_relaxed) != State::RUNNING) {
            return BSS_ERR;
        }
        return WriteMeta();
    }

    void SuccessSnapshot();

    void FailSnapshot() override;

    void Cancel() override;

    uint32_t AllocateOperatorId() override
    {
        return mOperatorIdCounter.fetch_add(1, std::memory_order_relaxed);
    }

    uint64_t GetSnapshotId() override
    {
        return mSnapshotId;
    }

    inline bool IsLocalSnapshot() override
    {
        return (mLocalSnapshotPath != nullptr);
    }

    inline bool IsSavepoint() override
    {
        return false;
    }

    inline PathRef GetLocalSnapshotPath() override
    {
        return mLocalSnapshotPath;
    }

    inline void AddSnapshotSyncTask(SnapshotSyncTaskRef &compactionTask, SnapshotSyncTaskRef &evictTask)
    {
        mSliceTable->AddSnapshotSyncTask(compactionTask, evictTask);
    }

    inline bool CheckAsyncTaskSuspend(SnapshotSyncTaskRef &compactionTask, SnapshotSyncTaskRef &evictTask)
    {
        if (compactionTask->GetStatus() != SnapshotStatus::RUNNING) {
            LOG_INFO("Wait evict task for checkPoint.");
            return false;
        }
        if (evictTask->GetStatus() != SnapshotStatus::RUNNING) {
            LOG_INFO("Wait compaction task for checkPoint.");
            return false;
        }
        return true;
    }

private:
    inline bool IsFinalState() const
    {
        return mState == State::FINISHED || mState == State::CANCELED || mState == State::FAILED;
    }

    enum class State { CREATED, RUNNING, COMPLETED, FINISHED, FAILED, CANCELED };

    BResult WriteMeta();

private:
    std::atomic<State> mState{ State::CREATED };
    std::atomic<uint32_t> mOperatorIdCounter{ 0 };
    std::mutex mMutex;
    std::map<uint32_t, AbstractSnapshotOperatorRef> mRegisteredSnapshotOperators;
    uint64_t mSnapshotId = 0;
    uint32_t mStartKeyGroup = 0;
    uint32_t mEndKeyGroup = 0;
    uint64_t mSeqId = 0;
    StateIdProviderRef mStateIdProvider = nullptr;
    PathRef mLocalSnapshotPath = nullptr;
    FileManagerRef mLocalFileManager = nullptr;
    FileManagerRef mRemoteFileManager = nullptr;
    ConfigRef mConfig = nullptr;
    FreshTableRef mFreshTable = nullptr;
    SliceTableManagerRef mSliceTable = nullptr;
    SnapshotStatRef mSnapshotStat = nullptr;
    MemManagerRef mMemManager = nullptr;
    std::vector<PQTableRef> mPqTables;
};
using PendingSnapshotOperatorCoordinatorRef = Ref<PendingSnapshotOperatorCoordinator>;

}  // namespace bss
}  // namespace ock
#endif  // BOOST_SS_PENDING_SNAPSHOT_OPERATOR_COORDINATOR_H