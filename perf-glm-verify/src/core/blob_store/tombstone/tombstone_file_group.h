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

#ifndef BOOST_SS_TOMBSTONE_FILE_GROUP_H
#define BOOST_SS_TOMBSTONE_FILE_GROUP_H

#include "tombstone_file.h"
#include "tombstone_file_sub_vec.h"

namespace ock {
namespace bss {

class TombstoneFileGroup {
public:
    explicit TombstoneFileGroup(std::vector<TombstoneFileRef> &files) : mFiles(files)
    {
    }

    inline std::vector<TombstoneFileRef> GetFiles()
    {
        ReadLocker<ReadWriteLock> lock(&mRwLock);
        return mFiles;
    }

    inline void CopyAndClearFiles(std::vector<TombstoneFileRef> &files)
    {
        WriteLocker<ReadWriteLock> lock(&mRwLock);
        for (const auto &file : mFiles) {
            files.emplace_back(file);
        }
        mFiles.clear();
    }

    inline uint64_t GetFileSize()
    {
        uint64_t totalFileSize = 0;
        ReadLocker<ReadWriteLock> lock(&mRwLock);
        for (const auto &item : mFiles) {
            CONTINUE_LOOP_AS_NULLPTR(item);
            auto fileMeta = item->GetFileMeta();
            CONTINUE_LOOP_AS_NULLPTR(fileMeta);
            totalFileSize += fileMeta->GetFileSize();
        }
        return totalFileSize;
    }

    inline uint64_t CalTombstoneNum(uint64_t minBlobId)
    {
        uint64_t tombstoneNum = 0;
        ReadLocker<ReadWriteLock> lock(&mRwLock);
        for (const auto &item : mFiles) {
            CONTINUE_LOOP_AS_NULLPTR(item);
            auto fileMeta = item->GetFileMeta();
            CONTINUE_LOOP_AS_NULLPTR(fileMeta);
            if (fileMeta->GetMaxBlobId() >= minBlobId) {
                tombstoneNum += fileMeta->CalValidBlobIDNum();
            }
        }
        return tombstoneNum;
    }

    TombstoneFileSubVecRef FindOverLapFile(uint64_t minBlobId, uint64_t maxBlobId)
    {
        if (UNLIKELY(minBlobId > maxBlobId)) {
            LOG_WARN("minBlobId is big than maxBlobId, min:" << minBlobId << ", max:" << maxBlobId);
            return nullptr;
        }
        ReadLocker<ReadWriteLock> lock(&mRwLock);
        if (mFiles.empty()) {
            LOG_INFO("Tombstone file is empty.");
            return nullptr;
        }
        int32_t low = 0;
        int32_t filesSize = static_cast<int32_t>(mFiles.size());
        int32_t high = filesSize - 1;
        int32_t minBlobFileIndex = 0;
        while (low <= high) {
            int32_t mid = low + ((high - low) / 2);
            auto file = mFiles.at(mid);
            RETURN_NULLPTR_AS_NULLPTR(file);
            auto fileMeta = file->GetFileMeta();
            RETURN_NULLPTR_AS_NULLPTR(fileMeta);
            if (fileMeta->GetMinBlobId() > minBlobId) {
                high = mid - 1;
                continue;
            }
            if (fileMeta->GetMaxBlobId() >= minBlobId) {
                minBlobFileIndex = mid;
                break;
            }
            low = mid + 1;
        }

        std::vector<TombstoneFileRef> res;
        for (int32_t i = minBlobFileIndex; i < filesSize; i++) {
            auto file = mFiles.at(i);
            RETURN_NULLPTR_AS_NULLPTR(file);
            auto fileMeta = file->GetFileMeta();
            RETURN_NULLPTR_AS_NULLPTR(fileMeta);
            if (maxBlobId < file->GetFileMeta()->GetMinBlobId()) {
                break;
            }
            res.emplace_back(file);
        }
        return std::make_shared<TombstoneFileSubVec>(res, minBlobFileIndex);
    }

    void MigrateFile(const TombstoneFileSubVecRef &fileSubVec, const std::vector<TombstoneFileRef> &newFiles)
    {
        if (UNLIKELY(fileSubVec == nullptr || (fileSubVec->GetFileVec().empty() && newFiles.empty()))) {
            return;
        }

        WriteLocker<ReadWriteLock> lock(&mRwLock);
        if (fileSubVec->GetEndIndex() > mFiles.size()) {
            LOG_ERROR("File index is invalid, endIndex:" << fileSubVec->GetEndIndex()
                                                         << ", files size:" << mFiles.size());
            return;
        }
        // end index也是需要删除的
        mFiles.erase(mFiles.begin() + fileSubVec->GetStartIndex(), mFiles.begin() + fileSubVec->GetEndIndex() + 1);
        mFiles.insert(mFiles.begin(), newFiles.begin(), newFiles.end());
    }

    std::shared_ptr<TombstoneFileGroup> Snapshot()
    {
        std::vector<TombstoneFileRef> files;
        ReadLocker<ReadWriteLock> lock(&mRwLock);
        for (const auto &item : mFiles) {
            CONTINUE_LOOP_AS_NULLPTR(item);
            auto newFile = item->Snapshot();
            files.emplace_back(newFile);
        }
        return std::make_shared<TombstoneFileGroup>(files);
    }

    bool Empty()
    {
        ReadLocker<ReadWriteLock> lock(&mRwLock);
        return mFiles.empty();
    }

    void CleanExpireTombstoneFile(uint64_t minBlobId, std::vector<TombstoneFileRef> &pendingDeleteFiles)
    {
        WriteLocker<ReadWriteLock> lock(&mRwLock);
        auto file = mFiles.begin();
        while (file != mFiles.end()) {
            TombstoneFileRef tombstoneFile = *file;
            CONTINUE_LOOP_AS_NULLPTR(tombstoneFile);
            TombstoneFileMetaRef tombstoneFileMeta = tombstoneFile->GetFileMeta();
            CONTINUE_LOOP_AS_NULLPTR(tombstoneFileMeta);
            if (tombstoneFileMeta->GetMaxBlobId() < minBlobId) {
                pendingDeleteFiles.emplace_back(tombstoneFile);
                file = mFiles.erase(file);
                continue;
            }
            ++file;
        }
    }

    std::vector<TombstoneFileMetaRef> GetTombstoneFileMetas()
    {
        std::vector<TombstoneFileMetaRef> fileMetas;
        ReadLocker<ReadWriteLock> lock(&mRwLock);
        fileMetas.reserve(mFiles.size());
        for (const auto &file : mFiles) {
            CONTINUE_LOOP_AS_NULLPTR(file);
            TombstoneFileMetaRef tombstoneFileMeta = file->GetFileMeta();
            CONTINUE_LOOP_AS_NULLPTR(tombstoneFileMeta);
            tombstoneFileMeta->IncRef();
            fileMetas.emplace_back(tombstoneFileMeta);
        }
        return fileMetas;
    }

    void SelectRangeFile(std::vector<TombstoneFileRef> &selectedVec, uint64_t &minBlobId, uint64_t &maxBlobId,
                         TombstoneFileMetaRef &largestFileMeta)
    {
        WriteLocker<ReadWriteLock> lock(&mRwLock);
        auto file = mFiles.begin();
        while (file != mFiles.end()) {
            auto fileMeta = (*file)->GetFileMeta();
            CONTINUE_LOOP_AS_NULLPTR(fileMeta);
            if (fileMeta->GetMinBlobId() > largestFileMeta->GetMaxBlobId() ||
                fileMeta->GetMaxBlobId() < largestFileMeta->GetMinBlobId()) {
                ++file;
                continue;
            }
            selectedVec.emplace_back((*file));
            minBlobId = std::min(minBlobId, fileMeta->GetMinBlobId());
            maxBlobId = std::max(maxBlobId, fileMeta->GetMaxBlobId());
            file = mFiles.erase(file);
        }
    }

private:
    std::vector<TombstoneFileRef> mFiles;
    ReadWriteLock mRwLock;
};
using TombstoneFileGroupRef = std::shared_ptr<TombstoneFileGroup>;
}  // namespace bss
}  // namespace ock

#endif  // BOOST_SS_TOMBSTONE_FILE_GROUP_H
