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

#include "sorted_key_value_merging_iterator.h"

namespace ock {
namespace bss {

BResult SortedKeyValueMergingIterator::Init(const SliceKVIteratorPtr &sliceTableIterator,
                                            KeyValueIteratorRef &lsmIterator, const MemManagerRef &memManager,
                                            bool sectionRead, FileProcHolder holder)
{
    if (UNLIKELY(sliceTableIterator == nullptr || lsmIterator == nullptr)) {
        LOG_ERROR("sliceTableIterator is nullptr");
        return BSS_ERR;
    }

    mSliceTableIterator = sliceTableIterator;
    mLsmIterator = lsmIterator;
    mMemManager = memManager;
    mSectionRead = sectionRead;
    mHolder = holder;

    if (mSliceTableIterator->HasNext() == Iterator_Result_Continue) {
        mSliceTablePointer = this->mSliceTableIterator->Next();
    }

    if (mLsmIterator->HasNext()) {
        mLsmIteratorPointer = this->mLsmIterator->Next();
    } else {
        mLsmIterator->Close();
    }

    Advance();
    return BSS_OK;
}

void SortedKeyValueMergingIterator::Advance()
{
    mCurrentEntry = {};
    if (mSliceTablePointer != nullptr && mLsmIteratorPointer != nullptr) {
        MergeValue();
        return;
    }

    if (mSliceTablePointer != nullptr) {
        mCurrentEntry = mSliceTablePointer;
        Iterator_Result result = mSliceTableIterator->HasNext();
        mSliceTablePointer = (result == Iterator_Result_Continue) ? mSliceTableIterator->Next() : nullptr;
        return;
    }

    if (mLsmIteratorPointer != nullptr) {
        mCurrentEntry = mLsmIteratorPointer;
        bool hasNext = mLsmIterator->HasNext();
        mLsmIteratorPointer = hasNext ? mLsmIterator->Next() : nullptr;
        if (!hasNext) {
            mLsmIterator->Close();
        }
    }
}

void SortedKeyValueMergingIterator::MergeValue()
{
    int32_t cmp = mSliceTablePointer->key.Compare(mLsmIteratorPointer->key);
    if (cmp < 0) {
        mCurrentEntry = mSliceTablePointer;
        Iterator_Result result = mSliceTableIterator->HasNext();
        mSliceTablePointer = (result == Iterator_Result_Continue) ? mSliceTableIterator->Next() : nullptr;
        return;
    }

    if (cmp > 0) {
        mCurrentEntry = mLsmIteratorPointer;
        mLsmIteratorPointer = mLsmIterator->HasNext() ? mLsmIterator->Next() : nullptr;
        while (mLsmIteratorPointer != nullptr && mCurrentEntry->value.ValueType() != APPEND &&
               mLsmIteratorPointer->key.Compare(mCurrentEntry->key) == 0) {
            mLsmIteratorPointer = mLsmIterator->HasNext() ? mLsmIterator->Next() : nullptr;
        }
        return;
    }

    if (mSectionRead && mSliceTablePointer->value.ValueType() == APPEND) {
        mCurrentEntry = mSliceTablePointer;
        mNextEntry = mLsmIteratorPointer;
    } else {
        Value &newerValue = mSliceTablePointer->value;
        auto &olderValue = mLsmIteratorPointer->value;
        auto allocator = [this](uint32_t size) -> ByteBufferRef { return CreateBuffer(size); };
        auto ret = newerValue.MergeWithOlderValue(olderValue, allocator);
        if (UNLIKELY(ret != BSS_OK)) {
            LOG_WARN("Failed to merge value. newerValue:" << newerValue.ToString()
                                                          << ", olderValue:" << olderValue.ToString());
        }
        KeyValueRef kv = MakeRef<KeyValue>();
        kv->key = mSliceTablePointer->key;
        kv->value = newerValue;
        mCurrentEntry = std::move(kv);
    }

    Iterator_Result result = mSliceTableIterator->HasNext();
    mSliceTablePointer = (result == Iterator_Result_Continue) ? mSliceTableIterator->Next() : nullptr;
    mLsmIteratorPointer = mLsmIterator->HasNext() ? mLsmIterator->Next() : nullptr;
    auto valueType = mNextEntry != nullptr ? mNextEntry->value.ValueType() : mCurrentEntry->value.ValueType();
    while (mLsmIteratorPointer != nullptr && valueType != APPEND &&
           mLsmIteratorPointer->key.Compare(mCurrentEntry->key) == 0) {
        mLsmIteratorPointer = mLsmIterator->HasNext() ? mLsmIterator->Next() : nullptr;
    }
}
}  // namespace bss
}  // namespace ock