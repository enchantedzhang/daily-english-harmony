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

#include "file_meta_index_block_writer.h"

#include "file_structure.h"

namespace ock {
namespace bss {
BResult FileMetaIndexBlockWriter::Finish(ByteBufferRef &byteBuffer)
{
    if (UNLIKELY(mMetaBlockVec.size() != NO_2)) {
        LOG_ERROR("Invalid meta index block size, size:" << mMetaBlockVec.size());
        return BSS_INNER_ERR;
    }
    RETURN_ERROR_AS_NULLPTR(mOutputView);
    uint32_t totalLen = sizeof(MetaIndexStructure) * NO_2;
    auto retVal = mOutputView->Reverse(totalLen);
    RETURN_NOT_OK_NO_LOG(retVal);
    for (const auto &item : mMetaBlockVec) {
        RETURN_NOT_OK_NO_LOG(mOutputView->WriteUint32(item->GetOffset()));
        RETURN_NOT_OK_NO_LOG(mOutputView->WriteUint32(item->GetSize()));
    }
    byteBuffer = MakeRef<ByteBuffer>(mOutputView->Data(), mOutputView->GetOffset());
    RETURN_INVALID_PARAM_AS_NULLPTR(byteBuffer);
    return BSS_OK;
}

FileMetaIndexRef FileMetaIndex::CreateFileMetaIndex(ByteBufferRef &byteBuffer)
{
    FileMetaIndexRef fileMetaIndex = std::make_shared<FileMetaIndex>();
    static uint8_t metaBlockNum = NO_2;
    uint32_t position = 0;
    for (uint8_t i = 0; i < metaBlockNum; i++) {
        uint32_t metaBlockOffset = 0;
        byteBuffer->ReadUint32(metaBlockOffset, position);
        position += sizeof(uint32_t);
        uint32_t metaBlockSize = 0;
        byteBuffer->ReadUint32(metaBlockSize, position);
        position += sizeof(uint32_t);
        BlockHandleRef metaBlockMeta = std::make_shared<BlockHandle>(metaBlockOffset, metaBlockSize);
        fileMetaIndex->AddMetaBlock(metaBlockMeta);
    }
    return fileMetaIndex;
}

}  // namespace bss
}  // namespace ock