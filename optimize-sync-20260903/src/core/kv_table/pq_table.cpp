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

#include "pq_table.h"

namespace ock {
namespace bss {

BResult PQTable::Initialize()
{
    if (mInitialized) {
        return BSS_OK;
    }
    mSkipList = InitNewSkipList();
    mStateId = mStateIdProvider->GetStateId(mDescription);
    mInitialized = true;
    return BSS_OK;
}

BResult PQTable::AddKey(const BinaryData &key, uint32_t hashcode)
{
    uint32_t keyGroup = hashcode % mDescription->GetMaxParallelism();
    mKeyGroupUtil->SetPQKeyGroup(hashcode, keyGroup);
    auto writer = [this, key, hashcode]() -> BResult {
        PQBinaryData pqBinaryData;
        pqBinaryData.mData = GetCopyData(key);
        pqBinaryData.mValueType = PUT;
        pqBinaryData.mHashCode = hashcode;
        if (pqBinaryData.mData.IsNull()) {
            return BSS_ALLOC_FAIL;
        }
        LOG_DEBUG("Add key: " << key.ToString());
        return mSkipList->Put(pqBinaryData);
    };
    RequireMemoryUntilSuccess(writer);
    return BSS_OK;
}

BResult PQTable::RemoveKey(const BinaryData &key, uint32_t hashcode)
{
    uint32_t keyGroup = hashcode % mDescription->GetMaxParallelism();
    mKeyGroupUtil->SetPQKeyGroup(hashcode, keyGroup);
    auto writer = [this, key, hashcode]() -> BResult {
        PQBinaryData pqBinaryData;
        pqBinaryData.mData = GetCopyData(key);
        pqBinaryData.mValueType = DELETE;
        pqBinaryData.mHashCode = hashcode;
        if (pqBinaryData.mData.IsNull()) {
            return BSS_ALLOC_FAIL;
        }
        LOG_DEBUG("Remove key: " << key.ToString());
        return mSkipList->Put(pqBinaryData);
    };
    RequireMemoryUntilSuccess(writer);
    return BSS_OK;
}

PQKeyIterator *PQTable::KeyIterator(const BinaryData &data)
{
    PQKeyIterator *iterator = new PQKeyIterator();
    std::vector<PQSkipList> list;
    {
        ReadLocker<ReadWriteLock> lock(&mRwLock);
        for (auto it = mSnapshotQueue.rbegin(); it != mSnapshotQueue.rend(); ++it) {
            list.emplace_back(*it);
        }
        list.emplace_back(mSkipList);
    }

    iterator->Init(data, list, mLsmStore->IteratorForPQ(mStateIdProvider->GetStateId(mDescription), data));
    return iterator;
}
}  // namespace bss
}  // namespace ock
