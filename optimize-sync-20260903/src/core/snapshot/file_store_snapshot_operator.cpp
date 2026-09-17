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
#include "file_store_snapshot_operator.h"

namespace ock {
namespace bss {
BResult FileStoreSnapshotOperator::SyncSnapshot(bool isSavepoint)
{
    for (const auto &lsmStore : mLsmStores) {
        auto fileMap = lsmStore->ExecuteSnapshot(mSnapshotId);
        for (const auto &entry : fileMap) {
            auto iter = mToSnapshotFileAddress.find(entry.first);
            if (iter == mToSnapshotFileAddress.end()) {
                mToSnapshotFileAddress.emplace(entry.first,
                                               std::make_tuple(entry.second.first, entry.second.second, 1));
            } else {
                std::get<NO_2>(iter->second) += NO_1;  // 更新count.
            }
        }
    }
    return BSS_OK;
}

SnapshotMetaRef FileStoreSnapshotOperator::OutputMeta(uint64_t snapshotId, const FileOutputViewRef &localOutputView)
{
    std::lock_guard<std::mutex> lk(mResourceMutex);
    if (mIsReleased.load()) {
        return nullptr;
    }

    // 文件中的元数据内容: prefixName+fileStore的元数据信息.
    SnapshotMetaRef result = std::make_shared<SnapshotMeta>();
    for (const auto &lsmStore : mLsmStores) {
        mLocalOutputOffsets.push_back(localOutputView->Size());  // 记录文件偏移.
        FileOutputViewRef remoteOutputView = std::make_shared<FileOutputView>();
        SnapshotMetaRef snapshotMeta = lsmStore->WriteMeta(localOutputView, remoteOutputView, GetSnapshotId());
        RETURN_NULLPTR_AS_NULLPTR(snapshotMeta);
        result->AddSnapshotMeta(snapshotMeta);
    }
    return result;
}

BResult FileStoreSnapshotOperator::WriteInfo(const FileOutputViewRef &localOutputView)
{
    FileStoreSnapshotOperatorInfoRef fileStoreSnapshotOperatorInfo =
        std::make_shared<FileStoreSnapshotOperatorInfo>(SnapshotOperatorType::FILE_STORE, mFileStoreIds,
                                                        mLocalOutputOffsets);
    return fileStoreSnapshotOperatorInfo->Serialize(localOutputView);
}

void FileStoreSnapshotOperator::InternalRelease()
{
    AbstractSnapshotOperator::InternalRelease();
    for (const auto &lsmStore : mLsmStores) {
        lsmStore->ReleaseSnapshot(GetSnapshotId());
    }
}

}  // namespace bss
}  // namespace ock