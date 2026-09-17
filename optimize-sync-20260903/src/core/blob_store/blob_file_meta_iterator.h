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
#ifndef BLOB_FILE_META_ITERATOR_H
#define BLOB_FILE_META_ITERATOR_H
#include "blob_file_group_manager.h"
#include "blob_immutable_file.h"
#include "file/file_meta_base.h"
#include "util/iterator.h"

namespace ock {
namespace bss {
class BlobFileIterator : public Iterator<BlobImmutableFileRef> {
    class BlobFileGroupIterator : public Iterator<BlobFileGroupRef> {
    public:
        explicit BlobFileGroupIterator(const BlobFileGroupManagerRef &blobFileGroupManager)
        {
            mBlobFileGroups = blobFileGroupManager->GetBlobFileGroups();
            mNextFileGroupIndex = static_cast<int64_t>(mBlobFileGroups.size());
        }

        bool HasNext() override
        {
            Advance();
            return mNextFileGroup != nullptr;
        }

        BlobFileGroupRef Next() override
        {
            mCurrentFileGroup = mNextFileGroup;
            mNextFileGroup = nullptr;
            return mCurrentFileGroup;
        }

        void Close() override
        {
        }

        void Advance()
        {
            while (mNextFileGroup == nullptr && mNextFileGroupIndex > 0) {
                mNextFileGroupIndex--;
                mNextFileGroup = mBlobFileGroups[mNextFileGroupIndex];
            }
        }

    private:
        std::vector<BlobFileGroupRef> mBlobFileGroups;
        int64_t mNextFileGroupIndex = 0;
        BlobFileGroupRef mNextFileGroup = nullptr;
        BlobFileGroupRef mCurrentFileGroup = nullptr;
    };

public:
    BResult Init(const BlobFileGroupManagerRef &blobFileGroupManager)
    {
        auto copyBlobFileGroupManager = blobFileGroupManager->DeepCopy(UINT64_MAX);
        if (copyBlobFileGroupManager == nullptr) {
            return BSS_INNER_RETRY;
        }
        mFileGroupIterator = std::make_shared<BlobFileGroupIterator>(copyBlobFileGroupManager);
        if (mFileGroupIterator->HasNext()) {
            BlobFileGroupRef currentFileGroup = mFileGroupIterator->Next();
            mFiles = currentFileGroup->GetFiles();
            mNextFileIndex = static_cast<int64_t>(mFiles.size());
        }
        return BSS_OK;
    }

    bool HasNext() override
    {
        Advance();
        return mNextFile != nullptr;
    }

    BlobImmutableFileRef Next() override
    {
        mCurrentFile = mNextFile;
        mNextFile = nullptr;
        return mCurrentFile;
    }

    void Close() override
    {
    }

    void Advance()
    {
        while (mNextFile == nullptr && mNextFileIndex > 0) {
            mNextFileIndex--;
            mNextFile = mFiles[mNextFileIndex];
            if (LIKELY(mNextFileIndex > 0)) {
                continue;
            }
            while (mCurrentFileGroup == nullptr && mFileGroupIterator->HasNext()) {
                mCurrentFileGroup = mFileGroupIterator->Next();
                mFiles = mCurrentFileGroup->GetFiles();
                if (mFiles.empty()) {
                    continue;
                }
                mNextFileIndex = static_cast<int64_t>(mFiles.size());
            }
            mCurrentFileGroup = nullptr;
        }
    }

private:
    std::vector<BlobImmutableFileRef> mFiles;
    int64_t mNextFileIndex = 0;
    BlobImmutableFileRef mNextFile = nullptr;
    BlobImmutableFileRef mCurrentFile = nullptr;
    BlobFileGroupRef mCurrentFileGroup = nullptr;
    std::shared_ptr<BlobFileGroupIterator> mFileGroupIterator = nullptr;
};

}  // namespace bss
}  // namespace ock

#endif
