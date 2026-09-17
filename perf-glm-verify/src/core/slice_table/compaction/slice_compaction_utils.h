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

#ifndef BOOST_SS_COMPACTION_UTILS_H
#define BOOST_SS_COMPACTION_UTILS_H

#include <unordered_map>
#include <vector>

#include "blob_cleaner.h"
#include "common/util/seq_generator.h"
#include "lsm_store/file/state_filter_manager.h"
#include "slice_table/slice/data_slice.h"
#include "tombstone/tombstone_service.h"

namespace ock {
namespace bss {
class SliceCompactionUtils {
public:
    static ByteBufferRef CreateBuffer(const MemManagerRef &memManager, uint32_t size)
    {
        uintptr_t dataAddress;
        auto retVal = memManager->GetMemory(MemoryType::SLICE_TABLE, size, dataAddress, false, NO_100);
        if (UNLIKELY(retVal != 0)) {
            LOG_WARN("Alloc memory for slice kv iterator failed, size:" << size);
            return nullptr;
        }
        // reserve some byte buffer for flush.
        auto byteBuffer = MakeRef<ByteBuffer>(reinterpret_cast<uint8_t *>(dataAddress), size, memManager);
        if (UNLIKELY(byteBuffer == nullptr)) {
            memManager->ReleaseMemory(dataAddress);
            LOG_ERROR("Make ref failed, byteBuffer is null.");
        }
        return byteBuffer;
    }

    static Value MergeCompactValue(const SliceKey &key, Value &first, Value &second, const MemManagerRef &memManager,
                                   const StateFilterManagerRef &stateFilter,
                                   TombstoneServiceRef tombstoneService = nullptr)
    {
        Value value;
        if (first.IsNull() && first.ValueType() != DELETE) {
            return second;
        }
        if (second.IsNull() && second.ValueType() != DELETE) {
            return first;
        }
        if (second.ValueType() == APPEND) {
            if (first.ValueType() == DELETE || stateFilter->StateFilter(key.StateId(), first.SeqId())) {
                value.Init(ValueType::PUT, second.ValueLen(), second.ValueData(), second.SeqId(), second.Buffer());
                return value;
            }
            if (first.ValueLen() + second.ValueLen() > IO_SIZE_4M) {
                return value;
            }
            auto allocator = [memManager](uint32_t size) -> ByteBufferRef { return CreateBuffer(memManager, size); };
            auto ret = second.MergeWithOlderValue(first, allocator);
            if (UNLIKELY(ret != BSS_OK)) {
                return value;
            }
        }
        if (tombstoneService != nullptr) {
            tombstoneService->DeleteValue(key, first);
        }
        return second;
    }

    static BResult MergeDataSlicesForCompaction(
        std::vector<std::pair<SliceKey, Value>> &finalResult, uint32_t &compactionCount,
        const std::vector<DataSliceRef> &compactionListReverseOrder, const MemManagerRef &memManager, bool forceFilter,
        const StateFilterManagerRef &stateFilterManager, const std::function<bool(SliceKey)> &indexSlotFilter,
        bool reserveDeleteMarker, TombstoneServiceRef tombstoneService = nullptr)
    {
        SliceKVMap newMap;
        uint32_t compactionListSize = compactionListReverseOrder.size();
        for (int i = (static_cast<int>(compactionListSize) - 1); i >= 0; i--) {
            auto &dataSlice = compactionListReverseOrder[i];
            SliceKVMap foreachMap;
            dataSlice->GetSlice()->GetSliceKVMap(foreachMap, false);
            for (auto &entry : foreachMap) {
                if (forceFilter &&
                    (stateFilterManager->Filter(entry.first, entry.second.SeqId()) || indexSlotFilter(entry.first))) {
                    continue;
                }
                auto newValue = MergeCompactValue(entry.first, newMap[entry.first], entry.second, memManager,
                                                  stateFilterManager, tombstoneService);
                if (UNLIKELY(newValue.IsNull() && newValue.ValueType() != DELETE)) {
                    return BSS_ALLOC_FAIL;
                }
                newMap[entry.first] = newValue;
            }
            compactionCount += dataSlice->GetCompactionCount();
        }

        finalResult.clear();
        finalResult.reserve(newMap.size());
        for (const auto &entry : newMap) {
            Value value = entry.second;
            if (!reserveDeleteMarker && (value.ValueType() == ValueType::DELETE ||
                                         stateFilterManager->StateFilter(entry.first.StateId(), value.SeqId()))) {
                continue;
            }
            finalResult.emplace_back(entry.first, value);
        }
        return BSS_OK;
    }
};

}  // namespace bss
}  // namespace ock
#endif  // BOOST_SS_COMPACTION_UTILS_H
