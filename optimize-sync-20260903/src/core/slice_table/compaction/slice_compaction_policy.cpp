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

#include "slice_table/compaction/slice_compaction_policy.h"

namespace ock {
namespace bss {

SelectedSliceContextRef SliceCompactionPolicy::SelectCompactionSlice(SliceIndexContextRef &sliceIndexContext,
                                                                     uint32_t curChainIndex, uint32_t minLength)
{
    LogicalSliceChainRef logicalSliceChain = sliceIndexContext->GetLogicalSliceChain();
    if (UNLIKELY(logicalSliceChain == nullptr)) {
        LOG_ERROR("Get logic slice chain is nullptr.");
        return nullptr;
    }
    if (UNLIKELY(logicalSliceChain != mBucketIndex->GetLogicChainedSlice(sliceIndexContext->GetSliceIndexSlot()))) {
        LOG_ERROR("Current logic slice chain not matched.");
        return nullptr;
    }

    SelectedSliceContextBuild choiceBuild;
    uint32_t startCompactionIndex = curChainIndex;
    while (startCompactionIndex >= logicalSliceChain->GetBaseSliceIndex()) {
        SliceAddressRef sliceAddress = logicalSliceChain->GetSliceAddress(static_cast<int32_t>(startCompactionIndex));
        if (UNLIKELY(sliceAddress == nullptr)) {
            LOG_WARN("Slice address is nullptr, index:" << startCompactionIndex);
            if (startCompactionIndex == 0) {  // 防止uint32_t翻转
                break;
            }
            startCompactionIndex--;
            continue;
        }

        DataSliceRef dataSlice = sliceAddress->GetDataSlice();
        // 排除正在做Flush和evicted的slice.
        if (dataSlice == nullptr || sliceAddress->IsTriggerFlush() || sliceAddress->IsEvicted()) {
            LOG_INFO("dataSlice is nullptr or sliceAddress is triggerFlush, index:" << startCompactionIndex);
            break;
        }

        if (!choiceBuild.IsReachMemoryLimit() && choiceBuild.Size() >= NO_2 &&
            (choiceBuild.GetSliceSize() + dataSlice->GetSize()) > mConfig->GetEvictMinSize()) {
            choiceBuild.SetReachMemoryLimit();
            break;
        }
        if (!sliceAddress->SetStatus(SliceEvent::COMPACT)) {
            break;
        }
        choiceBuild.Add(sliceAddress, dataSlice);
        choiceBuild.SetChainIndex(static_cast<int32_t>(startCompactionIndex));
        if (startCompactionIndex == 0) {  // 防止uint32_t翻转
            break;
        }
        startCompactionIndex--;
    }

    if (choiceBuild.Size() < minLength) {  // 如果选择的slice个数小于compaction阀值, 则返回nullptr.
        return nullptr;
    }
    return choiceBuild.Build();
}
}  // namespace bss
}  // namespace ock