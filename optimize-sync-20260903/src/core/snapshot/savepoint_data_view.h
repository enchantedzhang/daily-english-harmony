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

#ifndef BOOST_SS_SAVEPOINT_DATA_VIEW_H
#define BOOST_SS_SAVEPOINT_DATA_VIEW_H

#include "binary_key_value_Item_iterator.h"
#include "common/util/iterator.h"
#include "pending_savepoint_coordinator.h"
#include "slice_table_snapshot_operator.h"

namespace ock {
namespace bss {
class BinaryKeyValueItem;

class SavepointDataView {
public:
    class MultipleFileStoreIterator : public Iterator<KeyValueRef> {
    public:
        MultipleFileStoreIterator(SavepointDataView *savepointDataViewImpl,
                                  const FileStoreSnapshotOperatorRef &fileStoreSnapshotOperator, bool isPQ)
            : mSavepointDataViewImpl(savepointDataViewImpl), mIsPQ(isPQ)
        {
            mLsmStores = MakeRef<VectorIterator<LsmStoreRef>>(fileStoreSnapshotOperator->GetLsmStores());
        }

        bool HasNext() override;

        KeyValueRef Next() override;

        void Close() override;

    private:
        SavepointDataView *mSavepointDataViewImpl = nullptr;
        VectorIteratorRef<LsmStoreRef> mLsmStores = nullptr;
        KeyValueIteratorRef mCurrentIterator = nullptr;
        bool mIsPQ = false;
    };

    SavepointDataView(const SnapshotManagerRef &snapshotManager, const PendingSavepointCoordinatorRef &pendingSavepoint,
                      const KeyGroupUtilRef &keyGroupUtil, const MemManagerRef &mMemManager)
        : mSnapshotManager(snapshotManager),
          mPendingSavepoint(pendingSavepoint),
          mKeyGroupUtil(keyGroupUtil),
          mSnapshotId(mPendingSavepoint->GetSnapshotId()),
          mMemManager(mMemManager)
    {
        auto tuple = FindFileStoreMapping();
        // 仅迭代kv
        auto sortedKeyValueIterator = CreateSortedKeyValueIterator(tuple);
        // 仅迭代pq
        KeyValueIteratorRef sortedPQIterator = nullptr;
        if (tuple.second == nullptr) {
            LOG_ERROR("FileStore is nullptr.");
        } else {
            sortedPQIterator = std::make_shared<MultipleFileStoreIterator>(this, tuple.second, true);
        }

        auto sliceTable = pendingSavepoint->GetSliceTable();
        auto func = [sliceTable](uint64_t blobId, uint32_t keyHashCode, uint64_t seqId,
                                 Value &originalValue) -> BResult {
            return sliceTable->GetValueFromBlobStore(blobId, keyHashCode, seqId, originalValue);
        };
        mCurrentIterator = MakeRef<BinaryKeyValueItemIterator>(mPendingSavepoint->GetStateIdProviderSnapshot(),
                                                               mKeyGroupUtil, sortedKeyValueIterator, sortedPQIterator,
                                                               mMemManager, func);
    }

    KeyValueIteratorRef CreateSortedKeyValueIterator(
        const std::pair<SliceTableSnapshotOperatorRef, FileStoreSnapshotOperatorRef> &tuple);

    std::pair<SliceTableSnapshotOperatorRef, FileStoreSnapshotOperatorRef> FindFileStoreMapping();

    KeyValueIteratorRef BuildFileStoreSnapshotIterator(const LsmStoreRef &lsmStore, bool isPQ);

    void Close();

    IteratorRef<BinaryKeyValueItemRef> SavepointIterator();

    inline uint64_t GetSnapshotId() const
    {
        return mSnapshotId;
    }

    inline uint32_t GetMaxParallelism() const
    {
        return mKeyGroupUtil->GetMaxParallelism();
    }

private:
    SnapshotManagerRef mSnapshotManager = nullptr;
    PendingSavepointCoordinatorRef mPendingSavepoint = nullptr;
    KeyGroupUtilRef mKeyGroupUtil = nullptr;
    uint64_t mSnapshotId = 0;
    MemManagerRef mMemManager = nullptr;
    BinaryKeyValueItemIteratorRef mCurrentIterator = nullptr;
};

}  // namespace bss
}  // namespace ock
#endif  // BOOST_SS_SAVEPOINT_DATA_VIEW_H
