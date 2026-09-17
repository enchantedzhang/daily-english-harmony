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

#ifndef SLICECOMPACTIONPOLICY_H
#define SLICECOMPACTIONPOLICY_H
#include <memory>

#include "slice_table/index/slice_bucket_index.h"
#include "slice_table/index/slice_index_context.h"

namespace ock {
namespace bss {
class SelectedSliceContext {
public:
    ~SelectedSliceContext() = default;

    bool IsReachMemoryLimit() const
    {
        return mReachMemoryLimit;
    }

    void Init(std::vector<DataSliceRef> &canCompactSliceListReversed,
              std::vector<SliceAddressRef> &invalidSliceAddressList, bool reachMemoryLimit, uint32_t finalOldSliceSize,
              uint32_t compactionStartChainIndex, uint32_t compactionEndChainIndex)
    {
        mCanCompactSliceListReversed = canCompactSliceListReversed;
        mInvalidSliceAddressList = invalidSliceAddressList;
        mReachMemoryLimit = reachMemoryLimit;
        mCompactionStartChainIndex = compactionStartChainIndex;
        mCompactionEndChainIndex = compactionEndChainIndex;
        mFinalOldSliceSize = finalOldSliceSize;
    }

    inline uint32_t Size() const
    {
        return mCanCompactSliceListReversed.size();
    }

    inline uint32_t GetFinalOldSliceSize() const
    {
        return mFinalOldSliceSize;
    }

    inline const std::vector<DataSliceRef> &GetSliceListReversed() const
    {
        return mCanCompactSliceListReversed;
    }

    inline const std::vector<SliceAddressRef> &GetInvalidSliceAddressList() const
    {
        return mInvalidSliceAddressList;
    }

    inline bool IsMajor() const
    {
        return mCompactionStartChainIndex == 0;
    }

    inline uint32_t GetInclusiveCompactionStartChainIndex() const
    {
        return mCompactionStartChainIndex;
    }

    inline uint32_t GetInclusiveCompactionEndChainIndex() const
    {
        return mCompactionEndChainIndex;
    }

private:
    std::vector<DataSliceRef> mCanCompactSliceListReversed;
    std::vector<SliceAddressRef> mInvalidSliceAddressList;
    bool mReachMemoryLimit = false;
    uint32_t mCompactionStartChainIndex = 0;
    uint32_t mCompactionEndChainIndex = 0;
    uint32_t mFinalOldSliceSize = 0;
};
using SelectedSliceContextRef = std::shared_ptr<SelectedSliceContext>;

class SelectedSliceContextBuild {
public:
    inline void SetChainIndex(int32_t chainIndex)
    {
        if (mCompactionEndChainIndex == -1 || mCompactionEndChainIndex < chainIndex) {
            mCompactionEndChainIndex = chainIndex;
        }
        if (mCompactionStartChainIndex == -1 || mCompactionStartChainIndex > chainIndex) {
            mCompactionStartChainIndex = chainIndex;
        }
    }

    inline void Add(const SliceAddressRef &sliceAddress, const DataSliceRef &dataSlice)
    {
        mCanCompactSliceListReversed.push_back(dataSlice);
        mInvalidSliceAddressList.push_back(sliceAddress);
        mSliceSize += dataSlice->GetSize();
    }

    inline void SetReachMemoryLimit()
    {
        mReachMemoryLimit = true;
    }

    inline bool IsReachMemoryLimit() const
    {
        return mReachMemoryLimit;
    }

    inline uint32_t Size()
    {
        return mCanCompactSliceListReversed.size();
    }

    inline uint32_t GetSliceSize() const
    {
        return mSliceSize;
    }

    inline SelectedSliceContextRef Build()
    {
        SelectedSliceContextRef selectedSliceContext = std::make_shared<SelectedSliceContext>();
        selectedSliceContext->Init(mCanCompactSliceListReversed, mInvalidSliceAddressList, mReachMemoryLimit,
                                   mSliceSize, mCompactionStartChainIndex, mCompactionEndChainIndex);
        return selectedSliceContext;
    }

private:
    int32_t mCompactionStartChainIndex = -1;
    int32_t mCompactionEndChainIndex = -1;
    std::vector<DataSliceRef> mCanCompactSliceListReversed;
    std::vector<SliceAddressRef> mInvalidSliceAddressList;
    uint32_t mSliceSize = 0;
    bool mReachMemoryLimit = false;
};

class SliceCompactionPolicy {
public:
    ~SliceCompactionPolicy() = default;

    BResult Init(const ConfigRef &config, SliceBucketIndexRef &bucketIndex)
    {
        mConfig = config;
        mBucketIndex = bucketIndex;
        return BSS_OK;
    }

    SelectedSliceContextRef SelectCompactionSlice(SliceIndexContextRef &sliceIndexContext, uint32_t curChainIndex,
                                                  uint32_t minLength);

private:
    ConfigRef mConfig = nullptr;
    SliceBucketIndexRef mBucketIndex = nullptr;
};
using SliceCompactionPolicyRef = std::shared_ptr<SliceCompactionPolicy>;
}  // namespace bss
}  // namespace ock
#endif  // SLICECOMPACTIONPOLICY_H
