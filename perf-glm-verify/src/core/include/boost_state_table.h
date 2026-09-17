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

#ifndef BOOST_STATE_TABLE_H
#define BOOST_STATE_TABLE_H

#include <deque>
#include <memory>
#include <unordered_map>
#include <vector>

#include "table.h"

namespace ock {
namespace bss {
class FreshTable;
using FreshTableRef = std::shared_ptr<FreshTable>;

class SliceTable;
using SliceTableManagerRef = std::shared_ptr<SliceTable>;

class StateIdProvider;
using StateIdProviderRef = std::shared_ptr<StateIdProvider>;

struct StateId;

class StateFilterManager;
using StateFilterManagerRef = std::shared_ptr<StateFilterManager>;

class SeqGenerator;
using SeqGeneratorRef = std::shared_ptr<SeqGenerator>;

struct SectionsReadContext;
using SectionsReadContextRef = std::shared_ptr<SectionsReadContext>;

class SequenceIdFilter;
using SequenceIdFilterRef = std::shared_ptr<SequenceIdFilter>;

class KeyGroupUtil;
using KeyGroupUtilRef = std::shared_ptr<KeyGroupUtil>;

struct Value;
class MapIterator;
class MapIteratorWrraper;
class KeyIterator;

struct ListResult {
    int32_t resId = 0;
    uint32_t size = 0;
    std::vector<long> addresses;
    std::vector<int32_t> lengths;
    int32_t sectionReadId = 0;
};

class TableStateIdHelper {
public:
    TableStateIdHelper(const TableDescriptionRef &tableDesc, const StateIdProviderRef &stateIdProvider);

    uint16_t GetStateId(uint32_t keyGroupIndex);

private:
    TableDescriptionRef mTableDesc;
    uint32_t mStartKeyGroup;
    uint32_t mEndKeyGroup;
    std::vector<uint16_t> mStateIds;
};

using TableStateIdHelperRef = std::shared_ptr<TableStateIdHelper>;

class AbstractTable : public Table {
public:
    BResult Init(const FreshTableRef &freshTable, const SliceTableManagerRef &sliceTable,
                 const StateIdProviderRef &stateIdProvider, TableDescriptionRef &description,
                 SeqGeneratorRef seqGenerator, int64_t tableTtl, StateFilterManagerRef &stateFilterManager);

    virtual inline uint32_t GetHashCode(uint32_t keyHashCode, const uint8_t *data)
    {
        return keyHashCode;
    }

    uint16_t GetStateId(uint32_t &keyHashCode);

    inline void SetStateIdProvider(const StateIdProviderRef &stateIdProvider)
    {
        mStateIdProvider = stateIdProvider;
        mStateIdHelper = std::make_shared<TableStateIdHelper>(mDescription, stateIdProvider);
    }

    inline TableDescriptionRef GetTableDescription() const override
    {
        return mDescription;
    }

    SequenceIdFilterRef &UpdateTtl(int64_t tableTtl);

    virtual KeyIterator *KeysIterator(const BinaryData &namespaceData);

    void Close() override;

    virtual MapIterator *FullEntryIterator();

protected:
    uint16_t GetStateId();

    void PeakFilterAdd(const BinaryData &key)
    {
    }

    void PeakFilterAdd(const BinaryData &key, const BinaryData &secondKey)
    {
    }

    bool KeyMayMatchPeakFilter(const BinaryData &key)
    {
        return true;
    }

    bool KeyMayMatchSecondPeakFilter(const BinaryData &key)
    {
        return true;
    }

protected:
    FreshTableRef mFreshTable;
    SliceTableManagerRef mSliceTable;
    TableDescriptionRef mDescription;
    StateIdProviderRef mStateIdProvider;
    TableStateIdHelperRef mStateIdHelper;
    SequenceIdFilterRef mStateFilter;
    SeqGeneratorRef mSeqGenerator;
    uint32_t mMaxParallelism = 0;
    KeyGroupUtilRef mKeyGroupUtil = nullptr;
};
using AbstractTableRef = std::shared_ptr<AbstractTable>;

class KVTable : public AbstractTable {
public:
    BResult Put(uint32_t keyHashCode, const BinaryData &priKey, const BinaryData &value) override;
    BResult Get(uint32_t hashCode, const BinaryData &key, BinaryData &value) override;
    bool Contain(uint32_t hashCode, const BinaryData &key) override;
    BResult Remove(uint32_t hashCode, const BinaryData &key) override;
};
using KVTableRef = std::shared_ptr<KVTable>;

class AbstractKMapTable : public AbstractTable {
public:
    BResult Put(uint32_t keyHashCode, const BinaryData &priKey, const BinaryData &secKey,
                const BinaryData &value) override;
    BResult Get(uint32_t keyHashCode, const BinaryData &priKey, const BinaryData &secKey, BinaryData &value) override;
    bool Contain(uint32_t keyHashCode, const BinaryData &priKey, const BinaryData &secKey) override;
    bool Contain(uint32_t keyHashCode, const BinaryData &priKey) override;
    BResult Remove(uint32_t hashCode, const BinaryData &priKey, const BinaryData &secKey) override;
    BResult Remove(uint32_t hashCode, const BinaryData &priKey) override;
    MapIterator *EntryIterator(uint32_t hashCode, const BinaryData &priKey);
    MapIteratorWrraper *EntryIteratorWrraper(uint32_t hashCode, const BinaryData &priKey);
};
using AbstractKMapTableRef = std::shared_ptr<AbstractKMapTable>;

class NsKVTable : public AbstractKMapTable {
public:
    KeyIterator *KeysIterator(const BinaryData &nameSpace) override;
};
using NsKVTableRef = std::shared_ptr<NsKVTable>;

class KMapTable : public AbstractKMapTable {
    // Empty by design.
};
using KMapTableRef = std::shared_ptr<KMapTable>;

class NsKMapTable : public AbstractKMapTable {
public:
    KeyIterator *KeysIterator(const BinaryData &nameSpace) override;

    inline uint32_t GetHashCode(uint32_t keyHashCode, const uint8_t *data) override
    {
        uint32_t nsHashCode = *reinterpret_cast<const uint32_t *>(data);
        return keyHashCode ^ nsHashCode;
    }
};
using NsKMapTableRef = std::shared_ptr<NsKMapTable>;

class AbstractKListTable : public AbstractTable {
public:
    BResult Put(uint32_t hashCode, const BinaryData &key, const BinaryData &value) override;
    BResult Add(uint32_t hashCode, const BinaryData &key, const BinaryData &value) override;
    ListResult Get(uint32_t hashCode, const BinaryData &key);
    bool Contain(uint32_t hashCode, const BinaryData &key) override;
    BResult Remove(uint32_t hashCode, const BinaryData &key) override;
    ListResult SectionRead(int32_t readSectionId);

    void CleanResource(uint32_t resId);

private:
    std::atomic<int32_t> mIndex{ 0 };
    std::unordered_map<int32_t, std::vector<Value>> mResourceMap;
    std::atomic<int32_t> mReadSectionIndex{ 0 };
    std::unordered_map<int32_t, std::vector<SectionsReadContextRef>> mReadSectionMap;
    ListResult GetListCount(const std::deque<Value> &source, std::vector<SectionsReadContextRef> &readMetas);
    ListResult GetListCount(const std::deque<Value> &source, std::deque<Value> &slice,
                            std::vector<SectionsReadContextRef> &readMetas);
};
using AbstractKListTableRef = std::shared_ptr<AbstractKListTable>;

class KListTable : public AbstractKListTable {
    // Empty by design.
};
using KListTableRef = std::shared_ptr<KListTable>;

class NsKListTable : public AbstractKListTable {
public:
    KeyIterator *KeysIterator(const BinaryData &nameSpace) override;

    inline uint32_t GetHashCode(uint32_t keyHashCode, const uint8_t *data) override
    {
        uint32_t nsHashCode = *reinterpret_cast<const uint32_t *>(data);
        return keyHashCode ^ nsHashCode;
    }
};
using NsKListTableRef = std::shared_ptr<NsKListTable>;

class MapIteratorWrraper {
public:
    explicit MapIteratorWrraper(MapIterator *mapIterator);
    ~MapIteratorWrraper();
    bool HasNext();
    std::vector<BinaryData> Next();

private:
    MapIterator *mMapIterator;
};
}  // namespace bss
}  // namespace ock

#endif
