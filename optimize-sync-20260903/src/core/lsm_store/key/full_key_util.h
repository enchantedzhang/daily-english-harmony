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

#ifndef BOOST_SS_HASH_INTERNAL_KEY_UTIL_H
#define BOOST_SS_HASH_INTERNAL_KEY_UTIL_H

#include "binary/key/full_key.h"
#include "binary/key/key.h"
#include "binary/slice_binary.h"
#include "common/io/output_view.h"
#include "common/util/bss_math.h"
#include "common/util/var_encoding_util.h"
#include "include/bss_types.h"
#include "include/state_type.h"
#include "kv_table/stateId_provider.h"

namespace ock {
namespace bss {
class FullKeyUtil {
public:
    inline static uint32_t ComputePrimaryKeyLen(uint32_t primaryUserKeyLen)
    {
        return NO_4 + NO_4 + primaryUserKeyLen;
    }

    inline static uint32_t ComputeStateIdEncodedSize()
    {
        return NO_2;
    }

    inline static uint32_t ComputeSecondaryKeyLen(uint32_t secondaryUserKeyLen)
    {
        return NO_4 + NO_4 + secondaryUserKeyLen;
    }

    inline static uint32_t ComputeSeqIdAndValueTypeLen()
    {
        return NO_9;
    }

    static uint32_t MinNumBytesToRepresentValue(uint32_t value);

    inline static uint32_t MinNumBitsToRepresentValue(uint32_t value)
    {
        return (value == 0) ? 1 : (NO_32 - BssMath::NumberOfLeadingZeros(value));
    }

    inline static int32_t WriteStateId(uint16_t stateId, const OutputViewRef &outputView)
    {
        return outputView->WriteUint16(stateId);
    }

    inline static BResult WriteSecondaryKey(const FullKeyRef &key, const OutputViewRef &outputView)
    {
        auto retVal = outputView->WriteUint32(key->SecKey().HashCode());
        RETURN_NOT_OK_NO_LOG(retVal);
        retVal = VarEncodingUtil::EncodeUnsignedInt(key->SecKey().KeyLen(), outputView);
        RETURN_NOT_OK_NO_LOG(retVal);
        return outputView->Write(key->SecKey().KeyData(), key->SecKey().KeyLen());
    }

    inline static BResult WriteSeqIdAndValueType(const FullKeyRef &key, const OutputViewRef &outputView)
    {
        auto retVal = outputView->WriteUint64(key->SeqId());
        RETURN_NOT_OK_NO_LOG(retVal);
        return outputView->WriteUint8(key->ValueType());
    }

    inline static BResult WriteValueWithNumberOfBytes(uint64_t value, uint32_t numBytes,
                                                      const OutputViewRef &outputView)
    {
        if (UNLIKELY(numBytes == 0 || numBytes > NO_8)) {
            return BSS_INVALID_PARAM;
        }
        return outputView->Write(reinterpret_cast<uint8_t *>(&value), numBytes);
    }

    inline static uint64_t ReadValueWithNumberOfBytes(const ByteBufferRef &buffer, uint32_t offset, uint32_t byteNum)
    {
        uint64_t value = 0;
        auto ret = buffer->UnsafeReadLessThen8Bytes(offset, byteNum, value);
        if (UNLIKELY(ret != BSS_OK)) {
            LOG_ERROR("Read buffer failed, pos:" << offset << ", bytes:" << byteNum << ", ret:" << ret);
        }
        return value;
    }

    static FullKeyRef ToFullKey(const KeyValueRef &keyValue, const MemManagerRef &memManager, FileProcHolder holder);

    inline static BResult WriteInternalKey(const FullKeyRef &fullKey, const OutputViewRef &outputView)
    {
        auto retVal = WritePrimaryKey(fullKey, outputView);
        RETURN_NOT_OK_NO_LOG(retVal);
        retVal = WriteStateId(fullKey->StateId(), outputView);
        RETURN_NOT_OK_NO_LOG(retVal);
        if (fullKey->HasSecKey()) {
            retVal = WriteSecondaryKey(fullKey, outputView);
            RETURN_NOT_OK_NO_LOG(retVal);
        }
        return WriteSeqIdAndValueType(fullKey, outputView);
    }

    inline static BResult WritePrimaryKey(const FullKeyRef &fullKey, const OutputViewRef &outputView)
    {
        auto retVal = outputView->WriteUint32(fullKey->KeyHashCode());
        RETURN_NOT_OK_NO_LOG(retVal);
        retVal = VarEncodingUtil::EncodeUnsignedInt(fullKey->PriKey().KeyLen(), outputView);
        RETURN_NOT_OK_NO_LOG(retVal);
        return outputView->Write(fullKey->PriKey().KeyData(), fullKey->PriKey().KeyLen());
    }

    inline static BResult WritePriKey(const Key &key, const OutputViewRef &outputView)
    {
        PriKeyNode priKey = key.PriKey();
        auto retVal = outputView->WriteUint32(key.KeyHashCode());
        RETURN_NOT_OK_NO_LOG(retVal);
        retVal = VarEncodingUtil::EncodeUnsignedInt(priKey.KeyLen(), outputView);
        RETURN_NOT_OK_NO_LOG(retVal);
        return outputView->Write(priKey.KeyData(), priKey.KeyLen());
    }

    inline static BResult WriteSecKey(const Key &key, const OutputViewRef &outputView)
    {
        SecKeyNode secKey = key.SecKey();
        auto retVal = outputView->WriteUint32(secKey.HashCode());
        RETURN_NOT_OK_NO_LOG(retVal);
        retVal = VarEncodingUtil::EncodeUnsignedInt(secKey.KeyLen(), outputView);
        RETURN_NOT_OK_NO_LOG(retVal);
        return outputView->Write(secKey.KeyData(), secKey.KeyLen());
    }

    inline static int32_t ComparePrimaryKey(const uint8_t *keyData1, uint32_t userKeyOffset1, uint32_t userKeyLen1,
                                            uint32_t hash1, uint8_t *keyData2, uint32_t userKeyOffset2,
                                            uint32_t userKeyLen2, uint32_t hash2)
    {
        int32_t cmp = CompareUint32t(hash1, hash2);
        if (cmp != 0) {
            return cmp;
        }
        COMPARE_PARAM_WITH_NULLPTR(keyData1, keyData2);
        uint32_t minLen = std::min(userKeyLen1, userKeyLen2);
        cmp = memcmp(keyData1 + userKeyOffset1, keyData2 + userKeyOffset2, minLen);
        if (cmp == 0) {
            return BssMath::IntegerCompare<uint32_t>(userKeyLen1, userKeyLen2);
        }
        return cmp;
    }

    inline static uint64_t ParsePrimaryKeyLen(const ByteBufferRef &buffer, uint32_t offset)
    {
        uint64_t decodeResult = VarEncodingUtil::DecodeUnsignedInt(buffer, offset + NO_4);
        uint32_t userKeyLen = VarEncodingUtil::GetDecodedValue(decodeResult);
        uint32_t encodedLen = VarEncodingUtil::GetNumberOfEncodedBytes(decodeResult);
        return BuildPrimaryKeyLenInfo(userKeyLen, NO_4 + encodedLen + userKeyLen);
    }

    inline static uint64_t BuildPrimaryKeyLenInfo(uint32_t userKeyLen, uint32_t keyLen)
    {
        return (static_cast<uint64_t>(userKeyLen) << NO_32) | keyLen;
    }

    inline static uint32_t GetPrimaryKeyLen(uint64_t lenInfo)
    {
        return static_cast<uint32_t>(lenInfo);
    }

    inline static uint16_t ReadStateId(const ByteBufferRef &buffer, uint32_t offset)
    {
        uint16_t stateId = 0;
        buffer->ReadUint16(stateId, offset);
        return stateId;
    }

    inline static bool StateHasSecondaryKey(uint16_t stateId)
    {
        StateType stateType = StateId::GetStateType(stateId);
        return StateTypeUtil::HasSecKey(stateType);
    }

    inline static uint64_t ParseSecondaryKeyLen(const ByteBufferRef &buffer, uint32_t offset)
    {
        uint64_t decodeResult = VarEncodingUtil::DecodeUnsignedInt(buffer, offset + NO_4);  // 其中NO_4 = sizeof(hash)
        uint32_t userKeyLen = VarEncodingUtil::GetDecodedValue(decodeResult);
        uint32_t encodedLen = VarEncodingUtil::GetNumberOfEncodedBytes(decodeResult);
        return BuildSecondaryKeyLenInfo(userKeyLen, NO_4 + encodedLen + userKeyLen);
    }

    inline static uint64_t BuildSecondaryKeyLenInfo(uint32_t userKeyLen, uint32_t keyLen)
    {
        return (static_cast<uint64_t>(userKeyLen) << NO_32) | keyLen;
    }

    inline static uint32_t GetPrimaryUserKeyLen(uint64_t lenInfo)
    {
        return static_cast<uint32_t>(lenInfo >> NO_32);
    }

    inline static uint32_t GetSecondaryKeyLen(uint64_t lenInfo)
    {
        return static_cast<uint32_t>(lenInfo);
    }

    inline static uint32_t GetSecondaryUserKeyLen(uint64_t lenInfo)
    {
        return static_cast<uint32_t>(lenInfo >> NO_32);
    }

    inline static int32_t ComparePrimaryKeyForLookup(const Key &key, const ByteBufferRef &primaryByteBuffer,
                                                     uint32_t primaryKeyOffset, uint64_t primaryKeyLenInfo)
    {
        uint32_t primaryKeyLen = GetPrimaryKeyLen(primaryKeyLenInfo);
        uint32_t primaryUserKeyLen = GetPrimaryUserKeyLen(primaryKeyLenInfo);
        uint32_t primaryUserKeyHash = 0;
        primaryByteBuffer->ReadUint32(primaryUserKeyHash, primaryKeyOffset);
        return ComparePrimaryKey(key.PriKey().KeyData(), 0, key.PriKey().KeyLen(), key.PriKey().KeyHashCode(),
                                 primaryByteBuffer->Data(), primaryKeyOffset + primaryKeyLen - primaryUserKeyLen,
                                 primaryUserKeyLen, primaryUserKeyHash);
    }

    inline static int32_t CompareSecondaryKeyForLookup(const Key &key, const ByteBufferRef &secondaryByteBuffer,
                                                       uint32_t secondaryKeyOffset, uint64_t secondaryKeyLenInfo)
    {
        uint32_t secondaryKeyLen = GetSecondaryKeyLen(secondaryKeyLenInfo);
        uint32_t secondaryUserKeyLen = GetSecondaryUserKeyLen(secondaryKeyLenInfo);
        uint32_t secondaryUserKeyHash = 0;
        secondaryByteBuffer->ReadUint32(secondaryUserKeyHash, secondaryKeyOffset);
        return CompareSecondaryKey(!IsSortedState(key.StateId()), key.SecKey().KeyData(), 0, key.SecKey().KeyLen(),
                                   key.SecKey().HashCode(), secondaryByteBuffer->Data(),
                                   secondaryKeyOffset + secondaryKeyLen - secondaryUserKeyLen, secondaryUserKeyLen,
                                   secondaryUserKeyHash);
    }

    inline static int32_t CompareUint32t(uint32_t num1, uint32_t num2)
    {
        return BssMath::IntegerCompare(num1, num2);
    }

    inline static bool IsSortedState(uint16_t stateId)
    {
        return false;  // don't support sorted state.
    }

    inline static int32_t CompareSecondaryKey(bool compareHash, const uint8_t *keyData1, uint32_t userKeyOffset1,
                                              uint32_t userKeyLen1, uint32_t hash1, const uint8_t *keyData2,
                                              uint32_t userKeyOffset2, uint32_t userKeyLen2, uint32_t hash2)
    {
        int32_t cmp = compareHash ? CompareUint32t(hash1, hash2) : 0;
        if (cmp != 0) {
            return cmp;
        }
        COMPARE_PARAM_WITH_NULLPTR(keyData1, keyData2);
        uint32_t minLen = std::min(userKeyLen1, userKeyLen2);
        cmp = memcmp(keyData1 + userKeyOffset1, keyData2 + userKeyOffset2, minLen);
        if (cmp == 0) {
            return BssMath::IntegerCompare<uint32_t>(userKeyLen1, userKeyLen2);
        }
        return cmp;
    }

    static uint32_t ComputeRawInternalKeyLen(const ByteBufferRef &byteBuffer, uint32_t offset);

    static FullKeyRef ReadInternalKey(const FileInputViewRef &inputView, const MemManagerRef &memManager,
                                      FileProcHolder holder);

    inline static int32_t CompareStateId(uint16_t stateId1, uint16_t stateId2)
    {
        return BssMath::IntegerCompare(stateId1, stateId2);
    }

    static FullKeyRef CopyInternalKey(const FullKeyRef &fullKey, const MemManagerRef &memManager,
                                      FileProcHolder holder);

    static int32_t CompareKeyWithInternalKey(const Key &key, const FullKeyRef &fullKey);

    static void ReadPrimary(PriKeyNode &priKey, const FileInputViewRef &inputView, const MemManagerRef &memManager,
                            FileProcHolder holder, ByteBufferRef &priBuffer);

    inline static uint16_t ReadStateId(const FileInputViewRef &inputView)
    {
        uint16_t stateId = 0;
        auto ret = inputView->Read(stateId);
        if (UNLIKELY(ret != BSS_OK)) {
            LOG_ERROR("Read state id failed, ret:" << ret);
            return 0;
        }
        return stateId;
    }

    static void ReadSecondaryKey(SecKeyNode &secKey, const FileInputViewRef &inputView, const MemManagerRef &memManager,
                                 FileProcHolder holder, ByteBufferRef &secBuffer);

    static BResult BuildValue(Value &value, const ByteBufferRef &buffer, uint32_t secondaryKeyOffset,
                              uint32_t secondaryKeyLen);
};

}  // namespace bss
}  // namespace ock
#endif  // BOOST_SS_HASH_INTERNAL_KEY_UTIL_H