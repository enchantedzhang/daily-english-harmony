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

#include "data_block_index_writer.h"

#include "lsm_store/key/full_key_util.h"

namespace ock {
namespace bss {
uint32_t DataBlockIndexWriter::GetBucketIndex(uint32_t hash, uint32_t bucketNum)
{
    return hash % bucketNum;
}

uint64_t DataBlockIndexWriter::EncodePointer(uint64_t pointer, uint32_t numBitsForSecondaryKeyIndex)
{
    uint64_t primaryKeyIndex = pointer >> NO_32;
    uint64_t secondaryKeyIndex = pointer & 0xFFFFFFFFL;
    return (primaryKeyIndex << numBitsForSecondaryKeyIndex) | secondaryKeyIndex;
}

uint64_t DataBlockIndexWriter::DecodePointer(uint64_t encodedPointer, uint32_t numBitsForSecondaryKeyIndex)
{
    uint64_t primaryKeyIndex = encodedPointer >> numBitsForSecondaryKeyIndex;
    uint64_t secondaryKeyIndex = encodedPointer & ((-1L << numBitsForSecondaryKeyIndex) ^ 0xFFFFFFFFFFFFFFFFULL);
    return (primaryKeyIndex << NO_32) | secondaryKeyIndex;
}

uint32_t DataBlockIndexWriter::GetPrimaryKeyIndexFromPointer(uint64_t pointer)
{
    return static_cast<uint32_t>(pointer >> NO_32);
}

uint32_t DataBlockIndexWriter::GetSecondaryKeyIndexFromPointer(uint64_t pointer)
{
    return static_cast<uint32_t>(pointer);
}

BResult DataBlockIndexWriter::Finish(const OutputViewRef &outputView)
{
    uint32_t numEntries = mEntryPointer.size();
    uint32_t numBuckets = GetBucketNum(numEntries);
    mBucket.clear();
    for (uint32_t i = 0; i < numBuckets; i++) {
        mBucket.emplace_back(0L);
    }

    mSortBucket.clear();
    for (uint32_t i = 0; i < numEntries; i++) {
        mSortBucket.emplace_back(UINT64_MAX);
    }

    for (uint32_t i = 0; i < numEntries; i++) {
        uint32_t bkt = GetBucketIndex(mEntryHash.at(i), numBuckets);
        mBucket.at(bkt) = mBucket.at(bkt) + 1L;
    }

    uint64_t totalEntries = 0L;
    for (uint32_t j = 0; j < numBuckets; j++) {
        uint64_t tmp = totalEntries;
        totalEntries += mBucket.at(j);
        mBucket.at(j) = (tmp << NO_32);
    }

    for (uint32_t j = 0; j < numEntries; j++) {
        uint32_t bkt = GetBucketIndex(mEntryHash.at(j), numBuckets);
        uint64_t bucketMeta = mBucket.at(bkt);
        uint32_t pos = static_cast<uint32_t>(bucketMeta >> NO_32) + static_cast<uint32_t>(bucketMeta);
        mSortBucket.at(pos) = mEntryPointer.at(j);
        mBucket.at(bkt) = bucketMeta + 1L;
    }

    BResult retVal;
    uint32_t numBytesForBucketElement = FullKeyUtil::MinNumBytesToRepresentValue(numEntries);
    for (uint64_t bkt : mBucket) {
        retVal = FullKeyUtil::WriteValueWithNumberOfBytes((bkt >> NO_32), numBytesForBucketElement, outputView);
        RETURN_NOT_OK_NO_LOG(retVal);
    }

    uint32_t numBitsForPrimaryKeyIndex = FullKeyUtil::MinNumBitsToRepresentValue(mMaxPrimaryKeyIndex);
    uint32_t numBitsForSecondaryKeyIndex = FullKeyUtil::MinNumBitsToRepresentValue(mMaxSecondKeyIndex);
    uint32_t numBytesForPointer = (numBitsForSecondaryKeyIndex + numBitsForPrimaryKeyIndex + NO_8 - 1) / NO_8;
    for (uint64_t pointer : mSortBucket) {
        uint64_t encodedPointer = EncodePointer(pointer, numBitsForSecondaryKeyIndex);
        retVal = FullKeyUtil::WriteValueWithNumberOfBytes(encodedPointer, numBytesForPointer, outputView);
        RETURN_NOT_OK_NO_LOG(retVal);
    }

    retVal = outputView->WriteUint32(numEntries);
    RETURN_NOT_OK_NO_LOG(retVal);
    retVal = outputView->WriteUint32(numBuckets);
    RETURN_NOT_OK_NO_LOG(retVal);
    retVal = outputView->WriteUint8(static_cast<uint8_t>(numBytesForBucketElement));
    RETURN_NOT_OK_NO_LOG(retVal);
    retVal = outputView->WriteUint8(static_cast<uint8_t>(numBytesForPointer));
    RETURN_NOT_OK_NO_LOG(retVal);
    retVal = outputView->WriteUint8(static_cast<uint8_t>(numBitsForPrimaryKeyIndex));
    RETURN_NOT_OK_NO_LOG(retVal);
    retVal = outputView->WriteUint8(static_cast<uint8_t>(numBitsForSecondaryKeyIndex));
    RETURN_NOT_OK_NO_LOG(retVal);
    return BSS_OK;
}

BResult DataBlockIndexReader::Lookup(uint32_t hash, const ByteBufferRef &byteBuffer, DataBlockIndexIterator &iterator)
{
    if (UNLIKELY(mNumBuckets == 0)) {
        LOG_ERROR("Unexpected param, mNumBuckets is zero.");
        return BSS_ERR;
    }
    if (UNLIKELY(byteBuffer == nullptr)) {
        LOG_ERROR("Unexpected param, byteBuffer is null.");
        return BSS_INVALID_PARAM;
    }
    uint32_t bucketIndex = DataBlockIndexWriter::GetBucketIndex(hash, mNumBuckets);
    uint32_t bucketOffset = mBucketIndexOffset + bucketIndex * mNumBytesForBucketElement;
    uint32_t startPointerIndex = FullKeyUtil::ReadValueWithNumberOfBytes(byteBuffer, bucketOffset,
                                                                         mNumBytesForBucketElement);
    uint32_t endPointerIndex = ((bucketIndex + 1) == mNumBuckets) ?
                                   mNumEntries :
                                   FullKeyUtil::ReadValueWithNumberOfBytes(byteBuffer,
                                                                           bucketOffset + mNumBytesForBucketElement,
                                                                           mNumBytesForBucketElement);
    if (startPointerIndex == endPointerIndex) {
        return BSS_NOT_EXISTS;
    }
    iterator.Init(this, byteBuffer, startPointerIndex, endPointerIndex);
    return BSS_OK;
}

uint64_t DataBlockIndexReader::GetPointAt(uint32_t pointerIndex, const ByteBufferRef &byteBuffer) const
{
    uint64_t encodedPointer =
        FullKeyUtil::ReadValueWithNumberOfBytes(byteBuffer, mPointerIndexOffset + pointerIndex * mNumBytesForPointer,
                                                mNumBytesForPointer);
    return DataBlockIndexWriter::DecodePointer(encodedPointer, mNumBitsForSecondaryKeyIndex);
}

}  // namespace bss
}  // namespace ock