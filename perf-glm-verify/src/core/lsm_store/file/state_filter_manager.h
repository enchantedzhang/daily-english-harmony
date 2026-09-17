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

#ifndef BOOST_SS_STATE_FILTER_MANAGER_H
#define BOOST_SS_STATE_FILTER_MANAGER_H

#include "binary/slice_binary.h"
#include "include/table_description.h"
#include "kv_table/stateId_provider.h"
#include "lsm_store/key/full_key_filter.h"
#include "lsm_store/key/key_group_state_filter.h"
#include "sequence_id_filter.h"
#include "util/key_group_util.h"

namespace ock {
namespace bss {

struct TableDescriptionEqualTTL {
    bool operator()(const TableDescriptionRef a, const TableDescriptionRef b) const
    {
        if (a->GetTableName() != b->GetTableName()) {
            return false;
        }
        if (a->GetStateType() != b->GetStateType()) {
            return false;
        }
        if (a->GetStartGroup() != b->GetStartGroup()) {
            return false;
        }
        if (a->GetEndGroup() != b->GetEndGroup()) {
            return false;
        }
        if (a->GetMaxParallelism() != b->GetMaxParallelism()) {
            return false;
        }
        return true;
    }
};

struct TableDescriptionHashTTL {
    uint32_t operator()(const TableDescriptionRef ref) const
    {
        return ref->GetStartGroup() + ref->GetEndGroup() + ref->GetMaxParallelism();
    }
};

class StateFilterManager : public FullKeyFilter {
public:
    StateFilterManager(const StateIdProviderRef &stateIdProvider, const ConfigRef &conf, int32_t startKeyGroup,
                       int32_t endKeyGroup, const KeyGroupUtilRef &keyGroupUtil)
        : mStateIdProvider(stateIdProvider),
          mConf(conf),
          mKeyGroupUtil(keyGroupUtil),
          mStartKeyGroup(startKeyGroup),
          mEndKeyGroup(endKeyGroup)
    {
        mKeyGroupFilter = std::make_shared<KeyGroupStateFilter>(startKeyGroup, endKeyGroup);
    }

    ~StateFilterManager() override
    {
        std::unordered_map<TableDescriptionRef, SequenceIdFilterRef, TableDescriptionHashTTL, TableDescriptionEqualTTL>()
            .swap(mStateFilterMap);
    }

    inline bool Filter(const SliceKey &sliceKey, uint64_t seqId)
    {
        auto group = mKeyGroupUtil->ComputeKeyGroupForKeyHash(sliceKey.KeyHashCode());
        return Filter(group, sliceKey.StateId(), seqId);
    }

    inline bool Filter(int32_t group, uint16_t stateId, uint64_t seqId)
    {
        if (mKeyGroupFilter->Filter(group)) {
            return true;
        }
        return StateFilter(stateId, seqId);
    }

    inline bool Filter(const KeyValueRef &keyValue) override
    {
        Key &key = keyValue->key;
        auto keyGroup = ComputeKeyGroup(key.PriKey().KeyHashCode(), key.StateId());
        Value &value = keyValue->value;
        return Filter(keyGroup, key.StateId(), value.SeqId());
    }

    inline bool Filter(const Key &key, const Value &value) override
    {
        auto keyGroup = ComputeKeyGroup(key.PriKey().KeyHashCode(), key.StateId());
        return Filter(keyGroup, key.StateId(), value.SeqId());
    }

    inline bool RegisterStateFilter(const TableDescriptionRef &table, SequenceIdFilterRef &stateFilter)
    {
        if (stateFilter != nullptr) {
            mStateFilterMap[table] = stateFilter;
        }
        return true;
    }

    inline int32_t GetGroup(uint32_t keyHashCode, uint16_t stateId) const
    {
        return static_cast<int32_t>(ComputeKeyGroup(keyHashCode, stateId));
    }

    inline const KeyGroupUtilRef &GetKeyGroupUtil() const
    {
        return mKeyGroupUtil;
    }
    inline int32_t GetStartKeyGroup() const
    {
        return mStartKeyGroup;
    }
    inline int32_t GetEndKeyGroup() const
    {
        return mEndKeyGroup;
    }

    inline bool StateFilter(uint16_t stateId, uint64_t seqId)
    {
        if (!mStateFilterMap.empty()) {
            TableDescriptionRef table = mStateIdProvider->GetTableDescription(stateId);
            RETURN_FALSE_AS_NULLPTR(table);
            SequenceIdFilterRef stateFilter = mStateFilterMap.at(table);
            if (stateFilter != nullptr) {
                return stateFilter->Filter(seqId);
            }
        }
        return false;
    }

    inline int64_t GetTtlTime(uint16_t stateId)
    {
        if (UNLIKELY(mStateFilterMap.empty())) {
            return 0;
        }
        TableDescriptionRef table = mStateIdProvider->GetTableDescription(stateId);
        if (UNLIKELY(table == nullptr)) {
            LOG_ERROR("table is nullptr.");
            return 0;
        }
        auto it = mStateFilterMap.find(table);
        if (UNLIKELY(it == mStateFilterMap.end())) {
            return 0;
        }
        SequenceIdFilterRef stateFilter = it->second;
        if (UNLIKELY(stateFilter == nullptr)) {
            LOG_ERROR("stateFilter is nullptr.");
            return 0;
        }
        return stateFilter->GetTtlTime();
    }

private:
    inline uint32_t ComputeKeyGroup(uint32_t keyHashCode, uint16_t stateId) const
    {
        return StateId::GetStateType(stateId) == PQ ? mKeyGroupUtil->ComputePQKeyGroupForKeyHash(keyHashCode) :
                                                      mKeyGroupUtil->ComputeKeyGroupForKeyHash(keyHashCode);
    }

    StateIdProviderRef mStateIdProvider = nullptr;
    ConfigRef mConf = nullptr;
    KeyGroupStateFilterRef mKeyGroupFilter = nullptr;
    KeyGroupUtilRef mKeyGroupUtil = nullptr;
    int32_t mStartKeyGroup = 0;
    int32_t mEndKeyGroup = 0;
    std::unordered_map<TableDescriptionRef, SequenceIdFilterRef, TableDescriptionHashTTL, TableDescriptionEqualTTL>
        mStateFilterMap;
};
using StateFilterManagerRef = std::shared_ptr<StateFilterManager>;

}  // namespace bss
}  // namespace ock
#endif  // BOOST_SS_STATE_FILTER_MANAGER_H
