/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#ifndef BOOST_SS_MERGING_ITERATOR_H
#define BOOST_SS_MERGING_ITERATOR_H

#include <queue>
#include <utility>
#include <vector>

#include "binary/value/value_type.h"
#include "tombstone/tombstone_service.h"
#include "version/version.h"

namespace ock {
namespace bss {
class MergingIterator : public Iterator<KeyValueRef> {
public:
    MergingIterator(const std::vector<KeyValueIteratorRef> &iterators, const MemManagerRef &memManager,
                    FileProcHolder holder, bool sectionRead = false, TombstoneServiceRef tombstoneService = nullptr)
        : MergingIterator(iterators, memManager, nullptr, nullptr, false, holder, sectionRead, tombstoneService)
    {
    }

    MergingIterator(std::vector<KeyValueIteratorRef> iterators, const MemManagerRef &memManager,
                    std::function<void(VersionPtr &versionPtr)> cleaner, VersionPtr currentVersion, bool reverseOrder,
                    FileProcHolder holder, bool sectionRead = false, TombstoneServiceRef tombstoneService = nullptr)
        : mIterators(std::move(iterators)),
          mMemManager(memManager),
          mReverseOrder(reverseOrder),
          mHolder(holder),
          mSectionRead(sectionRead),
          mTombstoneService(tombstoneService)
    {
        mVersion = currentVersion;
        if (mVersion != nullptr) {
            mVersion->Retain();
        }
        mCleaner = cleaner;
        BResult ret = Advance();
        if (UNLIKELY(ret != BSS_OK)) {
            LOG_ERROR("advance failed, result: " << ret);
            mCurrentPair = nullptr;
        }
    }

    ~MergingIterator() override
    {
        std::vector<ByteBufferRef>().swap(mMergeBuffer);
    }

    bool HasNext() override
    {
        if (mCurrentPair != nullptr) {
            return true;
        }
        BResult ret = Advance();
        if (UNLIKELY(ret != BSS_OK)) {
            LOG_ERROR("advance failed, result: " << ret);
            mCurrentPair = nullptr;
        }
        return mCurrentPair != nullptr;
    }

    KeyValueRef Next() override
    {
        if (UNLIKELY(mCurrentPair == nullptr)) {
            return nullptr;
        }
        KeyValueRef result = mCurrentPair;
        mCurrentPair = nullptr;
        return result;
    }

    void Close() override
    {
        if (mVersion != nullptr && mCleaner != nullptr) {
            mCleaner(mVersion);
        }
    }

    static uint64_t MakeIteratorSeqId()
    {
        static std::atomic<uint64_t> baseSeq(NO_1);
        return baseSeq.fetch_add(NO_1);
    }

    inline void AddIter(const KeyValueIteratorRef &iterator)
    {
        if (iterator != nullptr && iterator->HasNext()) {
            mIteratorQueue.push(MakeRef<IteratorWrapper>(iterator, mReverseOrder));
        }
    }

    inline KeyValueRef NextKey()
    {
        if (mIteratorQueue.empty()) {
            return nullptr;
        }
        IteratorWrapperRef peekIterator = mIteratorQueue.top();
        return peekIterator->GetNextPair();
    }

private:
    void Initialize()
    {
        if (LIKELY(mInitialized)) {
            return;
        }
        for (auto &iterator : mIterators) {
            if (iterator != nullptr && iterator->HasNext()) {
                mIteratorQueue.push(MakeRef<IteratorWrapper>(iterator, mReverseOrder));
            }
        }
        std::vector<KeyValueIteratorRef>().swap(mIterators);
        mInitialized = true;
    }

    BResult Advance()
    {
        Initialize();

        mCurrentPair = nullptr;
        while (!mIteratorQueue.empty()) {
            IteratorWrapperRef peekIterator = mIteratorQueue.top();
            KeyValueRef pair = peekIterator->GetNextPair();
            if (!peekIterator->HasNext()) {
                LOG_ERROR("Iterator from priority queue is expected to have next pair.");
                return BSS_ERR;
            }

            if (mCurrentPair == nullptr) {
                mCurrentPair = pair;
                mPrevIterator = peekIterator;
            } else {
                const auto &key1 = mCurrentPair->key;
                const auto &key2 = pair->key;
                int cmp = key1.Compare(key2);
                if (CheckWrongOrder(key1, key2, cmp)) {
                    LOG_INFO("currentKey and nextKey is same iterator:" << (mPrevIterator == peekIterator));
                    peekIterator->PrintUsefulInfo();
                }
                mPrevIterator = peekIterator;

                // keys are not the same, return this entry.
                if (cmp != 0) {
                    break;
                }
                // keys are the same, if singKey value len > 4m, return to output to a file.
                // Keep oversized APPEND values in sections, but let PUT/DELETE consume obsolete values for the key.
                if (mSectionRead && mCurrentPair->value.ValueType() == ValueType::APPEND &&
                    (mCurrentPair->value.ValueLen() > IO_SIZE_4M) && StateId::IsList(key1.StateId())) {
                    break;
                }

                // keys are the same, merge them if needed.
                RETURN_NOT_OK_WITH_WARN_LOG(MergeValue(mCurrentPair->value, pair->value));

                if (mTombstoneService != nullptr) {
                    mTombstoneService->DeleteValue(pair->key, pair->value);
                }
            }

            mIteratorQueue.pop();
            peekIterator->Next();
            if (peekIterator->HasNext()) {
                mIteratorQueue.push(peekIterator);
            } else {
                peekIterator->Close();
            }
        }
        return BSS_OK;
    }

    bool CheckWrongOrder(const Key &key1, const Key &key2, int cmp) const
    {
        if (UNLIKELY((mReverseOrder && cmp < 0) || (!mReverseOrder && cmp > 0))) {
            LOG_ERROR("Wrong order, reverse order:" << mReverseOrder << ", cmp:" << cmp << ", curHash:"
                                                    << key1.ToString() << ", nextHash:" << key2.ToString());
            return true;
        }
        return false;
    }

    BResult MergeValue(Value &newerValue, Value &olderValue)
    {
        if (newerValue.ValueType() == ValueType::APPEND) {
            auto allocator = [this](uint32_t size) -> ByteBufferRef { return CreateBuffer(size); };
            return newerValue.MergeWithOlderValue(olderValue, allocator);
        }
        return BSS_OK;
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
    class IteratorWrapper;
    using IteratorWrapperRef = Ref<IteratorWrapper>;
    class IteratorWrapper : public Iterator<KeyValueRef> {
    public:
        IteratorWrapper(const KeyValueIteratorRef &entryIterator, bool reverseOrder)
            : mEntryIterator(entryIterator), mReverseOrder(reverseOrder)
        {
            mSeqId = MakeIteratorSeqId();
            Advance();
        }

        void Advance()
        {
            mNextPair = nullptr;
            if (mEntryIterator->HasNext()) {
                mNextPair = mEntryIterator->Next();
            }
        }

        inline bool HasNext() override
        {
            return mNextPair != nullptr;
        }

        KeyValueRef Next() override
        {
            KeyValueRef currentPair = mNextPair;
            Advance();
            return currentPair;
        }

        void PrintUsefulInfo() override
        {
            mEntryIterator->PrintUsefulInfo();
        }

        inline KeyValueRef GetNextPair() const
        {
            return mNextPair;
        }

        bool Compare(const IteratorWrapperRef &other)
        {
            auto kv1 = this->GetNextPair();
            auto kv2 = other->GetNextPair();
            return FullKey::CompareFullKey(kv1->key, kv1->value.SeqId(), kv2->key, kv2->value.SeqId(), mReverseOrder) <
                   0;
        }

        uint64_t GetSeqId() const
        {
            return mSeqId;
        }

    private:
        KeyValueIteratorRef mEntryIterator = nullptr;
        bool mReverseOrder = false;
        KeyValueRef mNextPair = nullptr;
        uint64_t mSeqId = 0;
    };

    struct CompareIteratorWrapper {
        bool operator()(const IteratorWrapperRef &r1, const IteratorWrapperRef &r2) const
        {
            return !r1->Compare(r2);
        }
    };

private:
    bool mInitialized = false;
    std::vector<KeyValueIteratorRef> mIterators;
    MemManagerRef mMemManager = nullptr;
    std::function<void(VersionPtr &versionPtr)> mCleaner;
    bool mReverseOrder = false;
    VersionPtr mVersion = nullptr;
    std::priority_queue<IteratorWrapperRef, std::vector<IteratorWrapperRef>, CompareIteratorWrapper> mIteratorQueue;
    KeyValueRef mCurrentPair = nullptr;
    std::vector<ByteBufferRef> mMergeBuffer;
    FileProcHolder mHolder;
    bool mSectionRead = false;
    TombstoneServiceRef mTombstoneService = nullptr;
    IteratorWrapperRef mPrevIterator = nullptr;
};
using MergingIteratorRef = std::shared_ptr<MergingIterator>;

}  // namespace bss
}  // namespace ock
#endif  // BOOST_SS_MERGING_ITERATOR_H