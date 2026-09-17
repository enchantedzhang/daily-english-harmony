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

#ifndef BLOB_FILE_READER_H
#define BLOB_FILE_READER_H
#include <bss_err.h>

#include "blob_data_block.h"
#include "blob_file_meta.h"
#include "block.h"
#include "block/block_cache.h"
#include "block/block_handle.h"
#include "file/file_address_util.h"
#include "file/file_cache_type.h"
#include "file/file_meta_data.h"
#include "io/file_input_view.h"

namespace ock {
namespace bss {
using BlockBuilderFunc = std::function<BlockRef(ByteBufferRef &byteBuffer)>;
class BlobFileReader {
public:
    BResult Init(const BlobFileMetaRef &fileMetaData, const ConfigRef &config, const PathRef &path,
                 const BlockCacheRef &blockCache, const MemManagerRef &memManager)
    {
        mFileMetaData = fileMetaData;
        mConfig = config;
        mPath = path;
        mBlockCache = blockCache;
        mMemManager = memManager;
        mInputView = std::make_shared<FileInputView>();
        RETURN_NOT_OK(mInputView->Init(static_cast<FileSystemType>(mFileStatus), mPath));
        mInit.store(true);
        RETURN_ERROR_AS_NULLPTR(mFileMetaData);
        mInitOffset = FileAddressUtil::GetFileOffset(mFileMetaData->GetFileAddress());
        return BSS_OK;
    }

    void Close()
    {
        mInit.store(false);
        if (mInputView != nullptr) {
            mInputView->Close();
            mInputView = nullptr;
        }
    }

    inline void SetFileStatus(FileStatus fileStatus)
    {
        mFileStatus = fileStatus;
    }

    uint64_t GetBlockId(const BlockHandle &blockHandle) const
    {
        return static_cast<uint64_t>(FileAddressUtil::GetFileId(mFileMetaData->GetFileAddress())) << NO_32 |
               (mInitOffset + blockHandle.GetOffset());
    }

    uint32_t GetBlockHandleOffset(const BlockHandle &blockHandle) const
    {
        return mInitOffset + blockHandle.GetOffset();
    }

    BlockRef GetOrLoadBlock(const BlockHandle &blockHandle, BlockType blockType, BlockBuilderFunc dataBuilder);

    BlockRef FetchBlock(const BlockHandle &blockHandle, BlockType blockType, BlockBuilderFunc dataBuilder);

    BResult ReadBlock(const BlockHandle &blockHandle, ByteBufferRef &byteBuffer);

    BResult DecompressBlock(ByteBufferRef &outputBuffer, uint32_t originLength, CompressAlgo compressAlgo);

    ByteBufferRef CreateBuffer(uint32_t size);

    BResult ReadBuffer(uint32_t offset, uint32_t length, ByteBufferRef &buffer);

private:
    PathRef mPath = nullptr;
    FileStatus mFileStatus = LOCAL;
    FileInputViewRef mInputView = nullptr;
    BlobFileMetaRef mFileMetaData = nullptr;
    BlockCacheRef mBlockCache = nullptr;
    uint32_t mInitOffset = 0;
    ConfigRef mConfig = nullptr;
    MemManagerRef mMemManager = nullptr;
    std::atomic<bool> mInit{ false };
};
using BlobFileReaderRef = std::shared_ptr<BlobFileReader>;
}  // namespace bss
}  // namespace ock

#endif