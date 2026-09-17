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

#ifndef BOOST_SS_SKIPLIST_TRANSFORMER_H
#define BOOST_SS_SKIPLIST_TRANSFORMER_H

#include <set>

#include "binary/key_value.h"
#include "binary/query_binary.h"
#include "fresh_table/memory/skiplist.h"
#include "include/binary_data.h"

namespace ock {
namespace bss {

class PQIterator : public Iterator<KeyValueRef> {
public:
    PQIterator(const std::shared_ptr<SkipList<PQBinaryData, PQBinaryDataComparator>> &skipList, uint16_t stateId)
        : mSkipList(skipList), mSeqId(skipList->GetSeqId()), mStateId(stateId)
    {
        mIter = skipList->NewIterator();
        mSkipListEntry = mIter.HasNext() ? mIter.Next() : PQBinaryData();
        Advance();
    }

    bool HasNext() override
    {
        return mCurrent != nullptr;
    }

    KeyValueRef Next() override
    {
        auto next = mCurrent;
        Advance();
        return next;
    }

private:
    void Advance()
    {
        if (mSkipListEntry.IsValid()) {
            mCurrent = Trans(mSkipListEntry);
            mSkipListEntry = mIter.HasNext() ? mIter.Next() : PQBinaryData();
        } else {
            mCurrent = nullptr;
        }
    }

    inline KeyValueRef Trans(const PQBinaryData &key)
    {
        KeyValueRef kv = MakeRef<KeyValue>();
        uint16_t stateId = StateId::Of(mStateId, PQ);
        kv->key = QueryKey(stateId, key.mHashCode, key.mData);
        kv->value.Init(key.mValueType, 0, nullptr, mSeqId);
        return kv;
    }

    std::shared_ptr<SkipList<PQBinaryData, PQBinaryDataComparator>> mSkipList;
    SkipList<PQBinaryData, PQBinaryDataComparator>::Iterator mIter;
    PQBinaryData mSkipListEntry;

    KeyValueRef mCurrent;
    uint64_t mSeqId = 0;
    uint16_t mStateId = 0;
};

using PQTableIteratorRef = std::shared_ptr<PQIterator>;

}  // namespace bss
}  // namespace ock
#endif  // BOOST_SS_SKIPLIST_TRANSFORMER_H