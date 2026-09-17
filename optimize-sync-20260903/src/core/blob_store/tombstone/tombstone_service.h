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

#ifndef BOOST_SS_TOMBSTONESERVICE_H
#define BOOST_SS_TOMBSTONESERVICE_H

#include <queue>

#include "binary/key_value.h"
#include "blob_store.h"
#include "blob_store/blob_store.h"
#include "include/config.h"
#include "lsm_store/file/group_range.h"
#include "tombstone_store.h"
#include "util/key_group_util.h"

namespace ock {
namespace bss {
class TombstoneService : public std::enable_shared_from_this<TombstoneService>, public Runnable {
public:
    TombstoneService(const ConfigRef &config, const GroupRangeRef &keyGroupRange, uint64_t version,
                     const ExecutorServicePtr &executorService, const TombstoneFileManagerRef &tombstoneFileManager,
                     const MemManagerRef &memManager, const KeyGroupUtilRef &keyGroupUtil)
        : mConfig(config),
          mKeyGroupRange(keyGroupRange),
          mVersion(version),
          mTombstoneFlushExecutor(executorService),
          mFileManager(tombstoneFileManager),
          mMemManager(memManager),
          mKeyGroupUtil(keyGroupUtil)
    {
    }

    TombstoneStoreRef GetInstance()
    {
        if (!mTombStoneStore) {
            mTombStoneStore = std::make_shared<TombstoneStore>(mConfig, mFileManager, mMemManager);
        }
        return mTombStoneStore;
    }

    inline void DeleteValue(const Key &key, const Value &value)
    {
        if (UNLIKELY(value.ValueType() != SEPARATE)) {
            return;
        }
        if (UNLIKELY(value.ValueLen() != NO_8)) {
            LOG_ERROR("Delete value len is not 8, value length: " << value.ValueLen());
            return;
        }
        uint64_t blobId = BlobStore::ToBlobId(value);
        uint32_t hash = key.KeyHashCode();
        uint32_t keyGroup = mKeyGroupUtil->ComputeKeyGroupForKeyHash(hash);
        if (UNLIKELY(keyGroup > UINT16_MAX)) {
            LOG_ERROR("Key group is big than expected: " << keyGroup);
            return;
        }
        if (!mKeyGroupRange->ContainsGroup(static_cast<int32_t>(keyGroup))) {
            return;
        }
        auto tombstoneStore = GetInstance();
        tombstoneStore->DeleteBlob(blobId, static_cast<uint16_t>(keyGroup));
    }

    void Commit(bool commit)
    {
        if (commit) {
            FlushRemainingData();
            return;
        }
        Discard();
    }

    void FlushRemainingData()
    {
        auto tombstoneStore = GetInstance();
        if (tombstoneStore->Empty()) {
            return;
        }
        {
            WriteLocker<ReadWriteLock> lock(&mLock);
            for (auto &item : tombstoneStore->GetWrittenFiles()) {
                item->SetFileVersion(mVersion);
                mWrittenFiles.emplace(item);
            }
        }
        auto memCache = tombstoneStore->GetMemCache();
        if (!memCache.empty()) {
            mPendingSet.insert(memCache.begin(), memCache.end());
        }
        tombstoneStore->Clear();
        if (mPendingSet.size() > mConfig->mMaxBlobNumInMemCache) {
            FlushPendingSet();
        }
    }

    void FlushPendingSet()
    {
        if (mPendingSet.empty()) {
            return;
        }
        auto self = shared_from_this();
        mTombstoneFlushExecutor->Execute(self);
    }

    void Run() override
    {
        TombstoneFileWriterRef fileWriter = std::make_shared<TombstoneFileWriter>(mConfig, mFileManager, mVersion,
                                                                                  mMemManager);
        std::vector<TombstoneFileRef> fileVec;
        auto ret = fileWriter->Flush(mPendingSet, fileVec);
        if (UNLIKELY(ret != BSS_OK)) {
            LOG_ERROR("flush pending set failed, ret:" << ret);
            return;
        }
        {
            WriteLocker<ReadWriteLock> lock(&mLock);
            for (const auto &item : fileVec) {
                mWrittenFiles.emplace(item);
            }
        }
    }

    inline void Discard()
    {
        auto tombstoneStore = GetInstance();
        tombstoneStore->Discard();
    }

    inline std::set<TombstoneFileRef> &GetWrittenFiles()
    {
        return mWrittenFiles;
    }

    inline std::set<TombstoneFileRef> GetAndClearWrittenFiles()
    {
        WriteLocker<ReadWriteLock> lock(&mLock);
        std::set<TombstoneFileRef> selectFiles;
        for (const auto &file : mWrittenFiles) {
            CONTINUE_LOOP_AS_NULLPTR(file);
            selectFiles.emplace(file);
        }
        mWrittenFiles.clear();
        return selectFiles;
    }

    void RestoreVersion(uint64_t version)
    {
        mVersion = version;
    }

    void TriggerSnapshot(uint64_t snapshotId)
    {
        FlushPendingSet();
        auto snapshotVersion = mFileManager->GetSnapshotVersion(snapshotId);
        if (snapshotVersion != UINT64_MAX) {
            mVersion = snapshotVersion + 1;
        }
    }

    void GetLessVersionFiles(std::vector<TombstoneFileRef> &ret, uint64_t version)
    {
        WriteLocker<ReadWriteLock> lock(&mLock);
        auto file = mWrittenFiles.begin();
        while (file != mWrittenFiles.end()) {
            TombstoneFileRef tombstoneFile = *file;
            CONTINUE_LOOP_AS_NULLPTR(tombstoneFile);
            if (tombstoneFile->GetVersion() > version) {
                continue;
            }
            ret.emplace_back(*file);
            file = mWrittenFiles.erase(file);
            if (ret.size() >= (mConfig->mTombstoneLevel0CompactionFileNum << NO_2)) {
                break;
            }
        }
    }

    inline uint64_t CalTombstoneNum(uint64_t minBlobId)
    {
        uint64_t count = mPendingSet.size();
        ReadLocker<ReadWriteLock> lock(&mLock);
        for (const auto &file : mWrittenFiles) {
            const auto &tombstoneFileMeta = file->GetFileMeta();
            CONTINUE_LOOP_AS_NULLPTR(tombstoneFileMeta);
            if (tombstoneFileMeta->GetMinBlobId() >= minBlobId) {
                count += tombstoneFileMeta->GetBlobNum();
            }
        }
        return count;
    }

private:
    ConfigRef mConfig = nullptr;
    GroupRangeRef mKeyGroupRange = nullptr;
    static thread_local TombstoneStoreRef mTombStoneStore;
    std::set<TombstoneFileRef> mWrittenFiles;
    std::set<TombstoneRef, CompareTombstone> mPendingSet;
    uint64_t mVersion;
    ExecutorServicePtr mTombstoneFlushExecutor = nullptr;
    TombstoneFileManagerRef mFileManager = nullptr;
    MemManagerRef mMemManager = nullptr;
    KeyGroupUtilRef mKeyGroupUtil = nullptr;
    ReadWriteLock mLock;
};
using TombstoneServiceRef = std::shared_ptr<TombstoneService>;

}  // namespace bss
}  // namespace ock

#endif  // BOOST_SS_TOMBSTONESERVICE_H
