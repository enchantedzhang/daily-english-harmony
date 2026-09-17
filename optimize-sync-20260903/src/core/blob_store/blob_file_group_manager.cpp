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

#include "blob_file_group_manager.h"

namespace ock {
namespace bss {
BResult BlobFileGroupManager::BinarySearchBlobValue(uint64_t blobId, uint32_t keyGroup, Value &value)
{
    auto blobFile = BinarySearchBlobFile(blobId, keyGroup);
    if (UNLIKELY(blobFile == nullptr)) {
        LOG_WARN("blobFile is null, blobId: " << blobId << ", keyGroup: " << keyGroup);
        return BSS_NOT_FOUND;
    }
    return blobFile->SelectBlobValue(blobId, keyGroup, value);
}

BlobImmutableFileRef BlobFileGroupManager::BinarySearchBlobFile(uint64_t blobId, uint32_t keyGroup)
{
    BlobFileGroupRef fileGroup = BinarySearchBlobFileGroup(blobId, keyGroup);
    RETURN_NULLPTR_AS_NULLPTR(fileGroup);
    auto files = fileGroup->GetFiles();
    if (UNLIKELY(files.empty())) {
        LOG_WARN("Files is empty, blobId: " << blobId << ", keyGroup: " << keyGroup);
        return nullptr;
    }
    uint32_t low = 0;
    uint32_t high = files.size() - NO_1;
    while (low <= high) {
        uint32_t mid = low + ((high - low) >> NO_1);
        if (UNLIKELY(mid >= files.size())) {
            LOG_ERROR("Index out of bound, mid: " << mid << ", size: " << files.size());
            return nullptr;
        }
        auto blobFile = files[mid];
        RETURN_NULLPTR_AS_NULLPTR(blobFile);
        auto blockMeta = blobFile->GetBlobFileMeta();
        RETURN_NULLPTR_AS_NULLPTR(blockMeta);
        if (blobId < blockMeta->GetMinBlobId()) {
            high = mid - NO_1;
            continue;
        }
        if (blobId > blockMeta->GetMaxBlobId()) {
            low = mid + NO_1;
            continue;
        }
        return blobFile;
    }
    LOG_ERROR("Can not find file, blobId: " << blobId << ", keyGroup: " << keyGroup);
    return nullptr;
}

BlobFileGroupRef BlobFileGroupManager::BinarySearchBlobFileGroup(uint64_t blobId, uint32_t keyGroup)
{
    for (const auto &fileGroup : mBlobFileGroups) {
        RETURN_NULLPTR_AS_NULLPTR(fileGroup);
        if (fileGroup->Contains(blobId, keyGroup)) {
            return fileGroup;
        }
    }
    LOG_ERROR("Can’t find file group, blobId: " << blobId << ", keyGroup: " << keyGroup);
    return nullptr;
}

BlobFileGroupManagerRef BlobFileGroupManager::SyncSnapshot(uint64_t version)
{
    return DeepCopy(version);
}

BlobFileGroupManagerRef BlobFileGroupManager::DeepCopy(uint64_t version)
{
    std::vector<BlobFileGroupRef> blobFileGroups;
    for (const auto &fileGroup : mBlobFileGroups) {
        const auto &blobFileGroup = fileGroup->DeepCopy(version);
        if (UNLIKELY(blobFileGroup == nullptr)) {
            continue;
        }
        blobFileGroups.emplace_back(blobFileGroup);
    }
    if (UNLIKELY(blobFileGroups.empty())) {
        LOG_DEBUG("Deep copy failed, copied file group is empty.");
        return nullptr;
    }
    return std::make_shared<BlobFileGroupManager>(blobFileGroups);
}

uint64_t BlobFileGroupManager::GetMinBlobId()
{
    uint64_t minBlobId = UINT64_MAX;
    for (const auto &fileGroup : mBlobFileGroups) {
        CONTINUE_LOOP_AS_NULLPTR(fileGroup);
        auto files = fileGroup->GetFiles();
        if (files.empty()) {
            continue;
        }
        BlobFileMetaRef firstBlobFileMeta = files[0]->GetBlobFileMeta();
        CONTINUE_LOOP_AS_NULLPTR(firstBlobFileMeta);
        minBlobId = std::min(minBlobId, firstBlobFileMeta->GetMinBlobId());
    }
    return minBlobId;
}

const std::vector<BlobFileGroupRef> &BlobFileGroupManager::GetFileGroups()
{
    return mBlobFileGroups;
}
}  // namespace bss
}  // namespace ock