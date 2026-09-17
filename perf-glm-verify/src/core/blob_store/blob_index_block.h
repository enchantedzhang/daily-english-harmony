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

#ifndef BLOB_INDEX_BLOCK_H
#define BLOB_INDEX_BLOCK_H
#include "blob_data_block_meta.h"
#include "block.h"

namespace ock {
namespace bss {

class BlobIndexBlock : public Block {
public:
    BlobIndexBlock(ByteBufferRef &byteBuffer, uint32_t dataBlockNums) : Block(byteBuffer), mDataBlockNums(dataBlockNums)
    {
    }

    BlockType GetBlockType() override
    {
        return BlockType::BLOB;
    }

    BlobDataBlockMetaRef SelectDataBlockMeta(uint64_t blobId);

    BlobDataBlockMetaRef GetDataBlockMeta(uint32_t index);

    inline uint32_t GetDataBlockNum() const
    {
        return mDataBlockNums;
    }

private:
    uint32_t mDataBlockNums = 0;
};
using BlobIndexBlockRef = std::shared_ptr<BlobIndexBlock>;
}  // namespace bss
}  // namespace ock
#endif
