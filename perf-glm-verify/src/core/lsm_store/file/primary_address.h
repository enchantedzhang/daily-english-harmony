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

#ifndef BOOST_SS_PRIMARY_ADDRESS_H
#define BOOST_SS_PRIMARY_ADDRESS_H

#include <cstdint>
#include <memory>
#include <string>

#include "lsm_store/file/file_cache_type.h"

namespace ock {
namespace bss {
class PersistentAddress {
public:
    PersistentAddress(uint64_t fileAddress, uint32_t fileLength, std::string identifier, bool currentManaged)
        : mFileAddress(fileAddress), mFileLength(fileLength), mIdentifier(identifier), mCurrentManaged(currentManaged)
    {
    }

    inline uint64_t GetFileAddress() const
    {
        return mFileAddress;
    }

    inline uint32_t GetFileLength() const
    {
        return mFileLength;
    }

    inline std::string GetIdentifier() const
    {
        return mIdentifier;
    }

    inline bool IsCurrentManaged() const
    {
        return mCurrentManaged;
    }

    inline bool Equals(const std::shared_ptr<PersistentAddress> &other) const
    {
        return mFileAddress == other->GetFileAddress() && mFileLength == other->GetFileLength() &&
               mIdentifier == other->GetIdentifier() && mCurrentManaged == other->IsCurrentManaged();
    }

private:
    uint64_t mFileAddress = 0;
    uint32_t mFileLength = 0;
    std::string mIdentifier;
    bool mCurrentManaged = false;
};
using PersistentAddressRef = std::shared_ptr<PersistentAddress>;

extern PersistentAddressRef PERSISTENT_ADDRESS_NONE;
class PrimaryAddress {
public:
    PrimaryAddress(uint64_t fileAddress, uint32_t fileLength, std::string identifier, FileStatus fileStatus)
        : mFileAddress(fileAddress),
          mFileLength(fileLength),
          mIdentifier(identifier),
          mFileStatus(fileStatus),
          mPersistentAddress(PERSISTENT_ADDRESS_NONE)
    {
    }

    inline uint64_t GetFileAddress() const
    {
        return mFileAddress;
    }

    inline uint32_t GetFileLength() const
    {
        return mFileLength;
    }

    inline std::string GetIdentifier() const
    {
        return mIdentifier;
    }

    inline FileStatus GetFileStatus() const
    {
        return mFileStatus;
    }

    inline PersistentAddressRef GetPersistentAddress() const
    {
        return mPersistentAddress;
    }

    inline uint32_t GetReference() const
    {
        return mReference;
    }

    inline void AddReference(uint32_t reference)
    {
        mReference += reference;
    }

    inline uint32_t DecReference(uint32_t ref)
    {
        mReference -= ref;
        return mReference;
    }

    inline void SetPersistentAddress(const PersistentAddressRef &persistentAddress)
    {
        mPersistentAddress = persistentAddress;
    }

private:
    uint64_t mFileAddress = 0;
    uint32_t mFileLength = 0;
    std::string mIdentifier;
    FileStatus mFileStatus;
    uint32_t mReference = 0;
    PersistentAddressRef mPersistentAddress = nullptr;
};
using PrimaryAddressRef = std::shared_ptr<PrimaryAddress>;

}  // namespace bss
}  // namespace ock
#endif  // BOOST_SS_PRIMARY_ADDRESS_H