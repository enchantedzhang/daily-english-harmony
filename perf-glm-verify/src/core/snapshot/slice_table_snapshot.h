/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#ifndef BOOST_SS_SLICE_TABLE_SNAPSHOT_H
#define BOOST_SS_SLICE_TABLE_SNAPSHOT_H

#include <atomic>
#include <cstdint>
#include <queue>
#include <vector>

#include "include/bss_err.h"
#include "slice_table/bucket_group_manager.h"
#include "slice_table/index/slice_bucket_index.h"
#include "snapshot_meta.h"

namespace ock {
namespace bss {
struct CopySliceChainParams {
    std::unordered_map<SliceAddressRef, DataSliceRef, SliceAddressHash, SliceAddressEqual> copiedDataSliceReference;
    std::vector<SliceAddressRef> invalidAddresses;
    bool recordUsedMemory;
    bool deepCopySliceAddress;
};

constexpr uint32_t MAX_RETRY_NUM = 5;

class SliceTableSnapshot;
using SliceTableSnapshotRef = std::shared_ptr<SliceTableSnapshot>;

class SliceTableSnapshot : public std::enable_shared_from_this<SliceTableSnapshot> {
    class LogicalSliceChainSnapshotMeta {
    public:
        LogicalSliceChainSnapshotMeta(int32_t headIndex, int32_t tailIndex, bool hasFilePage)
            : mSliceChainHeadIndex(headIndex), mSliceChainTailIndex(tailIndex), mHasFilePage(hasFilePage)
        {
        }

        inline bool Lock()
        {
            bool expect = false;
            return mLocked.compare_exchange_strong(expect, true);
        }

        inline bool IsLocked()
        {
            return mLocked.load();
        }

    public:
        int32_t mSliceChainHeadIndex = -1;
        int32_t mSliceChainTailIndex = -1;
        bool mHasFilePage = false;

    private:
        std::atomic<bool> mLocked{ false };
    };
    using LogicalSliceChainSnapshotMetaRef = std::shared_ptr<LogicalSliceChainSnapshotMeta>;

    // checkpoint流程用于迭代SliceTable中的数据而构建的迭代器.
    class SliceTableSnapshotSliceFlushIterator : public Iterator<SliceAddressRef> {
    public:
        ~SliceTableSnapshotSliceFlushIterator() override = default;

        BResult Initialize(const SliceTableSnapshotRef &sliceTableSnapshot)
        {
            mSliceTableSnapshot = sliceTableSnapshot;
            return BSS_OK;
        }

        bool HasNext() override
        {
            // 判断当前迭代器是否为空或HasNext为false. 如果满足其一则执行Advance更新当前迭代器.
            if (mCurrentChain == nullptr || !mCurrentChain->HasNext()) {
                Advance();
            }
            return mCurrentChain != nullptr && mCurrentChain->HasNext();
        }

        SliceAddressRef Next() override
        {
            if (UNLIKELY(!HasNext())) {
                LOG_ERROR("Has next is false, there is no next element.");
                return nullptr;
            }
            return mCurrentChain->Next();
        }

    private:
        void Advance();
        void GetSnapshotMeta(LogicalSliceChainSnapshotMetaRef &meta, bool &isEmptyChain);
        void GetSnapshotSliceChain(LogicalSliceChainRef &sliceChain);

    private:
        int32_t mChainArrayCursor = -1;
        IteratorRef<SliceAddressRef> mCurrentChain = nullptr;
        SliceTableSnapshotRef mSliceTableSnapshot = nullptr;
    };
    using SliceTableSnapshotSliceFlushIteratorRef = Ref<SliceTableSnapshotSliceFlushIterator>;

public:
    SliceTableSnapshot() = default;
    ~SliceTableSnapshot() = default;

    BResult Initialize(const SliceBucketIndexRef &sliceBucketIndex, const BucketGroupManagerRef &bucketGroupManager,
                       const MemManagerRef &memManager, bool isSavepoint, uint64_t snapshotId,
                       const KeyGroupUtilRef &keyGroupUtil, uint32_t startKeyGroup, uint32_t endKeyGroup);

    inline IteratorRef<SliceAddressRef> GetSnapshotSliceFlushIterator()
    {
        SliceTableSnapshotSliceFlushIteratorRef flushIterator = MakeRef<SliceTableSnapshotSliceFlushIterator>();
        flushIterator->Initialize(shared_from_this());
        return flushIterator;
    }

    BResult GenSliceTableIndexSnapshot();

    BResult CopyLogicSliceChain(const LogicalSliceChainRef &sliceChainBeforeCopy,
                                const LogicalSliceChainSnapshotView &snapshotView,
                                const LogicalSliceChainSnapshotMetaRef &chainMeta, CopySliceChainParams &params,
                                LogicalSliceChainRef &copiedChain);

    SnapshotMetaRef SnapshotMetaFunc(uint64_t snapshotId, const FileOutputViewRef &localOutputView);

    void WaitCopyOnWriteComplete(uint32_t slot)
    {
        auto checkLogicalSliceChain = mSliceChainSnapshotArray[slot].first;
        uint32_t reTryNum = 0;
        while (checkLogicalSliceChain == nullptr) {
            usleep(NO_1000);
            checkLogicalSliceChain = mSliceChainSnapshotArray[slot].first;
            if (reTryNum++ > MAX_RETRY_NUM) {
                break;
            }
        }
    }

    SliceKVIteratorPtr Iterator();

    void ReleaseResource();

private:
    void SnapshotNoDataSlicesLogicalSliceChain(uint32_t slot, bool hasFilePage);

    static const LogicalSliceChainSnapshotMetaRef mEmptySliceMeta;

    BucketGroupRef GetBucketGroup() const
    {
        return mBucketGroupManager->GetBucketGroupVector()[0];
    }

private:
    SliceBucketIndexRef mSliceBucketIndex;
    BucketGroupManagerRef mBucketGroupManager = nullptr;
    uint32_t mTotalBucketNum = 0;
    bool mIsSavepoint = false;
    uint64_t mSnapshotId = 0;
    SnapshotMetaRef mSnapshotMeta = std::make_shared<SnapshotMeta>();

    // sliceChain的快照列表.
    std::mutex mChainSnapMutex;
    std::vector<std::pair<LogicalSliceChainRef, LogicalSliceChainSnapshotMetaRef>> mSliceChainSnapshotArray;

    MemManagerRef mMemManager = nullptr;
    KeyGroupUtilRef mKeyGroupUtil = nullptr;
    uint32_t mStartKeyGroup = 0;
    uint32_t mEndKeyGroup = 0;
    std::atomic<bool> mIsReleased{ false };
    std::mutex mResourceMutex;
};
using SliceTableSnapshotRef = std::shared_ptr<SliceTableSnapshot>;

}  // namespace bss
}  // namespace ock
#endif  // BOOST_SS_SLICE_TABLE_SNAPSHOT_H
