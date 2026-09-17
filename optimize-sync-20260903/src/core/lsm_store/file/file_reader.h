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

#ifndef BOOST_SS_FILE_READER_H
#define BOOST_SS_FILE_READER_H

#include <memory>

#include "common/path.h"
#include "common/util/iterator.h"
#include "file_address_util.h"
#include "file_iterator.h"
#include "file_meta_data.h"
#include "file_meta_index_block_writer.h"
#include "file_reader_base.h"
#include "include/config.h"
#include "lsm_store/block/data_block.h"
#include "lsm_store/block/filter_block_writer.h"
#include "lsm_store/block/index_reader.h"
#include "lsm_store/key/full_key_filter.h"

namespace ock {
namespace bss {
class FileReader : public FileReaderBase, public std::enable_shared_from_this<FileReader> {
public:
    FileReader(const FileMetaDataRef &fileMetaData, const ConfigRef &config, const PathRef &path,
               const BlockCacheRef &blockCache, const MemManagerRef &memManager, FileProcHolder holder)
        : FileReaderBase(fileMetaData, config, path, blockCache, memManager, holder)
    {
        LOG_DEBUG("Create FileReader success.");
    }

    ~FileReader() override
    {
        LOG_DEBUG("Delete FileReader success.");
    }

    BResult Get(const Key &key, Value &value) override;

    FilterBlockRef GetOrLoadFilterBlock() override;

    IndexBlockRef GetOrLoadIndexBlock() override;

    BlockRef GetOrLoadBlock(const BlockHandle &blockHandle, BlockType blockType);

    BResult Initialize() override;

    void Close() override
    {
        if (LIKELY(mFilterBlockHandle != nullptr)) {
            uint64_t filterBlockId = GetBlockId(*mFilterBlockHandle);
            mBlockCache->Remove(filterBlockId, BlockType::FILTER);
        }
        if (LIKELY(mIndexBlockHandle != nullptr)) {
            uint64_t indexBlockId = GetBlockId(*mIndexBlockHandle);
            mBlockCache->Remove(indexBlockId, BlockType::INDEX);
        }
        mIndexReader = nullptr;
    }

    KeyValueIteratorRef IteratorAll(FullKeyFilterRef internalKeyFilter, bool shareReader) override
    {
        if (UNLIKELY(!mInitialized)) {
            return nullptr;
        }
        RETURN_NULLPTR_AS_NULLPTR(mIndexReader);
        IteratorRef<BlockHandleRef> blockHandleIterator = mIndexReader->IteratorInner(false);
        RETURN_NULLPTR_AS_NULLPTR(blockHandleIterator);
        return std::make_shared<FileIterator>(shared_from_this(), internalKeyFilter, blockHandleIterator, mMemManager,
                                              mHolder, shareReader);
    }

    KeyValueIteratorRef PrefixIterator(FullKeyFilterRef internalKeyFilter, const Key &prefixKey,
                                       bool reverseOrder) override
    {
        if (UNLIKELY(!mInitialized)) {
            return nullptr;
        }
        Key endKey(prefixKey.PriKey(), nullptr, END_KEY_FLAG);
        return InternalSubIterator(internalKeyFilter, prefixKey, endKey, reverseOrder);
    }

    BlockRef GetOrLoadDataBlock(const BlockHandle &blockHandle) override;

private:
    BlockRef BuildBlock(const ByteBufferRef &buffer, BlockType blockType);

    BlockRef BuildFilterBlock(ByteBufferRef byteBuffer) override;

    BlockRef BuildDataBlock(ByteBufferRef byteBuffer) override;

    BlockRef BuildIndexBlock(ByteBufferRef byteBuffer) override;

    BResult InitializeFooterAndMetaBlock();

    FilterBlockRef GetFilterBlock();

    IndexBlockRef GetIndexBlock();

    BResult InitializeFilterHandle(const FileMetaIndexRef &fileMetaIndex);

    BResult InitializeIndexHandle(const FileMetaIndexRef &fileMetaIndex);

    BResult InitializeIndexReader();

    KeyValueIteratorRef InternalSubIterator(const FullKeyFilterRef &internalKeyFilter, const Key &startKey,
                                            const Key &endKey, bool reverseOrder)
    {
        RETURN_NULLPTR_AS_NULLPTR(mIndexReader);
        auto blockHandleIterator = mIndexReader->SubIterator(startKey, endKey, reverseOrder);
        if (blockHandleIterator == nullptr) {
            return nullptr;
        }
        return std::make_shared<FileSubIterator>(shared_from_this(), internalKeyFilter, blockHandleIterator, startKey,
                                                 endKey, reverseOrder);
    }

private:
    IndexReaderRef mIndexReader;
    BlockHandleRef mFilterBlockHandle;
    BlockHandleRef mIndexBlockHandle;
    FilterBlockRef mFilterBlock = nullptr;
    IndexBlockRef mIndexBlock = nullptr;
};
}  // namespace bss
}  // namespace ock
#endif  // BOOST_SS_FILE_READER_H