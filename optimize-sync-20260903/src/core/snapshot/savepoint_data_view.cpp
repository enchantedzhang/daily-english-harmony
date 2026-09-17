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

#include "savepoint_data_view.h"

#include "snapshot_manager.h"
#include "sorted_key_value_merging_iterator.h"

namespace ock {
namespace bss {
bool SavepointDataView::MultipleFileStoreIterator::HasNext()
{
    RETURN_FALSE_AS_NULLPTR(mLsmStores);
    while ((mCurrentIterator == nullptr || !mCurrentIterator->HasNext()) && mLsmStores->HasNext()) {
        auto fileStore = mLsmStores->Next();
        mCurrentIterator = mSavepointDataViewImpl->BuildFileStoreSnapshotIterator(fileStore, mIsPQ);
    }
    return (mCurrentIterator != nullptr && mCurrentIterator->HasNext());
}

KeyValueRef SavepointDataView::MultipleFileStoreIterator::Next()
{
    return mCurrentIterator->Next();
}

void SavepointDataView::MultipleFileStoreIterator::Close()
{
    if (mCurrentIterator != nullptr) {
        mCurrentIterator->Close();
    }
}

IteratorRef<BinaryKeyValueItemRef> SavepointDataView::SavepointIterator()
{
    return mCurrentIterator;
}

KeyValueIteratorRef SavepointDataView::CreateSortedKeyValueIterator(
    const std::pair<SliceTableSnapshotOperatorRef, FileStoreSnapshotOperatorRef> &tuple)
{
    if (tuple.first == nullptr || tuple.second == nullptr) {
        LOG_ERROR("FileStore is nullptr");
        return nullptr;
    }
    auto sliceTableIterator = tuple.first->SnapshotIterator();

    KeyValueIteratorRef multipleFileStoreIterator = std::make_shared<MultipleFileStoreIterator>(this, tuple.second,
                                                                                                false);
    auto result = std::make_shared<SortedKeyValueMergingIterator>();
    result->Init(sliceTableIterator, multipleFileStoreIterator, mMemManager, true,
                 FileProcHolder::FILE_STORE_SAVEPOINT);
    return result;
}

KeyValueIteratorRef SavepointDataView::BuildFileStoreSnapshotIterator(const LsmStoreRef &lsmStore, bool isPQ)
{
    VersionPtr version = lsmStore->GetVersionForSnapshot(mSnapshotId);
    if (version == nullptr) {
        LOG_ERROR("Can't find version for savepoint:" << mSnapshotId
                                                      << ", lsmStore:" << lsmStore->GetFileStoreId()->ToString());
        return {};
    }
    return lsmStore->IteratorForSavepoint(version, isPQ);
}

std::pair<SliceTableSnapshotOperatorRef, FileStoreSnapshotOperatorRef> SavepointDataView::FindFileStoreMapping()
{
    SliceTableSnapshotOperatorRef sliceTableSnapshotOperator = nullptr;
    FileStoreSnapshotOperatorRef fileStoreSnapshotOperator = nullptr;
    auto snapshotOperatorMap = mPendingSavepoint->GetRegisterSnapshotOperator();
    for (auto &i : snapshotOperatorMap) {
        if (std::dynamic_pointer_cast<SliceTableSnapshotOperator>(i.second) != nullptr) {
            sliceTableSnapshotOperator = std::dynamic_pointer_cast<SliceTableSnapshotOperator>(i.second);
            continue;
        }

        if (std::dynamic_pointer_cast<FileStoreSnapshotOperator>(i.second) != nullptr) {
            fileStoreSnapshotOperator = std::dynamic_pointer_cast<FileStoreSnapshotOperator>(i.second);
        }
    }
    if (sliceTableSnapshotOperator == nullptr) {
        LOG_ERROR("sliceTableSnapshotOperator not found for savepoint");
    }
    if (fileStoreSnapshotOperator == nullptr) {
        LOG_ERROR("fileStoreSnapshotOperator not found for savepoint");
    }
    return { sliceTableSnapshotOperator, fileStoreSnapshotOperator };
}

void SavepointDataView::Close()
{
    if (mCurrentIterator != nullptr) {
        mCurrentIterator->Close();
    }
    mSnapshotManager->NotifySavepointAbort(mSnapshotId);
}

}  // namespace bss
}  // namespace ock