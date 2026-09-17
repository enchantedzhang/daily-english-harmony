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

#ifndef BOOST_SS_BLOB_FILE_GROUP_ITERATOR_H
#define BOOST_SS_BLOB_FILE_GROUP_ITERATOR_H
#include "blob_file_group_manager.h"
namespace ock {
namespace bss {

class BlobFileGroupIterator {
public:
    explicit BlobFileGroupIterator(BlobFileGroupManagerRef &blobFileGroupManager) : mGroupManager(blobFileGroupManager)
    {
        auto size = blobFileGroupManager->GetBlobFileGroups().size();
        mIndexVec.resize(size);
    }

    BlobImmutableFileRef PeekFirst(uint64_t blobId, uint32_t keyGroup)
    {
        for (uint32_t i = 0; i < mIndexVec.size(); i++) {
            auto blobFileGroup = mGroupManager->GetBlobFileGroups()[i];
            if (blobFileGroup->Contains(blobId, keyGroup)) {
                mCurFileVec = blobFileGroup->GetFiles();
                if (mIndexVec[i] < mCurFileVec.size()) {
                    mCurIndex = i;
                    return mCurFileVec[mIndexVec[i]];
                }
                return nullptr;
            }
        }
        return nullptr;
    }

    void Advance()
    {
        mIndexVec[mCurIndex] += 1;
    }

    BlobImmutableFileRef PeekNext()
    {
        auto valueIndex = mIndexVec[mCurIndex];
        if (UNLIKELY(valueIndex >= mCurFileVec.size())) {
            LOG_ERROR("Current file size: " << mCurFileVec.size() << ", index: " << valueIndex);
            return nullptr;
        }
        return mCurFileVec[valueIndex];
    }

private:
    BlobFileGroupManagerRef mGroupManager = nullptr;
    std::vector<uint32_t> mIndexVec;
    uint32_t mCurIndex = 0;
    std::vector<BlobImmutableFileRef> mCurFileVec;
};
using BlobFileGroupIteratorRef = std::shared_ptr<BlobFileGroupIterator>;
}  // namespace bss
}  // namespace ock

#endif
