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

#ifndef BOOST_SS_INDEX_BLOCK_WRITER_H
#define BOOST_SS_INDEX_BLOCK_WRITER_H

#include <memory>
#include <vector>

#include "block_meta.h"
#include "common/block.h"
#include "common/io/output_view.h"
#include "index_block_hash_index_writer.h"
#include "lsm_store/key/full_key_util.h"

namespace ock {
namespace bss {
class BlockHandleIterator;
using BlockHandleIteratorRef = Ref<BlockHandleIterator>;
class IndexBlock;
using IndexBlockRef = std::shared_ptr<IndexBlock>;

class IndexBlockWriter {
public:
    IndexBlockWriter(float loadRatio, const MemManagerRef &memManager, FileProcHolder holder)
        : mEstimateSize(GetMetaSize()), mMemManager(memManager), mHolder(holder)
    {
        mHashIndexBuilder = std::make_shared<IndexBlockHashIndexWriter>(loadRatio);
        mBufferOutputView = std::make_shared<OutputView>(mMemManager, holder);
    }

    inline uint32_t GetNumBlocks() const
    {
        return mKeyValueOffset.size();
    }

    inline uint32_t EstimateSizeAfterAdd(const FullKeyRef &fullKey)
    {
        return CurrentEstimateSize() + IncrementalSize(fullKey);
    }

    inline uint32_t CurrentEstimateSize()
    {
        return IsEmpty() ? 0 : (mEstimateSize + mHashIndexBuilder->CurrentEstimateSize());
    }

    inline uint32_t IncrementalSize(const FullKeyRef &fullKey)
    {
        return fullKey->GetFullKeyLen() + NO_12 + mHashIndexBuilder->IncrementalSize();
    }

    inline bool IsEmpty() const
    {
        return mKeyValueOffset.empty();
    }

    BResult Finish(ByteBufferRef &byteBuffer);

    void Reset();

    BResult Add(const BlockMetaRef &blockMeta);

private:
    inline uint32_t GetMetaSize() const
    {
        return NO_10;
    }

    IndexBlockHashIndexWriterRef mHashIndexBuilder;
    OutputViewRef mBufferOutputView;
    std::vector<uint32_t> mKeyValueOffset;
    uint32_t mEstimateSize;
    MemManagerRef mMemManager;
    FileProcHolder mHolder;
};
using IndexBlockWriterRef = std::shared_ptr<IndexBlockWriter>;

class IndexBlock : public Block {
public:
    explicit IndexBlock(ByteBufferRef &buffer, const MemManagerRef &memManager, FileProcHolder holder)
        : Block(buffer), mMemManager(memManager), mHolder(holder)
    {
    }

    ~IndexBlock() override = default;

    BlockType GetBlockType() override
    {
        return BlockType::INDEX;
    }

private:
    MemManagerRef mMemManager;
    FileProcHolder mHolder;
};
using IndexBlockRef = std::shared_ptr<IndexBlock>;

}  // namespace bss
}  // namespace ock
#endif  // BOOST_SS_INDEX_BLOCK_WRITER_H