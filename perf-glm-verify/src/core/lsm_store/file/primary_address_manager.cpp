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

#include "primary_address_manager.h"

#include "common/util/timestamp_util.h"
#include "lsm_store/file/file_address_util.h"
#include "lsm_store/file/file_cache_type.h"

namespace ock {
namespace bss {
uint32_t PrimaryAddressManager::AddPrimaryAddressRef(PrimaryAddressRef &primaryAddress, uint32_t refCount)
{
    if (UNLIKELY(primaryAddress == nullptr)) {
        LOG_ERROR("Add primary address is nullptr.");
        return 0;
    }
    std::lock_guard<std::mutex> lock(mMutex);
    auto it = mPrimaryAddressMap.find(primaryAddress->GetFileAddress());
    if (it != mPrimaryAddressMap.end()) {
        it->second->AddReference(refCount);
        return it->second->GetReference();
    }
    primaryAddress->AddReference(refCount);
    mPrimaryAddressMap.emplace(primaryAddress->GetFileAddress(), primaryAddress);
    return primaryAddress->GetFileAddress();
}

void PrimaryAddressManager::DecPrimaryAddressRef(uint64_t primaryAddress)
{
    std::lock_guard<std::mutex> lock(mMutex);
    PrimaryAddressRef toRemoveAddress;
    auto it = mPrimaryAddressMap.find(primaryAddress);
    if (it != mPrimaryAddressMap.end()) {
        uint32_t ref = it->second->DecReference(1);
        if (ref == 0) {
            toRemoveAddress = it->second;
            mPrimaryAddressMap.erase(it);
        } else {
            return;
        }
    } else {
        return;
    }

    if (toRemoveAddress->GetFileStatus() == FileStatus::LOCAL) {
        mLocalFileManager->DecDbRef(FileAddressUtil::GetFileId(toRemoveAddress->GetFileAddress()), 0,
                                    TimeStampUtil::GetCurrentTime(), toRemoveAddress->GetFileLength());
    } else if (toRemoveAddress->GetFileStatus() == FileStatus::DFS) {
        mDfsFileManager->DecDbRef(FileAddressUtil::GetFileId(toRemoveAddress->GetFileAddress()), 0,
                                  TimeStampUtil::GetCurrentTime(), toRemoveAddress->GetFileLength());
    }
    PersistentAddressRef persistentAddress = toRemoveAddress->GetPersistentAddress();
    if (!persistentAddress->Equals(PERSISTENT_ADDRESS_NONE)) {
        if (toRemoveAddress->GetFileStatus() == FileStatus::LOCAL ||
            toRemoveAddress->GetFileAddress() != persistentAddress->GetFileAddress()) {
            LOG_INFO("Decreasing persistent DB reference of file " << persistentAddress->GetFileAddress()
                                                                   << " from dfs file manager.");
            mDfsFileManager->DecDbRef(FileAddressUtil::GetFileId(persistentAddress->GetFileAddress()), 0L,
                                      TimeStampUtil::GetCurrentTime(), persistentAddress->GetFileLength());
        }
    }
}

FileInfoRef PrimaryAddressManager::GetPrimaryFileInfo(uint64_t primaryAddress)
{
    PrimaryAddressRef result = GetPrimaryAddress(primaryAddress);
    if (UNLIKELY(result == nullptr)) {
        LOG_ERROR("Get primary address ref is nullptr, addr:" << primaryAddress);
        return nullptr;
    }
    if (result->GetFileStatus() == FileStatus::LOCAL) {
        return mLocalFileManager->GetFileInfo(FileAddressUtil::GetFileId(primaryAddress));
    } else if (result->GetFileStatus() == FileStatus::DFS) {
        auto fileInfo = mDfsFileManager->GetFileInfo(FileAddressUtil::GetFileId(primaryAddress));
        if (UNLIKELY(fileInfo == nullptr)) {
            LOG_ERROR("Get dfs file info is nullptr, address:" << primaryAddress << ", fileId:"
                                                               << FileAddressUtil::GetFileId(primaryAddress));
            return nullptr;
        }
        fileInfo->SetFileStatus(FileStatus::DFS);
        return fileInfo;
    }
    return nullptr;
}

PrimaryAddressRef PrimaryAddressManager::GetPrimaryAddress(uint64_t primaryAddress)
{
    std::lock_guard<std::mutex> lock(mMutex);
    auto it = mPrimaryAddressMap.find(primaryAddress);
    if (it != mPrimaryAddressMap.end()) {
        return it->second;
    }
    return nullptr;
}

BResult PrimaryAddressManager::AddPrimaryAddressWithPersistentAddress(const PrimaryAddressRef &primaryAddress,
                                                                      const PersistentAddressRef &persistentAddress,
                                                                      uint32_t refCount)
{
    RETURN_ALLOC_FAIL_AS_NULLPTR(primaryAddress);
    if (primaryAddress->GetReference() != 0) {
        LOG_ERROR("The primary address to add has unexpected reference " << primaryAddress->GetReference());
        return BSS_ERR;
    }
    std::lock_guard<std::mutex> lock(mMutex);
    auto it = mPrimaryAddressMap.find(primaryAddress->GetFileAddress());
    if (it != mPrimaryAddressMap.end()) {
        LOG_ERROR("The primary address to add already exists.");
        return BSS_ERR;
    }
    primaryAddress->SetPersistentAddress(persistentAddress);
    primaryAddress->AddReference(refCount);
    mPrimaryAddressMap.emplace(primaryAddress->GetFileAddress(), primaryAddress);
    return BSS_OK;
}

}  // namespace bss
}  // namespace ock