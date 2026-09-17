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

#ifndef BOOST_SS_SORTEDKEYVALUEMERGINGITERATOR_H
#define BOOST_SS_SORTEDKEYVALUEMERGINGITERATOR_H

#include "slice_table/slice_table.h"

namespace ock {
namespace bss {
class SortedKeyValueMergingIterator : public Iterator<KeyValueRef> {
public:
    BResult Init(const SliceKVIteratorPtr &sliceTableIterator, KeyValueIteratorRef &lsmIterator,
                 const MemManagerRef &memManager, bool sectionRead = false,
                 FileProcHolder holder = FileProcHolder::FILE_STORE_ITERATOR);

    bool HasNext() override
    {
        return mCurrentEntry != nullptr || mNextEntry != nullptr;
    }

    KeyValueRef Next() override
    {
        if (mCurrentEntry != nullptr && mNextEntry != nullptr) {
            auto next = mCurrentEntry;
            mCurrentEntry = nullptr;
            return next;
        } else if (mCurrentEntry == nullptr && mNextEntry != nullptr) {
            auto next = mNextEntry;
            mNextEntry = nullptr;
            Advance();
            return next;
        } else {
            auto next = mCurrentEntry;
            Advance();
            return next;
        }
    }

    ByteBufferRef CreateBuffer(uint32_t size)
    {
        uintptr_t dataAddress = FileMemAllocator::Alloc(mMemManager, mHolder, size, __FUNCTION__);
        if (UNLIKELY(dataAddress == 0)) {
            LOG_WARN("Alloc memory for slice kv iterator failed, size:" << size);
            return nullptr;
        }
        // reserve some byte buffer for flush.
        auto byteBuffer = MakeRef<ByteBuffer>(reinterpret_cast<uint8_t *>(dataAddress), size, mMemManager);
        if (UNLIKELY(byteBuffer == nullptr)) {
            mMemManager->ReleaseMemory(dataAddress);
            LOG_ERROR("Make ref failed, byteBuffer is null.");
        }
        return byteBuffer;
    }

private:
    void Advance();

    void MergeValue();

private:
    SliceKVIteratorPtr mSliceTableIterator = nullptr;
    KeyValueRef mSliceTablePointer = nullptr;
    KeyValueIteratorRef mLsmIterator = nullptr;
    KeyValueRef mLsmIteratorPointer = nullptr;
    KeyValueRef mCurrentEntry = nullptr;
    KeyValueRef mNextEntry = nullptr;
    MemManagerRef mMemManager = nullptr;
    FileProcHolder mHolder = FileProcHolder::FILE_STORE_ITERATOR;
    bool mSectionRead = false;
};
using SortedKeyValueMergingIteratorRef = std::shared_ptr<SortedKeyValueMergingIterator>;

class SortedKeyValueMergingIteratorV2 : public Iterator<KeyValueRef> {
public:
    SortedKeyValueMergingIteratorV2(const SortedKeyValueMergingIteratorRef &mergingIterator, const KeyFilter &keyFilter)
        : mMergingIterator(mergingIterator), mKeyFilter(keyFilter)
    {
        mCurrentKeyValue = Advance();
    }

    bool HasNext() override
    {
        return mCurrentKeyValue != nullptr;
    }

    KeyValueRef Next() override
    {
        auto result = mCurrentKeyValue;
        mCurrentKeyValue = Advance();
        return result;
    }

    void Close() override
    {
        mMergingIterator->Close();
    }

private:
    KeyValueRef Advance()
    {
        if (!mMergingIterator->HasNext()) {
            return nullptr;
        }

        auto keyValue = mMergingIterator->Next();
        if (keyValue == nullptr) {
            LOG_ERROR("Next key is nullptr.");
            return nullptr;
        }

        // Transform to KeyValue.
        auto key = keyValue->key;
        auto value = keyValue->value;

        // do key filter.
        if (mKeyFilter(key)) {
            return Advance();
        }

        if (value.ValueType() == DELETE) {
            keyValue->value.Init(DELETE, 0, nullptr, value.SeqId(), nullptr);
        }
        return keyValue;
    }

private:
    SortedKeyValueMergingIteratorRef mMergingIterator;
    KeyFilter mKeyFilter;
    KeyValueRef mCurrentKeyValue;
};

}  // namespace bss
}  // namespace ock
#endif  // BOOST_SS_SORTEDKEYVALUEMERGINGITERATOR_H