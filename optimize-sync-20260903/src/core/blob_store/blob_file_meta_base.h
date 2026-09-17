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

#ifndef BOOST_SS_BLOB_FILE_META_BASE_H
#define BOOST_SS_BLOB_FILE_META_BASE_H

#include <memory>

#include "common/util/file_address_utils.h"
#include "lsm_store/file/file_cache_type.h"
#include "lsm_store/file/file_meta_base.h"
#include "lsm_store/file/group_range.h"

namespace ock {
namespace bss {
class BlobFileMetaBase : public FileMetaBase {
public:
    BlobFileMetaBase() = default;

    BlobFileMetaBase(const std::string &identifier, const GroupRangeRef &validGroupRange,
                     const GroupRangeRef &coveredGroupRange, uint32_t fileId)
        : FileMetaBase(identifier, FileAddressUtils::GetFileAddressWithZeroOffset(fileId)),
          mValidGroupRange(validGroupRange),
          mCoveredGroupRange(coveredGroupRange)
    {
    }

    inline uint64_t GetMinBlobId() const
    {
        return mMinBlobId;
    }

    inline uint64_t GetMaxBlobId() const
    {
        return mMaxBlobId;
    }

    inline uint32_t DecRef()
    {
        if (mRefCount.load() == 0) {
            return 0;
        }
        return mRefCount.fetch_sub(1) - 1;
    }

    inline uint32_t IncRef()
    {
        return mRefCount.fetch_add(1) + 1;
    }

    inline const GroupRangeRef &GetValidGroupRange() const
    {
        return mValidGroupRange;
    }

    inline FileStatus GetFileStatus() const
    {
        return mFileStatus;
    }

    inline void SetValidGroupRange(GroupRangeRef &groupRange)
    {
        mValidGroupRange = groupRange;
    }

    inline const GroupRangeRef &GetCoveredGroupRange() const
    {
        return mCoveredGroupRange;
    }

protected:
    uint64_t mMinBlobId = UINT64_MAX;
    uint64_t mMaxBlobId = 0;
    GroupRangeRef mValidGroupRange = nullptr;
    GroupRangeRef mCoveredGroupRange = nullptr;
    std::atomic<uint32_t> mRefCount{ 1 };
    FileStatus mFileStatus = LOCAL;
};

using BlobFileMetaBaseRef = std::shared_ptr<BlobFileMetaBase>;
}  // namespace bss
}  // namespace ock

#endif