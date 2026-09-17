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

#include "blob_store_snapshot_coordinator.h"

#include "blob_store_snapshot_operator.h"
#include "tombstone/tombstone_file_manager_snapshot.h"
#include "version/version_meta_serializer.h"

namespace ock {
namespace bss {

BResult BlobStoreSnapshotCoordinator::SyncSnapshotForBlob(uint64_t snapshotId)
{
    LOG_INFO("Sync snapshot blob file meta start, snapshotId: " << snapshotId);
    WriteLocker<ReadWriteLock> lk(&mRwLock);
    mBlobFilesReady = true;
    return DoSyncSnapshot(mVersion);
}

SnapshotMetaRef BlobStoreSnapshotCoordinator::SerializeFileMeta(OutputViewRef &outputViews)
{
    RETURN_NULLPTR_AS_NULLPTR(mBlobFileGroupSnapshot);
    SnapshotMetaRef snapshotMeta = std::make_shared<SnapshotMeta>();
    // 1、序列化version和seqId
    RETURN_NULLPTR_AS_NOT_OK(outputViews->Write(reinterpret_cast<uint8_t *>(&mVersion), sizeof(mVersion)));
    RETURN_NULLPTR_AS_NOT_OK(outputViews->Write(reinterpret_cast<uint8_t *>(&mSeqId), sizeof(mSeqId)));
    // 2、序列化BlobFileGroup
    uint32_t blobFileGroupsNum = mBlobFileGroupSnapshot->GetBlobFileGroupsNum();
    RETURN_NULLPTR_AS_NOT_OK(
        outputViews->Write(reinterpret_cast<uint8_t *>(&blobFileGroupsNum), sizeof(blobFileGroupsNum)));
    auto blobFileGroups = mBlobFileGroupSnapshot->GetBlobFileGroups();
    for (const auto &blobFileGroup : blobFileGroups) {
        // 3、序列化GroupRange
        RETURN_NULLPTR_AS_NOT_OK(SerializeGroupRange(blobFileGroup, outputViews));
        // 4、序列化BlobFileMeta
        RETURN_NULLPTR_AS_NOT_OK(SerializeBlobFiles(blobFileGroup, outputViews, snapshotMeta));
    }
    return snapshotMeta;
}

BResult BlobStoreSnapshotCoordinator::SerializeGroupRange(const BlobFileGroupRef &blobFileGroup,
                                                          OutputViewRef &outputViews)
{
    auto groupRange = blobFileGroup->GetGroupRange();
    RETURN_ERROR_AS_NULLPTR(groupRange);
    int32_t startGroup = groupRange->GetStartGroup();
    int32_t endGroup = groupRange->GetEndGroup();
    RETURN_NOT_OK(outputViews->Write(reinterpret_cast<uint8_t *>(&startGroup), sizeof(startGroup)));
    RETURN_NOT_OK(outputViews->Write(reinterpret_cast<uint8_t *>(&endGroup), sizeof(endGroup)));
    return BSS_OK;
}

BResult BlobStoreSnapshotCoordinator::SerializeBlobFiles(const BlobFileGroupRef &blobFileGroup,
                                                         OutputViewRef &outputViews,
                                                         const SnapshotMetaRef &snapshotMeta)
{
    const auto &blobImmutableFiles = blobFileGroup->GetFiles();
    auto size = static_cast<int32_t>(blobImmutableFiles.size());
    RETURN_NOT_OK(outputViews->Write(reinterpret_cast<uint8_t *>(&size), sizeof(size)));
    for (const auto &blobImmutableFile : blobImmutableFiles) {
        auto blobFileMeta = blobImmutableFile->GetBlobFileMeta();
        RETURN_ERROR_AS_NULLPTR(blobFileMeta);
        blobFileMeta->SnapshotMeta(outputViews);
        if (UNLIKELY(blobFileMeta->GetFileStatus() != LOCAL)) {
            continue;
        }
        snapshotMeta->AddSnapshotBlobMeta(blobFileMeta);
    }
    return BSS_OK;
}

SnapshotMetaRef BlobStoreSnapshotCoordinator::WriteMeta(uint64_t snapshotId,
                                                        const FileOutputViewRef &localFileOutputView)
{
    RETURN_NULLPTR_AS_NULLPTR(mBlobStoreSnapshotOperator);
    // 1. 序列化Blob File
    auto localBufferOutputView = std::make_shared<OutputView>(mMemManager, FileProcHolder::FILE_STORE_SNAPSHOT);
    SnapshotMetaRef snapshotMeta = SerializeFileMeta(localBufferOutputView);
    if (UNLIKELY(snapshotMeta == nullptr)) {
        LOG_ERROR("Serialize blob file meta failed.");
        return nullptr;
    }
    // 2. 将序列化的snapshotMeta写入文件.
    auto sizeToCheck = localFileOutputView->Size();
    if (UNLIKELY(sizeToCheck > INT64_MAX)) {
        LOG_ERROR("Local file output view size cannot static cast, size: " << sizeToCheck);
        return nullptr;
    }
    auto ret = localFileOutputView->WriteBuffer(localBufferOutputView->Data(), static_cast<int64_t>(sizeToCheck),
                                                localBufferOutputView->GetOffset());
    if (UNLIKELY(ret != BSS_OK)) {
        LOG_ERROR("Write serialize blob meta to snapshot file failed, ret:" << ret);
        return nullptr;
    }
    mTombstoneFileManagerSnapshot->WriteMeta(localFileOutputView, snapshotMeta);
    return snapshotMeta;
}

void BlobStoreSnapshotCoordinator::ReleaseResource()
{
    if (mBlobFileGroupSnapshot != nullptr) {
        const auto blobFileGroups = mBlobFileGroupSnapshot->GetBlobFileGroups();
        for (const auto &blobFileGroup : blobFileGroups) {
            CONTINUE_LOOP_AS_NULLPTR(blobFileGroup);
            const auto &blobImmutableFiles = blobFileGroup->GetFiles();
            for (const auto &blobImmutableFile : blobImmutableFiles) {
                CONTINUE_LOOP_AS_NULLPTR(blobImmutableFile);
                blobImmutableFile->DiscardFile();
            }
        }
    }
    if (mTombstoneFileManagerSnapshot != nullptr) {
        auto fileMetas = mTombstoneFileManagerSnapshot->GetFileMetas();
        for (const auto &item : fileMetas) {
            CONTINUE_LOOP_AS_NULLPTR(item);
            if (UNLIKELY(item->DecRef() == 0)) {
                LOG_DEBUG("tombstone file ref is 0, file:" << item->GetFileAddress());
                mFileCacheManager->DiscardFile(item->GetFileAddress());
            }
        }
    }
}

BResult BlobStoreSnapshotCoordinator::DoSyncSnapshot(uint64_t version)
{
    FileMetaInfoMap toSnapshotFileAddress;
    mBlobFileGroupSnapshot = mBlobFileManager->CopyBlobFileGroupManager(version);
    if (mBlobFileGroupSnapshot == nullptr) {
        LOG_INFO("Blob file group is empty, don't need snapshot");
        return BSS_BLOB_NOT_SNAPSHOT;
    }
    mTombstoneFileManagerSnapshot = mTombstoneFileManager->CreateSnapshot(version);
    uint64_t blobFileNum = SyncSnapshotBlobFileMeta(toSnapshotFileAddress);
    uint64_t tombstoneFileNum = 0;
    for (const auto &item : mTombstoneFileManagerSnapshot->GetFileMetas()) {
        CONTINUE_LOOP_AS_NULLPTR(item);
        toSnapshotFileAddress.emplace(item->GetIdentifier(),
                                      std::make_tuple(item->GetFileAddress(), item->GetFileSize(), 1));
        tombstoneFileNum++;
    }
    mBlobStoreSnapshotOperator->SetToSnapshotFileAddress(toSnapshotFileAddress);
    LOG_INFO("Sync snapshot file meta success with version: " << version << ", blob file num: " << blobFileNum
                                                              << ", tombstone file num: " << tombstoneFileNum);
    return BSS_OK;
}

uint64_t BlobStoreSnapshotCoordinator::SyncSnapshotBlobFileMeta(FileMetaInfoMap &toSnapshotFileAddress)
{
    uint64_t blobFileNum = 0;
    const auto blobFileGroups = mBlobFileGroupSnapshot->GetBlobFileGroups();
    for (const auto &blobFileGroup : blobFileGroups) {
        CONTINUE_LOOP_AS_NULLPTR(blobFileGroup);
        const auto &blobImmutableFiles = blobFileGroup->GetFiles();
        for (const auto &blobImmutableFile : blobImmutableFiles) {
            CONTINUE_LOOP_AS_NULLPTR(blobImmutableFile);
            auto blobFileMeta = blobImmutableFile->GetBlobFileMeta();
            CONTINUE_LOOP_AS_NULLPTR(blobFileMeta);
            blobImmutableFile->IncFileRef();
            toSnapshotFileAddress.emplace(blobFileMeta->GetIdentifier(),
                                          std::make_tuple(blobFileMeta->GetFileAddress(), blobFileMeta->GetFileSize(),
                                                          1));
            blobFileNum++;
        }
    }
    return blobFileNum;
}

}  // namespace bss
}  // namespace ock