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

#include "index_block_writer.h"

#include "binary/lsm_binary.h"
#include "block_meta.h"
#include "common/util/var_encoding_util.h"
#include "lsm_store/key/full_key_util.h"

namespace ock {
namespace bss {
BResult IndexBlockWriter::Finish(ByteBufferRef &byteBuffer)
{
    uint32_t numBlocks = mKeyValueOffset.size();
    if (UNLIKELY(numBlocks == 0)) {
        LOG_DEBUG("Key value offset size is zero");
        auto addr = FileMemAllocator::Alloc(mMemManager, mHolder, NO_16, __FUNCTION__);
        if (UNLIKELY(addr == 0)) {
            return BSS_ALLOC_FAIL;
        }
        byteBuffer = MakeRef<ByteBuffer>(reinterpret_cast<uint8_t *>(addr), NO_16, mMemManager);
        if (UNLIKELY(byteBuffer == nullptr)) {
            mMemManager->ReleaseMemory(addr);
            LOG_ERROR("Make ref failed, byteBuffer is null.");
            return BSS_ALLOC_FAIL;
        }
        return BSS_OK;
    }
    BResult retVal;
    uint32_t numBytesForEndKeyIndex = FullKeyUtil::MinNumBytesToRepresentValue(mKeyValueOffset.at(numBlocks - 1));
    for (uint32_t i = 0; i < numBlocks; i++) {
        retVal = FullKeyUtil::WriteValueWithNumberOfBytes(mKeyValueOffset.at(i), numBytesForEndKeyIndex,
                                                          mBufferOutputView);
        RETURN_NOT_OK_NO_LOG(retVal);
    }
    retVal = mBufferOutputView->WriteUint8(numBytesForEndKeyIndex);
    RETURN_NOT_OK_NO_LOG(retVal);
    if (mHashIndexBuilder != nullptr) {
        retVal = mHashIndexBuilder->Build(mBufferOutputView);
        RETURN_NOT_OK_NO_LOG(retVal);
    }
    retVal = mBufferOutputView->WriteUint32(numBlocks);
    RETURN_NOT_OK_NO_LOG(retVal);

    byteBuffer = MakeRef<ByteBuffer>(mBufferOutputView->Data(), mBufferOutputView->GetOffset());
    RETURN_INVALID_PARAM_AS_NULLPTR(byteBuffer);
    return BSS_OK;
}

void IndexBlockWriter::Reset()
{
    mKeyValueOffset.clear();
    mEstimateSize = GetMetaSize();
    mBufferOutputView->SetOffset(0);
    mHashIndexBuilder->Reset();
}

BResult IndexBlockWriter::Add(const BlockMetaRef &blockMeta)
{
    auto endKey = blockMeta->GetEndKey();
    auto blockHandle = blockMeta->GetBlockHandle();

    uint32_t currentOffset = mBufferOutputView->GetOffset();
    mKeyValueOffset.emplace_back(currentOffset);
    auto retVal = FullKeyUtil::WriteInternalKey(endKey, mBufferOutputView);
    RETURN_NOT_OK_NO_LOG(retVal);
    retVal = VarEncodingUtil::EncodeUnsignedInt(blockHandle->GetOffset(), mBufferOutputView);
    RETURN_NOT_OK_NO_LOG(retVal);
    retVal = VarEncodingUtil::EncodeUnsignedInt(blockHandle->GetSize(), mBufferOutputView);
    RETURN_NOT_OK_NO_LOG(retVal);
    if (mHashIndexBuilder != nullptr) {
        mHashIndexBuilder->Add(blockMeta->GetStartKey()->KeyHashCode(), endKey->KeyHashCode());
    }
    mEstimateSize += mBufferOutputView->GetOffset() - currentOffset + sizeof(currentOffset);
    return BSS_OK;
}

}  // namespace bss
}  // namespace ock