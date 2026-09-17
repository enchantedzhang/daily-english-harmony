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

#ifndef BLOB_FILE_GROUP_MANAGER_H
#define BLOB_FILE_GROUP_MANAGER_H
#include <vector>

#include "binary/value/value.h"
#include "blob_file_group.h"

namespace ock {
namespace bss {
class BlobFileGroupManager;
using BlobFileGroupManagerRef = std::shared_ptr<BlobFileGroupManager>;

class BlobFileGroupManager {
public:
    ~BlobFileGroupManager()
    {
        std::vector<BlobFileGroupRef>().swap(mBlobFileGroups);
    }

    explicit BlobFileGroupManager(const GroupRangeRef &range)
    {
        mBlobFileGroups.emplace_back(std::make_shared<BlobFileGroup>(range));
    }

    explicit BlobFileGroupManager(const std::vector<BlobFileGroupRef> &blobFileGroups)
    {
        mBlobFileGroups = blobFileGroups;
    }

    BlobFileGroupRef GetBlobFileGroup()
    {
        if (mBlobFileGroups.empty()) {
            return nullptr;
        }
        return mBlobFileGroups[0];
    }

    inline const std::vector<BlobFileGroupRef> &GetBlobFileGroups()
    {
        return mBlobFileGroups;
    }

    inline uint32_t GetBlobFileGroupsNum() const
    {
        return mBlobFileGroups.size();
    }

    inline void RestoreFirstGroup(std::vector<BlobImmutableFileRef> &blobFiles)
    {
        if (mBlobFileGroups.empty() || mBlobFileGroups[0] == nullptr) {
            return;
        }
        mBlobFileGroups[0]->Restore(blobFiles);
    }

    BResult BinarySearchBlobValue(uint64_t blobId, uint32_t keyGroup, Value &value);

    BlobImmutableFileRef BinarySearchBlobFile(uint64_t blobId, uint32_t keyGroup);

    BlobFileGroupRef BinarySearchBlobFileGroup(uint64_t blobId, uint32_t keyGroup);

    BlobFileGroupManagerRef SyncSnapshot(uint64_t version);

    BlobFileGroupManagerRef DeepCopy(uint64_t version);

    void AddGroup(BlobFileGroupRef &blobFileGroup)
    {
        mBlobFileGroups.emplace_back(blobFileGroup);
    }

    BResult RestoreFileFooterAndIndexBlock()
    {
        for (const auto &item : mBlobFileGroups) {
            RETURN_NOT_OK(item->RestoreFileFooterAndIndexBlock());
        }
        return BSS_OK;
    }

    uint64_t GetMinBlobId();

    const std::vector<BlobFileGroupRef> &GetFileGroups();

    uint64_t GetFileSize()
    {
        uint64_t size = 0;
        for (const auto &fileGroup : mBlobFileGroups) {
            CONTINUE_LOOP_AS_NULLPTR(fileGroup);
            size += fileGroup->GetFileSize();
        }
        return size;
    }

private:
    std::vector<BlobFileGroupRef> mBlobFileGroups;
};
}  // namespace bss
}  // namespace ock

#endif
