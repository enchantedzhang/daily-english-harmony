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

#ifndef BLOB_FILE_GROUP_H
#define BLOB_FILE_GROUP_H
#include <vector>

#include "blob_immutable_file.h"

namespace ock {
namespace bss {
class BlobFileGroup;
using BlobFileGroupRef = std::shared_ptr<BlobFileGroup>;
class BlobFileGroup {
public:
    ~BlobFileGroup()
    {
        std::vector<BlobImmutableFileRef>().swap(mFiles);
    }

    explicit BlobFileGroup(const GroupRangeRef &range) : mGroupRange(range)
    {
    }

    BlobFileGroup(const GroupRangeRef &range, const std::vector<BlobImmutableFileRef> &files)
        : mGroupRange(range), mFiles(files)
    {
    }

    const std::vector<BlobImmutableFileRef> &GetFiles()
    {
        return mFiles;
    }

    uint64_t GetFileSize()
    {
        ReadLocker<ReadWriteLock> lock(&mLock);
        uint64_t size = 0;
        for (const auto &immutableFile : mFiles) {
            CONTINUE_LOOP_AS_NULLPTR(immutableFile);
            auto fileMeta = immutableFile->GetBlobFileMeta();
            CONTINUE_LOOP_AS_NULLPTR(fileMeta);
            size += fileMeta->GetFileSize();
        }
        return size;
    }

    const GroupRangeRef &GetGroupRange()
    {
        return mGroupRange;
    }

    bool Contains(uint64_t blobId, uint32_t keyGroup)
    {
        std::vector<BlobImmutableFileRef> files = GetFiles();
        if (UNLIKELY(files.empty())) {
            return false;
        }
        if (UNLIKELY(keyGroup > INT32_MAX)) {
            LOG_WARN("KeyGroup too big, blobId: " << blobId << ", keyGroup: " << keyGroup);
            return false;
        }
        auto firstFile = files[0];
        RETURN_FALSE_AS_NULLPTR(firstFile);
        auto lastFile = files[files.size() - NO_1];
        RETURN_FALSE_AS_NULLPTR(lastFile);
        auto firstFileMeta = firstFile->GetBlobFileMeta();
        RETURN_FALSE_AS_NULLPTR(firstFileMeta);
        auto lastFileMeta = lastFile->GetBlobFileMeta();
        RETURN_FALSE_AS_NULLPTR(lastFileMeta);
        return blobId >= firstFileMeta->GetMinBlobId() && blobId <= lastFileMeta->GetMaxBlobId() &&
               mGroupRange->ContainsGroup((int32_t)keyGroup);
    }

    BResult AddLastFile(const BlobImmutableFileRef &file);

    BlobFileGroupRef DeepCopy(uint64_t version);

    void Restore(std::vector<BlobImmutableFileRef> &blobFiles)
    {
        WriteLocker<ReadWriteLock> lock(&mLock);
        mFiles = blobFiles;
    }

    BResult RestoreFileFooterAndIndexBlock()
    {
        for (const auto &item : mFiles) {
            RETURN_NOT_OK(item->RestoreFileFooterAndIndexBlock());
        }
        return BSS_OK;
    }

    std::vector<BlobImmutableFileRef> RegisterCompaction(uint32_t start, uint32_t fileCount)
    {
        if (UNLIKELY(start + fileCount > mFiles.size())) {
            LOG_ERROR("Invalid file range, start: " << start << ", fileCount: " << fileCount
                                                    << ", fileSize: " << mFiles.size());
            return {};
        }
        std::vector<BlobImmutableFileRef> compactionFiles;
        WriteLocker<ReadWriteLock> lock(&mLock);
        for (uint32_t i = start; i < start + fileCount; i++) {
            auto file = mFiles[i];
            CONTINUE_LOOP_AS_NULLPTR(file);
            mInCompactionFiles.emplace(file);
            compactionFiles.emplace_back(file);
        }
        return compactionFiles;
    }

    void UnregisterCompaction(const std::vector<BlobImmutableFileRef> &files)
    {
        WriteLocker<ReadWriteLock> lock(&mLock);
        for (const auto &item : files) {
            mInCompactionFiles.erase(item);
        }
    }

    BResult MigrateFiles(uint32_t startIndex, uint32_t endIndex, std::vector<BlobImmutableFileRef> &newFiles)
    {
        WriteLocker<ReadWriteLock> lock(&mLock);
        // 替换范围包括start, 不包括end
        if (UNLIKELY(startIndex > endIndex || endIndex > mFiles.size() || newFiles.empty())) {
            LOG_ERROR("Invalid file range, startIndex: " << startIndex << ", endIndex: " << endIndex << ", fileSize: "
                                                         << mFiles.size() << ", newFiles size: " << newFiles.size());
            return BSS_ERR;
        }
        mFiles.erase(mFiles.begin() + startIndex, mFiles.begin() + endIndex);
        mFiles.insert(mFiles.begin() + startIndex, newFiles.begin(), newFiles.end());
        return BSS_OK;
    }

    std::vector<BlobImmutableFileRef> CleanExpireFiles(uint64_t blobFileRetainTimeInMill)
    {
        WriteLocker<ReadWriteLock> lock(&mLock);
        std::vector<BlobImmutableFileRef> ret;
        auto file = mFiles.begin();
        while (file != mFiles.end()) {
            BlobImmutableFileRef blobImmutableFile = *file;
            CONTINUE_LOOP_AS_NULLPTR(blobImmutableFile);
            auto fileMeta = blobImmutableFile->GetBlobFileMeta();
            CONTINUE_LOOP_AS_NULLPTR(fileMeta);
            auto minExpire = fileMeta->GetMinExpireTime();
            auto maxExpire = fileMeta->GetMaxExpireTime();
            auto now = TimeStampUtil::GetCurrentTime();
            if (minExpire > 0 && now > maxExpire + blobFileRetainTimeInMill) {
                LOG_INFO("Blob file is expire, address: " << fileMeta->GetFileAddress() << ", minExpire: " << minExpire
                                                          << ", maxExpire: " << maxExpire << ", now: " << now);
                ret.emplace_back(*file);
                file = mFiles.erase(file);
                continue;
            }
            ++file;
        }
        return ret;
    }

private:
    GroupRangeRef mGroupRange = nullptr;
    std::vector<BlobImmutableFileRef> mFiles;
    ReadWriteLock mLock;
    std::set<BlobImmutableFileRef> mInCompactionFiles;
};

}  // namespace bss
}  // namespace ock

#endif
