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

#ifndef BOOST_SS_KV_TABLE_ITERATOR_H
#define BOOST_SS_KV_TABLE_ITERATOR_H

#include <map>
#include <set>
#include <utility>

#include "common/util/iterator.h"
#include "include/auto_closeable.h"
#include "serialized_data.h"
#include "slice_table/slice_table.h"

namespace ock {
namespace bss {
enum class EntryIteratorType { KVALUE_ITERATOR, KMAP_ITERATOR, KLIST_ITERATOR, KSUBLIST_ITERATOR, KSUBMAP_ITERATOR };
using PQSkipList = std::shared_ptr<SkipList<PQBinaryData, PQBinaryDataComparator>>;
using PQListIterator = SkipList<PQBinaryData, PQBinaryDataComparator>::Iterator;
class MapIterator : public Iterator<KeyValueRef> {
public:
    MapIterator(const KeyValueIteratorRef &freshTableIterator, const KeyValueIteratorRef &sliceTableIterator,
                EntryIteratorType iteratorType, BlobValueTransformFunc blobValueTransformFunc = nullptr)
        : mSliceTableIterator(std::move(sliceTableIterator)),
          mIteratorType(iteratorType),
          mFunc(std::move(blobValueTransformFunc))
    {
        if (freshTableIterator != nullptr) {
            while (freshTableIterator->HasNext()) {
                auto keyValue = freshTableIterator->Next();
                if (keyValue != nullptr) {
                    mExistKeyValue.emplace(&keyValue->key, keyValue);
                }
            }
            mFreshTableIterator = mExistKeyValue.begin();
        } else {
            mFreshTableIterator = mExistKeyValue.end();
        }

        mCurrentKeyValue = Advance();
    }

    bool HasNext() override
    {
        if (UNLIKELY(mCurrentKeyValue == nullptr)) {
            return false;
        }
        if (UNLIKELY(!BlobStore::ToBlobValue(mCurrentKeyValue->key, mCurrentKeyValue->value, mFunc))) {
            LOG_ERROR("Get value from blob failed, KeyHashCode: " << mCurrentKeyValue->key.KeyHashCode());
            return false;
        }
        return true;
    }

    KeyValueRef Next() override
    {
        mPreKeyValue = mCurrentKeyValue;
        mCurrentKeyValue = Advance();
        return mPreKeyValue;
    }

private:
    inline KeyValueRef Advance()
    {
        while (mFreshTableIterator != mExistKeyValue.end()) {
            auto keyValue = mFreshTableIterator->second;
            CONTINUE_LOOP_AS_NULLPTR(keyValue);
            mFreshTableIterator++;
            if (keyValue->value.ValueType() != DELETE) {
                return keyValue;
            }
        }
        while (mSliceTableIterator != nullptr && mSliceTableIterator->HasNext()) {
            auto keyValue = mSliceTableIterator->Next();
            if (keyValue != nullptr && keyValue->value.ValueType() != DELETE &&
                mExistKeyValue.find(&keyValue->key) == mExistKeyValue.end()) {
                return keyValue;
            }
        }
        return nullptr;
    }

private:
    KeyValueRef mPreKeyValue;
    KeyValueRef mCurrentKeyValue;
    KeyValueMap mExistKeyValue;
    KeyValueMap::iterator mFreshTableIterator;
    KeyValueIteratorRef mSliceTableIterator;
    EntryIteratorType mIteratorType;
    BlobValueTransformFunc mFunc;
};

class KeyIterator : public Iterator<KeyValueRef> {
public:
    KeyIterator(const KeyValueIteratorRef &freshTableIterator, const KeyValueIteratorRef &sliceTableIterator,
                EntryIteratorType iteratorType)
        : mSliceTableIterator(std::move(sliceTableIterator)), mIteratorType(iteratorType)
    {
        if (freshTableIterator != nullptr) {
            while (freshTableIterator->HasNext()) {
                auto keyValue = freshTableIterator->Next();
                if (keyValue != nullptr) {
                    mExistKeyValue.emplace(&keyValue->key, keyValue);
                }
            }
            mFreshTableIterator = mExistKeyValue.begin();
        } else {
            mFreshTableIterator = mExistKeyValue.end();
        }

        mCurrentKeyValue = Advance();
    }

    bool HasNext() override
    {
        return mCurrentKeyValue != nullptr;
    }

    KeyValueRef Next() override
    {
        mPreKeyValue = mCurrentKeyValue;
        mCurrentKeyValue = Advance();
        return mPreKeyValue;
    }

    inline EntryIteratorType GetIteratorType()
    {
        return mIteratorType;
    }

private:
    inline KeyValueRef Advance()
    {
        while (mFreshTableIterator != mExistKeyValue.end()) {
            auto keyValue = mFreshTableIterator->second;
            CONTINUE_LOOP_AS_NULLPTR(keyValue);
            mFreshTableIterator++;
            if (keyValue->value.ValueType() != DELETE && !IsVisited(keyValue)) {
                return keyValue;
            }
        }
        while (mSliceTableIterator != nullptr && mSliceTableIterator->HasNext()) {
            auto keyValue = mSliceTableIterator->Next();
            if (keyValue != nullptr && keyValue->value.ValueType() != DELETE &&
                mExistKeyValue.find(&keyValue->key) == mExistKeyValue.end() && !IsVisited(keyValue)) {
                return keyValue;
            }
        }
        return nullptr;
    }

    inline bool IsVisited(const KeyValueRef &keyValue)
    {
        RETURN_FALSE_AS_NULLPTR(keyValue);
        if (mIteratorType != EntryIteratorType::KMAP_ITERATOR) {
            return false;
        }
        bool visited = true;
        PriKeyNode *priKeyPtr = const_cast<PriKeyNode *>(&keyValue->key.PriKey());
        if (mVisited.find(priKeyPtr) == mVisited.end()) {
            visited = false;
            mVisited.emplace(priKeyPtr, keyValue);
        }
        return visited;
    }

private:
    KeyValueRef mCurrentKeyValue;
    KeyValueRef mPreKeyValue;
    KeyValueMap mExistKeyValue;
    KeyValueMap::iterator mFreshTableIterator;
    KeyValueIteratorRef mSliceTableIterator;
    EntryIteratorType mIteratorType;
    PriKeyValueMap mVisited;
};

struct WarpEntry {
public:
    WarpEntry() = default;
    WarpEntry(PQBinaryData data, uint64_t seq) : mData(data), mSeqId(seq)
    {
    }

    PQBinaryData mData;
    uint64_t mSeqId;
};
using WarpEntryPtr = std::shared_ptr<WarpEntry>;

class WarpIterator {
public:
    WarpIterator(PQListIterator iter, uint64_t seq, const PQBinaryData data)
        : mIterator(iter), mSeqId(seq), mSeekKey(data)
    {
        Advance();
    }

    bool HasNext()
    {
        return mCurrent != nullptr && PQBinaryDataComparator::ComparePrefix(mCurrent->mData, mSeekKey) == 0;
    }

    WarpEntryPtr GetNextKey()
    {
        return mCurrent;
    }

    WarpEntryPtr Next()
    {
        WarpEntryPtr tmp = mCurrent;
        Advance();
        return tmp;
    }

private:
    void Advance()
    {
        if (mIterator.HasNext()) {
            mCurrent = std::make_shared<WarpEntry>(mIterator.GetKey(), mSeqId);
            mIterator.Next();
        } else {
            mCurrent = nullptr;
        }
    }
    PQListIterator mIterator;
    WarpEntryPtr mCurrent = nullptr;
    uint64_t mSeqId;
    PQBinaryData mSeekKey;
};
using WarpIteratorPtr = std::shared_ptr<WarpIterator>;

class PQKeyIterator : public Iterator<BinaryData> {
public:
    explicit PQKeyIterator() = default;

    ~PQKeyIterator()
    {
        free(const_cast<uint8_t *>(mGroupId.Data()));
    }

    void Close() override
    {
        if (mFileIterator != nullptr) {
            mFileIterator->Close();
        }
    }

    BResult Init(const BinaryData &data, std::vector<PQSkipList> &skipList, const KeyValueIteratorRef &fileIterator)
    {
        PQBinaryData start;
        start.mData = data;
        for (const auto &item : skipList) {
            WarpIteratorPtr warpIterator = std::make_shared<WarpIterator>(item->NewIterator(start), item->GetSeqId(),
                                                                          start);
            if (warpIterator->HasNext()) {
                mIteratorQueue.emplace(warpIterator);
            }
        }

        DoGetNextEntry();
        mFileIterator = fileIterator;
        mFileData = fileIterator != nullptr && fileIterator->HasNext() ? fileIterator->Next() : nullptr;
        mSkipLists = skipList;
        Advance();
        return BSS_OK;
    }

    bool HasNext() override
    {
        if (mCurrentKey.IsNull()) {
            Advance();
        }
        return !mCurrentKey.IsNull();
    }

    BinaryData Next() override
    {
        auto tmp = mCurrentKey;
        mCurrentKey = BinaryData();
        return tmp;
    }

    inline PQBinaryData DoTrans(const KeyValueRef &keyValue)
    {
        if (keyValue == nullptr) {
            return PQBinaryData();
        }
        BinaryData key1(keyValue->key.PriKey().KeyData(), keyValue->key.PriKey().KeyLen());
        PQBinaryData pqBinaryData;
        pqBinaryData.mData = key1;
        pqBinaryData.mHashCode = keyValue->key.KeyHashCode();
        return pqBinaryData;
    }

    inline PQBinaryData GetNextValidFileData()
    {
        PQBinaryData key1 = DoTrans(mFileData);
        if (!key1.IsValid()) {
            return key1;
        }
        while (mFileData != nullptr && mFileData->value.ValueType() == DELETE) {
            mFileData = mFileIterator->HasNext() ? mFileIterator->Next() : nullptr;
            key1 = DoTrans(mFileData);
            if (!key1.IsValid()) {
                return key1;
            }
        }
        return key1;
    }

    inline void GetNextValidSkipListData(PQBinaryData &key1)
    {
        while (mSkipListData.IsValid()) {
            if (!key1.IsValid() && mSkipListData.mValueType == DELETE) {
                DoGetNextEntry();
                continue;
            } else if (key1.IsValid()) {
                auto cmp = PQBinaryDataComparator::Compare(key1, mSkipListData);
                if (cmp > 0 && mSkipListData.mValueType == DELETE) {
                    DoGetNextEntry();
                    continue;
                }
                if (cmp == 0 && mSkipListData.mValueType == DELETE) {
                    DoGetNextEntry();
                    CONTINUE_LOOP_AS_NULLPTR(mFileIterator);
                    mFileData = mFileIterator->HasNext() ? mFileIterator->Next() : nullptr;
                    key1 = GetNextValidFileData();
                    continue;
                }
                break;
            }
            break;
        }
    }

    inline void DoGetNextEntry()
    {
        WarpEntryPtr current;
        while (!mIteratorQueue.empty()) {
            WarpIteratorPtr iter = mIteratorQueue.top();
            WarpEntryPtr tmp = iter->GetNextKey();
            if (current == nullptr) {
                current = tmp;
            } else {
                auto cmp = PQBinaryDataComparator::Compare(current->mData, tmp->mData);
                if (cmp != 0) {
                    break;
                }
            }
            mIteratorQueue.pop();
            iter->Next();
            if (iter->HasNext()) {
                mIteratorQueue.emplace(iter);
            }
        }
        mSkipListData = current != nullptr ? current->mData : PQBinaryData();
    }

    void Advance()
    {
        PQBinaryData key1 = GetNextValidFileData();
        GetNextValidSkipListData(key1);

        if (key1.IsValid() && mSkipListData.IsValid()) {
            auto cmp = PQBinaryDataComparator::Compare(key1, mSkipListData);
            if (cmp < 0) {
                mCurrentKey = key1.mData;
                mFileData = mFileIterator->HasNext() ? mFileIterator->Next() : nullptr;
            } else if (cmp > 0) {
                mCurrentKey = mSkipListData.mData;
                DoGetNextEntry();
            } else {
                mCurrentKey = mSkipListData.mData;
                mFileData = mFileIterator->HasNext() ? mFileIterator->Next() : nullptr;
                DoGetNextEntry();
            }
        } else if (key1.IsValid()) {
            mCurrentKey = key1.mData;
            mFileData = mFileIterator->HasNext() ? mFileIterator->Next() : nullptr;
        } else if (mSkipListData.IsValid()) {
            mCurrentKey = mSkipListData.mData;
            DoGetNextEntry();
        } else {
            mCurrentKey = BinaryData();
        }
    }
    struct ComparePQListIterator {
        bool operator()(const WarpIteratorPtr &r1, const WarpIteratorPtr &r2) const
        {
            if (UNLIKELY(r1 == nullptr || r2 == nullptr || r1->GetNextKey() == nullptr ||
                         r2->GetNextKey() == nullptr)) {
                return false;
            }
            auto cmp = PQBinaryDataComparator::Compare(r1->GetNextKey()->mData, r2->GetNextKey()->mData);
            if (cmp != 0) {
                return cmp > 0;
            }
            return BssMath::IntegerCompare(r2->GetNextKey()->mSeqId, r1->GetNextKey()->mSeqId) > 0;
        }
    };

private:
    BinaryData mCurrentKey;
    std::priority_queue<WarpIteratorPtr, std::vector<WarpIteratorPtr>, ComparePQListIterator> mIteratorQueue;
    PQBinaryData mSkipListData;
    KeyValueIteratorRef mFileIterator;
    KeyValueRef mFileData;
    BinaryData mGroupId;
    std::vector<PQSkipList> mSkipLists;
};
}  // namespace bss
}  // namespace ock
#endif  // BOOST_SS_KV_TABLE_ITERATOR_H
