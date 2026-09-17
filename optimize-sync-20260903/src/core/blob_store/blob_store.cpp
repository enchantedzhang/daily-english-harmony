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

#include "blob_store.h"

#include "binary/value/value_type.h"
#include "blob_cleaner.h"

namespace ock {
namespace bss {
BResult BlobStore::Init(const MemManagerRef &memManager, const FileCacheManagerRef &fileCacheManager,
                        const ConfigRef &config, const BlockCacheRef &blockCache)
{
    mMemManager = memManager;
    mFileCacheManager = fileCacheManager;
    mBlobStoreStat = std::make_shared<BlobStoreStat>();
    mBlobFileManager = std::make_shared<BlobFileManager>();
    mConfig = config;
    mBlockCache = blockCache;
    RETURN_NOT_OK(mBlobFileManager->Init(memManager, fileCacheManager, mConfig, mBlockCache, mVersion.load()));
    mBlobWriteBuffer = std::make_shared<BlobWriteBuffer>();
    mBlobFlushExecutor = std::make_shared<ExecutorService>(NO_1, NO_1024);
    mBlobFlushExecutor->SetThreadName("BlobEvictExecutor");
    if (UNLIKELY(!mBlobFlushExecutor->Start())) {
        LOG_ERROR("Failed to start blob write buffer flush executor.");
        return BSS_ERR;
    }
    RETURN_NOT_OK(mBlobWriteBuffer->Init(mBlobFileManager, mMemManager, mBlobFlushExecutor));
    RETURN_INVALID_PARAM_AS_NULLPTR(config);
    mBlobWriteBuffer->SetDefaultBlockSize(config->GetBlobDefaultBlockSize());
    return BSS_OK;
}

BResult BlobStore::WriteBlobValue(const uint8_t *value, uint32_t length, uint64_t expireTime, uint32_t keyGroup,
                                  uint64_t &blobId)
{
    RETURN_ERROR_AS_NULLPTR(mBlobStoreStat);
    RETURN_ERROR_AS_NULLPTR(mBlobWriteBuffer);
    mBlobStoreStat->ReportWriteBlobValue(NO_1, length);
    return mBlobWriteBuffer->Write(value, length, expireTime, keyGroup, blobId);
}

BResult BlobStore::GetBlobValue(uint64_t blobId, uint32_t keyGroup, Value &value)
{
    if (UNLIKELY(mBlobWriteBuffer == nullptr)) {
        LOG_ERROR("Blob store write buffer is null.");
        return BSS_ERR;
    }
    if (UNLIKELY(mBlobFileManager == nullptr)) {
        LOG_ERROR("Blob store file manager is null.");
        return BSS_ERR;
    }

    BResult result = mBlobWriteBuffer->Get(blobId, keyGroup, value);
    if (result != BSS_NOT_FOUND) {
        return result;
    }
    return mBlobFileManager->Get(blobId, keyGroup, value);
}

bool BlobStore::ToBlobValue(const Key &key, Value &value, BlobValueTransformFunc getFromBlobFunc)
{
    if (value.ValueType() != SEPARATE) {
        return true;
    }
    Value originalValue;
    if (UNLIKELY(value.ValueLen() != NO_8)) {
        LOG_ERROR("Blob value len is not 8, value len:" << value.ValueLen());
        return false;
    }
    uint64_t blobId = ToBlobId(value);
    if (UNLIKELY(getFromBlobFunc == nullptr)) {
        LOG_ERROR("GetFromBlobFunc is null.");
        return false;
    }
    BResult ret = getFromBlobFunc(blobId, key.KeyHashCode(), value.SeqId(), originalValue);
    if (UNLIKELY(ret != BSS_OK)) {
        LOG_ERROR("Get value from blob failed, ret: " << ret);
        return false;
    }
    value = std::move(originalValue);
    LOG_DEBUG("Transform blobId to real value success, blobId: " << blobId << ", value size: " << value.ValueLen());
    return true;
}

BResult BlobStore::FlushCurrentBlobFile(uint64_t snapshotId)
{
    auto it = mSnapshotMapping.find(snapshotId);
    if (it == mSnapshotMapping.end()) {
        LOG_ERROR("Flush current blob file failed, snapshotId:" << snapshotId);
        return BSS_ERR;
    }
    return mBlobFileManager->FlushCurrentBlobFile(it->second);
}

uint64_t BlobStore::ToBlobId(const Value &value)
{
    return *(reinterpret_cast<const uint64_t *>(value.ValueData()));
}

BResult BlobStore::Restore(const std::vector<std::pair<FileInputViewRef, int64_t>> &metaList,
                           std::unordered_map<std::string, uint32_t> &restorePathFileIdMap, bool rescale)
{
    uint64_t maxVersion = 0;
    uint64_t maxSeqId = 0;
    for (const auto &meta : metaList) {
        auto fileInputView = meta.first;
        CONTINUE_LOOP_AS_NULLPTR(fileInputView);
        if (UNLIKELY(meta.second == -1)) {
            continue;
        }
        fileInputView->Seek(meta.second);
        uint64_t version = 0;
        uint64_t seqId = 0;
        RETURN_NOT_OK(fileInputView->Read(version));
        RETURN_NOT_OK(fileInputView->Read(seqId));
        maxVersion = std::max(maxVersion, version);
        maxSeqId = std::max(maxSeqId, seqId);
    }
    if (maxVersion == 0) {
        LOG_INFO("Blob store not need restore.");
        return BSS_OK;
    }
    RestoreVersion(maxVersion);
    RestoreSeqId(maxSeqId);
    return mBlobFileManager->Restore(metaList, maxVersion, restorePathFileIdMap, rescale);
}

BResult BlobStore::SyncSnapshot(uint64_t snapshotId, BlobStoreSnapshotOperatorRef &snapshotOperator)
{
    uint64_t version = mVersion.load();
    mSnapshotMapping.emplace(snapshotId, version);
    mVersion.fetch_add(NO_1);
    auto cleaner = mBlobFileManager->GetBlobCleaner();
    RETURN_ERROR_AS_NULLPTR(cleaner);
    cleaner->TriggerSnapshot(snapshotId, mVersion.load(), mBlobWriteBuffer->GetCurrentSeqId(), snapshotOperator);
    return mBlobWriteBuffer->FlushCurrentWriteBuffer(snapshotId);
}

void BlobStore::ReleaseSnapshot(uint64_t snapshotId)
{
    auto it = mSnapshotMapping.find(snapshotId);
    if (it == mSnapshotMapping.end()) {
        LOG_ERROR("Release snapshot resources, snapshotId:" << snapshotId);
        return;
    }
    mSnapshotMapping.erase(snapshotId);
    if (UNLIKELY(mBlobFileManager == nullptr)) {
        return;
    }
    mBlobFileManager->ReleaseTombstoneSnapshot(snapshotId);
}

TombstoneServiceRef BlobStore::CreateTombstoneService(const std::string &name, const KeyGroupUtilRef &keyGroupUtil)
{
    return mBlobFileManager->RegisterTombstoneService(name, keyGroupUtil);
}
}  // namespace bss
}  // namespace ock
