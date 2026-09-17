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

#include "blob_store_snapshot_operator.h"

#include "blob_store_snapshot_coordinator.h"

namespace ock {
namespace bss {
BResult BlobStoreSnapshotOperator::SyncSnapshot(bool isSavepoint)
{
    mBlobStore->FlushCurrentBlobFile(mSnapshotId);
    RETURN_ERROR_AS_NULLPTR(mSnapshotCoordinator);
    return mSnapshotCoordinator->SyncSnapshotForBlob(mSnapshotId);
}

SnapshotMetaRef BlobStoreSnapshotOperator::OutputMeta(uint64_t snapshotId, const FileOutputViewRef &localOutputView)
{
    std::lock_guard<std::mutex> lk(mResourceMutex);
    if (mIsReleased.load()) {
        return nullptr;
    }
    RETURN_NULLPTR_AS_NULLPTR(mSnapshotCoordinator);
    SnapshotMetaRef snapshotMeta = mSnapshotCoordinator->WriteMeta(mSnapshotId, localOutputView);
    RETURN_NULLPTR_AS_NULLPTR(snapshotMeta);
    LOG_INFO("Blob store output meta success, snapshotId: " << snapshotId);
    return snapshotMeta;
}

void BlobStoreSnapshotOperator::InternalRelease()
{
    std::lock_guard<std::mutex> lk(mResourceMutex);
    if (mIsReleased.load()) {
        return;
    }
    mBlobStore->ReleaseSnapshot(mSnapshotId);
    // 置空避免循环引用.
    mSnapshotCoordinator = nullptr;
    mIsReleased.store(true);
}

}  // namespace bss
}  // namespace ock