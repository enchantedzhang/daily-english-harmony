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

#ifndef BOOST_SS_RECORD_DELETE_ITERATOR_WRAPPER_H
#define BOOST_SS_RECORD_DELETE_ITERATOR_WRAPPER_H

#include "blob_file_group_iterator.h"
#include "tombstone_file_merging_iterator.h"

namespace ock {
namespace bss {

class RecordDeleteIteratorWrapper : public Iterator<TombstoneRef> {
public:
    RecordDeleteIteratorWrapper(TombstoneFileMergingIteratorRef &tombstoneFileMergingIterator,
                                BlobFileGroupIteratorRef &blobFileGroupIterator)
        : mTombstoneIterator(tombstoneFileMergingIterator), mBlobFileIterator(blobFileGroupIterator)
    {
    }

    inline bool HasNext()
    {
        return mTombstoneIterator->HasNext();
    }

    TombstoneRef Next()
    {
        auto tombstone = mTombstoneIterator->Next();
        BlobImmutableFileRef blobImmutableFile = mBlobFileIterator->PeekFirst(tombstone->GetBlobId(),
                                                                              tombstone->GetKeyGroup());
        while (blobImmutableFile != nullptr &&
               tombstone->GetBlobId() > blobImmutableFile->GetBlobFileMeta()->GetMaxBlobId()) {
            mBlobFileIterator->Advance();
            blobImmutableFile = mBlobFileIterator->PeekNext();
        }
        if (blobImmutableFile != nullptr) {
            auto blobFileMeta = blobImmutableFile->GetBlobFileMeta();
            if (tombstone->GetBlobId() >= blobFileMeta->GetMinBlobId() &&
                tombstone->GetBlobId() <= blobFileMeta->GetMaxBlobId()) {
                blobFileMeta->IncBlobDeleteNum();
            }
        }
        return tombstone;
    }

    void Close()
    {
        mTombstoneIterator->Close();
    }

private:
    TombstoneFileMergingIteratorRef mTombstoneIterator;
    BlobFileGroupIteratorRef mBlobFileIterator;
};

}  // namespace bss
}  // namespace ock

#endif  // BOOST_SS_RECORD_DELETE_ITERATOR_WRAPPER_H