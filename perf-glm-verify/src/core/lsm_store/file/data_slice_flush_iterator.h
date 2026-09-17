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

#ifndef BOOST_SS_HASH_DATA_SLICE_FLUSH_ITERATOR_H
#define BOOST_SS_HASH_DATA_SLICE_FLUSH_ITERATOR_H

#include <vector>

#include "common/util/iterator.h"
#include "compaction/slice_compaction_utils.h"
#include "slice_table/slice/data_slice.h"

namespace ock {
namespace bss {
class SliceKVIterator {
public:
    SliceKVIterator(const IteratorRef<std::vector<DataSliceRef>> &dataSliceVectorIterator,
                    const MemManagerRef &memManager, const KeyGroupUtilRef &keyGroupUtil, uint32_t startKeyGroup,
                    uint32_t endKeyGroup, TombstoneServiceRef tombstoneService = nullptr)
        : mDataSliceVectorIterator(dataSliceVectorIterator),
          mMemManager(memManager),
          mStart(startKeyGroup),
          mEnd(endKeyGroup),
          mKeyGroupUtil(keyGroupUtil),
          mTombstoneService(tombstoneService)
    {
        mCurrentKV = mCurrentKVList.end();
    }

    ~SliceKVIterator()
    {
        std::vector<KeyValueRef>().swap(mCurrentKVList);
    }

    inline Iterator_Result HasNext()
    {
        Iterator_Result result = Iterator_Result_Failed;
        if (mCurrentKV == mCurrentKVList.end()) {
            int32_t retVal = BuildKVPairList();
            if (UNLIKELY(retVal != BSS_OK)) {
                return result;
            }
        }
        result = (mCurrentKV == mCurrentKVList.end()) ? Iterator_Result_End : Iterator_Result_Continue;
        return result;
    }

    inline KeyValueRef Next()
    {
        return *mCurrentKV++;
    }

    void SetSavepointFlag(bool isSavepoint)
    {
        mForSavepoint = isSavepoint;
        if (mForSavepoint) {
            // need to filter keyGroup for savepoint
            mKeyGroupFilter = [this](const KeyValueRef &keyValue) -> bool {
                uint32_t keyGroup = mKeyGroupUtil->ComputeKeyGroupForKeyHash(keyValue->key.KeyHashCode());
                if (keyGroup < mStart || keyGroup > mEnd) {
                    return true;
                }
                return false;
            };
        }
    }

private:
    BResult BuildKVPairList();

    ByteBufferRef CreateBuffer(uint32_t size);

    void NextValidDataSliceList(std::vector<DataSliceRef> &dataSliceList);

    uint32_t GetKvCount(const std::vector<DataSliceRef> &dataSliceList);

    BResult MergeDataSlices(const std::vector<DataSliceRef> &dataSliceList,
                            std::unordered_map<KeyPtr, KeyValueRef, KeyHash, KeyEqual> &kvMap);

private:
    IteratorRef<std::vector<DataSliceRef>> mDataSliceVectorIterator;
    MemManagerRef mMemManager{ nullptr };
    bool mForceAlloc = false;
    uint32_t mAllocWaitTime = 100;
    std::vector<KeyValueRef> mCurrentKVList;
    std::vector<KeyValueRef>::iterator mCurrentKV;
    uint32_t mStart = 0;
    uint32_t mEnd = 0;
    KeyGroupUtilRef mKeyGroupUtil = nullptr;
    bool mForSavepoint = false;
    TombstoneServiceRef mTombstoneService;
    std::function<bool(const KeyValueRef &keyValue)> mKeyGroupFilter = nullptr;
};
using SliceKVIteratorPtr = std::shared_ptr<SliceKVIterator>;

}  // namespace bss
}  // namespace ock
#endif  // BOOST_SS_HASH_DATA_SLICE_FLUSH_ITERATOR_H
