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

#ifndef BLOB_STORE_SNAPSHOT_COORDINATOR_H
#define BLOB_STORE_SNAPSHOT_COORDINATOR_H
#include "abstract_snapshot_operator.h"
#include "blob_file_manager.h"
#include "snapshot_meta.h"

namespace ock {
namespace bss {
class BlobStoreSnapshotOperator;
using BlobStoreSnapshotOperatorRef = std::shared_ptr<BlobStoreSnapshotOperator>;
class TombstoneService;
using TombstoneServiceRef = std::shared_ptr<TombstoneService>;
class TombstoneFileManager;
using TombstoneFileManagerRef = std::shared_ptr<TombstoneFileManager>;
class TombstoneFileGroup;
using TombstoneFileGroupRef = std::shared_ptr<TombstoneFileGroup>;
class TombstoneFileManagerSnapshot;
using TombstoneFileManagerSnapshotRef = std::shared_ptr<TombstoneFileManagerSnapshot>;
class BlobStoreSnapshotCoordinator {
public:
    BlobStoreSnapshotCoordinator(MemManagerRef &memManager, std::vector<TombstoneServiceRef> &tombstoneLevel0,
                                 uint64_t blobStoreVersion, uint64_t seqId, BlobFileManagerRef &blobFileManager,
                                 TombstoneFileManagerRef &tombstoneFileManager,
                                 BlobStoreSnapshotOperatorRef &blobStoreSnapshotOperator,
                                 FileCacheManagerRef &fileCacheManager)
        : mMemManager(memManager),
          mTombstoneLevel0(tombstoneLevel0),
          mVersion(blobStoreVersion),
          mSeqId(seqId),
          mBlobFileManager(blobFileManager),
          mTombstoneFileManager(tombstoneFileManager),
          mBlobStoreSnapshotOperator(blobStoreSnapshotOperator),
          mFileCacheManager(fileCacheManager)
    {
    }

    uint64_t GetVersion() const
    {
        return mVersion;
    }

    const BlobFileManagerRef &GetBlobFileManager()
    {
        return mBlobFileManager;
    }

    BResult SyncSnapshotForBlob(uint64_t snapshotId);

    SnapshotMetaRef WriteMeta(uint64_t snapshotId, const FileOutputViewRef &localFileOutputView);

    void ReleaseResource();

private:
    BResult DoSyncSnapshot(uint64_t version);
    SnapshotMetaRef SerializeFileMeta(OutputViewRef &primaryOutputView);
    BResult SerializeGroupRange(const BlobFileGroupRef &blobFileGroup, OutputViewRef &outputViews);
    BResult SerializeBlobFiles(const BlobFileGroupRef &blobFileGroup, OutputViewRef &outputViews,
                               const SnapshotMetaRef &snapshotMeta);
    uint64_t SyncSnapshotBlobFileMeta(FileMetaInfoMap &toSnapshotFileAddress);

private:
    bool mBlobFilesReady{ false };
    MemManagerRef mMemManager = nullptr;
    std::vector<TombstoneServiceRef> mTombstoneLevel0;
    uint64_t mVersion = 0;
    uint64_t mSeqId = 0;
    BlobFileManagerRef mBlobFileManager = nullptr;
    TombstoneFileManagerRef mTombstoneFileManager = nullptr;
    BlobStoreSnapshotOperatorRef mBlobStoreSnapshotOperator = nullptr;
    FileCacheManagerRef mFileCacheManager = nullptr;
    BlobFileGroupManagerRef mBlobFileGroupSnapshot = nullptr;
    TombstoneFileManagerSnapshotRef mTombstoneFileManagerSnapshot = nullptr;
    ReadWriteLock mRwLock;
};
using BlobStoreSnapshotCoordinatorRef = std::shared_ptr<BlobStoreSnapshotCoordinator>;
}  // namespace bss
}  // namespace ock

#endif  // BLOB_STORE_SNAPSHOT_COORDINATOR_H
