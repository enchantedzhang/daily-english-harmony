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

#include "blob_index_block.h"

namespace ock {
namespace bss {
BlobDataBlockMetaRef BlobIndexBlock::SelectDataBlockMeta(uint64_t blobId)
{
    uint32_t low = 0;
    if (UNLIKELY(mDataBlockNums == 0)) {
        LOG_ERROR("Select data block meta failed, nums is zero.");
        return nullptr;
    }
    uint32_t high = mDataBlockNums - NO_1;
    RETURN_NULLPTR_AS_NULLPTR(mBuffer);
    while (low <= high) {
        uint32_t mid = low + ((high - low) >> NO_1);
        uint32_t midOffset = NO_24 * mid;
        uint64_t minBlobId = 0;
        uint64_t maxBlobId = 0;
        RETURN_NULLPTR_AS_NOT_OK(mBuffer->ReadUint64(minBlobId, midOffset));
        RETURN_NULLPTR_AS_NOT_OK(mBuffer->ReadUint64(maxBlobId, midOffset + sizeof(uint64_t)));
        if (blobId < minBlobId) {
            high = mid - NO_1;
            continue;
        }
        if (blobId > maxBlobId) {
            low = mid + NO_1;
            continue;
        }
        uint32_t offset = 0;
        uint32_t size = 0;
        uint32_t blockHandleOffset = midOffset + sizeof(uint64_t) * NO_2;
        RETURN_NULLPTR_AS_NOT_OK(mBuffer->ReadUint32(offset, blockHandleOffset));
        RETURN_NULLPTR_AS_NOT_OK(mBuffer->ReadUint32(size, blockHandleOffset + sizeof(uint32_t)));
        BlockHandle blockHandle = { offset, size };
        return std::make_shared<BlobDataBlockMeta>(minBlobId, maxBlobId, blockHandle);
    }
    return nullptr;
}

BlobDataBlockMetaRef BlobIndexBlock::GetDataBlockMeta(uint32_t index)
{
    uint32_t baseOffset = BLOB_DATA_BLOCK_META_STRUCT_SIZE * index;
    uint64_t minBlobId = 0;
    uint64_t maxBlobId = 0;
    RETURN_NULLPTR_AS_NULLPTR(mBuffer);
    RETURN_NULLPTR_AS_NOT_OK(mBuffer->ReadUint64(minBlobId, baseOffset));
    RETURN_NULLPTR_AS_NOT_OK(mBuffer->ReadUint64(maxBlobId, baseOffset + sizeof(uint64_t)));
    uint32_t offset = 0;
    uint32_t size = 0;
    uint32_t blockHandleOffset = baseOffset + sizeof(uint64_t) * NO_2;
    RETURN_NULLPTR_AS_NOT_OK(mBuffer->ReadUint32(offset, blockHandleOffset));
    RETURN_NULLPTR_AS_NOT_OK(mBuffer->ReadUint32(size, blockHandleOffset + sizeof(uint32_t)));
    BlockHandle blockHandle = { offset, size };
    return std::make_shared<BlobDataBlockMeta>(minBlobId, maxBlobId, blockHandle);
}
}  // namespace bss
}  // namespace ock