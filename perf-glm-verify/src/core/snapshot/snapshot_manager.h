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

#ifndef BOOST_SS_SNAPSHOT_MANAGER_H
#define BOOST_SS_SNAPSHOT_MANAGER_H

#include <map>
#include <mutex>

#include "lsm_store/file/file_cache_manager.h"
#include "lsm_store/file/file_manager.h"
#include "pending_savepoint_coordinator.h"
#include "pending_snapshot_operator_coordinator.h"

namespace ock {
namespace bss {
class SnapshotManager {
public:
    SnapshotManager(FileManagerRef &remoteFileManager, const FileCacheManagerRef fileCache)
        : mRemoteFileManager(remoteFileManager), mFileCache(fileCache)
    {
    }

    ~SnapshotManager() = default;

    BResult RegisterPendingSnapshot(const SnapshotOperatorCoordinatorRef &pendingSnapshot);

    BResult RegisterPendingSavepoint(const PendingSavepointCoordinatorRef &pendingSavepointCoordinator);

    SnapshotOperatorCoordinatorRef GetSnapshotCoordinator(uint64_t snapshotId);

    void CompletePendingSnapshot(uint64_t snapshotId);

    void Close();

    inline uint64_t AllocateSavepointId()
    {
        return mSavepointIdCounter.fetch_add(NO_1);
    }

    inline void Restore(uint64_t snapshotId)
    {
        mCompletedSnapshots.emplace(snapshotId, nullptr);
    }

    inline void NotifySavepointAbort(uint64_t snapshotId)
    {
        std::lock_guard<std::mutex> lock(mMutex);
        if (mPendingSavepoints.find(snapshotId) != mPendingSavepoints.end()) {
            auto savepointCoordinator = mPendingSavepoints[snapshotId];
            savepointCoordinator->Cancel();
            LOG_INFO("Notify to abort the pending savepoint " << snapshotId);
            mPendingSavepoints.erase(snapshotId);
        }
    }

    inline void NotifySnapshotAbort(uint64_t snapshotId)
    {
        std::lock_guard<std::mutex> lock(mMutex);
        if (mClosed) {
            LOG_INFO("Can't notify snapshot abort, snapshotId:" << snapshotId << ", because snapshot manager closed.");
            return;
        }
        if (mPendingSnapshots.find(snapshotId) != mPendingSnapshots.end()) {
            auto snapshotCoordinator = mPendingSnapshots[snapshotId];
            snapshotCoordinator->Cancel();
            mPendingSnapshots.erase(snapshotId);
            LOG_INFO("Notify to abort the pending snapshot " << snapshotId);
        }
    }

private:
    FileManagerRef mRemoteFileManager = nullptr;
    FileCacheManagerRef mFileCache = nullptr;
    std::mutex mMutex;
    std::map<uint64_t, SnapshotOperatorCoordinatorRef> mPendingSnapshots;
    std::map<uint64_t, SnapshotOperatorCoordinatorRef> mCompletedSnapshots;
    std::map<uint64_t, PendingSavepointCoordinatorRef> mPendingSavepoints;
    std::atomic<uint64_t> mSavepointIdCounter{ 0 };
    bool mClosed = false;
};
using SnapshotManagerRef = std::shared_ptr<SnapshotManager>;

}  // namespace bss
}  // namespace ock
#endif  // BOOST_SS_SNAPSHOT_MANAGER_H
