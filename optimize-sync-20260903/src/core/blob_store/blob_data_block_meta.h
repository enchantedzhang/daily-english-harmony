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

#ifndef BLOB_DATA_BLOCK_META_H
#define BLOB_DATA_BLOCK_META_H
#include <cstdint>

#include "block/block_handle.h"

namespace ock {
namespace bss {
class BlobDataBlockMeta {
public:
    ~BlobDataBlockMeta() = default;
    BlobDataBlockMeta() = default;
    BlobDataBlockMeta(uint64_t minBlobId, uint64_t maxBlobId, const BlockHandle &blockHandle)
        : mMinBlobId(minBlobId), mMaxBlobId(maxBlobId), mBlockHandle(blockHandle)
    {
    }

    inline void SetBlockHandle(const BlockHandle &blockHandle)
    {
        mBlockHandle = blockHandle;
    }

    inline BlockHandle &GetBlockHandle()
    {
        return mBlockHandle;
    }

    inline void SetBlobNum(uint32_t blobNum)
    {
        mBlobBlockNum = blobNum;
    }

    inline void UpdateExpireTime(uint64_t expireTime)
    {
        mMinExpireTime = std::min(expireTime, mMinExpireTime);
        mMaxExpireTime = std::max(expireTime, mMaxExpireTime);
    }

public:
    uint64_t mMinBlobId = UINT64_MAX;
    uint64_t mMaxBlobId = 0;
    uint64_t mMinExpireTime = UINT64_MAX;
    uint64_t mMaxExpireTime = 0;
    BlockHandle mBlockHandle;
    uint32_t mBlobBlockNum = 0;
};
using BlobDataBlockMetaRef = std::shared_ptr<BlobDataBlockMeta>;
}  // namespace bss
}  // namespace ock

#endif