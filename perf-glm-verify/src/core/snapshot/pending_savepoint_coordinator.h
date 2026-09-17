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

#ifndef BOOST_SS_PENDING_SAVEPOINT_COORDINATOR_H
#define BOOST_SS_PENDING_SAVEPOINT_COORDINATOR_H

#include "file_store_snapshot_operator.h"
#include "fresh_table/fresh_table.h"
#include "fresh_table_snapshot_operator.h"
#include "kv_table/stateId_provider.h"
#include "slice_table_snapshot_operator.h"

namespace ock {
namespace bss {
class SnapshotManager;
using SnapshotManagerRef = std::shared_ptr<SnapshotManager>;

class PendingSavepointCoordinator : public SnapshotOperatorCoordinator {
public:
    enum class State { CREATED, RUNNING, COMPLETED, FAILED, CANCELED };

    PendingSavepointCoordinator(const SnapshotManagerRef &snapshotManager, const FreshTableRef &freshTable,
                                const SliceTableManagerRef &sliceTable, uint64_t savepointId,
                                const StateIdProviderRef &stateIdProvider, const MemManagerRef &memManager,
                                const std::vector<PQTableRef> &pqTable)
        : mSnapshotManagerRef(snapshotManager),
          mFreshTable(freshTable),
          mSliceTable(sliceTable),
          mSavepointId(savepointId),
          mStateIdProvider(stateIdProvider),
          mMemManager(memManager),
          mPqTables(pqTable)
    {
    }

    ~PendingSavepointCoordinator() override = default;

    inline uint64_t GetSnapshotId() override
    {
        return mSavepointId;
    }

    BResult Start() override;

    BResult DoBlobStoreSnapshot();

    void Cancel() override;

    void FailSnapshot() override;

    inline bool IsFinalState() const
    {
        return mState == State::CANCELED || mState == State::FAILED;
    }

    inline uint32_t AllocateOperatorId() override
    {
        return mOperatorIdCounter.fetch_add(1, std::memory_order_relaxed);
    }

    inline void RegisterSnapshotOperator(const AbstractSnapshotOperatorRef &snapshotOperator) override
    {
        mRegisteredSnapshotOperators.emplace(snapshotOperator->GetOperatorId(), snapshotOperator);
    }

    inline void UnregisterSnapshotOperator(const AbstractSnapshotOperatorRef &snapshotOperator) override
    {
        mRegisteredSnapshotOperators.erase(snapshotOperator->GetOperatorId());
    }

    inline StateIdProviderRef GetStateIdProviderSnapshot()
    {
        return mStateIdProvider;
    }

    std::map<uint32_t, AbstractSnapshotOperatorRef> GetRegisterSnapshotOperator() override
    {
        return mRegisteredSnapshotOperators;
    }

    inline SliceTableManagerRef &GetSliceTable()
    {
        return mSliceTable;
    }

private:
    SnapshotManagerRef mSnapshotManagerRef = nullptr;
    FreshTableRef mFreshTable = nullptr;
    SliceTableManagerRef mSliceTable = nullptr;
    uint64_t mSavepointId = 0;
    StateIdProviderRef mStateIdProvider = nullptr;
    std::mutex mMutex;
    std::atomic<State> mState{ State::CREATED };
    std::atomic<uint32_t> mOperatorIdCounter{ 0 };
    MemManagerRef mMemManager = nullptr;
    std::map<uint32_t, AbstractSnapshotOperatorRef> mRegisteredSnapshotOperators;
    std::vector<PQTableRef> mPqTables;
};
using PendingSavepointCoordinatorRef = Ref<PendingSavepointCoordinator>;

}  // namespace bss
}  // namespace ock
#endif  // BOOST_SS_PENDING_SAVEPOINT_COORDINATOR_H