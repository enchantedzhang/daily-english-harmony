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

#ifndef BOOST_SS_BLOB_CLEANER_H
#define BOOST_SS_BLOB_CLEANER_H

#include "blob_immutable_file.h"
#include "common/io/file_input_view.h"
#include "executor/executor_service.h"
#include "mem_manager.h"
#include "tombstone/tombstone.h"
#include "tombstone/tombstone_file.h"
#include "tombstone/tombstone_file_manager.h"

namespace ock {
namespace bss {
class TombstoneFileWriter;
class BlobCompactionFileWriter;
class TombstoneFileMergingIterator;
class BlobFileMergingIterator;
class BlobFileGroupManager;
using BlobFileGroupManagerRef = std::shared_ptr<BlobFileGroupManager>;
class TombstoneFileManager;
using TombstoneFileManagerRef = std::shared_ptr<TombstoneFileManager>;
class FileCacheManager;
using FileCacheManagerRef = std::shared_ptr<FileCacheManager>;
class BlobFileManager;
using BlobFileManagerRef = std::shared_ptr<BlobFileManager>;
class BlobStoreSnapshotOperator;
class BlobStore;
class TombstoneService;
using TombstoneServiceRef = std::shared_ptr<TombstoneService>;
using BlobStoreRef = std::shared_ptr<BlobStore>;
using CompactionResult = std::pair<std::vector<BlobImmutableFileRef>, std::vector<TombstoneFileRef>>;
using BlobStoreSnapshotOperatorRef = std::shared_ptr<BlobStoreSnapshotOperator>;

class BlobCleaner : public std::enable_shared_from_this<BlobCleaner> {
public:
    ~BlobCleaner()
    {
        LOG_INFO("Delete BlobCleaner success.");
    }
    BlobCleaner() = default;
    BResult Init(const ConfigRef &config, const BlobFileGroupManagerRef &blobFileGroupManager,
                 const FileCacheManagerRef &fileCacheManager, const BlobFileManagerRef &blobFileManager,
                 const MemManagerRef &memManager, uint64_t version);

    BResult Restore(const FileInputViewRef &fileInputView,
                    std::unordered_map<std::string, uint32_t> &restorePathFileIdMap, uint64_t restoreVersion,
                    bool rescale);

    void FinishRestore();

    void TriggerSnapshot(uint64_t snapshotId, uint64_t blobStoreVersion, uint64_t seqId,
                         BlobStoreSnapshotOperatorRef &blobStoreSnapshotOperator);

    bool TriggerCompaction();

    bool IsSleeping() const
    {
        return mIsSleeping.load();
    }

    void ToSleeping()
    {
        mIsSleeping.store(true);
    }

    bool IsClosed() const
    {
        return mIsClose.load();
    }

    void StartTombstoneCompaction()
    {
        mIsClose.store(false);
        mIsCompacting.store(true);
        mIsSleeping.store(false);
    }

    void StopTombstoneCompaction()
    {
        mIsClose.store(true);
        uint32_t times = NO_1;
        auto start = std::chrono::high_resolution_clock::now();
        while (UNLIKELY(IsCompacting()) && !IsSleeping()) {
            if ((times++) % NO_100 == 0) {
                auto end = std::chrono::high_resolution_clock::now();
                auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
                double elapsed = duration.count() / 1e3;  // 转换为ms
                LOG_WARN("Stop tombstone compaction suspend cost time: " << elapsed << "ms.");
            }
            usleep(NO_100000);  // 100ms
        }
    }

    void SetCompacting(bool isCompacting)
    {
        mIsCompacting.store(isCompacting);
    }

    bool IsCompacting() const
    {
        return mIsCompacting.load();
    }

    void DoCompaction();

    BResult ProcessCompaction(const std::vector<BlobImmutableFileRef> &blobFiles,
                              const std::vector<TombstoneFileRef> &tombstoneFiles, CompactionResult &result);

    int32_t CompareBlobAndTombstone(BlobValueWrapperRef blobValueWrapper, TombstoneRef tombstone);

    BResult DoCompactionForBlobAndTombstone(const std::shared_ptr<BlobFileMergingIterator> &blobMergeIterator,
                                            const std::shared_ptr<TombstoneFileMergingIterator> &tombstoneMergeIterator,
                                            const std::shared_ptr<BlobCompactionFileWriter> &blobFileWriter,
                                            const std::shared_ptr<TombstoneFileWriter> &tombstoneFileWriter);

    bool IsBlobNotExpire(BlobValueWrapperRef &blobValueWrapper);

    void Close()
    {
        StopTombstoneCompaction();
        if (mCompactionExecutor != nullptr) {
            mCompactionExecutor->Stop();
            mCompactionExecutor = nullptr;
        }
        if (mTombstoneFileManager != nullptr) {
            mTombstoneFileManager->Close();
        }
        LOG_INFO("Blob cleaner is close.");
    }

    void StartScheduleCompaction();

    void ReleaseTombstoneSnapshot(uint64_t snapshotId);

    TombstoneServiceRef RegisterTombstoneService(const std::string &name, const KeyGroupUtilRef &keyGroupUtil);

    double CalBlobSpaceWasteRate(uint64_t minBlobId);

private:
    ConfigRef mConfig = nullptr;
    BlobFileGroupManagerRef mBlobFileGroupManager = nullptr;
    TombstoneFileManagerRef mTombstoneFileManager = nullptr;
    FileCacheManagerRef mFileCacheManager = nullptr;
    BlobFileManagerRef mBlobFileManager = nullptr;
    ExecutorServicePtr mCompactionExecutor = nullptr;
    MemManagerRef mMemManager = nullptr;
    std::atomic<bool> mIsClose{ false };
    std::atomic<bool> mIsCompacting{ false };
    std::atomic<bool> mIsSleeping{ false };
    uint32_t mCompactionIntervalInSecond = 10;
    bool mEnableTombstone = false;
};
using BlobCleanerRef = std::shared_ptr<BlobCleaner>;
}  // namespace bss
}  // namespace ock

#endif
