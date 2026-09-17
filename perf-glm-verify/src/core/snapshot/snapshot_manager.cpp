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

#include "snapshot_manager.h"

namespace ock {
namespace bss {
BResult SnapshotManager::RegisterPendingSnapshot(const SnapshotOperatorCoordinatorRef &pendingSnapshot)
{
    uint64_t snapshotId = pendingSnapshot->GetSnapshotId();
    std::lock_guard<std::mutex> lock(mMutex);
    if (UNLIKELY(mClosed)) {
        LOG_WARN("Cannot register snapshot " << snapshotId << ", because snapshot manager is closed.");
        return BSS_NOT_READY;
    }
    if (UNLIKELY(mPendingSnapshots.find(snapshotId) != mPendingSnapshots.end())) {
        LOG_WARN("Repeated register pending checkpoint " << snapshotId << ".");
        return BSS_ALREADY_DONE;
    }
    if (mCompletedSnapshots.find(snapshotId) != mCompletedSnapshots.end()) {
        LOG_WARN("The checkpoint snapshot " << snapshotId << " has been completed.");
        return BSS_ALREADY_DONE;
    }
    mPendingSnapshots.emplace(snapshotId, pendingSnapshot);
    return BSS_OK;
}

BResult SnapshotManager::RegisterPendingSavepoint(const PendingSavepointCoordinatorRef &savepoint)
{
    uint64_t snapshotId = savepoint->GetSnapshotId();
    std::lock_guard<std::mutex> lock(mMutex);
    if (UNLIKELY(mClosed)) {
        LOG_ERROR("Cannot register savepoint " << snapshotId << ", because snapshot manager is closed.");
        return BSS_ALREADY_DONE;
    }
    if (UNLIKELY(mPendingSavepoints.find(snapshotId) != mPendingSavepoints.end())) {
        LOG_ERROR("Repeated register pending savepoint " << snapshotId << ".");
        return BSS_INNER_ERR;
    }
    mPendingSavepoints.insert({ snapshotId, savepoint });
    LOG_DEBUG("Register pending savepoint " << snapshotId << " success.");
    return BSS_OK;
}

SnapshotOperatorCoordinatorRef SnapshotManager::GetSnapshotCoordinator(uint64_t snapshotId)
{
    std::lock_guard<std::mutex> lock(mMutex);
    auto iter = mPendingSnapshots.find(snapshotId);
    if (UNLIKELY(iter == mPendingSnapshots.end())) {
        LOG_ERROR("The snapshot " << snapshotId << " not exist.");
        return nullptr;
    }
    return iter->second;
}

void SnapshotManager::CompletePendingSnapshot(uint64_t snapshotId)
{
    std::lock_guard<std::mutex> lock(mMutex);
    if (UNLIKELY(mClosed)) {
        return;
    }
    auto iter = mPendingSnapshots.find(snapshotId);
    if (iter == mPendingSnapshots.end()) {
        return;
    }
    auto coordinator = iter->second;
    for (const auto &item : coordinator->GetRegisterSnapshotOperator()) {
        item.second->Success();
    }
    mPendingSnapshots.erase(snapshotId);
}

void SnapshotManager::Close()
{
    std::lock_guard<std::mutex> lock(mMutex);
    if (UNLIKELY(mClosed)) {
        LOG_INFO("Snapshot manager is closed.");
        return;
    }
    mClosed = true;
    for (const auto &pendingSnapshot : mPendingSnapshots) {
        pendingSnapshot.second->Cancel();
        LOG_INFO("Cancel pending snapshot " << pendingSnapshot.first << " when snapshot manager is closed.");
    }
    for (const auto &pendingSavepoint : mPendingSavepoints) {
        pendingSavepoint.second->Cancel();
        LOG_INFO("Cancel pending savepoint " << pendingSavepoint.first << " when snapshot manager is closed.");
    }
    mPendingSnapshots.clear();
    mCompletedSnapshots.clear();
    mPendingSavepoints.clear();
    LOG_INFO("Close snapshot manager success.");
}

}  // namespace bss
}  // namespace ock