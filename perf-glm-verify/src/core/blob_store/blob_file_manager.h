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

#ifndef BLOB_FILE_MANAGER_H
#define BLOB_FILE_MANAGER_H

#include "blob_cleaner.h"
#include "blob_file_group_manager.h"
#include "blob_file_writer.h"
#include "common/util/timestamp_util.h"
#include "file/file_cache_manager.h"

namespace ock {
namespace bss {

class BlobCleaner;
using BlobCleanerRef = std::shared_ptr<BlobCleaner>;
class TombstoneService;
using TombstoneServiceRef = std::shared_ptr<TombstoneService>;

class BlobFileManager : public std::enable_shared_from_this<BlobFileManager> {
public:
    ~BlobFileManager()
    {
        LOG_INFO("Delete BlobFileManager success.");
    }

    BResult Init(const MemManagerRef &memManager, const FileCacheManagerRef &fileCacheManager, const ConfigRef &config,
                 const BlockCacheRef &blockCache, uint64_t version);

    void Close()
    {
        // 置空避免循环引用.
        if (LIKELY(mBlobCleaner != nullptr)) {
            mBlobCleaner->Close();
            mBlobCleaner = nullptr;
        }
    }

    BResult WriteBlock(BlobDataBlockRef &dataBlock);

    BResult AddBlobImmutableFile(const BlobImmutableFileRef &blobImmutableFile);

    BlobFileWriterRef NewBlobFileWriter();

    BResult InitBlobFileWriter();

    BResult Get(uint64_t blobId, uint32_t keyGroup, Value &value);

    BResult FlushCurrentBlobFile(uint64_t version);

    BlobFileGroupManagerRef CopyBlobFileGroupManager(uint64_t version);

    BResult Restore(const std::vector<std::pair<FileInputViewRef, int64_t>> &fileInputViews, uint64_t version,
                    std::unordered_map<std::string, uint32_t> &restorePathFileIdMap, bool rescale);

    BResult RestoreFileGroup(FileInputViewRef &fileInputView,
                             std::unordered_map<std::string, uint32_t> &restorePathFileIdMap, bool rescale);

    BResult RestoreFileMeta(FileInputViewRef &fileInputView,
                            std::unordered_map<std::string, uint32_t> &restorePathFileIdMap,
                            BlobFileMetaRef &blobFileMeta, RestoreFileInfo &restoreFileInfo);

    BlobCleanerRef &GetBlobCleaner()
    {
        return mBlobCleaner;
    }

    void FindMaxDeleteRatioFile(double &maxDeleteRatio, BlobFileGroupRef &maxDeleteRatioGroup,
                                BlobImmutableFileRef &maxDeleteRatioFile, uint32_t &selectFileIndex);

    std::vector<BlobImmutableFileRef> SelectMaxCompactionRateFiles(uint32_t &startIndex,
                                                                   BlobFileGroupRef &maxDeleteRatioGroup);

    std::vector<BlobImmutableFileRef> SelectContinuousFilesByMaxDeleteRatioFile(
        double &maxDeleteRatio, BlobFileGroupRef &maxDeleteRatioGroup, BlobImmutableFileRef &maxDeleteRatioFile,
        uint32_t &selectFileIndex, uint32_t &startIndex);

    void DeleteBlobFiles(std::vector<BlobImmutableFileRef> &files)
    {
        for (auto &item : files) {
            DeleteBlobFile(item);
        }
    }

    void DeleteBlobFile(BlobImmutableFileRef &blobImmutableFile, bool deleteNow = false)
    {
        {
            std::lock_guard<std::mutex> lk(mMutex);
            mWaitingDeleteFiles.emplace_back(blobImmutableFile);
        }
        if (deleteNow) {
            CleanDeleteFiles();
        }
    }

    void CleanDeleteFiles()
    {
        std::lock_guard<std::mutex> lk(mMutex);
        auto cur = mWaitingDeleteFiles.begin();
        while (cur != mWaitingDeleteFiles.end()) {
            BlobImmutableFileRef blobImmutableFile = *cur;
            CONTINUE_LOOP_AS_NULLPTR(blobImmutableFile);
            blobImmutableFile->DiscardFile();
            cur = mWaitingDeleteFiles.erase(cur);
        }
    }

    void CleanExpireFile()
    {
        auto groups = mBlobFileGroupManager->GetFileGroups();
        auto group = groups.begin();
        while (group != groups.end()) {
            BlobFileGroupRef blobFileGroup = *group;
            CONTINUE_LOOP_AS_NULLPTR(blobFileGroup);
            auto files = blobFileGroup->CleanExpireFiles(mConfig->GetBlobFileRetainTimeInMill());
            if (!files.empty()) {
                DeleteBlobFiles(files);
            }
            ++group;
        }
    }

    TombstoneServiceRef RegisterTombstoneService(const std::string &name, const KeyGroupUtilRef &keyGroupUtil);

    void ReleaseTombstoneSnapshot(uint64_t snapshotId);

    const BlobFileGroupManagerRef &GetBlobFileGroupManager()
    {
        return mBlobFileGroupManager;
    }

    uint64_t GetFileSize() const
    {
        return mBlobFileGroupManager->GetFileSize();
    }

private:
    BlobFileWriterRef mBlobFileWriter = nullptr;
    ReadWriteLock mRwLock;
    FileCacheManagerRef mFileCacheManager = nullptr;
    BlockCacheRef mBlockCache = nullptr;
    FileDirectoryRef mFileDirectory = nullptr;
    MemManagerRef mMemManager = nullptr;
    ConfigRef mConfig = nullptr;
    BlobFileGroupManagerRef mBlobFileGroupManager = nullptr;
    std::atomic<uint64_t> mVersion{ 0 };
    BlobCleanerRef mBlobCleaner = nullptr;
    std::vector<BlobImmutableFileRef> mWaitingDeleteFiles;
    std::mutex mMutex;
};
using BlobFileManagerRef = std::shared_ptr<BlobFileManager>;
}  // namespace bss
}  // namespace ock

#endif
