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

#ifndef BOOST_SS_FILE_STORE_SNAPSHOT_OPERATOR_H
#define BOOST_SS_FILE_STORE_SNAPSHOT_OPERATOR_H

#include "abstract_snapshot_operator.h"
#include "lsm_store/file/file_store_impl.h"

namespace ock {
namespace bss {
class FileStoreSnapshotOperator : public AbstractSnapshotOperator {
public:
    FileStoreSnapshotOperator(uint64_t operatorId, uint64_t snapshotId, const std::vector<LsmStoreRef> &lsmStores,
                              const FileCacheManagerRef &fileCache)
        : AbstractSnapshotOperator(operatorId), mSnapshotId(snapshotId), mLsmStores(lsmStores), mFileCache(fileCache)
    {
        for (const auto &lsmStore : lsmStores) {
            mFileStoreIds.push_back(lsmStore->GetFileStoreId());
        }
    }

    ~FileStoreSnapshotOperator() override = default;

    inline std::vector<LsmStoreRef> GetLsmStores() const
    {
        return mLsmStores;
    }

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
        return SnapshotOperatorType::FILE_STORE;
    }

    BResult WriteInfo(const FileOutputViewRef &localOutputView) override;

    void InternalRelease() override;

private:
    uint64_t mSnapshotId = 0;
    std::vector<LsmStoreRef> mLsmStores;
    FileMetaInfoMap mToSnapshotFileAddress;
    std::vector<uint64_t> mLocalOutputOffsets;
    FileCacheManagerRef mFileCache = nullptr;
    std::vector<FileStoreIDRef> mFileStoreIds;
};
using FileStoreSnapshotOperatorRef = std::shared_ptr<FileStoreSnapshotOperator>;

class FileStoreSnapshotOperatorInfo : public SnapshotOperatorInfo {
public:
    FileStoreSnapshotOperatorInfo(SnapshotOperatorType type, const std::vector<FileStoreIDRef> &fileStoreIds,
                                  const std::vector<uint64_t> &fileMetaOffsets)
        : SnapshotOperatorInfo(type), mFileStoreIds(fileStoreIds), mFileMetaOffsets(fileMetaOffsets)
    {
    }

    inline const std::vector<FileStoreIDRef> &GetFileStoreIds() const
    {
        return mFileStoreIds;
    }

    inline const std::vector<uint64_t> &GetFileMetaOffsets() const
    {
        return mFileMetaOffsets;
    }

    BResult Serialize(const FileOutputViewRef &outputView) override
    {
        // fileStore operator info内容：fileStoreId的个数+各个FileMeta偏移+fileStoreId序列化信息.
        outputView->WriteUint32(mFileStoreIds.size());
        for (uint32_t i = 0; i < mFileStoreIds.size(); i++) {
            outputView->WriteUint64(mFileMetaOffsets[i]);
            mFileStoreIds[i]->Serialize(outputView);
        }
        return BSS_OK;
    }

private:
    std::vector<FileStoreIDRef> mFileStoreIds;
    std::vector<uint64_t> mFileMetaOffsets;
};
using FileStoreSnapshotOperatorInfoRef = std::shared_ptr<FileStoreSnapshotOperatorInfo>;

}  // namespace bss
}  // namespace ock
#endif  // BOOST_SS_FILE_STORE_SNAPSHOT_OPERATOR_H