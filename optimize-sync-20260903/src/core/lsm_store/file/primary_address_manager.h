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

#ifndef BOOST_SS_PRIMARY_ADDRESS_MANAGER_H
#define BOOST_SS_PRIMARY_ADDRESS_MANAGER_H

#include <memory>
#include <unordered_map>

#include "lsm_store/file/file_manager.h"
#include "primary_address.h"

namespace ock {
namespace bss {
class PrimaryAddressManager {
public:
    PrimaryAddressManager(const FileManagerRef &localFileManager, const FileManagerRef &dfsFileManager)
        : mLocalFileManager(localFileManager), mDfsFileManager(dfsFileManager)
    {
    }

    ~PrimaryAddressManager()
    {
        std::unordered_map<uint64_t, PrimaryAddressRef>().swap(mPrimaryAddressMap);
    }

    uint32_t AddPrimaryAddressRef(PrimaryAddressRef &primaryAddress, uint32_t refCount);

    void DecPrimaryAddressRef(uint64_t primaryAddress);

    FileInfoRef GetPrimaryFileInfo(uint64_t primaryAddress);

    PrimaryAddressRef GetPrimaryAddress(uint64_t primaryAddress);

    BResult AddPrimaryAddressWithPersistentAddress(const PrimaryAddressRef &primaryAddress,
                                                   const PersistentAddressRef &persistentAddress, uint32_t refCount);

private:
    FileManagerRef mLocalFileManager = nullptr;
    FileManagerRef mDfsFileManager = nullptr;
    std::unordered_map<uint64_t, PrimaryAddressRef> mPrimaryAddressMap;
    mutable std::mutex mMutex;
};
using PrimaryAddressManagerRef = std::shared_ptr<PrimaryAddressManager>;

}  // namespace bss
}  // namespace ock
#endif  // BOOST_SS_PRIMARY_ADDRESS_MANAGER_H