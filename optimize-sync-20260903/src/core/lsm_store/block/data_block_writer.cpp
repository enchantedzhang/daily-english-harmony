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

#include "data_block_writer.h"

#include "common/util/var_encoding_util.h"
#include "data_block.h"
#include "lsm_store/key/full_key_util.h"

namespace ock {
namespace bss {
uint32_t DataBlockWriter::EstimateAfter(const KeyValueRef &keyValue)
{
    auto &key = keyValue->key;
    auto &value = keyValue->value;
    auto size = CurrentEstimateSize();
    if (size == UINT32_MAX) {
        return UINT32_MAX;
    }
    return size + FullKeyUtil::ComputePrimaryKeyLen(key.PriKey().KeyLen()) +
           FullKeyUtil::ComputeSeqIdAndValueTypeLen() + FullKeyUtil::ComputeSecondaryKeyLen(key.SecKey().KeyLen()) +
           FullKeyUtil::ComputeSeqIdAndValueTypeLen() + NO_4 + value.ValueLen();
}

uint32_t DataBlockWriter::CurrentEstimateSize()
{
    uint32_t primaryDataSize = mPrimaryOutputView->GetOffset();
    uint32_t secondDataSize = mSecondaryOutputView->GetOffset();
    if (UNLIKELY(UINT32_MAX - secondDataSize < primaryDataSize)) {
        LOG_WARN("Current size is too long, pri:" << primaryDataSize << ", sec:" << secondDataSize);
        return UINT32_MAX;
    }
    if (mLastKeyValue != nullptr) {
        if (mSecondaryOffset.empty()) {
            uint32_t valueSize = NO_1 + FullKeyUtil::ComputeStateIdEncodedSize() +
                                 FullKeyUtil::ComputeSecondaryKeyLen(mLastKeyValue->key.SecKey().KeyLen()) +
                                 FullKeyUtil::ComputeSeqIdAndValueTypeLen() + NO_4 + mLastKeyValue->value.ValueLen();
            primaryDataSize += valueSize;
        } else {
            uint32_t numSecondaryKeys = mSecondaryOffset.size();
            uint32_t minSecondaryKeyOffset = mSecondaryOffset.at(0);
            uint32_t maxSecondaryOffset = mSecondaryOffset.at(numSecondaryKeys - 1);
            if (UNLIKELY(maxSecondaryOffset < minSecondaryKeyOffset)) {
                LOG_ERROR("Unexpected situation, maxSecondaryOffset:"
                          << maxSecondaryOffset << ", minSecondaryKeyOffset:" << minSecondaryKeyOffset);
                return UINT32_MAX;
            }
            uint32_t numBytes = FullKeyUtil::MinNumBytesToRepresentValue(maxSecondaryOffset - minSecondaryKeyOffset);
            uint32_t valueSize = NO_13 + numBytes * numSecondaryKeys;
            primaryDataSize += valueSize;
        }
    }
    uint32_t numPrimaryKeys = mPrimaryOffset.size();
    if (numPrimaryKeys > 0) {
        uint32_t maxPrimaryKeyOffset = secondDataSize + mPrimaryOffset.at(numPrimaryKeys - 1);
        uint32_t numBytes = FullKeyUtil::MinNumBytesToRepresentValue(maxPrimaryKeyOffset);
        primaryDataSize += numPrimaryKeys * numBytes;
    }
    if (UNLIKELY(primaryDataSize < mPrimaryOutputView->GetOffset())) {
        LOG_WARN("Current size is too long,pri:" << primaryDataSize);
        return UINT32_MAX;
    }

    return primaryDataSize + secondDataSize + ((mIndexBuilder == nullptr) ? 0 : mIndexBuilder->EstimateSize()) + NO_10;
}

BResult DataBlockWriter::Finish(ByteBufferRef &byteBuffer)
{
    if (UNLIKELY(mNumKeyValuePairs == 0)) {
        return BSS_INNER_ERR;
    }
    OutputViewRef finalOutputView;
    uint32_t primaryKeyBaseOffset;

    auto ret = WritePrimaryValue(mLastKeyValue);
    RETURN_NOT_OK_NO_LOG(ret);
    RETURN_ERROR_AS_NULLPTR(mSecondaryOutputView);

    if (mSecondaryOutputView->GetOffset() != 0) {
        primaryKeyBaseOffset = mSecondaryOutputView->GetOffset();
        ret = mSecondaryOutputView->Write(mPrimaryOutputView->Data(), mPrimaryOutputView->GetOffset());
        RETURN_NOT_OK_NO_LOG(ret);
        finalOutputView = mSecondaryOutputView;
    } else {
        primaryKeyBaseOffset = 0;
        finalOutputView = mPrimaryOutputView;
    }

    uint32_t numPrimaryKeys = mPrimaryOffset.size();
    if (UNLIKELY(numPrimaryKeys == 0)) {
        LOG_WARN("Data block is empty.");
        return BSS_INNER_ERR;
    }
    uint32_t maxPriKeyOffset = primaryKeyBaseOffset + mPrimaryOffset.at(numPrimaryKeys - 1);
    uint32_t numBytes = FullKeyUtil::MinNumBytesToRepresentValue(maxPriKeyOffset);
    uint32_t primaryIndexOffset = finalOutputView->GetOffset();
    for (uint32_t i = 0; i < numPrimaryKeys; i++) {
        ret = FullKeyUtil::WriteValueWithNumberOfBytes(primaryKeyBaseOffset + mPrimaryOffset.at(i), numBytes,
                                                       finalOutputView);
        RETURN_NOT_OK_NO_LOG(ret);
    }

    if (mIndexBuilder != nullptr) {
        ret = mIndexBuilder->Finish(finalOutputView);
        RETURN_NOT_OK_NO_LOG(ret);
    }

    ret = finalOutputView->WriteUint32(primaryIndexOffset);
    RETURN_NOT_OK_NO_LOG(ret);
    ret = finalOutputView->WriteUint8(numBytes);
    RETURN_NOT_OK_NO_LOG(ret);
    ret = finalOutputView->WriteUint32(numPrimaryKeys);
    RETURN_NOT_OK_NO_LOG(ret);
    ret = finalOutputView->WriteUint8(static_cast<uint8_t>(DataBlockType::HASH));
    RETURN_NOT_OK_NO_LOG(ret);
    if (mNumKeyValuePairs == 1) {
        mEndKey = mStartKey;
    } else {
        mEndKey = FullKeyUtil::ToFullKey(mEndKeyValue, mMemManager, mHolder);
        RETURN_ALLOC_FAIL_AS_NULLPTR(mEndKey);
    }
    byteBuffer = MakeRef<ByteBuffer>(finalOutputView->Data(), finalOutputView->GetOffset());
    RETURN_ERROR_AS_NULLPTR(byteBuffer);
    return BSS_OK;
}

BResult DataBlockWriter::WritePrimaryValue(const KeyValueRef &keyValue)
{
    if (mSecondaryOffset.empty()) {
        auto ret = mPrimaryOutputView->WriteUint8(0);
        RETURN_NOT_OK_NO_LOG(ret);
        ret = WriteSecondaryKeyAndValue(keyValue, mPrimaryOutputView, mPrimaryOffset.size() - 1, 0);
        RETURN_NOT_OK_NO_LOG(ret);
    } else {
        uint32_t numSecondaryKeys = mSecondaryOffset.size();
        uint32_t minSecondaryKeyOffset = mSecondaryOffset.at(0);
        uint32_t maxSecondaryOffset = mSecondaryOffset.at(numSecondaryKeys - 1);
        if (UNLIKELY(maxSecondaryOffset < minSecondaryKeyOffset)) {
            LOG_ERROR("Unexpected situation, maxSecondaryOffset:" << maxSecondaryOffset << ", minSecondaryKeyOffset:"
                                                                  << minSecondaryKeyOffset << ".");
            return BSS_INNER_ERR;
        }
        uint32_t numBytes = FullKeyUtil::MinNumBytesToRepresentValue(maxSecondaryOffset - minSecondaryKeyOffset);

        auto ret = mPrimaryOutputView->WriteUint8((numBytes << 1) | 0x1);
        RETURN_NOT_OK_NO_LOG(ret);
        ret = VarEncodingUtil::EncodeUnsignedInt(numSecondaryKeys, mPrimaryOutputView);
        RETURN_NOT_OK_NO_LOG(ret);
        ret = VarEncodingUtil::EncodeUnsignedInt(minSecondaryKeyOffset, mPrimaryOutputView);
        RETURN_NOT_OK_NO_LOG(ret);
        for (uint32_t i = 0; i < numSecondaryKeys; i++) {
            ret = FullKeyUtil::WriteValueWithNumberOfBytes(mSecondaryOffset.at(i) - minSecondaryKeyOffset, numBytes,
                                                           mPrimaryOutputView);
            RETURN_NOT_OK_NO_LOG(ret);
        }
    }
    mSecondaryOffset.clear();
    return BSS_OK;
}

BResult DataBlockWriter::WriteSecondaryKeyAndValue(const KeyValueRef &keyValue, const OutputViewRef &outputView,
                                                   uint32_t primaryKeyIndex, uint32_t secondaryKeyIndex)
{
    auto &key = keyValue->key;
    // write stateId
    auto ret = FullKeyUtil::WriteStateId(key.StateId(), outputView);
    RETURN_NOT_OK_NO_LOG(ret);
    if (key.HasSecKey()) {
        // write secondary key.
        ret = FullKeyUtil::WriteSecKey(key, outputView);
        RETURN_NOT_OK_NO_LOG(ret);
    }
    // write value.
    ret = WriteValue(keyValue->value, outputView);
    RETURN_NOT_OK_NO_LOG(ret);

    // build index.
    if (mIndexBuilder != nullptr) {
        mIndexBuilder->Add(primaryKeyIndex, secondaryKeyIndex, key.MixedHashCode());
    }
    return BSS_OK;
}

BResult DataBlockWriter::WriteValue(const Value &value, const OutputViewRef &outputView)
{
    // write value seqId.
    auto ret = outputView->WriteUint64(value.SeqId());
    RETURN_NOT_OK_NO_LOG(ret);
    // write value type.
    ret = outputView->WriteUint8(value.ValueType());
    RETURN_NOT_OK_NO_LOG(ret);
    // write value length.
    uint32_t len = value.ValueData() == nullptr ? 0 : value.ValueLen();
    ret = VarEncodingUtil::EncodeUnsignedInt(len, outputView);
    if (UNLIKELY(ret != BSS_OK)) {
        return BSS_INNER_ERR;
    }
    // write value data.
    if (LIKELY(len > 0)) {
        return outputView->Write(value.ValueData(), len);
    }
    return BSS_OK;
}

BResult DataBlockWriter::Add(const KeyValueRef &keyValue)
{
    BResult ret = BSS_OK;
    mNumKeyValuePairs++;

    // set start and end key.
    if (mStartKey == nullptr) {
        mStartKey = FullKeyUtil::ToFullKey(keyValue, mMemManager, mHolder);
        RETURN_ALLOC_FAIL_AS_NULLPTR(mStartKey);
    }
    mEndKeyValue = keyValue;

    // write key and value to output view, we will put the same primary key node together, so just compare key node
    // without stateId.
    if (mLastKeyValue == nullptr || !mLastKeyValue->key.HasSamePriKeyNode(keyValue->key)) {
        if (mLastKeyValue != nullptr) {
            ret = WritePrimaryValue(mLastKeyValue);
            RETURN_NOT_OK_NO_LOG(ret);
        }
        mLastKeyValue = keyValue;
        mPrimaryOffset.emplace_back(mPrimaryOutputView->GetOffset());
        // write primary key.
        return FullKeyUtil::WritePriKey(mLastKeyValue->key, mPrimaryOutputView);
    }

    if (mSecondaryOffset.empty()) {
        mSecondaryOffset.emplace_back(mSecondaryOutputView->GetOffset());
        ret = WriteSecondaryKeyAndValue(mLastKeyValue, mSecondaryOutputView, mPrimaryOffset.size() - 1, 0);
        RETURN_NOT_OK_NO_LOG(ret);
        mLastKeyValue->value = {};
    }
    mSecondaryOffset.emplace_back(mSecondaryOutputView->GetOffset());
    ret = WriteSecondaryKeyAndValue(keyValue, mSecondaryOutputView, mPrimaryOffset.size() - 1,
                                    mSecondaryOffset.size() - 1);
    RETURN_NOT_OK_NO_LOG(ret);
    mLastKeyValue->key = keyValue->key;
    return ret;
}

void DataBlockWriter::Reset()
{
    mNumKeyValuePairs = 0;
    mPrimaryOutputView->ReleaseMemory();
    mSecondaryOutputView->ReleaseMemory();
    mPrimaryOffset.clear();
    mSecondaryOffset.clear();
    mLastInternalKey = nullptr;
    mStartKey = nullptr;
    mEndKey = nullptr;
    mEndKeyValue = nullptr;
    mLastKeyValue = nullptr;
    if (mIndexBuilder != nullptr) {
        mIndexBuilder->Reset();
    }
}

}  // namespace bss
}  // namespace ock