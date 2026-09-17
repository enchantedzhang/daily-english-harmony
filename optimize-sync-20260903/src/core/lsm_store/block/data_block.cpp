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

#include "data_block.h"

namespace ock {
namespace bss {
DataBlockIndexReaderRef DataBlock::InitIndexReader()
{
    if (UNLIKELY(mBuffer->Capacity() < NO_10)) {
        LOG_ERROR("Data block size " << mBuffer->Capacity() << " is less than min size " << NO_10);
        return nullptr;
    }
    uint8_t blockType = 0;
    auto ret = mBuffer->ReadUint8(blockType, mBuffer->Capacity() - 1);
    if (UNLIKELY(ret != BSS_OK || static_cast<DataBlockType>(blockType) != DataBlockType::HASH)) {
        LOG_ERROR("Read data block type failed, ret:" << ret << ", blockType:" << static_cast<uint32_t>(blockType));
        return nullptr;
    }
    auto reader = std::make_shared<DataBlockIndexReader>();
    ret = reader->Init(mBuffer, mBuffer->Capacity() - NO_10);
    RETURN_NULLPTR_AS_NOT_OK(ret);
    return reader;
}

BResult DataBlock::GetKey(const Key &key, Value &value)
{
    if (LIKELY(mIndexReader != nullptr)) {
        uint32_t hash = key.MixedHashCode();
        DataBlockIndexIterator iterator;
        auto ret = mIndexReader->Lookup(hash, mBuffer, iterator);
        if (UNLIKELY(ret != BSS_OK || !iterator.HasNext())) {
            return BSS_NOT_EXISTS;
        }
        if (iterator.Size() < mBinarySearchThreshold) {
            return LinearSearch(key, value, iterator);  // 线性查找, 通过迭代器遍历找到对应的数据.
        } else {
            return BinarySearch(key, value);  // 二分查找.
        }
    }
    return BSS_NOT_EXISTS;
}

BResult DataBlock::LinearSearch(const Key &key, Value &value, DataBlockIndexIterator &iterator)
{
    bool hasSecondaryKey = FullKeyUtil::StateHasSecondaryKey(key.StateId());
    uint32_t primaryKeyIndex = UINT32_MAX;
    uint32_t primaryKeyOffset = UINT32_MAX;
    uint64_t primaryKeyLenInfo = UINT64_MAX;
    bool primaryKeySameAsLookupKey = false;
    uint32_t secondaryKeyIndexOffset = UINT32_MAX;
    uint32_t firstSecondaryKeyOffset = UINT32_MAX;
    uint32_t numBytesForSecondaryKeyIndexElement = UINT32_MAX;

    while (iterator.HasNext()) {
        uint64_t pointer = iterator.Next();
        uint32_t newPrimaryKeyIndex = DataBlockIndexWriter::GetPrimaryKeyIndexFromPointer(pointer);
        uint32_t newSecondaryKeyIndex = DataBlockIndexWriter::GetSecondaryKeyIndexFromPointer(pointer);
        bool samePrimaryKeyAsPrev = (newPrimaryKeyIndex == primaryKeyIndex);
        if (!samePrimaryKeyAsPrev) {
            primaryKeyIndex = newPrimaryKeyIndex;
            primaryKeyOffset = GetKeyIndexElement(mPrimaryKeyIndexOffset, mPrimaryKeyIndexElementSize,
                                                  newPrimaryKeyIndex);
            primaryKeyLenInfo = FullKeyUtil::ParsePrimaryKeyLen(mBuffer, primaryKeyOffset);
            int32_t i = FullKeyUtil::ComparePrimaryKeyForLookup(key, mBuffer, primaryKeyOffset, primaryKeyLenInfo);
            primaryKeySameAsLookupKey = (i == 0);
        }

        if (!primaryKeySameAsLookupKey) {
            continue;
        }

        uint32_t primaryKeyLen = FullKeyUtil::GetPrimaryKeyLen(primaryKeyLenInfo);
        uint32_t bufferOffset = primaryKeyOffset + primaryKeyLen;
        uint8_t flag = 0;
        RETURN_ERROR_AS_NULLPTR(mBuffer);
        RETURN_NOT_OK(mBuffer->ReadUint8(flag, bufferOffset));
        ++bufferOffset;
        uint16_t stateId = 0;
        uint64_t decodedNumSecondaryKeys;
        if (IsSingleSecondaryKey(flag)) {
            stateId = FullKeyUtil::ReadStateId(mBuffer, bufferOffset);
            bufferOffset += sizeof(uint16_t);
            auto result = FullKeyUtil::CompareStateId(key.StateId(), stateId);
            uint64_t secondaryKeyLenInfo = 0L;
            if (result == 0 && hasSecondaryKey) {
                secondaryKeyLenInfo = FullKeyUtil::ParseSecondaryKeyLen(mBuffer, bufferOffset);
                result = FullKeyUtil::CompareSecondaryKeyForLookup(key, mBuffer, bufferOffset, secondaryKeyLenInfo);
            }

            if (result == 0) {
                return FullKeyUtil::BuildValue(value, mBuffer, bufferOffset,
                                               FullKeyUtil::GetSecondaryKeyLen(secondaryKeyLenInfo));
            }
        } else {
            if (!samePrimaryKeyAsPrev) {
                numBytesForSecondaryKeyIndexElement = GetNumBytesForSecondaryKeyIndexElement(flag);
                decodedNumSecondaryKeys = VarEncodingUtil::DecodeUnsignedInt(mBuffer, bufferOffset);
                bufferOffset += VarEncodingUtil::GetNumberOfEncodedBytes(decodedNumSecondaryKeys);
                uint64_t decodedFirstSecondaryKeyOffset = VarEncodingUtil::DecodeUnsignedInt(mBuffer, bufferOffset);
                firstSecondaryKeyOffset = VarEncodingUtil::GetDecodedValue(decodedFirstSecondaryKeyOffset);
                bufferOffset += VarEncodingUtil::GetNumberOfEncodedBytes(decodedFirstSecondaryKeyOffset);
                secondaryKeyIndexOffset = bufferOffset;
            }

            uint32_t secondaryKeyOffset = firstSecondaryKeyOffset +
                                          GetKeyIndexElement(secondaryKeyIndexOffset,
                                                             numBytesForSecondaryKeyIndexElement, newSecondaryKeyIndex);
            stateId = FullKeyUtil::ReadStateId(mBuffer, secondaryKeyOffset);
            secondaryKeyOffset += sizeof(uint16_t);
            int32_t cmp = FullKeyUtil::CompareStateId(key.StateId(), stateId);
            uint64_t secondaryKeyLenInfo = 0L;
            if (cmp == 0 && hasSecondaryKey) {
                secondaryKeyLenInfo = FullKeyUtil::ParseSecondaryKeyLen(mBuffer, secondaryKeyOffset);
                cmp = FullKeyUtil::CompareSecondaryKeyForLookup(key, mBuffer, secondaryKeyOffset, secondaryKeyLenInfo);
            }

            if (cmp == 0) {
                return FullKeyUtil::BuildValue(value, mBuffer, secondaryKeyOffset,
                                               FullKeyUtil::GetSecondaryKeyLen(secondaryKeyLenInfo));
            }
        }
    }

    return BSS_NOT_EXISTS;
}

BResult DataBlock::BinarySearch(const Key &key, Value &value)
{
    PrimaryKeyInfo primaryKeyInfo;
    SecondaryLevelInfoRef secondaryLevelInfo = nullptr;
    auto ret = FindPrimaryKey(key, primaryKeyInfo, secondaryLevelInfo);
    if (ret != BSS_OK || secondaryLevelInfo == nullptr) {
        return ret;
    } else {
        RETURN_ERROR_AS_NULLPTR(mBuffer);
        return secondaryLevelInfo->GetPair(primaryKeyInfo, key, value, mBuffer);
    }
}

BResult DataBlock::FindKeyValueInfo(const Key &key, LsmKeyValueInfo &keyValueInfo)
{
    if (UNLIKELY(mNumPrimaryKey == 0)) {
        return BSS_NOT_EXISTS;
    }

    uint32_t leftPriKeyIndex = 0;
    uint32_t rightPriKeyIndex = mNumPrimaryKey - 1;
    uint32_t midPriKeyIndex;
    uint32_t infoOffset = UINT32_MAX;
    auto &lookupPriKey = key.PriKey();
    while (leftPriKeyIndex <= rightPriKeyIndex) {
        midPriKeyIndex = leftPriKeyIndex + (rightPriKeyIndex - leftPriKeyIndex) / NO_2;
        uint32_t offset = GetKeyIndexElement(mPrimaryKeyIndexOffset, mPrimaryKeyIndexElementSize, midPriKeyIndex);
        LsmPriKeyNode &priKey = LsmPriKeyNode::From(mBuffer->Data() + offset);
        int32_t cmp = -priKey.CompareKeyNode(lookupPriKey);
        if (cmp == 0) {
            BResult result = keyValueInfo.Unpack(mBuffer, offset);
            if (UNLIKELY(result != BSS_OK)) {
                LOG_ERROR("Unpack kv info failed, priKey hashCode:" << priKey.mKeyHashCode << ", offset: " << offset);
                break;
            }
            cmp = BssMath::IntegerCompare(key.StateId(), keyValueInfo.StateId());
            if (cmp == 0) {
                infoOffset = offset;
                break;
            }
        }
        if (cmp < 0) {
            if (midPriKeyIndex == 0) {
                break;
            }
            rightPriKeyIndex = midPriKeyIndex - 1;
            continue;
        }
        leftPriKeyIndex = midPriKeyIndex + 1;
    }

    if (infoOffset == UINT32_MAX) {
        return BSS_NOT_EXISTS;
    }

    return BSS_OK;
}

BResult DataBlock::FindPrimaryKey(const Key &key, PrimaryKeyInfo &primaryKeyInfo,
                                  SecondaryLevelInfoRef &secondaryLevelInfo)
{
    if (UNLIKELY(mNumPrimaryKey == 0)) {
        return BSS_NOT_EXISTS;
    }

    uint32_t leftPrimaryKeyIndex = 0;
    uint32_t rightPrimaryKeyIndex = mNumPrimaryKey - 1;
    uint32_t midPrimaryKeyIndex;
    uint32_t primaryKeyOffset = UINT32_MAX;
    while (leftPrimaryKeyIndex <= rightPrimaryKeyIndex) {
        midPrimaryKeyIndex = leftPrimaryKeyIndex + (rightPrimaryKeyIndex - leftPrimaryKeyIndex) / NO_2;
        uint32_t offset = GetKeyIndexElement(mPrimaryKeyIndexOffset, mPrimaryKeyIndexElementSize, midPrimaryKeyIndex);
        uint64_t lenInfo = FullKeyUtil::ParsePrimaryKeyLen(mBuffer, offset);
        int32_t cmp = FullKeyUtil::ComparePrimaryKeyForLookup(key, mBuffer, offset, lenInfo);
        if (cmp == 0) {
            BuildPrimaryInfo(primaryKeyInfo, midPrimaryKeyIndex, offset, lenInfo);
            secondaryLevelInfo = GetSecondaryLevelInfo(primaryKeyInfo);
            cmp = BssMath::IntegerCompare(key.StateId(), secondaryLevelInfo->GetStateId(mBuffer));
            if (cmp == 0) {
                primaryKeyOffset = offset;
                break;
            }
        }
        if (cmp < 0) {
            if (midPrimaryKeyIndex == 0) {
                break;
            }
            rightPrimaryKeyIndex = midPrimaryKeyIndex - 1;
            continue;
        }
        leftPrimaryKeyIndex = midPrimaryKeyIndex + 1;
    }

    if (primaryKeyOffset == UINT32_MAX) {
        return BSS_NOT_EXISTS;
    }

    return BSS_OK;
}

void DataBlock::BuildPrimaryInfo(PrimaryKeyInfo &primaryKeyInfo, uint32_t midPrimaryKeyIndex, uint32_t primaryKeyOffset,
                                 uint64_t primaryKeyLenInfo)
{
    uint32_t primaryKeyLen = FullKeyUtil::GetPrimaryKeyLen(primaryKeyLenInfo);
    uint32_t bufferOffset = primaryKeyOffset + primaryKeyLen;
    uint8_t flag = 0;
    mBuffer->ReadUint8(flag, bufferOffset);
    primaryKeyInfo.Init(midPrimaryKeyIndex, primaryKeyOffset, primaryKeyLenInfo, flag);
}

SecondaryLevelInfoRef DataBlock::GetSecondaryLevelInfo(PrimaryKeyInfo &primaryKeyInfo)
{
    if (primaryKeyInfo.IsSingleSecondaryKey()) {
        return std::make_shared<SingleSecondaryLevelInfo>(primaryKeyInfo.GetSecondaryLevelOffset(), mMemManager,
                                                          mHolder);
    }

    uint32_t bufferOffset = primaryKeyInfo.GetSecondaryLevelOffset();
    uint32_t numBytesForSecondaryKeyIndexElement = primaryKeyInfo.GetNumBytesForSecondaryKeyIndexElement();

    uint64_t decodedNumSecondaryKeys = VarEncodingUtil::DecodeUnsignedInt(mBuffer, bufferOffset);
    uint32_t numSecondaryKeys = VarEncodingUtil::GetDecodedValue(decodedNumSecondaryKeys);
    bufferOffset += VarEncodingUtil::GetNumberOfEncodedBytes(decodedNumSecondaryKeys);

    uint64_t decodedFirstSecondaryKeyOffset = VarEncodingUtil::DecodeUnsignedInt(mBuffer, bufferOffset);
    uint32_t firstSecondaryKeyOffset = VarEncodingUtil::GetDecodedValue(decodedFirstSecondaryKeyOffset);
    bufferOffset += VarEncodingUtil::GetNumberOfEncodedBytes(decodedFirstSecondaryKeyOffset);

    return std::make_shared<MultiSecondaryLevelInfo>(numBytesForSecondaryKeyIndexElement, numSecondaryKeys,
                                                     bufferOffset, firstSecondaryKeyOffset, mMemManager, mHolder);
}

int32_t DataBlock::BinarySearch(const LsmKeyValueInfo &keyValueInfo, const Key &key, int32_t leftIndex,
                                int32_t rightIndex)
{
    while (leftIndex < rightIndex) {
        int32_t midIndex = (leftIndex + rightIndex) / 2;
        int32_t cmp = keyValueInfo.CompareSecKeyNode(midIndex, key);
        if (cmp >= 0) {
            rightIndex = midIndex;
            continue;
        }
        leftIndex = midIndex + 1;
    }
    return rightIndex;
}

KeyValueIteratorRef DataBlock::SubIterator(const Key &startKey, const Key &endKey, bool reverseOrder)
{
    // find key value info by start key.
    LsmKeyValueInfo keyValueInfo = {};
    auto ret = FindKeyValueInfo(startKey, keyValueInfo);
    if (UNLIKELY(ret != BSS_OK)) {
        return nullptr;
    }

    // find entries by start key and end key.
    std::vector<KeyValueRef> keyValues;
    auto numSecKey = static_cast<int32_t>(keyValueInfo.GetSecKeyCount());
    if (numSecKey == 1) {
        // only one secondary key.
        if (!keyValueInfo.IsSecKeyInRange(0, startKey, endKey)) {
            return nullptr;
        }
        auto keyValue = MakeRef<KeyValue>();
        RETURN_NULLPTR_AS_READ_BUFFER_ERROR(keyValueInfo.GetKeyAndValue(0, keyValue));
        keyValues.emplace_back(std::move(keyValue));
    } else {
        // multi secondary keys.
        // boost search left index and right index.
        int32_t leftIndex = BinarySearch(keyValueInfo, startKey, 0, numSecKey);
        if (leftIndex == numSecKey) {
            return nullptr;
        }
        int32_t rightIndex = BinarySearch(keyValueInfo, endKey, leftIndex, numSecKey) - 1;
        if (rightIndex < leftIndex) {
            return nullptr;
        }
        keyValues.reserve(static_cast<size_t>(rightIndex - leftIndex + 1));
        if (reverseOrder) {
            // reverse order, right key first, get key value and put to vector.
            for (int32_t secKeyIndex = rightIndex; secKeyIndex >= leftIndex; --secKeyIndex) {
                auto keyValue = MakeRef<KeyValue>();
                RETURN_NULLPTR_AS_READ_BUFFER_ERROR(keyValueInfo.GetKeyAndValue(secKeyIndex, keyValue));
                keyValues.emplace_back(std::move(keyValue));
            }
        } else {
            // reverse order, left key first, get key value and put to vector.
            for (int32_t secKeyIndex = leftIndex; secKeyIndex <= rightIndex; ++secKeyIndex) {
                auto keyValue = MakeRef<KeyValue>();
                RETURN_NULLPTR_AS_READ_BUFFER_ERROR(keyValueInfo.GetKeyAndValue(secKeyIndex, keyValue));
                keyValues.emplace_back(std::move(keyValue));
            }
        }
    }
    // trans vector to iterator.
    return std::make_shared<KeyValueVectorIterator>(std::move(keyValues));
}

KeyValueIteratorRef DataBlock::Iterator(KeyFilter keyFilter)
{
    // unpack key and value array from data block.
    std::vector<KeyValueRef> keyValues;
    for (uint32_t priKeyIndex = 0; priKeyIndex < mNumPrimaryKey; priKeyIndex++) {
        uint32_t priKeyOffset = GetKeyIndexElement(priKeyIndex);
        LsmKeyValueInfo keyValueInfo;
        BResult result = keyValueInfo.Unpack(mBuffer, priKeyOffset);
        if (UNLIKELY(result != BSS_OK)) {
            LOG_ERROR("Unpack kv info failed, priKey index:" << priKeyIndex << ", offset: " << priKeyOffset);
            continue;
        }
        uint32_t numSecKey = keyValueInfo.GetSecKeyCount();

        for (uint32_t secKeyIndex = 0; secKeyIndex < numSecKey; ++secKeyIndex) {
            auto keyValue = MakeRef<KeyValue>();
            RETURN_NULLPTR_AS_READ_BUFFER_ERROR(keyValueInfo.GetKeyAndValue(secKeyIndex, keyValue));
            if (keyFilter == nullptr || !keyFilter(keyValue->key)) {
                keyValues.emplace_back(std::move(keyValue));
            }
        }
    }
    return std::make_shared<KeyValueVectorIterator>(std::move(keyValues));
}
}  // namespace bss
}  // namespace ock
