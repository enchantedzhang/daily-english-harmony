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

#ifndef BOOST_SS_BLOB_FILE_MERGING_ITERATOR_H
#define BOOST_SS_BLOB_FILE_MERGING_ITERATOR_H

#include "blob_Immutable_file_iterator.h"
#include "blob_immutable_file.h"

namespace ock {
namespace bss {
class BlobFileMergingIterator {
public:
    BlobFileMergingIterator(const std::vector<BlobImmutableFileRef> &files, const MemManagerRef &memManager)
        : mFiles(files), mMemManager(memManager)
    {
    }
    BResult Init()
    {
        auto ret = Advance();
        if (LIKELY(ret == BSS_OK)) {
            mInit = true;
        }
        return ret;
    }

    bool HasNext()
    {
        if (!mInit) {
            LOG_ERROR("BlobFileMergingIterator not init.");
            return false;
        }
        return mNext != nullptr;
    }

    BlobValueWrapperRef Next()
    {
        auto next = mNext;
        mNext = nullptr;
        Advance();
        return next;
    }

    inline BlobValueWrapperRef PeekNext()
    {
        return mNext;
    }

    inline void Close()
    {
        if (mCurFile != nullptr) {
            mCurFile->Close();
        }
    }

    inline BlobFileMetaRef &GetCurFileMeta()
    {
        return mCurFileMeta;
    }

private:
    BResult Advance()
    {
        while (mCurFile == nullptr || !mCurFile->HasNext()) {
            if (mFiles.empty()) {
                break;
            }
            auto nextFile = mFiles.front();
            mFiles.erase(mFiles.begin());
            if (mCurFile != nullptr) {
                mCurFile->Close();
            }
            mCurFile = std::make_shared<BlobImmutableFileIterator>(nextFile, mMemManager);
            RETURN_NOT_OK(mCurFile->Init());
            mCurFileMeta = nextFile->GetBlobFileMeta();
        }
        if (mCurFile != nullptr && mCurFile->HasNext()) {
            mNext = mCurFile->Next();
        }
        return BSS_OK;
    }

private:
    std::vector<BlobImmutableFileRef> mFiles;
    BlobImmutableFileIteratorRef mCurFile = nullptr;
    BlobFileMetaRef mCurFileMeta = nullptr;
    BlobValueWrapperRef mNext = nullptr;
    MemManagerRef mMemManager = nullptr;
    bool mInit = false;
};
}  // namespace bss
}  // namespace ock

#endif
