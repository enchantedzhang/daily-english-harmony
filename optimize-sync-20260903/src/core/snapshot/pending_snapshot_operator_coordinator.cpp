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

#include "pending_snapshot_operator_coordinator.h"

#include "fresh_table_snapshot_operator.h"
#include "slice_table_snapshot_operator.h"
#include "snapshot_meta.h"
#include "snapshot_restore_utils.h"

namespace ock {
namespace bss {
BResult PendingSnapshotOperatorCoordinator::Start()
{
    // 检查并更新snapshot状态.
    State expected = State::CREATED;
    if (!atomic_compare_exchange_strong(&mState, &expected, State::RUNNING)) {
        return BSS_ERR;
    }

    // 关闭SliceCompaction
    mSliceTable->SetCompactionStatus(SliceTableCompaction::CLOSED);

    // 1. FreshTable执行checkpoint.
    auto freshTableSnapshotOperator = std::make_shared<FreshTableSnapshotOperator>(AllocateOperatorId(), mFreshTable,
                                                                                   mConfig, mMemManager, mPqTables);
    RegisterSnapshotOperator(freshTableSnapshotOperator);
    freshTableSnapshotOperator->Start();
    RETURN_NOT_OK(freshTableSnapshotOperator->SyncSnapshot(IsSavepoint()));

    // 添加compaction、evict任务到两个线程池阻塞异步任务, 保证打快照元数据时数据为静止状态.
    SnapshotSyncTaskRef compactionTask = std::make_shared<SnapshotSyncTask>(SnapshotStatusType::COMPACTION);
    SnapshotSyncTaskRef evictTask = std::make_shared<SnapshotSyncTask>(SnapshotStatusType::EVICT);
    AddSnapshotSyncTask(compactionTask, evictTask);

    // SliceTable打快照前必须确保待处理的compaction和evict流程都处理完成, 并且快照过程中不执行异步流程.
    uint32_t times = NO_1;
    auto start = std::chrono::high_resolution_clock::now();
    while (!CheckAsyncTaskSuspend(compactionTask, evictTask)) {
        if ((times++) % NO_100 == 0) {
            auto end = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
            double elapsed = duration.count() / 1e3;  // 转换为ms
            LOG_WARN("Slice table check compact and evict task suspend cost time:" << elapsed << "ms.");
        }
        usleep(NO_100000);  // 100ms
    }

    // 2. SliceTable执行checkpoint.
    auto sliceTableSnapshotOperator = std::make_shared<SliceTableSnapshotOperator>(AllocateOperatorId(), mSliceTable,
                                                                                   mConfig, mMemManager,
                                                                                   GetSnapshotId());
    RegisterSnapshotOperator(sliceTableSnapshotOperator);
    sliceTableSnapshotOperator->Start();
    RETURN_NOT_OK(sliceTableSnapshotOperator->SyncSnapshot(IsSavepoint()));

    // 3. BlobStore执行checkpoint.
    RETURN_NOT_OK(DoBlobStoreSnapshot());

    // 4. FileStore执行checkpoint.
    auto fileStoreSnapshotOperator = mSliceTable->PrepareFileStoreSnapshot(AllocateOperatorId(), GetSnapshotId());
    RegisterSnapshotOperator(fileStoreSnapshotOperator);
    fileStoreSnapshotOperator->Start();
    auto ret = fileStoreSnapshotOperator->SyncSnapshot(IsSavepoint());

    // 释放同步任务, 恢复异步线程运行
    compactionTask->FinishSnapshot();
    evictTask->FinishSnapshot();
    return ret;
}

BResult PendingSnapshotOperatorCoordinator::DoBlobStoreSnapshot()
{
    auto blobStoreSnapshotOperator = mSliceTable->PrepareBlobStoreSnapshot(AllocateOperatorId(), GetSnapshotId());
    if (LIKELY(blobStoreSnapshotOperator != nullptr)) {
        RegisterSnapshotOperator(blobStoreSnapshotOperator);
        blobStoreSnapshotOperator->Start();
        BResult result = blobStoreSnapshotOperator->SyncSnapshot(IsSavepoint());
        if (UNLIKELY(result == BSS_BLOB_NOT_SNAPSHOT)) {
            // 如果没有blob快照,则删除快照调度器
            blobStoreSnapshotOperator->InternalRelease();
            UnregisterSnapshotOperator(blobStoreSnapshotOperator);
            LOG_INFO("Current blob files is empty, not need snapshot.");
            return BSS_OK;
        }
        RETURN_NOT_OK(result);
    }
    return BSS_OK;
}

BResult PendingSnapshotOperatorCoordinator::WriteMeta()
{
    PathRef localSnapshotMetaPath = std::make_shared<Path>(mLocalSnapshotPath, "metadata");
    FileOutputViewRef localOutputView = std::make_shared<FileOutputView>();
    if (IsLocalSnapshot()) {
        // 1.创建snapshot meta的fileOutputView, 文件名为metadata.
        RETURN_NOT_OK(localOutputView->Init(localSnapshotMetaPath, mConfig));
        std::vector<AbstractSnapshotOperatorRef> snapshotOperators;
        snapshotOperators.reserve(mRegisteredSnapshotOperators.size());
        for (const auto &pair : mRegisteredSnapshotOperators) {
            snapshotOperators.emplace_back(pair.second);
        }

        // 2. 写入DB的元数据.
        auto totalSnapshotMeta = SnapshotRestoreUtils::WriteDbMeta(mSnapshotId, mStartKeyGroup, mEndKeyGroup, mSeqId,
                                                                   mStateIdProvider, mLocalSnapshotPath,
                                                                   localOutputView, mLocalFileManager,
                                                                   snapshotOperators, mSnapshotStat);
        if (UNLIKELY(totalSnapshotMeta == nullptr)) {
            return BSS_ERR;
        }
        localOutputView->Close();

        // 3. 若snapshot路径为本地, 则通过建立硬链接实现upload.
        if (IsLocalSnapshot()) {
            auto ret = CreateHardLinkForLocalFiles(mLocalSnapshotPath, totalSnapshotMeta->GetLocalFileIds());
            if (UNLIKELY(ret != BSS_OK)) {
                LOG_ERROR("Create hard link for local file failed, ret:" << ret);
                return BSS_ERR;
            }
        }
        mSnapshotStat->SetEndTime(
            std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch())
                .count());
        mSnapshotStat->SetTotalSnapshotMeta(totalSnapshotMeta);
    }

    SuccessSnapshot();
    LOG_DEBUG("Async snapshot write meta end, snapshot file path:" << localSnapshotMetaPath->ExtractFileName());
    return BSS_OK;
}

BResult PendingSnapshotOperatorCoordinator::CreateHardLinkForLocalFiles(const PathRef &basePath,
                                                                        const std::vector<uint32_t> &fileIds)
{
    if (access(basePath->Name().c_str(), F_OK) != 0) {
        LOG_ERROR("Local working directory " << basePath->ExtractFileName() << " does not exist");
        return BSS_ERR;
    }

    for (auto &fileId : fileIds) {
        FileInfoRef fileInfo = mLocalFileManager->GetFileInfo(fileId);
        if (UNLIKELY(fileInfo == nullptr)) {
            LOG_ERROR("File info is nullptr, fileId:" << fileId);
            return BSS_ERR;
        }

        PathRef srcFile = fileInfo->GetFilePath();
        PathRef targetFile = std::make_shared<Path>(basePath, srcFile->ExtractFileName());
        auto ret = link(srcFile->Name().c_str(), targetFile->Name().c_str());
        if (ret != 0) {
            LOG_ERROR("Create hard link failed, errno:" << errno << ", from " << srcFile->ExtractFileName() << " to "
                                                        << targetFile->ExtractFileName());
            return BSS_ERR;
        }
    }
    return BSS_OK;
}

void PendingSnapshotOperatorCoordinator::SuccessSnapshot()
{
    std::lock_guard<std::mutex> lock(mMutex);
    for (const auto &snapshotOperator : mRegisteredSnapshotOperators) {
        snapshotOperator.second->InternalRelease();
    }
    mRegisteredSnapshotOperators.clear();
    State expected = State::RUNNING;
    atomic_compare_exchange_strong(&mState, &expected, State::COMPLETED);
}

void PendingSnapshotOperatorCoordinator::FailSnapshot()
{
    std::lock_guard<std::mutex> lock(mMutex);
    for (const auto &snapshotOperator : mRegisteredSnapshotOperators) {
        snapshotOperator.second->Cancel();
    }
    mRegisteredSnapshotOperators.clear();
}

void PendingSnapshotOperatorCoordinator::Cancel()
{
    {
        std::lock_guard<std::mutex> lock(mMutex);
        if (IsFinalState()) {
            return;
        }
        mState.store(State::CANCELED);
    }
    FailSnapshot();
}

}  // namespace bss
}  // namespace ock