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

#ifndef BLOB_INDEX_BLOCK_WRITER_H
#define BLOB_INDEX_BLOCK_WRITER_H
#include <vector>

#include "binary/byte_buffer.h"
#include "blob_data_block_meta.h"
#include "bss_err.h"
#include "bss_types.h"
#include "util/bss_lock.h"

namespace ock {
namespace bss {
class BlobIndexBlockWriter {
public:
    void AddBlob(const BlobDataBlockMetaRef &blobMeta)
    {
        WriteLocker<ReadWriteLock> lock(&mRwLock);
        mBlobs.emplace_back(blobMeta);
    }

    BResult WriteIndexBlock(const ByteBufferRef &byteBuffer);

    inline uint32_t EstimateSize()
    {
        ReadLocker<ReadWriteLock> lock(&mRwLock);
        if (UNLIKELY(UINT32_MAX - sizeof(uint32_t) < BLOB_DATA_BLOCK_META_STRUCT_SIZE * mBlobs.size())) {
            LOG_ERROR("The value crosses the boundary and exceeds the UINT32_MAX maximum, blob vec size: "
                      << mBlobs.size() << ", block meta size: " << NO_24);
            return UINT32_MAX;
        }
        return BLOB_DATA_BLOCK_META_STRUCT_SIZE * mBlobs.size() + sizeof(uint32_t);
    }

    // 拷贝防止Blobs vector发生变化
    std::vector<BlobDataBlockMetaRef> GetBlobMetas()
    {
        ReadLocker<ReadWriteLock> lock(&mRwLock);
        return mBlobs;
    }

    BlobDataBlockMetaRef SelectDataBlockMeta(uint64_t blobId);

private:
    std::vector<BlobDataBlockMetaRef> mBlobs;
    ReadWriteLock mRwLock;
};
using BlobIndexBlockWriterRef = std::shared_ptr<BlobIndexBlockWriter>;
}  // namespace bss
}  // namespace ock

#endif
