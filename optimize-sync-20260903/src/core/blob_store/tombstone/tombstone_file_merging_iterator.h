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

#ifndef BOOST_SS_TOMBSTONE_FILE_MERGING_ITERATOR_H
#define BOOST_SS_TOMBSTONE_FILE_MERGING_ITERATOR_H

#include <queue>

#include "tombstone.h"
#include "tombstone_file_iterator_wrapper.h"
#include "util/iterator.h"

namespace ock {
namespace bss {

struct CompareTombstoneIterator {
    bool operator()(const TombstoneFileIteratorWrapperRef &t1, const TombstoneFileIteratorWrapperRef &t2) const
    {
        auto p1 = t1->PeekNext();
        auto p2 = t2->PeekNext();
        if (UNLIKELY(p1 == nullptr || p2 == nullptr)) {
            LOG_ERROR("iterator tombstone is null");
            return false;
        }
        uint64_t blobId1 = p1->GetBlobId();
        uint64_t blobId2 = p2->GetBlobId();
        return blobId1 > blobId2;
    }
};

class TombstoneFileMergingIterator : public Iterator<TombstoneRef> {
public:
    explicit TombstoneFileMergingIterator(std::vector<TombstoneFileIteratorRef> &iterators) : mIterators(iterators)
    {
        for (const auto &item : mIterators) {
            auto wrapper = std::make_shared<TombstoneFileIteratorWrapper>(item);
            if (!wrapper->HasNext()) {
                wrapper->Close();
                continue;
            }
            mIteratorQueue.emplace(wrapper);
        }
    }

    BResult Init()
    {
        auto ret = Advance();
        if (LIKELY(ret == BSS_OK)) {
            mInit = true;
        }
        return ret;
    }

    bool HasNext() override
    {
        if (UNLIKELY(!mInit)) {
            return false;
        }
        return mNext != nullptr;
    }

    TombstoneRef Next() override
    {
        auto next = mNext;
        auto ret = Advance();
        RETURN_NULLPTR_AS_NOT_OK(ret);
        while (next->Equals(mNext)) {
            ret = Advance();
            RETURN_NULLPTR_AS_NOT_OK(ret);
        }
        return next;
    }

    TombstoneRef PeekNext()
    {
        return mNext;
    }

    void Close() override
    {
        while (!mIteratorQueue.empty()) {
            auto iterator = mIteratorQueue.top();
            mIteratorQueue.pop();
            iterator->Close();
        }
    }

private:
    BResult Advance()
    {
        mNext = nullptr;
        if (mIteratorQueue.empty()) {
            return BSS_OK;
        }
        auto iterator = mIteratorQueue.top();
        mIteratorQueue.pop();
        RETURN_ERROR_AS_NULLPTR(iterator);
        auto next = iterator->Next();
        if (iterator->HasNext()) {
            mIteratorQueue.emplace(iterator);
        } else {
            iterator->Close();
        }
        mNext = next;
        return BSS_OK;
    }

private:
    std::priority_queue<TombstoneFileIteratorWrapperRef, std::vector<TombstoneFileIteratorWrapperRef>,
                        CompareTombstoneIterator>
        mIteratorQueue;
    std::vector<TombstoneFileIteratorRef> mIterators;
    TombstoneRef mNext = nullptr;
    bool mInit = false;
};
using TombstoneFileMergingIteratorRef = std::shared_ptr<TombstoneFileMergingIterator>;
}  // namespace bss
}  // namespace ock

#endif  // BOOST_SS_TOMBSTONE_FILE_MERGING_ITERATOR_H
