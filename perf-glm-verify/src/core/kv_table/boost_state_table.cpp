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

#include "include/boost_state_table.h"

#include "common/util/seq_generator.h"
#include "fresh_table/fresh_table.h"
#include "include/table_description.h"
#include "kv_table_iterator.h"
#include "slice_table/slice_table.h"

namespace ock {
namespace bss {
TableStateIdHelper::TableStateIdHelper(const TableDescriptionRef &tableDesc, const StateIdProviderRef &stateIdProvider)
{
    mTableDesc = tableDesc;
    mStartKeyGroup = tableDesc->GetStartGroup();
    mEndKeyGroup = tableDesc->GetEndGroup();

    int32_t groupNum = static_cast<int32_t>(mEndKeyGroup - mStartKeyGroup) + 1;
    mStateIds = std::vector<uint16_t>(groupNum);
    for (int32_t i = 0; i < groupNum; i++) {
        mStateIds[i] = stateIdProvider->GetStateId(tableDesc);
    }
}

uint16_t TableStateIdHelper::GetStateId(uint32_t keyGroupIndex)
{
    if (UNLIKELY(keyGroupIndex - mStartKeyGroup >= mStateIds.size())) {
        LOG_ERROR("wrong keyGroupIndex");
        return 0;
    }
    return mStateIds[keyGroupIndex - mStartKeyGroup];
}

BResult AbstractTable::Init(const FreshTableRef &freshTable, const SliceTableManagerRef &sliceTable,
                            const StateIdProviderRef &stateIdProvider, TableDescriptionRef &description,
                            SeqGeneratorRef seqGenerator, int64_t tableTtl, StateFilterManagerRef &stateFilterManager)
{
    mFreshTable = freshTable;
    mDescription = description;
    mStateIdProvider = stateIdProvider;
    mSliceTable = sliceTable;
    mSeqGenerator = seqGenerator;
    mMaxParallelism = description->GetMaxParallelism();
    mKeyGroupUtil = stateFilterManager->GetKeyGroupUtil();
    if (stateIdProvider == nullptr) {
        return BSS_ERR;
    }
    mStateIdHelper = std::make_shared<TableStateIdHelper>(description, stateIdProvider);
    if (tableTtl > 0) {
        mStateFilter = std::make_shared<TtlStateFilter>(tableTtl);
        stateFilterManager->RegisterStateFilter(description, mStateFilter);
    } else {
        mStateFilter = std::make_shared<NoStateFilter>();
    }
    return BSS_OK;
}

uint16_t AbstractTable::GetStateId(uint32_t &keyHashCode)
{
    uint32_t keyGroupIndex = keyHashCode % mMaxParallelism;
    // 先计算stateId，再修改hash，避免修改hash影响计算stateId
    mKeyGroupUtil->SetKeyGroup(keyHashCode, keyGroupIndex);
    return mStateIdHelper->GetStateId(keyGroupIndex);
}

SequenceIdFilterRef &AbstractTable::UpdateTtl(int64_t tableTtl)
{
    mStateFilter = std::make_shared<TtlStateFilter>(tableTtl);
    return mStateFilter;
}

// ignore namespace default.
KeyIterator *AbstractTable::KeysIterator(const BinaryData &ignore)
{
    // get all entry iterator that belong to this table from fresh table.
    uint16_t stateId = GetStateId();
    auto priKeyFilter = [stateId](const PriKeyNode &priKey) -> bool { return stateId != priKey.StateId(); };
    auto freshTableIterator = mFreshTable->EntryIterator(priKeyFilter);

    // get all entry iterator from slice table.
    auto keyFilter = [stateId](const Key &key) -> bool { return stateId != key.PriKey().StateId(); };
    auto sliceTableIterator = mSliceTable->EntryIterator(keyFilter, stateId);

    // create entry iterator by fresh table iterator and slice table iterator.
    return new (std::nothrow) KeyIterator{ freshTableIterator, sliceTableIterator, EntryIteratorType::KVALUE_ITERATOR };
}

void AbstractTable::Close()
{
    return;
}

BResult KVTable::Put(uint32_t keyHashCode, const BinaryData &priKey, const BinaryData &value)
{
    uint16_t stateId = GetStateId(keyHashCode);
    QueryKey queryKey(stateId, keyHashCode, priKey);
    Value putVal;
    putVal.Init(ValueType::PUT, value.Length(), value.Data(), mSeqGenerator->Next());
    BResult result = mFreshTable->Put(queryKey, putVal);
    return result;
}

BResult KVTable::Get(uint32_t hashCode, const BinaryData &key, BinaryData &value)
{
    uint16_t stateId = GetStateId(hashCode);
    QueryKey queryKey(stateId, hashCode, key);
    Value result;

    // 1. 从FreshTable中读取value.
    auto found = mFreshTable->Get(queryKey, result);
    if (found) {
        mFreshTable->IncrementFreshHit();
        value.Init(result.ValueData(), result.ValueLen(), result.Buffer());
        return BSS_OK;
    }
    mFreshTable->IncrementFreshMiss();

    // 2. get from slice table or get from lsm store.
    found = mSliceTable->Get(queryKey, result);
    if (found) {
        value.Init(result.ValueData(), result.ValueLen(), result.Buffer());
        return BSS_OK;
    }

    return BSS_NOT_EXISTS;
}

bool KVTable::Contain(uint32_t hashCode, const BinaryData &key)
{
    BinaryData value = {};
    auto retVal = Get(hashCode, key, value);
    if (retVal == BSS_OK && !value.IsNull()) {
        return true;
    }
    return false;
}

BResult KVTable::Remove(uint32_t hashCode, const BinaryData &priKey)
{
    uint16_t stateId = GetStateId(hashCode);
    QueryKey queryKey(stateId, hashCode, priKey);
    Value putVal;
    putVal.Init(ValueType::DELETE, mSeqGenerator->Next());
    return mFreshTable->Put(queryKey, putVal);
}

MapIterator *AbstractTable::FullEntryIterator()
{
    // select the keys that belong to this table
    uint16_t stateId = GetStateId();
    // List类型的fresh不能使用entry iterator,先强刷到slice
    if (StateId::IsList(stateId)) {
        auto ret = mFreshTable->ForceFlushToSlice();
        if (UNLIKELY(ret != BSS_OK)) {
            LOG_ERROR("Force flush segment to slice failed, ret: " << ret);
            return nullptr;
        }
    }
    auto priKeyFilter = [stateId](const PriKeyNode &priKey) -> bool { return stateId != priKey.StateId(); };

    // get all entry iterator from fresh table.
    auto freshTableIterator = mFreshTable->EntryIterator(priKeyFilter);

    // get all entry iterator from slice table.
    auto keyFilter = [stateId](const Key &key) -> bool { return stateId != key.PriKey().StateId(); };
    BlobValueTransformFunc blobValueTransformFunc;
    auto sliceTableIterator = mSliceTable->EntryIterator(keyFilter, stateId, blobValueTransformFunc);

    // create entry iterator by fresh table iterator and slice table iterator.
    return new (std::nothrow) MapIterator{ freshTableIterator, sliceTableIterator, EntryIteratorType::KVALUE_ITERATOR,
                                           blobValueTransformFunc };
}

uint16_t AbstractTable::GetStateId()
{
    return mStateIdProvider->GetStateId(mDescription);
}

BResult AbstractKMapTable::Put(uint32_t keyHashCode, const BinaryData &priKey, const BinaryData &secKey,
                               const BinaryData &value)
{
    KeyValue keyValue;
    uint16_t stateId = GetStateId(keyHashCode);
    QueryKey queryKey(stateId, keyHashCode, priKey, secKey);
    keyValue.key = queryKey;
    Value putVal;
    putVal.Init(ValueType::PUT, value.Length(), value.Data(), mSeqGenerator->Next());
    keyValue.value = putVal;
    auto ret = mFreshTable->Add(keyValue);
    PeakFilterAdd(priKey, secKey);
    return ret;
}

BResult AbstractKMapTable::Get(uint32_t keyHashCode, const BinaryData &priKey, const BinaryData &secKey,
                               BinaryData &value)
{
    if (!KeyMayMatchSecondPeakFilter(secKey)) {
        return BSS_NOT_EXISTS;
    }

    // trans to dual key.
    uint16_t stateId = GetStateId(keyHashCode);
    QueryKey queryKey(stateId, keyHashCode, priKey, secKey);
    Value result;

    // get from fresh table.
    auto found = mFreshTable->GetKMap(queryKey, result);
    if (LIKELY(found)) {
        mFreshTable->IncrementFreshHit();
        value.Init(result.ValueData(), result.ValueLen(), result.Buffer());
        return BSS_OK;
    }
    mFreshTable->IncrementFreshMiss();

    // get from slice table or get from lsm store.
    found = mSliceTable->Get(queryKey, result);
    if (LIKELY(found)) {
        value.Init(result.ValueData(), result.ValueLen(), result.Buffer());
        return BSS_OK;
    }

    return BSS_NOT_EXISTS;
}

bool AbstractKMapTable::Contain(uint32_t keyHashCode, const BinaryData &priKey, const BinaryData &secKey)
{
    if (!KeyMayMatchSecondPeakFilter(secKey)) {
        return false;
    }

    BinaryData value = {};
    BResult result = Get(keyHashCode, priKey, secKey, value);
    if (result == BSS_OK && !value.IsNull()) {
        return true;
    }
    return false;
}

bool AbstractKMapTable::Contain(uint32_t keyHashCode, const BinaryData &key)
{
    if (!KeyMayMatchPeakFilter(key)) {
        return false;
    }

    // get map iterator from fresh table.
    uint16_t stateId = GetStateId(keyHashCode);
    QueryKey queryKey(stateId, keyHashCode, key);

    // value is a map, deletedKeyValues is used for storing key and map that already been deleted.
    KeyValueMap deletedKeyValues;
    auto freshTableIterator = mFreshTable->GetMapIterator(queryKey);
    while (freshTableIterator != nullptr && freshTableIterator->HasNext()) {
        auto keyValue = freshTableIterator->Next();
        if (keyValue == nullptr) {
            continue;
        }
        if (keyValue->value.ValueType() != ValueType::DELETE) {
            return true;
        }
        deletedKeyValues.emplace(&keyValue->key, keyValue);
    }
    auto sliceTableIterator = mSliceTable->PrefixIterator(queryKey);
    while (sliceTableIterator != nullptr && sliceTableIterator->HasNext()) {
        auto keyValue = sliceTableIterator->Next();
        if (keyValue != nullptr && keyValue->value.ValueType() != ValueType::DELETE &&
            deletedKeyValues.find(&keyValue->key) == deletedKeyValues.end()) {
            return true;
        }
    }
    return false;
}

BResult AbstractKMapTable::Remove(uint32_t hashCode, const BinaryData &priKey, const BinaryData &secKey)
{
    if (!KeyMayMatchSecondPeakFilter(secKey)) {
        return BSS_OK;
    }
    KeyValue keyValue;
    uint16_t stateId = GetStateId(hashCode);
    QueryKey queryKey(stateId, hashCode, priKey, secKey);
    keyValue.key = queryKey;
    Value putVal;
    putVal.Init(ValueType::DELETE, mSeqGenerator->Next());
    keyValue.value = putVal;
    return mFreshTable->Add(keyValue);
}

BResult AbstractKMapTable::Remove(uint32_t hashCode, const BinaryData &priKey)
{
    if (!KeyMayMatchPeakFilter(priKey)) {
        return BSS_OK;
    }
    MapIterator *mapIterator = EntryIterator(hashCode, priKey);
    uint16_t stateId = GetStateId(hashCode);
    while (mapIterator->HasNext()) {
        KeyValueRef pair = mapIterator->Next();
        BinaryData secKey(pair->key.SecKey().KeyData(), pair->key.SecKey().KeyLen());
        QueryKey queryKey(stateId, hashCode, priKey, secKey);
        Value putVal;
        putVal.Init(ValueType::DELETE, mSeqGenerator->Next());
        KeyValue keyValue;
        keyValue.key = queryKey;
        keyValue.value = putVal;
        mFreshTable->Add(keyValue);
    }
    delete mapIterator;
    return BSS_OK;
}

MapIterator *AbstractKMapTable::EntryIterator(uint32_t hashCode, const BinaryData &priKey)
{
    if (!KeyMayMatchPeakFilter(priKey)) {
        auto emptyIter = std::make_shared<KeyValueIterator>();
        return new (std::nothrow) MapIterator(emptyIter, emptyIter, EntryIteratorType::KMAP_ITERATOR);
    }
    // to query key.
    uint16_t stateId = GetStateId(hashCode);
    QueryKey queryKey(stateId, hashCode, priKey);

    // get map iterator from fresh table.
    auto freshTableIterator = mFreshTable->GetMapIterator(queryKey);

    // get prefix iterator from slice table.
    BlobValueTransformFunc blobValueTransformFunc;
    auto sliceTableIterator = mSliceTable->PrefixIterator(queryKey, blobValueTransformFunc);

    // create map iterator by slice table iterator and fresh table iterator.
    return new (std::nothrow)
        MapIterator(freshTableIterator, sliceTableIterator, EntryIteratorType::KMAP_ITERATOR, blobValueTransformFunc);
}

MapIteratorWrraper *AbstractKMapTable::EntryIteratorWrraper(uint32_t hashCode, const BinaryData &priKey)
{
    return new (std::nothrow) MapIteratorWrraper(EntryIterator(hashCode, priKey));
}

KeyIterator *NsKVTable::KeysIterator(const BinaryData &nameSpace)
{
    uint16_t stateId = GetStateId();
    // select the keys that belong to this table and has the same name space.
    auto priKeyFilter = [stateId](const PriKeyNode &priKey) -> bool { return stateId != priKey.StateId(); };
    // NameSpace is the secondary key in NsKVTable, so we trans it to SecKeyNode.
    SecKeyNode querySecKey(HashCode::Hash(nameSpace.Data(), nameSpace.Length()), nameSpace.Data(), nameSpace.Length());
    auto secKeyFilter = [querySecKey](const SecKeyNode &secKey) {
        // compare name space.
        return secKey.CompareKeyNode(querySecKey) != 0;
    };

    // get iterator from fresh table.
    auto freshTableIterator = mFreshTable->EntryIterator(priKeyFilter, secKeyFilter);

    // get iterator from slice table.
    auto keyFilter = [stateId, querySecKey](const Key &key) -> bool {
        return stateId != key.PriKey().StateId() || key.SecKey().CompareKeyNode(querySecKey) != 0;
    };
    auto sliceTableIterator = mSliceTable->EntryIterator(keyFilter, stateId);

    // create key iterator by slice table iterator and fresh table iterator.
    return new (std::nothrow) KeyIterator{ freshTableIterator, sliceTableIterator, EntryIteratorType::KVALUE_ITERATOR };
}

KeyIterator *NsKMapTable::KeysIterator(const BinaryData &nameSpace)
{
    uint16_t stateId = GetStateId();
    // select the keys that belong to this table and has the same name space.
    auto priKeyFilter = [stateId, &nameSpace](const PriKeyNode &priKey) {
        return stateId != priKey.StateId() ||
               // NameSpace is in PrimaryKey when it is KMapTable type.
               !priKey.HasSameNameSpace(nameSpace.Data(), nameSpace.Length());
    };
    // get iterator from fresh table.
    auto freshTableIterator = mFreshTable->EntryIterator(priKeyFilter);

    // get iterator from slice table.
    auto keyFilter = [stateId, nameSpace](const Key &key) -> bool {
        return stateId != key.PriKey().StateId() ||
               !key.PriKey().HasSameNameSpace(nameSpace.Data(), nameSpace.Length());
    };
    auto sliceTableIterator = mSliceTable->EntryIterator(keyFilter, stateId);

    // create key iterator by slice table iterator and fresh table iterator.
    return new (std::nothrow)
        KeyIterator{ freshTableIterator, sliceTableIterator, EntryIteratorType::KSUBMAP_ITERATOR };
}

BResult AbstractKListTable::Put(uint32_t hashCode, const BinaryData &key, const BinaryData &value)
{
    uint16_t stateId = GetStateId(hashCode);
    QueryKey queryKey(stateId, hashCode, key);
    Value putVal;
    putVal.Init(ValueType::PUT, value.Length(), value.Data(), mSeqGenerator->Next());
    return mFreshTable->Put(queryKey, putVal);
}

BResult AbstractKListTable::Add(uint32_t hashCode, const BinaryData &key, const BinaryData &value)
{
    uint16_t stateId = GetStateId(hashCode);
    QueryKey queryKey(stateId, hashCode, key);
    Value addVal;
    addVal.Init(ValueType::APPEND, value.Length(), value.Data(), mSeqGenerator->Next());
    return mFreshTable->Append(queryKey, addVal);
}

ListResult AbstractKListTable::Get(uint32_t hashCode, const BinaryData &key)
{
    // to query key.
    uint16_t stateId = GetStateId(hashCode);
    QueryKey queryKey(stateId, hashCode, key);
    // 1. 从FreshTable中List value.
    std::deque<Value> valueInFreshTable;
    std::vector<SectionsReadContextRef> readMetas;
    bool shouldContinue = mFreshTable->GetList(queryKey, valueInFreshTable);
    if (!shouldContinue) {
        mFreshTable->IncrementFreshHit();
        return GetListCount(valueInFreshTable, readMetas);
    }
    mFreshTable->IncrementFreshListHitMiss(valueInFreshTable);

    // 2. 从SliceTable中读取List value.
    std::deque<Value> valueInSliceTable;
    mSliceTable->GetList(queryKey, valueInSliceTable, readMetas);
    if (valueInSliceTable.empty()) {
        return GetListCount(valueInFreshTable, readMetas);
    }

    // 3. 汇总结果.
    return GetListCount(valueInFreshTable, valueInSliceTable, readMetas);
}

ListResult AbstractKListTable::GetListCount(const std::deque<Value> &source,
                                            std::vector<SectionsReadContextRef> &readMetas)
{
    ListResult listCount;
    std::vector<Value> result;
    for (const auto &value : source) {
        result.emplace_back(value);
        listCount.addresses.emplace_back(reinterpret_cast<long>(value.ValueData()));
        listCount.lengths.emplace_back(value.ValueLen());
    }

    listCount.resId = (mIndex == INT32_MAX) ? NO_1 : ++mIndex;
    listCount.size = result.size();
    if (!result.empty()) {
        mResourceMap[listCount.resId] = std::move(result);
    }
    if (readMetas.size() > 0) {
        listCount.sectionReadId = (mReadSectionIndex == INT32_MAX) ? NO_1 : ++mReadSectionIndex;
        mReadSectionMap[listCount.sectionReadId] = std::move(readMetas);
    }
    return listCount;
}

ListResult AbstractKListTable::GetListCount(const std::deque<Value> &source, std::deque<Value> &slice,
                                            std::vector<SectionsReadContextRef> &readMetas)
{
    std::vector<Value> result;
    ListResult listCount;
    uint64_t maxSeqIdInSlice = 0;
    for (const auto &value : slice) {
        maxSeqIdInSlice = std::max(maxSeqIdInSlice, value.SeqId());
        result.emplace_back(value);
        listCount.addresses.emplace_back(reinterpret_cast<long>(value.ValueData()));
        listCount.lengths.emplace_back(value.ValueLen());
    }

    for (const auto &value : source) {
        if (maxSeqIdInSlice >= value.SeqId()) {
            continue;
        }
        result.emplace_back(value);
        listCount.addresses.emplace_back(reinterpret_cast<long>(value.ValueData()));
        listCount.lengths.emplace_back(value.ValueLen());
    }

    listCount.resId = (mIndex == INT32_MAX) ? NO_1 : ++mIndex;
    listCount.size = result.size();
    if (!result.empty()) {
        mResourceMap[listCount.resId] = std::move(result);
    }
    if (readMetas.size() > 0) {
        listCount.sectionReadId = (mReadSectionIndex == INT32_MAX) ? NO_1 : ++mReadSectionIndex;
        mReadSectionMap[listCount.sectionReadId] = std::move(readMetas);
    }
    return listCount;
}

bool AbstractKListTable::Contain(uint32_t hashCode, const BinaryData &key)
{
    ListResult tempList = Get(hashCode, key);
    if (tempList.size == 0) {
        return false;
    }

    CleanResource(tempList.resId);
    if (tempList.sectionReadId > 0) {
        auto iter = mReadSectionMap.find(tempList.sectionReadId);
        if (iter != mReadSectionMap.end()) {
            std::vector<SectionsReadContextRef> &readMetas = iter->second;
            for (const auto &item : readMetas) {
                item->filePage->CleanVersion(item->sectionsReadMeta->mCurrent);
            }
        }
        mReadSectionMap.erase(iter);
    }
    return true;
}

BResult AbstractKListTable::Remove(uint32_t hashCode, const BinaryData &key)
{
    uint16_t stateId = GetStateId(hashCode);
    QueryKey queryKey(stateId, hashCode, key);
    Value addVal;
    addVal.Init(ValueType::DELETE, mSeqGenerator->Next());
    return mFreshTable->Put(queryKey, addVal);
}

ListResult AbstractKListTable::SectionRead(int32_t readSectionId)
{
    auto iter = mReadSectionMap.find(readSectionId);
    if (iter == mReadSectionMap.end()) {
        return ListResult();
    }
    ListResult listCount;
    listCount.sectionReadId = readSectionId;
    std::vector<SectionsReadContextRef> &readMetas = iter->second;
    auto first = readMetas.begin();
    std::vector<FileMetaDataRef> &fileMetas = (*first)->sectionsReadMeta->mFilesForKey;
    Value value;
    auto firstFileMetaRef = fileMetas.begin();
    while (firstFileMetaRef != fileMetas.end()) {
        BResult ret = (*first)->filePage->GetByReadSection(*firstFileMetaRef, (*first)->sectionsReadMeta->key, value);
        firstFileMetaRef = fileMetas.erase(firstFileMetaRef);
        if (ret == BSS_OK) {
            break;
        }
    }
    if (firstFileMetaRef == fileMetas.end() ||
        (value.ValueType() == ValueType::DELETE || value.ValueType() == ValueType::PUT)) {
        (*first)->filePage->CleanVersion((*first)->sectionsReadMeta->mCurrent);
        first = readMetas.erase(first);
    }
    if (first == readMetas.end()) {
        mReadSectionMap.erase(readSectionId);
        listCount.sectionReadId = 0;
    }
    if (!value.IsNull()) {
        listCount.size = NO_1;
        listCount.addresses.emplace_back(reinterpret_cast<long>(value.ValueData()));
        listCount.lengths.emplace_back(value.ValueLen());
        listCount.resId = ++mIndex;
        mResourceMap[listCount.resId] = std::vector<Value>{ value };
    } else if (listCount.sectionReadId > 0) {
        return SectionRead(listCount.sectionReadId);
    }
    return listCount;
}

void AbstractKListTable::CleanResource(uint32_t resId)
{
    auto iter = mResourceMap.find(resId);
    if (iter != mResourceMap.end()) {
        std::vector<Value>().swap(iter->second);
        mResourceMap.erase(iter);
    }
}

KeyIterator *NsKListTable::KeysIterator(const BinaryData &nameSpace)
{
    // select the keys that belong to this table and has the same name space.
    uint16_t stateId = GetStateId();
    auto priKeyFilter = [stateId, &nameSpace](const PriKeyNode &priKey) {
        return stateId != priKey.StateId() ||
               // NameSpace is in PrimaryKey when it is KMapTable type.
               !priKey.HasSameNameSpace(nameSpace.Data(), nameSpace.Length());
    };
    // get iterator from fresh table.
    auto freshTableIterator = mFreshTable->EntryIterator(priKeyFilter);

    // get iterator from slice table.
    auto keyFilter = [stateId, nameSpace](const Key &key) -> bool {
        return stateId != key.PriKey().StateId() ||
               !key.PriKey().HasSameNameSpace(nameSpace.Data(), nameSpace.Length());
    };
    auto sliceTableIterator = mSliceTable->EntryIterator(keyFilter, stateId);

    // create key iterator by slice table iterator and fresh table iterator.
    return new (std::nothrow)
        KeyIterator{ freshTableIterator, sliceTableIterator, EntryIteratorType::KSUBLIST_ITERATOR };
}

MapIteratorWrraper::MapIteratorWrraper(MapIterator *mapIterator)
{
    mMapIterator = mapIterator;
}

MapIteratorWrraper::~MapIteratorWrraper()
{
    if (mMapIterator == nullptr) {
        return;
    }
    mMapIterator->Close();
    delete mMapIterator;
    mMapIterator = nullptr;
}

bool MapIteratorWrraper::HasNext()
{
    return mMapIterator != nullptr && mMapIterator->HasNext();
}

std::vector<BinaryData> MapIteratorWrraper::Next()
{
    std::vector<BinaryData> kVList;
    BinaryData key = {};
    BinaryData value = {};
    kVList.reserve(NO_2);
    KeyValueRef keyValue = mMapIterator->Next();
    if (keyValue != nullptr) {
        key.Init(keyValue->key.SecKey().KeyData(), keyValue->key.SecKey().KeyLen());
        value.Init(keyValue->value.ValueData(), keyValue->value.ValueLen());
        kVList.emplace_back(key);
        kVList.emplace_back(value);
    }
    return kVList;
}
}  // namespace bss
}  // namespace ock
