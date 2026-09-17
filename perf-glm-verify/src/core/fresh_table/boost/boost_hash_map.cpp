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

#include "boost_hash_map.h"

#include "iostream"

namespace ock {
namespace bss {
using namespace std;
BResult BoostHashMap::Init(const MemorySegmentRef &memorySegment, uint32_t rootPos, bool initStructure)
{
    RETURN_INVALID_PARAM_AS_NULLPTR(memorySegment);
    mMemorySegment = memorySegment;
    mRootPos = rootPos;
    if (initStructure) {
        BResult result = mMemorySegment->Allocate(HASHMAP_INIT_SIZE);
        if (UNLIKELY(result != BSS_OK)) {
            LOG_ERROR("Memory segment allocate failed, result:" << result << ", length:" << HASHMAP_INIT_SIZE);
            return result;
        }
        return InitStructure();
    }
    return BSS_OK;
}

BResult BoostHashMap::InitStructure()
{
    mMemorySegment->PutUint32_t(mRootPos + INDEX_NODE_SIZE_OFFSET, 0);
    mMemorySegment->PutUint32_t(mRootPos + BUCKET_COUNT_OFFSET, 0);
    mMemorySegment->PutUint32_t(mRootPos + SECTION_INDEX_BASE_OFFSET, 0);
    return ExpandBucket();
}

BResult BoostHashMap::ExpandBucket()
{
    uint32_t nowBucketCount = BucketCount();
    if (nowBucketCount > (UINT32_MAX >> 1)) {
        LOG_ERROR("Expand bucket failed, current bucket count is " << nowBucketCount << ", it's too large.");
        return BSS_FRESH_TABLE_IS_FULL;
    }

    uint32_t toBucketCount = (nowBucketCount == 0) ? NO_8 : (nowBucketCount * 2);
    // SectionIndex需要连续，不可复用之前的。
    uint32_t nowBucketSectionIndexSize = nowBucketCount / BUCKET_COUNT_PER_SECTION * SECTION_INDEX_SIZE;
    uint32_t nowBucketSectionIndex = mMemorySegment->ToUint32_t(mRootPos + SECTION_INDEX_BASE_OFFSET);
    uint32_t newBucketSectionIndexSize = toBucketCount / BUCKET_COUNT_PER_SECTION * SECTION_INDEX_SIZE;
    uint32_t newBucketSectionIndex;
    BResult result = mMemorySegment->Allocate(newBucketSectionIndexSize, newBucketSectionIndex);
    RETURN_NOT_OK_NO_LOG(result);

    result = mMemorySegment->Copy(nowBucketSectionIndex, newBucketSectionIndex, nowBucketSectionIndexSize);
    if (UNLIKELY(result != BSS_OK)) {
        LOG_ERROR("Memory segment copy failed, result:" << result);
        return result;
    }

    // Bucket可复用之前的，只用调整增加的Bucket部分即可。
    uint32_t incrementalBucketSize = (toBucketCount - nowBucketCount) * BUCKET_SIZE;
    uint32_t incrementalBucketPos;
    result = mMemorySegment->Allocate(incrementalBucketSize, incrementalBucketPos);
    RETURN_NOT_OK_NO_LOG(result);

    for (uint32_t bucketCount = 0; bucketCount < toBucketCount - nowBucketCount; bucketCount++) {
        // 初始化Bucket指针
        mMemorySegment->PutUint32_t(incrementalBucketPos + bucketCount * BUCKET_SIZE, 0);
    }

    for (uint32_t sectionIdx = 0; sectionIdx < (toBucketCount - nowBucketCount) / BUCKET_COUNT_PER_SECTION;
         sectionIdx++) {
        uint32_t incrementalSectionIndexPos = newBucketSectionIndex + nowBucketSectionIndexSize;
        uint32_t curBucketPos = incrementalBucketPos + sectionIdx * BUCKET_COUNT_PER_SECTION * BUCKET_SIZE;
        mMemorySegment->PutUint32_t(incrementalSectionIndexPos + sectionIdx * SECTION_INDEX_SIZE, curBucketPos);
    }

    mMemorySegment->PutUint32_t(mRootPos + SECTION_INDEX_BASE_OFFSET, newBucketSectionIndex);
    mMemorySegment->PutUint32_t(mRootPos + BUCKET_COUNT_OFFSET, toBucketCount);

    // 需要重新调整IndexNode挂的Bucket
    if (nowBucketCount != 0) {
        ReHashing(nowBucketCount, newBucketSectionIndex, toBucketCount);
    }
    return BSS_OK;
}

void BoostHashMap::ReHashing(uint32_t nowBucketCount, uint32_t bucketSectionIndex, uint32_t toBucketCount)
{
    for (uint32_t sectionIdx = 0; sectionIdx < (nowBucketCount / BUCKET_COUNT_PER_SECTION); sectionIdx++) {
        uint32_t sectionBase = mMemorySegment->ToUint32_t(bucketSectionIndex + sectionIdx * SECTION_INDEX_SIZE);
        for (uint32_t bucketIndex = 0; bucketIndex < BUCKET_COUNT_PER_SECTION; bucketIndex++) {
            uint32_t bucketChainPointer = mMemorySegment->ToUint32_t(sectionBase + bucketIndex * BUCKET_SIZE);
            mMemorySegment->PutUint32_t(sectionBase + bucketIndex * BUCKET_SIZE, 0);

            while (bucketChainPointer != 0) {
                uint32_t hashCode = mMemorySegment->ToUint32_t(bucketChainPointer + 0);
                uint32_t bucketAddress = FindBucketAddress(hashCode & (toBucketCount - 1));
                uint32_t nextPointer = mMemorySegment->ToUint32_t(bucketChainPointer + NEXT_INDEX_NODE_OFFSET);
                PutAtBucket(bucketAddress, bucketChainPointer);
                bucketChainPointer = nextPointer;
            }
        }
    }
}

uint32_t BoostHashMap::FindIndexNode(const Key &key, bool subKeyFlag)
{
    uint32_t targetHashCode = subKeyFlag ? key.SecKey().HashCode() : key.PriKey().HashCode();
    uint32_t bucketAddress = FindBucketAddress(targetHashCode & (BucketCount() - 1));
    uint32_t bucketChainPointer = mMemorySegment->ToUint32_t(bucketAddress);
    while (bucketChainPointer != 0) {
        uint32_t hashCode = mMemorySegment->ToUint32_t(bucketChainPointer + HASHCODE_OFFSET);
        if (targetHashCode == hashCode) {
            uint32_t keyNode = mMemorySegment->ToUint32_t(bucketChainPointer + KEY_NODE_OFFSET);
            const auto *freshKeyNode = reinterpret_cast<const FreshKeyNode *>(mMemorySegment->GetSegment() + keyNode);
            if ((!subKeyFlag && freshKeyNode->Equal(key.PriKey())) ||
                (subKeyFlag && freshKeyNode->Equal(key.SecKey()))) {
                return bucketChainPointer;
            }
        }
        bucketChainPointer = mMemorySegment->ToUint32_t(bucketChainPointer + NEXT_INDEX_NODE_OFFSET);
    }
    return bucketChainPointer;
}

BoostHashMapRef BoostHashMap::ParseValueNode(uint32_t valueNode)
{
    if (UNLIKELY(UINT32_MAX - valueNode < VALUE_TYPE_OFFSET)) {
        LOG_WARN("Value node is inValid: " << valueNode);
        return nullptr;
    }
    uint8_t typeCode = mMemorySegment->ToUint8_t(valueNode + VALUE_TYPE_OFFSET);
    if (typeCode == static_cast<uint8_t>(NodeType::HASHMAP)) {
        BoostHashMapRef hashMap = MakeRef<BoostHashMap>();
        RETURN_NULLPTR_AS_NULLPTR(hashMap);
        hashMap->Init(mMemorySegment, valueNode + VALUE_DATA_OFFSET, false);
        return hashMap;
    }

    return nullptr;
}

std::deque<Value> BoostHashMap::GetList(const Key &key)
{
    uint32_t indexNode = FindIndexNode(key);
    if (indexNode == 0) {
        return {};
    }
    if (UNLIKELY(indexNode > UINT32_MAX - VALUE_NODE_OFFSET)) {
        LOG_ERROR("IndexNode is invalid, priKey hashcode: " << key.KeyHashCode() << ".");
        return {};
    }
    uint32_t valueNode = mMemorySegment->ToUint32_t(indexNode + VALUE_NODE_OFFSET);
    FreshValueNode *freshValueNode = FreshValueNode::FromBuffer(mMemorySegment->Data() + valueNode);

    std::deque<Value> result;
    while (freshValueNode->NodeType() == NodeType::COMPOSITE) {
        Value value;
        value.Init(static_cast<uint8_t>(freshValueNode->ValueType()), freshValueNode->ValueDataLen(),
                   freshValueNode->Value(), freshValueNode->ValueSeqId(), mMemorySegment);
        result.emplace_back(value);
        valueNode = mMemorySegment->ToUint32_t(valueNode + VALUE_DATA_OFFSET +
                                               mMemorySegment->ToUint32_t(valueNode + VALUE_LENGTH_OFFSET));
        freshValueNode = FreshValueNode::FromBuffer(mMemorySegment->Data() + valueNode);
    }
    if (freshValueNode->NodeType() == NodeType::SERIALIZED) {
        Value value;
        value.Init(static_cast<uint8_t>(freshValueNode->ValueType()), freshValueNode->ValueDataLen(),
                   freshValueNode->Value(), freshValueNode->ValueSeqId(), mMemorySegment);
        result.emplace_back(value);
    }
    return result;
}

bool BoostHashMap::Get(const Key &key, Value &value)
{
    const FreshValueNode *valueNode = FindValueNode(mMemorySegment->GetSegment(), key.PriKey());
    if (valueNode == nullptr) {
        return false;
    }

    if (valueNode->ValueType() != DELETE) {
        value.Init(valueNode->ValueType(), valueNode->ValueDataLen(), valueNode->Value(), valueNode->ValueSeqId(),
                   mMemorySegment);
    }
    return true;
}

bool BoostHashMap::GetKMap(const Key &key, Value &value)
{
    uint8_t *segment = mMemorySegment->GetSegment();

    BoostHashMap hashMap;
    bool found = GetByPriKey(segment, key.PriKey(), hashMap);
    if (!found) {
        return false;
    }

    return hashMap.GetBySecKey(segment, key.SecKey(), value);
}

BResult BoostHashMap::Append(const Key &key, const Value &value)
{
    BResult result;
    uint32_t indexNode = FindIndexNode(key);
    if (indexNode != 0) {
        RETURN_NOT_OK_AS_FALSE(indexNode > UINT32_MAX - LIST_TOTAL_VALUE_LEN_OFFSET, BSS_ERR);
        uint32_t listTotalValueLen = mMemorySegment->ToUint32_t(indexNode + LIST_TOTAL_VALUE_LEN_OFFSET);
        if (listTotalValueLen > IO_SIZE_4M) {
            return BSS_FRESH_TABLE_IS_FULL;
        }
        mMemorySegment->PutUint32_t(indexNode + LIST_TOTAL_VALUE_LEN_OFFSET, listTotalValueLen + value.ValueLen());
        uint32_t valueNode = mMemorySegment->ToUint32_t(indexNode + VALUE_NODE_OFFSET);
        uint32_t appendValueNode;
        result = CreateValueNode(value, NodeType::COMPOSITE, appendValueNode);
        RETURN_NOT_OK_NO_LOG(result);
        RETURN_NOT_OK_AS_FALSE(VALUE_DATA_OFFSET + GetValueLength(value) > UINT32_MAX - appendValueNode, BSS_ERR);
        mMemorySegment->PutUint32_t(appendValueNode + VALUE_DATA_OFFSET + GetValueLength(value), valueNode);
        mMemorySegment->PutUint32_t(indexNode + VALUE_NODE_OFFSET, appendValueNode);
    } else {
        result = AddIndexNode(key, value);
    }
    return result;
}

BResult BoostHashMap::Put(const Key &key, const Value &value, bool subKeyFlag)
{
    BResult result = BSS_OK;
    uint32_t indexNode = FindIndexNode(key, subKeyFlag);
    if (indexNode != 0) {
        // update value node.
        RETURN_NOT_OK_AS_FALSE(indexNode > UINT32_MAX - VALUE_NODE_OFFSET, BSS_ERR);
        auto valueNode = mMemorySegment->ToUint32_t(indexNode + VALUE_NODE_OFFSET);
        if (mMemorySegment->ToUint32_t(valueNode + VALUE_LENGTH_OFFSET) >= BoostHashMap::GetValueLength(value)) {
            result = InplaceUpdateValue(valueNode, value);
        } else {
            result = CreateValueNode(value, NodeType::SERIALIZED, valueNode);
            RETURN_NOT_OK_NO_LOG(result);
        }
        if (UNLIKELY(result != BSS_OK)) {
            LOG_ERROR("Put value node failed, ret:" << result << ".");
            return result;
        }
        mMemorySegment->PutUint32_t(indexNode + VALUE_NODE_OFFSET, valueNode);
    } else {
        // add index node.
        result = AddIndexNode(key, value, subKeyFlag);
    }
    return result;
}

BResult BoostHashMap::Put(const KeyValue &keyValue)
{
    uint32_t firstKeyIndexNode = FindIndexNode(keyValue.key);
    uint32_t mapValueNode = 0;
    BResult result;
    bool mapValueNodeExist = false;
    BoostHashMapRef map;
    if (firstKeyIndexNode != NO_0) {
        RETURN_NOT_OK_AS_FALSE(firstKeyIndexNode > UINT32_MAX - VALUE_NODE_OFFSET, BSS_ERR);
        mapValueNode = mMemorySegment->ToUint32_t(firstKeyIndexNode + VALUE_NODE_OFFSET);
        BoostHashMapRef firstLevelValue = ParseValueNode(mapValueNode);
        if (UNLIKELY(firstLevelValue == nullptr)) {
            LOG_ERROR("firstLevelValue is nullptr");
            return BSS_ERR;
        }
        if (firstLevelValue->GetType() == NodeType::HASHMAP) {
            map = StaticPointerCast<BoostHashMap>(firstLevelValue);
            if (UNLIKELY(map == nullptr)) {
                LOG_ERROR("map is nullptr");
                return BSS_ERR;
            }
            mapValueNodeExist = true;
        }
    }
    if (!mapValueNodeExist) {
        result = CreateMapValueNode(mapValueNode);
        RETURN_NOT_OK_NO_LOG(result);
        map = StaticPointerCast<BoostHashMap>(ParseValueNode(mapValueNode));
        result = map->InitStructure();
        RETURN_NOT_OK_NO_LOG(result);
    }
    result = map->Put(keyValue.key, keyValue.value, true);
    RETURN_NOT_OK_NO_LOG(result);
    if (!mapValueNodeExist) {
        result = AddIndexNodeWithValueNode(keyValue.key, mapValueNode);
        RETURN_NOT_OK_NO_LOG(result);
    }

    return result;
}

BResult BoostHashMap::AddIndexNode(const Key &key, const Value &value, bool subKeyFlag)
{
    uint32_t size = Size();
    if (static_cast<float>(size + 1) > static_cast<float>(BucketCount()) * mLoadFactor) {
        auto result = ExpandBucket();
        RETURN_NOT_OK_NO_LOG(result);
    }
    uint32_t keyNode;
    uint32_t valueNode;
    uint32_t indexNode;
    auto result = CreateKeyNode(key, keyNode, subKeyFlag);
    RETURN_NOT_OK_NO_LOG(result);
    result = CreateValueNode(value, NodeType::SERIALIZED, valueNode);
    RETURN_NOT_OK_NO_LOG(result);
    uint32_t hashCode = subKeyFlag ? key.SecKey().HashCode() : key.PriKey().HashCode();
    bool isList = StateId::IsList(key.StateId());
    result = CreateIndexNode(hashCode, indexNode, isList, value.ValueLen());
    RETURN_NOT_OK_NO_LOG(result);

    RETURN_NOT_OK_AS_FALSE(indexNode > UINT32_MAX - VALUE_NODE_OFFSET, BSS_ERR);
    mMemorySegment->PutUint32_t(indexNode + KEY_NODE_OFFSET, keyNode);
    mMemorySegment->PutUint32_t(indexNode + VALUE_NODE_OFFSET, valueNode);
    AddSize();
    return BSS_OK;
}

BResult BoostHashMap::AddIndexNodeWithValueNode(const Key &key, uint32_t valueNode)
{
    uint32_t size = Size();
    BResult result;
    if (static_cast<float>(size + NO_1) > static_cast<float>(BucketCount()) * mLoadFactor) {
        result = ExpandBucket();
        RETURN_NOT_OK_NO_LOG(result);
    }
    uint32_t keyNode;
    uint32_t indexNode;
    result = CreateKeyNode(key, keyNode);
    RETURN_NOT_OK_NO_LOG(result);
    result = CreateIndexNode(key.PriKey().HashCode(), indexNode);
    RETURN_NOT_OK_NO_LOG(result);
    RETURN_NOT_OK_AS_FALSE(indexNode > UINT32_MAX - VALUE_NODE_OFFSET, BSS_ERR);
    mMemorySegment->PutUint32_t(indexNode + KEY_NODE_OFFSET, keyNode);
    mMemorySegment->PutUint32_t(indexNode + VALUE_NODE_OFFSET, valueNode);
    AddSize();
    return BSS_OK;
}

BResult BoostHashMap::CreateIndexNode(uint32_t hashCode, uint32_t &indexNode, bool isList, uint32_t valueLen)
{
    uint32_t bucketAddress = FindBucketAddress(hashCode & (BucketCount() - 1));
    BResult result = mMemorySegment->Allocate(isList ? LIST_INDEX_NODE_SIZE : INDEX_NODE_SIZE, indexNode);
    RETURN_NOT_OK_NO_LOG(result);
    mMemorySegment->PutUint32_t(indexNode + HASHCODE_OFFSET, hashCode);
    PutAtBucket(bucketAddress, indexNode);
    if (isList) {
        RETURN_NOT_OK_AS_FALSE(indexNode > UINT32_MAX - LIST_TOTAL_VALUE_LEN_OFFSET, BSS_ERR);
        mMemorySegment->PutUint32_t(indexNode + LIST_TOTAL_VALUE_LEN_OFFSET, valueLen);
    }
    return BSS_OK;
}

BResult BoostHashMap::CreateKeyNode(const Key &key, uint32_t &keyNode, bool subKeyFlag)
{
    uint32_t length = subKeyFlag ? BoostHashMap::GetSecKeyLength(key) : BoostHashMap::GetPriKeyLength(key);
    if (UNLIKELY(UINT32_MAX - length < KEY_DATA_OFFSET)) {
        LOG_WARN("Key is too long :" << length);
        return BSS_INVALID_PARAM;
    }
    uint32_t size = KEY_DATA_OFFSET + length;
    BResult result = mMemorySegment->Allocate(size, keyNode);
    RETURN_NOT_OK_NO_LOG(result);
    RETURN_NOT_OK_AS_FALSE(keyNode > UINT32_MAX - KEY_LENGTH_OFFSET, BSS_ERR);
    mMemorySegment->PutUint32_t(keyNode + KEY_LENGTH_OFFSET, length);
    uint32_t destOffset = keyNode + KEY_DATA_OFFSET;
    return subKeyFlag ? WriteSecKey(key.SecKey(), destOffset) : WritePriKey(key.PriKey(), destOffset);
}

BResult BoostHashMap::CreateValueNode(const Value &value, NodeType type, uint32_t &valueNode)
{
    uint32_t allocateValueNodeSize = 0;
    uint32_t size = BoostHashMap::GetValueLength(value);
    if (UNLIKELY(UINT32_MAX - VALUE_DATA_OFFSET - NODE_POINTER_SIZE < size)) {
        LOG_WARN("Value is too long :" << size);
        return BSS_INVALID_PARAM;
    }
    if (type == NodeType::SERIALIZED) {
        allocateValueNodeSize = VALUE_DATA_OFFSET + size;
    }
    if (type == NodeType::COMPOSITE) {
        allocateValueNodeSize = VALUE_DATA_OFFSET + size + NODE_POINTER_SIZE;
    }
    if (type == NodeType::HASHMAP) {
        allocateValueNodeSize = VALUE_DATA_OFFSET + HASH_MAP_NODE_SIZE;
    }
    BResult result = mMemorySegment->Allocate(allocateValueNodeSize, valueNode);
    RETURN_NOT_OK_NO_LOG(result);
    mMemorySegment->PutUint32_t(valueNode + VALUE_LENGTH_OFFSET, size);
    mMemorySegment->PutUint8_t(valueNode + VALUE_TYPE_OFFSET, static_cast<uint8_t>(type));
    return WriteValue(value, valueNode + VALUE_DATA_OFFSET);
}

BResult BoostHashMap::CreateMapValueNode(uint32_t &valueNode)
{
    uint32_t allocateValueNodeSize = VALUE_DATA_OFFSET + HASH_MAP_NODE_SIZE;
    BResult result = mMemorySegment->Allocate(allocateValueNodeSize, valueNode);
    RETURN_NOT_OK_NO_LOG(result);
    RETURN_NOT_OK_AS_FALSE(valueNode > UINT32_MAX - VALUE_TYPE_OFFSET, BSS_ERR);
    mMemorySegment->PutUint8_t(valueNode + VALUE_TYPE_OFFSET, static_cast<uint8_t>(NodeType::HASHMAP));
    mMemorySegment->PutUint32_t(valueNode + VALUE_LENGTH_OFFSET, HASH_MAP_NODE_SIZE);
    return BSS_OK;
}

BResult BoostHashMap::InplaceUpdateValue(uint32_t valueNode, const Value &value)
{
    RETURN_NOT_OK_AS_FALSE(valueNode > UINT32_MAX - VALUE_DATA_OFFSET, BSS_ERR);
    mMemorySegment->PutUint32_t(valueNode + VALUE_LENGTH_OFFSET, BoostHashMap::GetValueLength(value));
    mMemorySegment->PutUint8_t(valueNode + VALUE_TYPE_OFFSET, static_cast<uint8_t>(NodeType::SERIALIZED));
    return WriteValue(value, valueNode + VALUE_DATA_OFFSET);
}

BResult BoostHashMap::WriteValue(const Value &value, uint32_t destOffset)
{
    mMemorySegment->PutUint64_t(destOffset, value.SeqId());
    destOffset += sizeof(uint64_t);
    mMemorySegment->PutUint8_t(destOffset, value.ValueType());
    destOffset += sizeof(uint8_t);
    if (value.ValueType() != DELETE) {
        RETURN_NOT_OK(mMemorySegment->CopyFrom(const_cast<uint8_t *>(value.ValueData()), value.ValueLen(), destOffset));
    }
    return BSS_OK;
}

BResult BoostHashMap::WritePriKey(const PriKeyNode &key, uint32_t destOffset)
{
    mMemorySegment->PutUint32_t(destOffset, key.HashCode());
    destOffset += sizeof(uint32_t);
    mMemorySegment->PutUint16_t(destOffset, key.StateId());
    destOffset += sizeof(uint16_t);
    RETURN_NOT_OK(mMemorySegment->CopyFrom(const_cast<uint8_t *>(key.KeyData()), key.KeyLen(), destOffset));
    return BSS_OK;
}

BResult BoostHashMap::WriteSecKey(const SecKeyNode &key, uint32_t destOffset)
{
    mMemorySegment->PutUint32_t(destOffset, key.HashCode());
    destOffset += sizeof(uint32_t);
    RETURN_NOT_OK(mMemorySegment->CopyFrom(const_cast<uint8_t *>(key.KeyData()), key.KeyLen(), destOffset));
    return BSS_OK;
}

IteratorRef<std::pair<FreshKeyNodePtr, FreshValueNodePtr>> BoostHashMap::KVIterator()
{
    auto iterator = MakeRef<BoostHashMapIterator>();
    auto ret = iterator->Init(StaticPointerCast<BoostHashMap>(RefFromThis()));
    if (UNLIKELY(ret != BSS_OK)) {
        return nullptr;
    }
    return StaticPointerCast<Iterator<pair<FreshKeyNodePtr, FreshValueNodePtr>>>(iterator);
}

KeyValueIteratorRef BoostHashMap::MapEntryIterator(const Key &key)
{
    uint8_t *segment = mMemorySegment->GetSegment();
    BoostHashMap hashMap;
    bool found = GetByPriKey(segment, key.PriKey(), hashMap);
    if (!found) {
        return nullptr;
    }
    std::vector<KeyValueRef> keyValues;
    VisitNested(hashMap, key.PriKey(), nullptr, keyValues);
    return std::make_shared<KeyValueVectorIterator>(std::move(keyValues));
}

void BoostHashMap::Visit(std::function<bool(const FreshNode &)> handle) const
{
    if (UNLIKELY(handle == nullptr)) {
        LOG_ERROR("handle is nullptr");
        return;
    }
    uint32_t bucketCount = BucketCount();
    for (uint32_t currentBucket = 0; currentBucket < bucketCount; ++currentBucket) {
        uint32_t bucketAddress = FindBucketAddress(currentBucket);
        uint32_t nextIndexNode = mMemorySegment->ToUint32_t(bucketAddress);

        while (nextIndexNode != 0) {
            FreshNode freshNode(mMemorySegment->GetSegment(), nextIndexNode);
            if (!handle(freshNode)) {
                break;
            }
            if (UNLIKELY(nextIndexNode > UINT32_MAX - NEXT_INDEX_NODE_OFFSET)) {
                LOG_ERROR("Buffer overflow is greater than UINT32_MAX.");
                return;
            }
            nextIndexNode = mMemorySegment->ToUint32_t(nextIndexNode + NEXT_INDEX_NODE_OFFSET);
        }
    }
}

KeyValueRef BoostHashMap::CreateKeyValue(const PriKeyNode &priKey)
{
    auto keyValue = MakeRef<KeyValue>();
    keyValue->key.Init(priKey, mMemorySegment);
    keyValue->value = {};
    return keyValue;
}

KeyValueRef BoostHashMap::CreateKeyValue(const PriKeyNode &priKey, const FreshValueNode *valueNode)
{
    auto keyValue = MakeRef<KeyValue>();
    keyValue->key.Init(priKey, mMemorySegment);
    if (UNLIKELY(valueNode == nullptr)) {
        LOG_ERROR("valueNode is nullptr");
        return keyValue;
    }
    keyValue->value.Init(valueNode->ValueType(), valueNode->ValueDataLen(), valueNode->Value(), valueNode->ValueSeqId(),
                         mMemorySegment);
    return keyValue;
}

KeyValueRef BoostHashMap::CreateKeyValue(const PriKeyNode &priKey, const SecKeyNode &secKey,
                                         const FreshValueNode *valueNode)
{
    auto keyValue = MakeRef<KeyValue>();
    keyValue->key.Init(priKey, secKey, mMemorySegment);
    if (UNLIKELY(valueNode == nullptr)) {
        LOG_ERROR("valueNode is nullptr");
        return keyValue;
    }
    keyValue->value.Init(valueNode->ValueType(), valueNode->ValueDataLen(), valueNode->Value(), valueNode->ValueSeqId(),
                         mMemorySegment);
    return keyValue;
}

void BoostHashMap::VisitNested(const BoostHashMap &hashMap, const PriKeyNode &priKey, const SecKeyFilter &secKeyFilter,
                               std::vector<KeyValueRef> &keyValues)
{
    hashMap.Visit([&](const FreshNode &secondFreshNode) {
        auto secondKeyNode = secondFreshNode.KeyNode();
        if (UNLIKELY(secondKeyNode == nullptr)) {
            return false;
        }
        SecKeyNode secKey;
        secondKeyNode->ToSecKeyNode(secKey);
        if (secKeyFilter != nullptr && secKeyFilter(secKey)) {
            return true;
        }
        auto valueNode = secondFreshNode.ValueNode();
        auto keyValue = CreateKeyValue(priKey, secKey, valueNode);
        keyValues.emplace_back(keyValue);
        return true;
    });
}

KeyValueIteratorRef BoostHashMap::EntryIterator(const PriKeyFilter &priKeyFilter, const SecKeyFilter &secKeyFilter)
{
    std::vector<KeyValueRef> keyValues;
    Visit([&](const FreshNode &firstFreshNode) {
        auto firstKeyNode = firstFreshNode.KeyNode();
        PriKeyNode priKey;
        firstKeyNode->ToPriKeyNode(priKey);
        if (priKeyFilter != nullptr && priKeyFilter(priKey)) {
            // continue visit.
            return true;
        }
        NodeType binaryType = firstFreshNode.ValueNode()->NodeType();
        auto firstValueNode = firstFreshNode.ValueNode();
        switch (binaryType) {
            // first level is serialized key, it is VALUE table.
            case NodeType::SERIALIZED: {
                auto keyValue = CreateKeyValue(priKey, firstValueNode);
                keyValues.emplace_back(keyValue);

                // continue visit.
                return true;
            }
            // first level is composite, it is LIST or SUB_LIST table.
            case NodeType::COMPOSITE: {
                // NOTICE: this type don't support EntryIterator, just put key here.
                auto keyValue = CreateKeyValue(priKey);
                keyValues.emplace_back(keyValue);

                // continue visit.
                return true;
            }
            // first level is hash map, visit it.
            case NodeType::HASHMAP: {
                // it is SUB_VALUE, MAP or SUB_MAP table.
                uint32_t offset = mMemorySegment->Offset(firstValueNode->ValueDataBase());
                if (offset == UINT32_MAX) {
                    return false;
                }
                BoostHashMap hashMap(mMemorySegment, offset, false);
                VisitNested(hashMap, priKey, secKeyFilter, keyValues);

                // continue visit.
                return true;
            }
            default: {
                LOG_ERROR("error boost type." << static_cast<uint8_t>(binaryType));
                // break visit.
                return false;
            }
        }
    });
    return std::make_shared<KeyValueVectorIterator>(std::move(keyValues));
}

pair<FreshKeyNodePtr, FreshValueNodePtr> BoostHashMap::GetEntryFromPointer(uint32_t indexNodePointer)
{
    FreshKeyNodePtr key = nullptr;
    FreshValueNodePtr value = nullptr;
    if (UNLIKELY(indexNodePointer > UINT32_MAX - VALUE_NODE_OFFSET)) {
        LOG_ERROR("Buffer overflow is greater than UINT32_MAX.");
        return make_pair(key, value);
    }
    uint32_t keyNode = mMemorySegment->ToUint32_t(indexNodePointer + KEY_NODE_OFFSET);
    key = FreshKeyNode::FromBuffer(mMemorySegment->GetSegment() + keyNode);
    auto valueNode = mMemorySegment->ToUint32_t(indexNodePointer + VALUE_NODE_OFFSET);
    value = FreshValueNode::FromBuffer(mMemorySegment->GetSegment() + valueNode);
    if (UNLIKELY(key == nullptr)) {
        LOG_ERROR("key is nullptr");
        return make_pair(key, value);
    }
    if (UNLIKELY(value == nullptr)) {
        LOG_ERROR("value is nullptr");
        return make_pair(key, value);
    }
    switch (value->NodeType()) {
        case NodeType::SERIALIZED:
        case NodeType::COMPOSITE:
        case NodeType::HASHMAP:
            break;
        default:
            LOG_ERROR("Unknown value's node type: " << static_cast<uint8_t>(value->NodeType()));
            value = nullptr;
    }
    return make_pair(key, value);
}

}  // namespace bss
}  // namespace ock