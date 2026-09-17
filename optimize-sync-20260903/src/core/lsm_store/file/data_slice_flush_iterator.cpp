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

#include "data_slice_flush_iterator.h"

#include <algorithm>

#include "include/bss_types.h"

namespace ock {
namespace bss {
ByteBufferRef SliceKVIterator::CreateBuffer(uint32_t size)
{
    uintptr_t dataAddress;
    auto retVal = mMemManager->GetMemory(MemoryType::SLICE_TABLE, size, dataAddress, mForceAlloc, mAllocWaitTime);
    // for q5
    if (retVal == BSS_ALLOC_FAIL) {
        retVal = mMemManager->GetMemory(MemoryType::FILE_STORE, size, dataAddress, mForceAlloc, mAllocWaitTime);
    }
    if (UNLIKELY(retVal != 0)) {
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

BResult SliceKVIterator::BuildKVPairList()
{
    // get the next non-empty data slice list.
    std::vector<DataSliceRef> dataSliceList;
    NextValidDataSliceList(dataSliceList);
    if (UNLIKELY(dataSliceList.empty())) {
        return BSS_OK;
    }

    // count the number of key-value pairs.
    uint32_t kvCount = GetKvCount(dataSliceList);
    if (UNLIKELY(kvCount == 0)) {
        return BSS_OK;
    }

    // put kv to list.
    mCurrentKVList.clear();
    mCurrentKVList.reserve(kvCount);

    if (dataSliceList.size() == 1) {
        // only one slice.
        auto &dataSlice = dataSliceList[0];
        auto &sliceSpace = dataSlice->GetSlice()->GetSliceSpace();
        uint32_t keyCount = sliceSpace.KeyCount();
        for (uint32_t index = 0; index < keyCount; index++) {
            auto keyValue = MakeRef<KeyValue>();
            sliceSpace.GetKeyValue(index, keyValue);
            // mKeyGroupFilter will not be null when mForSavepoint is true
            if (mForSavepoint && mKeyGroupFilter(keyValue)) {
                continue;
            }
            mCurrentKVList.emplace_back(std::move(keyValue));
        }
    } else {
        //  merge data slice.
        std::unordered_map<KeyPtr, KeyValueRef, KeyHash, KeyEqual> kvMap;
        auto ret = MergeDataSlices(dataSliceList, kvMap);
        if (UNLIKELY(ret != BSS_OK)) {
            return ret;
        }
        for (const auto &kv : kvMap) {
            if (mForSavepoint && mKeyGroupFilter(kv.second)) {
                continue;
            }
            mCurrentKVList.emplace_back(kv.second);
        }
    }

    // sort list by key.
    std::sort(mCurrentKVList.begin(), mCurrentKVList.end(), [](const KeyValueRef &a, const KeyValueRef &b) {
        int32_t cmp = a->key.Compare(b->key);
        if (cmp != 0) {
            return cmp < 0;
        }
        return b->value.SeqId() < a->value.SeqId();
    });

    mCurrentKV = mCurrentKVList.begin();
    // 如果本次迭代出的dataSliceList中所有数据均被过滤掉，则继续迭代获取下一个dataSliceList
    if (mForSavepoint) {
        if (UNLIKELY(mCurrentKVList.begin() == mCurrentKVList.end())) {
            BuildKVPairList();
        }
    }
    return BSS_OK;
}

void SliceKVIterator::NextValidDataSliceList(std::vector<DataSliceRef> &dataSliceList)
{
    while (mDataSliceVectorIterator->HasNext()) {
        dataSliceList = mDataSliceVectorIterator->Next();
        if (!dataSliceList.empty()) {
            break;
        }
    }
}

uint32_t SliceKVIterator::GetKvCount(const std::vector<DataSliceRef> &dataSliceList)
{
    uint32_t kvCount = 0;
    for (const auto &dataSlice : dataSliceList) {
        kvCount += dataSlice->GetSlice()->GetSliceSpace().KeyCount();
    }
    return kvCount;
}

BResult SliceKVIterator::MergeDataSlices(const std::vector<DataSliceRef> &dataSliceList,
                                         std::unordered_map<KeyPtr, KeyValueRef, KeyHash, KeyEqual> &kvMap)
{
    for (const auto &dataSlice : dataSliceList) {
        auto &sliceSpace = dataSlice->GetSlice()->GetSliceSpace();
        uint32_t keyCount = sliceSpace.KeyCount();
        for (uint32_t index = 0; index < keyCount; index++) {
            auto keyValue = MakeRef<KeyValue>();
            sliceSpace.GetKeyValue(index, keyValue);
            auto &key = keyValue->key;
            auto olderKv = kvMap.find(&key);
            if (olderKv == kvMap.end()) {
                kvMap.emplace(&key, std::move(keyValue));
            } else {
                auto allocator = [this](uint32_t size) { return CreateBuffer(size); };
                auto &olderValue = olderKv->second->value;
                auto &newerValue = keyValue->value;
                auto deleteAction = [this](const Key &key1, const Value &oldValue) -> void {
                    if (mTombstoneService != nullptr) {
                        mTombstoneService->DeleteValue(key1, oldValue);
                    }
                };
                BResult result = olderValue.MergeWithNewerValue(key, newerValue, allocator, deleteAction);
                if (result != BSS_OK) {
                    LOG_WARN("Failed to merge value. newerValue:" << newerValue.ToString()
                                                                  << ", olderValue:" << olderValue.ToString());
                    return BSS_ERR;
                }
            }
        }
    }
    return BSS_OK;
}

}  // namespace bss
}  // namespace ock