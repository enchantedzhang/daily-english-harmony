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

#ifndef BOOST_SS_TOMBSTONE_FILE_ITERATOR_WRAPPER_H
#define BOOST_SS_TOMBSTONE_FILE_ITERATOR_WRAPPER_H

#include "tombstone_file_iterator.h"

namespace ock {
namespace bss {
class TombstoneFileIteratorWrapper {
public:
    explicit TombstoneFileIteratorWrapper(TombstoneFileIteratorRef iterator) : mIterator(iterator)
    {
        mNext = mIterator->HasNext() ? mIterator->Next() : nullptr;
    }

    bool HasNext()
    {
        return mNext != nullptr;
    }

    TombstoneRef Next()
    {
        TombstoneRef next = mNext;
        mNext = mIterator->HasNext() ? mIterator->Next() : nullptr;
        return next;
    }

    TombstoneRef PeekNext() const
    {
        return mNext;
    }

    void Close()
    {
        mIterator->Close();
    }

private:
    TombstoneRef mNext;
    TombstoneFileIteratorRef mIterator;
};
using TombstoneFileIteratorWrapperRef = std::shared_ptr<TombstoneFileIteratorWrapper>;
}  // namespace bss
}  // namespace ock

#endif  // BOOST_SS_TOMBSTONE_FILE_ITERATOR_WRAPPER_H
