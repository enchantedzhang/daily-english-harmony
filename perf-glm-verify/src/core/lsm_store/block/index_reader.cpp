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

#include "index_reader.h"

#include "binary/lsm_binary.h"

namespace ock {
namespace bss {

uint32_t IndexReader::GetBlockIndex(const Key &key, const ByteBufferRef &buffer)
{
    uint32_t startBlockIndex;
    uint32_t numBlocks;
    if (mHashIndexReader != nullptr) {
        uint64_t bucketValue = mHashIndexReader->GetBucket(key.PriKey().KeyHashCode(), buffer);
        if (bucketValue == UINT64_MAX) {
            return INVALID_U32_INDEX;
        }
        startBlockIndex = static_cast<uint32_t>(bucketValue >> NO_32);
        numBlocks = static_cast<uint32_t>(bucketValue);
    } else {
        startBlockIndex = 0;
        numBlocks = mNumDataBlocks;
    }
    uint32_t endBlockIndex = startBlockIndex + numBlocks;
    return BinarySearchFirstBlockNoLessThan(key, startBlockIndex, endBlockIndex, buffer);
}

uint32_t IndexReader::BinarySearchFirstBlockNoLessThan(const Key &lookupKey, uint32_t startBlockIndex,
                                                       uint32_t endBlockIndex, const ByteBufferRef &buffer)
{
    uint32_t leftBlockIndex = startBlockIndex;
    uint32_t rightBlockIndex = endBlockIndex;
    uint32_t midBlockIndex;
    uint32_t offset;
    int32_t cmp;
    while (leftBlockIndex < rightBlockIndex) {
        midBlockIndex = leftBlockIndex + (rightBlockIndex - leftBlockIndex) / NO_2;
        offset = GetEndKeyOffsetAt(midBlockIndex, buffer);
        // For only one secondary key:  | PriHashCode | PriKeyLen | PriKeyData| StateId |SecHashCode | SecKeyLen |
        // SecKeyData
        LsmKeyValueInfo keyValueInfo;
        BResult result = keyValueInfo.UnpackToSglSecKey(buffer, offset);
        if (result != BSS_OK) {
            LOG_ERROR("unpack kv info failed, block index:" << midBlockIndex << ", offset: " << offset);
            rightBlockIndex = midBlockIndex;
            continue;
        }
        cmp = keyValueInfo.Compare(lookupKey);
        if (cmp >= 0) {
            rightBlockIndex = midBlockIndex;
            continue;
        }
        leftBlockIndex = midBlockIndex + 1;
    }
    return (rightBlockIndex == endBlockIndex) ? INVALID_U32_INDEX : rightBlockIndex;
}

BResult IndexReader::GetDataBlock(const Key &key, BlockHandle &blockHandle)
{
    ByteBufferRef indexBlockBuffer = GetIndexBlockBuffer();
    RETURN_ERROR_AS_NULLPTR(indexBlockBuffer);
    uint32_t blockIndex = GetBlockIndex(key, indexBlockBuffer);
    return blockIndex == INVALID_U32_INDEX ? BSS_NOT_EXISTS : GetBlockAt(blockIndex, blockHandle, indexBlockBuffer);
}

BResult IndexReader::GetBlockAt(uint32_t index, BlockHandle &blockHandle, const ByteBufferRef &buffer)
{
    uint32_t offset = GetEndKeyOffsetAt(index, buffer);
    uint32_t handleOffset = offset + FullKeyUtil::ComputeRawInternalKeyLen(buffer, offset);
    uint64_t blockOffsetDecode = VarEncodingUtil::DecodeUnsignedInt(buffer, handleOffset);
    uint64_t blockSizeDecode =
        VarEncodingUtil::DecodeUnsignedInt(buffer, handleOffset + static_cast<uint32_t>(blockOffsetDecode >> NO_32));
    blockHandle.Fill(VarEncodingUtil::GetDecodedValue(blockOffsetDecode),
                     VarEncodingUtil::GetDecodedValue(blockSizeDecode));

    return BSS_OK;
}

BlockHandleIteratorRef IndexReader::SubIterator(const Key &startKey, const Key &endKey, bool reverseOrder)
{
    ByteBufferRef indexBlockBuffer = GetIndexBlockBuffer();
    RETURN_NULLPTR_AS_NULLPTR(indexBlockBuffer);
    uint32_t startIndex = GetFirstBlockIndexNoLessThan(startKey, indexBlockBuffer);
    if (startIndex == INVALID_U32_INDEX) {
        return nullptr;
    }
    uint32_t endIndex = GetFirstBlockIndexNoLessThan(endKey, indexBlockBuffer);
    if (endIndex == INVALID_U32_INDEX) {
        endIndex = mNumDataBlocks == 0 ? 0 : mNumDataBlocks - 1;
    }
    return MakeRef<BlockHandleIterator>(startIndex, endIndex, reverseOrder, shared_from_this(), indexBlockBuffer);
}

BlockHandleIteratorRef IndexReader::IteratorInner(bool reverseOrder)
{
    ByteBufferRef indexBlockBuffer = GetIndexBlockBuffer();
    RETURN_NULLPTR_AS_NULLPTR(indexBlockBuffer);
    uint32_t endIndex = mNumDataBlocks == 0 ? 0 : mNumDataBlocks - 1;
    return MakeRef<BlockHandleIterator>(0, endIndex, reverseOrder, shared_from_this(), indexBlockBuffer);
}

bool BlockHandleIterator::HasNext()
{
    if (UNLIKELY(mNextIndex == INVALID_U32_INDEX)) {
        return false;
    }
    return mReverseOrder ? mNextIndex >= mStartIndex : mNextIndex <= mEndIndex;
}

BlockHandleRef BlockHandleIterator::Next()
{
    if (UNLIKELY(!HasNext())) {
        return nullptr;
    }
    uint32_t index = mNextIndex;
    mNextIndex = mReverseOrder ? mNextIndex - 1 : mNextIndex + 1;
    BlockHandle blockHandle;
    RETURN_NULLPTR_AS_NULLPTR(mIndexBlockBuffer);
    mIndexReader->GetBlockAt(index, blockHandle, mIndexBlockBuffer);
    return std::make_shared<BlockHandle>(blockHandle);
}
}  // namespace bss
}  // namespace ock