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

#ifndef BOOST_HASH_MAP_H
#define BOOST_HASH_MAP_H

#include <deque>

#include "binary/fresh_binary.h"
#include "binary/key_value.h"
#include "boost_const.h"
#include "include/ref.h"

namespace ock {
namespace bss {

class BoostHashMap;
using BoostHashMapRef = Ref<BoostHashMap>;

class BoostHashMap : public EnableRefFromThis<BoostHashMap>, public Referable {
public:
    BoostHashMap() = default;
    BoostHashMap(const MemorySegmentRef &memorySegment, uint32_t rootPos, bool initStructure)
    {
        Init(memorySegment, rootPos, initStructure);
    }

    inline NodeType GetType() const
    {
        return NodeType::HASHMAP;
    }

    inline BResult Init(const MemorySegmentRef &memorySegment, bool initStructure)
    {
        return Init(memorySegment, NO_0, initStructure);
    }

    BResult Init(const MemorySegmentRef &memorySegment, uint32_t rootPos, bool initStructure);

    inline uint32_t Size()
    {
        return mMemorySegment->ToUint32_t(mRootPos + INDEX_NODE_SIZE_OFFSET);
    }

    inline uint32_t BucketCount() const
    {
        return mMemorySegment->ToUint32_t(mRootPos + BUCKET_COUNT_OFFSET);
    }

    inline uint32_t FindBucketAddress(uint32_t bucketIndex) const
    {
        uint32_t sectionIndexBase = mMemorySegment->ToUint32_t(mRootPos + SECTION_INDEX_BASE_OFFSET);
        uint32_t sectionBasePos = sectionIndexBase + (bucketIndex >> NO_3 << NO_2);  // bucketIndex /
                                                                                     // BUCKET_COUNT_PER_SECTION *
                                                                                     // SECTION_INDEX_SIZE;
        uint32_t sectionBase = mMemorySegment->ToUint32_t(sectionBasePos);
        return sectionBase + ((bucketIndex & NO_7) << NO_2);  // bucketIndex % BUCKET_COUNT_PER_SECTION * BUCKET_SIZE;
    }

    bool Get(const Key &key, Value &value);

    bool GetKMap(const Key &key, Value &value);

    std::deque<Value> GetList(const Key &key);

    BResult Put(const Key &key, const Value &value, bool subKeyFlag = false);
    BResult Put(const KeyValue &keyValue);

    BResult Append(const Key &key, const Value &value);

    inline MemorySegmentRef GetMemorySegment()
    {
        return mMemorySegment;
    }

    /**
     * Find the HashMap through the key, and then obtain all entries in the HashMap that pass the KeyFilter.
     * NOTICE: Just support SUB_VALUE/MAP/SUB_MAP type!!!
     * @param key the specified key for find HashMap.
     * @return
     */
    KeyValueIteratorRef MapEntryIterator(const Key &key);

    /**
     * Get all entries from fresh table.
     * NOTICE: support SUB_VALUE/MAP/SUB_MAP type to get key and value,
     *         support LIST/SUB_LIST to get key.
     * @param priKeyFilter primary key filter.
     * @param secKeyFilter secondary key filter.
     * @return key value iterator.
     */
    KeyValueIteratorRef EntryIterator(const PriKeyFilter &priKeyFilter, const SecKeyFilter &secKeyFilter);

    IteratorRef<std::pair<FreshKeyNodePtr, FreshValueNodePtr>> KVIterator();

    std::pair<FreshKeyNodePtr, FreshValueNodePtr> GetEntryFromPointer(uint32_t IndexNodePointer);

    static inline uint32_t GetPriKeyLength(const Key &key)
    {
        return key.PriKey().KeyLen() + NO_6;
    }

    static inline uint32_t GetSecKeyLength(const Key &key)
    {
        return key.SecKey().KeyLen() + NO_4;
    }

    static inline uint32_t GetKeyLength(const Key &key)
    {
        return GetPriKeyLength(key) + (key.HasSecKey() ? GetSecKeyLength(key) : NO_0);
    }

    static inline uint32_t GetValueLength(const Value &value)
    {
        return value.ValueType() == DELETE ? NO_9 : (NO_9 + value.ValueLen());
    }

private:
    BResult InitStructure();

    inline void AddSize()
    {
        mMemorySegment->PutUint32_t(mRootPos + INDEX_NODE_SIZE_OFFSET, 1 + Size());
    }

    uint32_t FindIndexNode(const Key &key, bool subKeyFlag = false);

    inline void PutAtBucket(uint32_t bucketAddress, uint32_t indexNodeAddress)
    {
        uint32_t lastValue = mMemorySegment->ToUint32_t(bucketAddress);
        mMemorySegment->PutUint32_t(bucketAddress, indexNodeAddress);
        mMemorySegment->PutUint32_t(indexNodeAddress + NEXT_INDEX_NODE_OFFSET, lastValue);
    }

    BoostHashMapRef ParseValueNode(uint32_t valueNode);

    BResult CreateKeyNode(const Key &key, uint32_t &keyNode, bool subKeyFlag = false);

    BResult CreateValueNode(const Value &value, NodeType type, uint32_t &valueNode);

    BResult CreateMapValueNode(uint32_t &valueNode);

    BResult CreateIndexNode(uint32_t hashCode, uint32_t &indexNode, bool isList = false, uint32_t valueLen = 0);

    BResult InplaceUpdateValue(uint32_t valueNode, const Value &value);

    BResult WritePriKey(const PriKeyNode &key, uint32_t destOffset);
    BResult WriteSecKey(const SecKeyNode &key, uint32_t destOffset);
    BResult WriteValue(const Value &value, uint32_t destOffset);

    BResult ExpandBucket();

    BResult AddIndexNode(const Key &key, const Value &value, bool subKeyFlag = false);

    BResult AddIndexNodeWithValueNode(const Key &key, uint32_t valueNode);

    void ReHashing(uint32_t nowBucketCount, uint32_t bucketSectionIndex, uint32_t toBucketCount);

    KeyValueRef CreateKeyValue(const PriKeyNode &priKey);

    KeyValueRef CreateKeyValue(const PriKeyNode &priKey, const FreshValueNode *valueNode);

    KeyValueRef CreateKeyValue(const PriKeyNode &priKey, const SecKeyNode &secKey, const FreshValueNode *valueNode);

    void VisitNested(const BoostHashMap &hashMap, const PriKeyNode &priKey, const SecKeyFilter &secKeyFilter,
                     std::vector<KeyValueRef> &keyValues);

    void Visit(std::function<bool(const FreshNode &freshNode)> handle) const;

    inline bool GetByPriKey(const uint8_t *segment, const PriKeyNode &priKey, BoostHashMap &hashMap)
    {
        const FreshValueNode *valueNode = FindValueNode(segment, priKey);
        if (valueNode == nullptr) {
            return false;
        }

        uint32_t offset = mMemorySegment->Offset(valueNode->ValueDataBase());
        if (offset == UINT32_MAX) {
            LOG_ERROR("pos is nullptr.");
            return false;
        }
        if (hashMap.Init(mMemorySegment, offset, false) != BSS_OK) {
            return false;
        }
        return true;
    }

    inline bool GetBySecKey(const uint8_t *segment, const SecKeyNode &secKey, Value &value)
    {
        const FreshValueNode *valueNode = FindValueNode(segment, secKey);
        if (valueNode == nullptr) {
            return false;
        }

        // if deleted, return.
        if (valueNode->ValueType() != DELETE) {
            value.Init(valueNode->ValueType(), valueNode->ValueDataLen(), valueNode->Value(), valueNode->ValueSeqId(),
                       mMemorySegment);
        }

        return true;
    }

    inline uint32_t GetBucketBase(const uint8_t *segment, uint32_t hashCode)
    {
        uint32_t bucketAddress = FindBucketAddress(hashCode & (BucketCount() - 1));
        return *reinterpret_cast<const uint32_t *>(segment + bucketAddress);
    }

    template <typename T> const FreshValueNode *FindValueNode(const uint8_t *segment, const T &key)
    {
        uint32_t nodeOffset = GetBucketBase(segment, key.HashCode());  // fresh 层用hashcode获取 bucketBase
        while (nodeOffset != 0) {
            FreshNode freshNode(segment, nodeOffset);
            if (freshNode.KeyNode()->Equal(key)) {
                // value node address is in segment, so we can return it .
                return freshNode.ValueNode();
            }
            nodeOffset = freshNode.IndexNode()->NextIndexNodeOffset();
        }

        return nullptr;
    }

private:
    MemorySegmentRef mMemorySegment;
    uint32_t mRootPos;
    float mLoadFactor = 0.6;
};

class BoostHashMapIterator : public Iterator<std::pair<FreshKeyNodePtr, FreshValueNodePtr>> {
public:
    BoostHashMapIterator() = default;

    BResult Init(const BoostHashMapRef &hashMap)
    {
        if (UNLIKELY(hashMap == nullptr)) {
            LOG_ERROR("BoostHashMapRef is nullptr!");
            return BSS_ERR;
        }
        mBoostHashMap = hashMap;
        mCurrentBucket = 0;
        mNextIndexNode = 0;
        Advance();
        return BSS_OK;
    }

    bool HasNext() override
    {
        if (mNextIndexNode == 0) {
            return false;
        }
        return true;
    }

    std::pair<FreshKeyNodePtr, FreshValueNodePtr> Next() override
    {
        uint32_t next = this->mNextIndexNode;
        Advance();
        return mBoostHashMap->GetEntryFromPointer(next);
    }

private:
    void Advance()
    {
        uint32_t bucketCount = mBoostHashMap->BucketCount();
        while (mCurrentBucket < bucketCount) {
            if (mNextIndexNode != 0) {
                auto off = mNextIndexNode + NEXT_INDEX_NODE_OFFSET;
                mNextIndexNode = mBoostHashMap->GetMemorySegment()->ToUint32_t(off);
            } else {
                uint32_t bucketAddress = mBoostHashMap->FindBucketAddress(mCurrentBucket);
                mNextIndexNode = mBoostHashMap->GetMemorySegment()->ToUint32_t(bucketAddress);
            }
            if (mNextIndexNode != 0) {
                break;
            }
            mCurrentBucket++;
        }
    }

    BoostHashMapRef mBoostHashMap;
    uint32_t mCurrentBucket = 0;
    uint32_t mNextIndexNode = 0;
};

}  // namespace bss
}  // namespace ock

#endif  // BOOST_HASH_MAP_H
