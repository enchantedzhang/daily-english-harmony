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

#ifndef BOOST_SS_STATEID_PROVIDER_H
#define BOOST_SS_STATEID_PROVIDER_H

#include <atomic>
#include <cstdint>
#include <memory>
#include <set>
#include <unordered_map>
#include <vector>

#include "common/io/output_view.h"
#include "include/bss_types.h"
#include "include/table_description.h"
#include "io/file_input_view.h"
#include "io/file_output_view.h"
#include "state_id.h"

namespace ock {
namespace bss {
class StateIdMap;
using StateIdMapRef = std::shared_ptr<StateIdMap>;

struct TableDescriptionEqual {
    bool operator()(const TableDescriptionRef a, const TableDescriptionRef b) const
    {
        RETURN_FALSE_AS_NULLPTR(a);
        RETURN_FALSE_AS_NULLPTR(b);
        return a->Equals(*b);
    }
};

struct TableDescriptionHash {
    uint32_t operator()(const TableDescriptionRef ref) const
    {
        if (ref == nullptr) {
            return 0;
        }
        return ref->HashCode();
    }
};

class StateIdMap {
public:
    explicit StateIdMap()
    {
    }

    ~StateIdMap()
    {
        std::unordered_map<TableDescriptionRef, uint16_t, TableDescriptionHash, TableDescriptionEqual>().swap(
            mStateIdMap);
        std::unordered_map<uint16_t, TableDescriptionRef>().swap(mReverseMap);
    }

    uint16_t GetStateId(TableDescriptionRef description)
    {
        auto it = mStateIdMap.find(description);
        if (it != mStateIdMap.end()) {
            return it->second;
        }
        uint32_t seq = mSize.fetch_add(NO_1);
        uint16_t stateId = StateId::Of(seq, description->GetStateType());
        mStateIdMap[description] = stateId;
        mReverseMap[stateId] = description;
        return stateId;
    }

    TableDescriptionRef GetTableDescription(uint16_t stateId)
    {
        if (mReverseMap.find(stateId) == mReverseMap.end()) {
            LOG_ERROR("stateId may be wrong, cant get tableDescription");
            return nullptr;
        }
        return mReverseMap[stateId];
    }

    BResult Serialize(const OutputViewRef &outputView)
    {
        RETURN_INVALID_PARAM_AS_NULLPTR(outputView);
        RETURN_NOT_OK(outputView->WriteUint32(mSize.load()));
        for (const auto &entry : mStateIdMap) {
            RETURN_ERROR_AS_NULLPTR(entry.first);
            RETURN_NOT_OK(outputView->WriteUTF(entry.first->GetTableName()));
            outputView->WriteUint16(entry.second);
        }
        return BSS_OK;
    }

    static BResult Deserialize(StateIdMapRef &restoreGroupMapInstance, const FileInputViewRef &inputView,
                               std::unordered_map<std::string, TableDescriptionRef> &tableDescriptionMap)
    {
        RETURN_INVALID_PARAM_AS_NULLPTR(inputView);
        StateIdMapRef restoredStateIdMap = std::make_shared<StateIdMap>();
        uint32_t count = 0;
        RETURN_NOT_OK_AS_READ_ERROR(inputView->Read(count));
        restoredStateIdMap->mSize.store(count);
        for (uint32_t i = 0; i < count; i++) {
            std::string tableName;
            RETURN_NOT_OK_AS_READ_ERROR(inputView->ReadUTF(tableName));
            uint16_t stateId = 0;
            RETURN_NOT_OK_AS_READ_ERROR(inputView->Read(stateId));
            TableDescriptionRef tableDescription = tableDescriptionMap[tableName];
            if (tableDescription == nullptr && StateId::GetStateType(stateId) == PQ) {
                continue;
            }
            if (tableDescription == nullptr) {
                LOG_ERROR("error in restoring map for GTableDescription. No GTableDescription found.");
                return BSS_ERR;
            }
            restoredStateIdMap->mStateIdMap.emplace(tableDescription, stateId);
            restoredStateIdMap->mReverseMap.emplace(stateId, tableDescription);
        }
        restoreGroupMapInstance = restoredStateIdMap;
        return BSS_OK;
    }

public:
    std::unordered_map<TableDescriptionRef, uint16_t, TableDescriptionHash, TableDescriptionEqual> mStateIdMap{};
    std::unordered_map<uint16_t, TableDescriptionRef> mReverseMap{};
    std::atomic<uint32_t> mSize{};
};

class StateIdProvider;
using StateIdProviderRef = std::shared_ptr<StateIdProvider>;
class StateIdProvider {
public:
    StateIdProvider(uint32_t startGroup, uint32_t endGroup, const MemManagerRef &memManager)
    {
        mStartGroupId = startGroup;
        mEndGroupId = endGroup;
        mMemManager = memManager;
        mStateIdMap = std::make_shared<StateIdMap>();
    }

    StateIdProvider(uint32_t startGroup, const MemManagerRef &memManager, uint32_t endGroup)
    {
        mStartGroupId = startGroup;
        mEndGroupId = endGroup;
        mMemManager = memManager;
        mStateIdMap = std::make_shared<StateIdMap>();
    }

    ~StateIdProvider()
    {
        LOG_INFO("Delete StateIdProvider success.");
    }

    inline uint16_t GetStateId(TableDescriptionRef description)
    {
        return mStateIdMap->GetStateId(description);
    }

    TableDescriptionRef GetTableDescription(uint16_t stateId)
    {
        if (UNLIKELY(mStateIdMap == nullptr)) {
            LOG_ERROR("StateIdMap is null!");
            return nullptr;
        }
        return mStateIdMap->GetTableDescription(stateId);
    }

    BResult Snapshot(const FileOutputViewRef &fileOutputView)
    {
        RETURN_INVALID_PARAM_AS_NULLPTR(fileOutputView);
        OutputViewRef outputView = std::make_shared<OutputView>(mMemManager, FileProcHolder::FILE_STORE_SNAPSHOT);
        RETURN_NOT_OK(outputView->WriteUint32(mStartGroupId));
        RETURN_NOT_OK(outputView->WriteUint32(mEndGroupId));
        RETURN_NOT_OK(mStateIdMap->Serialize(outputView));
        auto sizeToCheck = fileOutputView->Size();
        if (UNLIKELY(sizeToCheck > INT64_MAX)) {
            LOG_ERROR("mFileOutputView->Size() cannot cast to int64_t.");
            return BSS_ERR;
        }
        return fileOutputView->WriteBuffer(outputView->Data(), static_cast<int64_t>(sizeToCheck),
                                           outputView->GetOffset());
    }

    BResult Restore(const FileInputViewRef &fileInputView, const std::vector<TableDescriptionRef> &descriptions)
    {
        RETURN_INVALID_PARAM_AS_NULLPTR(fileInputView);
        uint32_t restoredStartGroupId = 0;
        uint32_t restoredEndGroupId = 0;
        RETURN_NOT_OK_AS_READ_ERROR(fileInputView->Read(restoredStartGroupId));
        RETURN_NOT_OK_AS_READ_ERROR(fileInputView->Read(restoredEndGroupId));
        std::unordered_map<std::string, TableDescriptionRef> tableDescriptionMap;
        for (const auto &description : descriptions) {
            if (UNLIKELY(description == nullptr)) {
                LOG_ERROR("description is nullptr");
                continue;
            }
            tableDescriptionMap.emplace(description->GetTableName(), description);
        }
        StateIdMapRef restoredStateIdMap = nullptr;
        RETURN_NOT_OK(StateIdMap::Deserialize(restoredStateIdMap, fileInputView, tableDescriptionMap));
        mStartGroupId = restoredStartGroupId;
        mEndGroupId = restoredEndGroupId;
        mStateIdMap = restoredStateIdMap;
        LOG_DEBUG("Restore stateId provider success, groupId:" << mStartGroupId << "-" << mEndGroupId);
        return BSS_OK;
    }

    BResult RestoreFrom(const StateIdProviderRef &snapshot)
    {
        RETURN_INVALID_PARAM_AS_NULLPTR(snapshot);
        RETURN_INVALID_PARAM_AS_NULLPTR(snapshot->mStateIdMap);
        mStartGroupId = snapshot->mStartGroupId;
        mEndGroupId = snapshot->mEndGroupId;
        mStateIdMap = snapshot->mStateIdMap;
        return BSS_OK;
    }

    StateIdProviderRef Copy()
    {
        StateIdProviderRef stateIdProvider = std::make_shared<StateIdProvider>(mStartGroupId, mMemManager, mEndGroupId);
        stateIdProvider->mStateIdMap = mStateIdMap;
        return stateIdProvider;
    }

private:
    uint32_t mStartGroupId = 0;
    uint32_t mEndGroupId = 0;

    StateIdMapRef mStateIdMap = nullptr;
    MemManagerRef mMemManager = nullptr;
};

}  // namespace bss
}  // namespace ock
#endif  // BOOST_SS_STATEID_PROVIDER_H
