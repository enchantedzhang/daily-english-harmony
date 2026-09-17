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

#ifndef BOOST_SS_SNAPSHOT_META_H
#define BOOST_SS_SNAPSHOT_META_H

#include <unordered_set>
#include <vector>

#include "blob_store/blob_file_meta.h"
#include "common/path.h"

namespace ock {
namespace bss {
class SnapshotMeta {
public:
    SnapshotMeta() = default;

    SnapshotMeta(uint64_t localFullSize, uint64_t localIncrementalSize,
                 std::unordered_set<PathRef, PathHash, PathEqual> &localFilePaths)
        : mLocalFullSize(localFullSize), mLocalIncrementalSize(localIncrementalSize), mLocalFilePaths(localFilePaths)
    {
    }

    SnapshotMeta(std::vector<uint32_t> &localFileIds, uint64_t localFullSize, uint64_t localIncrementalSize)
        : mLocalFileIds(localFileIds), mLocalFullSize(localFullSize), mLocalIncrementalSize(localIncrementalSize)
    {
    }

    inline std::unordered_set<PathRef, PathHash, PathEqual> &GetLocalFilePaths()
    {
        return mLocalFilePaths;
    }

    inline void AddLocalFilePaths(const PathRef &path)
    {
        mLocalFilePaths.emplace(path);
    }

    inline std::vector<uint32_t> GetLocalFileIds() const
    {
        return mLocalFileIds;
    }

    inline void AddLocalFilePath(const PathRef &path)
    {
        mLocalFilePaths.emplace(path);
    }

    inline void AddLocalFilePaths(std::unordered_set<PathRef, PathHash, PathEqual> localFilePaths)
    {
        mLocalFilePaths.insert(localFilePaths.begin(), localFilePaths.end());
    }

    inline void AddLocalFileIds(std::vector<uint32_t> localFileIds)
    {
        mLocalFileIds.insert(mLocalFileIds.end(), localFileIds.begin(), localFileIds.end());
    }

    inline void AddLocalFileId(uint32_t fileId)
    {
        mLocalFileIds.emplace_back(fileId);
    }

    inline uint64_t GetLocalFullSize() const
    {
        return mLocalFullSize;
    }

    inline void AddLocalFullSize(uint64_t size)
    {
        mLocalFullSize += size;
    }

    inline uint64_t GetLocalIncrementalSize() const
    {
        return mLocalIncrementalSize;
    }

    inline void AddLocalIncrementalSize(uint64_t size)
    {
        mLocalIncrementalSize += size;
    }

    inline void AddSnapshotMeta(const std::shared_ptr<SnapshotMeta> &snapshotMeta)
    {
        AddLocalFileIds(snapshotMeta->GetLocalFileIds());
        AddLocalFilePaths(snapshotMeta->GetLocalFilePaths());
        AddLocalFullSize(snapshotMeta->GetLocalFullSize());
        AddLocalIncrementalSize(snapshotMeta->GetLocalIncrementalSize());
    }

    inline void AddSnapshotBlobMeta(const BlobFileMetaRef &blobFileMeta)
    {
        AddLocalFileId(FileAddressUtils::GetFileId(blobFileMeta->GetFileAddress()));
        PathRef filePath = std::make_shared<Path>(Uri(blobFileMeta->GetIdentifier()));
        AddLocalFilePath(filePath);
        AddLocalFullSize(blobFileMeta->GetFileSize());
        AddLocalIncrementalSize(blobFileMeta->GetFileSize());
    }

private:
    std::vector<uint32_t> mLocalFileIds;
    uint64_t mLocalFullSize = 0;
    uint64_t mLocalIncrementalSize = 0;
    std::unordered_set<PathRef, PathHash, PathEqual> mLocalFilePaths;
};
using SnapshotMetaRef = std::shared_ptr<SnapshotMeta>;

}  // namespace bss
}  // namespace ock
#endif  // BOOST_SS_SNAPSHOT_META_H