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

#ifndef BOOST_SS_BLOB_STORE_SNAPSHOT_OPERATOR_H
#define BOOST_SS_BLOB_STORE_SNAPSHOT_OPERATOR_H

#include "abstract_snapshot_operator.h"
#include "blob_store/blob_store.h"
#include "lsm_store/file/file_store_impl.h"

namespace ock {
namespace bss {
class BlobStoreSnapshotCoordinator;
using BlobStoreSnapshotCoordinatorRef = std::shared_ptr<BlobStoreSnapshotCoordinator>;

class BlobStoreSnapshotOperator : public AbstractSnapshotOperator,
                                  public std::enable_shared_from_this<BlobStoreSnapshotOperator> {
public:
    BlobStoreSnapshotOperator() = default;

    BlobStoreSnapshotOperator(uint64_t operatorId, uint64_t snapshotId, const BlobStoreRef &blobStore,
                              const FileCacheManagerRef &fileCache)
        : AbstractSnapshotOperator(operatorId), mSnapshotId(snapshotId), mBlobStore(blobStore), mFileCache(fileCache)
    {
    }

    inline void Cancel() override
    {
        AbstractSnapshotOperator::Cancel();
        mSnapshotCoordinator = nullptr;
    }

    inline void Success() override
    {
        mSnapshotCoordinator = nullptr;
    }

    ~BlobStoreSnapshotOperator() override = default;

    inline uint64_t GetSnapshotId() const
    {
        return mSnapshotId;
    }

    BResult SyncSnapshot(bool isSavepoint) override;

    BResult AsyncSnapshot(uint64_t snapshotId, const PathRef &snapshotPath, bool isIncremental,
                          bool enableLocalRecovery, const PathRef &backupPath,
                          std::unordered_map<std::string, uint32_t> &sliceRefCounts) override
    {
        return BSS_OK;
    }

    SnapshotMetaRef OutputMeta(uint64_t snapshotId, const FileOutputViewRef &localOutputView) override;

    SnapshotOperatorType GetType() override
    {
        return SnapshotOperatorType::BLOB_STORE;
    }

    void SetToSnapshotFileAddress(const FileMetaInfoMap &toSnapshotFileAddress)
    {
        mToSnapshotFileAddress = toSnapshotFileAddress;
    }

    void SetSnapshotCoordinator(const BlobStoreSnapshotCoordinatorRef &snapshotCoordinator)
    {
        mSnapshotCoordinator = snapshotCoordinator;
    }

    BResult WriteInfo(const FileOutputViewRef &localOutputView) override
    {
        return BSS_OK;
    }

    void InternalRelease() override;

    const FileMetaInfoMap &GetToSnapshotFileAddress()
    {
        return mToSnapshotFileAddress;
    }

private:
    uint64_t mSnapshotId = 0;
    BlobStoreRef mBlobStore = nullptr;
    FileMetaInfoMap mToSnapshotFileAddress;
    std::vector<uint64_t> mLocalOutputOffsets;
    FileCacheManagerRef mFileCache = nullptr;
    BlobStoreSnapshotCoordinatorRef mSnapshotCoordinator = nullptr;
};
using BlobStoreSnapshotOperatorRef = std::shared_ptr<BlobStoreSnapshotOperator>;

}  // namespace bss
}  // namespace ock
#endif  // BOOST_SS_BLOB_STORE_SNAPSHOT_OPERATOR_H