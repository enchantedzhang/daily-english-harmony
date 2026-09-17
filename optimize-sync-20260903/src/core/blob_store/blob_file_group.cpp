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

#include "blob_file_group.h"

namespace ock {
namespace bss {

BResult BlobFileGroup::AddLastFile(const BlobImmutableFileRef &newFile)
{
    WriteLocker<ReadWriteLock> lock(&mLock);
    if (!mFiles.empty()) {
        auto immutableFile = mFiles[mFiles.size() - NO_1];
        RETURN_ERROR_AS_NULLPTR(immutableFile);
        auto lastMeta = immutableFile->GetBlobFileMeta();
        RETURN_ERROR_AS_NULLPTR(lastMeta);
        auto meta = newFile->GetBlobFileMeta();
        RETURN_ERROR_AS_NULLPTR(meta);
        RETURN_NOT_OK_AS_FALSE(meta->GetVersion() < lastMeta->GetVersion(), BSS_INVALID_PARAM);
        RETURN_NOT_OK_AS_FALSE(meta->GetMinBlobId() <= lastMeta->GetMaxBlobId(), BSS_INVALID_PARAM);
    }
    mFiles.emplace_back(newFile);
    return BSS_OK;
}

BlobFileGroupRef BlobFileGroup::DeepCopy(uint64_t version)
{
    std::vector<BlobImmutableFileRef> snapshotFiles;
    WriteLocker<ReadWriteLock> lock(&mLock);
    std::vector<BlobImmutableFileRef> currentFiles = GetFiles();
    if (currentFiles.empty()) {
        LOG_INFO("Current blob files is empty, not need copy.");
        return nullptr;
    }
    auto tailIndex = static_cast<int32_t>(currentFiles.size() - NO_1);
    for (int32_t i = tailIndex; i >= 0; i--) {
        auto immutableFile = currentFiles[i];
        RETURN_NULLPTR_AS_NULLPTR(immutableFile);
        auto blobFileMeta = immutableFile->GetBlobFileMeta();
        RETURN_NULLPTR_AS_NULLPTR(blobFileMeta);
        if (UNLIKELY(blobFileMeta->GetVersion() > version)) {
            continue;
        }
        tailIndex = i;
        break;
    }
    snapshotFiles.reserve(tailIndex + NO_1);
    for (int32_t i = 0; i <= tailIndex; i++) {
        snapshotFiles.emplace_back(currentFiles[i]);
    }
    return std::make_shared<BlobFileGroup>(mGroupRange, snapshotFiles);
}
}  // namespace bss
}  // namespace ock