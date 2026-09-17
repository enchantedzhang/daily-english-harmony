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

#include "blob_index_block_writer.h"

namespace ock {
namespace bss {
BResult BlobIndexBlockWriter::WriteIndexBlock(const ByteBufferRef &byteBuffer)
{
    RETURN_ERROR_AS_NULLPTR(byteBuffer);
    std::vector<BlobDataBlockMetaRef> blobMetas = GetBlobMetas();
    for (auto &blob : blobMetas) {
        RETURN_ERROR_AS_NULLPTR(blob);
        RETURN_NOT_OK(
            byteBuffer->Write(reinterpret_cast<const uint8_t *>(&blob->mMinBlobId), sizeof(blob->mMinBlobId)));
        RETURN_NOT_OK(
            byteBuffer->Write(reinterpret_cast<const uint8_t *>(&blob->mMaxBlobId), sizeof(blob->mMaxBlobId)));
        BlockHandle blockHandle = blob->GetBlockHandle();
        uint32_t offset = blockHandle.GetOffset();
        uint32_t size = blockHandle.GetSize();
        RETURN_NOT_OK(byteBuffer->Write(reinterpret_cast<const uint8_t *>(&offset), sizeof(offset)));
        RETURN_NOT_OK(byteBuffer->Write(reinterpret_cast<const uint8_t *>(&size), sizeof(size)));
    }
    auto size = static_cast<uint32_t>(blobMetas.size());
    RETURN_NOT_OK(byteBuffer->Write(reinterpret_cast<const uint8_t *>(&size), sizeof(size)));
    return BSS_OK;
}

BlobDataBlockMetaRef BlobIndexBlockWriter::SelectDataBlockMeta(uint64_t blobId)
{
    std::vector<BlobDataBlockMetaRef> blobMetas = GetBlobMetas();
    if (UNLIKELY(blobMetas.empty())) {
        return nullptr;
    }
    uint32_t low = 0;
    uint32_t high = blobMetas.size() - NO_1;
    while (low <= high) {
        uint32_t mid = low + ((high - low) >> NO_1);
        if (UNLIKELY(mid >= blobMetas.size())) {
            LOG_ERROR("Index out of bound, mid: " << mid << ", size: " << blobMetas.size());
            return nullptr;
        }
        auto blockMeta = blobMetas[mid];
        RETURN_NULLPTR_AS_NULLPTR(blockMeta);
        if (blobId < blockMeta->mMinBlobId) {
            high = mid - NO_1;
            continue;
        }
        if (blobId > blockMeta->mMaxBlobId) {
            low = mid + NO_1;
            continue;
        }
        return blockMeta;
    }
    LOG_ERROR("Can’t find data block meta, blobId: " << blobId);
    return nullptr;
}

}  // namespace bss
}  // namespace ock