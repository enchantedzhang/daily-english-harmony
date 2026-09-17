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

#ifndef BLOB_STORE_H
#define BLOB_STORE_H
#include "blob_file_manager.h"
#include "blob_store_snapshot_coordinator.h"
#include "blob_store_stat.h"
#include "blob_write_buffer.h"
#include "file/file_cache_manager.h"
#include "lazy/file_holder.h"
#include "mem_manager.h"

namespace ock {
namespace bss {

class BlobStoreSnapshotOperator;
class BlobStore : public FileHolder {
public:
    ~BlobStore() override
    {
        LOG_INFO("Delete BlobStore success.");
    }
    BlobStore() = default;

    BResult Init(const MemManagerRef &memManager, const FileCacheManagerRef &fileCacheManager, const ConfigRef &config,
                 const BlockCacheRef &blockCache);

    void Close()
    {
        if (mBlobFlushExecutor != nullptr) {
            mBlobFlushExecutor->Stop();
        }
        if (UNLIKELY(mBlobFileManager != nullptr)) {
            mBlobFileManager->Close();
        }
    }

    inline void RestoreVersion(uint64_t version)
    {
        mVersion.store(version + NO_1);
    }

    inline void RestoreSeqId(uint64_t seqId) const
    {
        mBlobWriteBuffer->RestoreSeqId(seqId);
    }

    BResult WriteBlobValue(const uint8_t *value, uint32_t length, uint64_t expireTime, uint32_t keyGroup,
                           uint64_t &blobId);

    BResult GetBlobValue(uint64_t blobId, uint32_t keyGroup, Value &value);

    static bool ToBlobValue(const Key &key, Value &value, BlobValueTransformFunc getFromBlobFunc);

    static uint64_t ToBlobId(const Value &value);

    BResult Restore(const std::vector<std::pair<FileInputViewRef, int64_t>> &metaList,
                    std::unordered_map<std::string, uint32_t> &restorePathFileIdMap, bool rescale);

    BResult FlushCurrentBlobFile(uint64_t snapshotId);

    BResult SyncSnapshot(uint64_t snapshotId, std::shared_ptr<BlobStoreSnapshotOperator> &snapshotOperator);

    void ReleaseSnapshot(uint64_t snapshotId);

    TombstoneServiceRef CreateTombstoneService(const std::string &name, const KeyGroupUtilRef &keyGroupUtil);

private:
    MemManagerRef mMemManager = nullptr;
    ConfigRef mConfig = nullptr;
    FileCacheManagerRef mFileCacheManager = nullptr;
    BlobStoreStatRef mBlobStoreStat = nullptr;
    BlockCacheRef mBlockCache = nullptr;
    BlobFileManagerRef mBlobFileManager = nullptr;
    ExecutorServicePtr mBlobFlushExecutor = nullptr;
    BlobWriteBufferRef mBlobWriteBuffer = nullptr;
    std::unordered_map<uint64_t, uint64_t> mSnapshotMapping;
    std::atomic<uint64_t> mVersion{ 0 };
};
using BlobStoreRef = std::shared_ptr<BlobStore>;
}  // namespace bss
}  // namespace ock

#endif
