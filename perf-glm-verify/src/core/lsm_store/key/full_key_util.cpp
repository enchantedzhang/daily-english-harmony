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

#include "full_key_util.h"

#include "common/util/var_encoding_util.h"

namespace ock {
namespace bss {
uint32_t FullKeyUtil::MinNumBytesToRepresentValue(uint32_t value)
{
    if (value <= NO_255) {
        return NO_1;
    }
    if (value <= NO_65535) {
        return NO_2;
    }
    if (value <= NO_16777215) {
        return NO_3;
    }
    return NO_4;
}

FullKeyRef FullKeyUtil::ToFullKey(const KeyValueRef &keyValue, const MemManagerRef &memManager, FileProcHolder holder)
{
    auto &key = keyValue->key;
    auto &value = keyValue->value;
    auto &priKey = key.PriKey();
    uint32_t priKeyLen = priKey.KeyLen();

    bool hasSecKey = key.HasSecKey();
    uint32_t secKeyLen = hasSecKey ? key.SecKey().KeyLen() : 0;
    uint32_t userKeyLen = priKeyLen + secKeyLen;
    auto addr = FileMemAllocator::Alloc(memManager, holder, userKeyLen, __FUNCTION__);
    if (UNLIKELY(addr == 0)) {
        return nullptr;
    }
    ByteBufferRef newBuffer = MakeRef<ByteBuffer>(reinterpret_cast<uint8_t *>(addr), userKeyLen, memManager);
    if (UNLIKELY(newBuffer == nullptr)) {
        memManager->ReleaseMemory(addr);
        LOG_ERROR("New buffer is nullptr, key:" << key.ToString());
        return nullptr;
    }
    auto ret = newBuffer->WriteAt(priKey.KeyData(), priKeyLen, 0);
    if (UNLIKELY(ret != BSS_OK)) {
        return nullptr;
    }

    if (hasSecKey) {
        auto &secKey = key.SecKey();
        ret = newBuffer->WriteAt(secKey.KeyData(), secKeyLen, priKeyLen);
        if (UNLIKELY(ret != BSS_OK)) {
            return nullptr;
        }
    }
    FullKeyRef fullKey = std::make_shared<FullKey>();
    fullKey->Init(key.PriKey(), key.SecKey(), value.SeqId(), static_cast<ValueType>(value.ValueType()), newBuffer);
    fullKey->PriKeyData(newBuffer->Data());
    if (hasSecKey) {
        fullKey->SecKeyData(newBuffer->Data() + priKeyLen);
    }
    return fullKey;
}

uint32_t FullKeyUtil::ComputeRawInternalKeyLen(const ByteBufferRef &byteBuffer, uint32_t offset)
{
    uint32_t tmpOffset = offset;
    uint64_t primaryKeyLenInfo = ParsePrimaryKeyLen(byteBuffer, tmpOffset);
    uint32_t primaryKeyLen = GetPrimaryKeyLen(primaryKeyLenInfo);
    tmpOffset += primaryKeyLen;

    uint16_t stateId = ReadStateId(byteBuffer, offset + primaryKeyLen);
    tmpOffset += sizeof(uint16_t);

    if (StateHasSecondaryKey(stateId)) {
        uint64_t secondaryKeyLenInfo = ParseSecondaryKeyLen(byteBuffer, tmpOffset);
        tmpOffset += GetSecondaryKeyLen(secondaryKeyLenInfo);
    }
    tmpOffset += ComputeSeqIdAndValueTypeLen();
    return tmpOffset - offset;
}

FullKeyRef FullKeyUtil::ReadInternalKey(const FileInputViewRef &inputView, const MemManagerRef &memManager,
                                        FileProcHolder holder)
{
    PriKeyNode priKey;
    ByteBufferRef priBuffer;
    ReadPrimary(priKey, inputView, memManager, holder, priBuffer);
    SecKeyNode secKey;
    ByteBufferRef secBuffer = nullptr;
    if (StateHasSecondaryKey(priKey.StateId())) {
        ReadSecondaryKey(secKey, inputView, memManager, holder, secBuffer);
    }
    uint64_t seqId = 0;
    RETURN_NULLPTR_AS_NOT_OK(inputView->Read(seqId));
    uint8_t valueType = 0;
    RETURN_NULLPTR_AS_NOT_OK(inputView->Read(valueType));
    FullKeyRef fullKey = std::make_shared<FullKey>();
    if (secBuffer == nullptr) {
        fullKey->Init(priKey, secKey, seqId, static_cast<ValueType>(valueType), priBuffer);
    } else {
        BufferRef buffer = MakeRef<CompositeBuffer>(priBuffer, secBuffer);
        fullKey->Init(priKey, secKey, seqId, static_cast<ValueType>(valueType), buffer);
    }
    return fullKey;
}

FullKeyRef FullKeyUtil::CopyInternalKey(const FullKeyRef &fullKey, const MemManagerRef &memManager,
                                        FileProcHolder holder)
{
    RETURN_NULLPTR_AS_NULLPTR(fullKey);

    bool hasSecondaryKey = fullKey->HasSecKey();
    uint32_t primaryUserKeyLen = fullKey->PriKey().KeyLen();
    uint32_t secondaryUserKeyLen = fullKey->SecKey().KeyLen();
    uint32_t userKeyLen = primaryUserKeyLen + (hasSecondaryKey ? secondaryUserKeyLen : 0);
    auto addr = FileMemAllocator::Alloc(memManager, holder, userKeyLen, __FUNCTION__);
    if (UNLIKELY(addr == 0)) {
        return nullptr;
    }
    ByteBufferRef newBuffer = MakeRef<ByteBuffer>(reinterpret_cast<uint8_t *>(addr), userKeyLen, memManager);
    if (UNLIKELY(newBuffer == nullptr)) {
        memManager->ReleaseMemory(addr);
        LOG_ERROR("Make new buffer failed, because of out of memory.");
        return nullptr;
    }
    newBuffer->WriteAt(fullKey->PriKey().KeyData(), primaryUserKeyLen, 0);
    fullKey->PriKeyData(newBuffer->Data());
    if (hasSecondaryKey) {
        newBuffer->WriteAt(fullKey->SecKey().KeyData(), secondaryUserKeyLen, primaryUserKeyLen);
        fullKey->SecKeyData(newBuffer->Data() + primaryUserKeyLen);
    }
    FullKeyRef fullKeyCopy = std::make_shared<FullKey>();
    fullKeyCopy->Init(fullKey->PriKey(), fullKey->SecKey(), fullKey->SeqId(), fullKey->ValueType(), newBuffer);
    return fullKeyCopy;
}

int32_t FullKeyUtil::CompareKeyWithInternalKey(const Key &key, const FullKeyRef &fullKey)
{
    if (UNLIKELY(fullKey->Buffer() == nullptr)) {
        LOG_ERROR("full key primary user key is nullptr.");
        return -1;
    }
    if (fullKey->IsPqKey()) {
        return 1;
    }
    int cmp = ComparePrimaryKey(key.PriKey().KeyData(), 0, key.PriKey().KeyLen(), key.PriKey().KeyHashCode(),
                                const_cast<uint8_t *>(fullKey->PriKey().KeyData()), 0, fullKey->PriKey().KeyLen(),
                                fullKey->KeyHashCode());
    if (cmp != 0) {
        return cmp;
    }

    cmp = CompareStateId(key.StateId(), static_cast<int32_t>(fullKey->StateId()));
    if (cmp != 0) {
        return cmp;
    }

    if (key.IsEndKey()) {
        return 1;
    }

    if (!StateHasSecondaryKey(key.StateId())) {
        return 0;
    }

    if (key.IsPrefixKey()) {
        return -1;
    }

    return CompareSecondaryKey(!IsSortedState(key.StateId()), key.SecKey().KeyData(), 0, key.SecKey().KeyLen(),
                               key.SecKey().HashCode(), fullKey->SecKey().KeyData(), 0, fullKey->SecKey().KeyLen(),
                               fullKey->SecKey().HashCode());
}

void FullKeyUtil::ReadPrimary(PriKeyNode &priKey, const FileInputViewRef &inputView, const MemManagerRef &memManager,
                              FileProcHolder holder, ByteBufferRef &priBuffer)
{
    if (UNLIKELY(inputView == nullptr)) {
        LOG_ERROR("File input view is nullptr.");
        return;
    }
    uint32_t primaryUserKeyHash = 0;
    RETURN_AS_NOT_OK_NO_LOG(inputView->Read(primaryUserKeyHash));
    uint32_t keyLen = 0;
    RETURN_AS_NOT_OK_NO_LOG(inputView->Read(keyLen));
    auto addr = FileMemAllocator::Alloc(memManager, holder, keyLen, __FUNCTION__);
    if (UNLIKELY(addr == 0)) {
        return;
    }
    priBuffer = MakeRef<ByteBuffer>(reinterpret_cast<uint8_t *>(addr), keyLen, memManager);
    if (UNLIKELY(priBuffer == nullptr)) {
        memManager->ReleaseMemory(addr);
        LOG_ERROR("Make buffer failed, because of out of memory.");
        return;
    }
    auto ret = inputView->ReadBuffer(priBuffer->Data(), keyLen);
    if (UNLIKELY(ret != BSS_OK)) {
        LOG_ERROR("Read primary from file failed, ret:" << ret << ", length:" << keyLen);
        return;
    }
    uint16_t stateId = ReadStateId(inputView);
    uint32_t nsHashcode = StateId::HasNameSpace(stateId) ? *reinterpret_cast<const uint32_t *>(priBuffer->Data()) : 0;
    priKey.Init(stateId, primaryUserKeyHash ^ nsHashcode, priBuffer->Data(), keyLen);
}

void FullKeyUtil::ReadSecondaryKey(SecKeyNode &secKey, const FileInputViewRef &inputView,
                                   const MemManagerRef &memManager, FileProcHolder holder, ByteBufferRef &secBuffer)
{
    if (UNLIKELY(inputView == nullptr)) {
        LOG_ERROR("Input view is nullptr.");
        return;
    }
    uint32_t secondaryUserKeyHash = 0;
    RETURN_AS_NOT_OK_NO_LOG(inputView->Read(secondaryUserKeyHash));
    uint32_t keyLen = 0;
    RETURN_AS_NOT_OK_NO_LOG(inputView->Read(keyLen));
    auto addr = FileMemAllocator::Alloc(memManager, holder, keyLen, __FUNCTION__);
    if (UNLIKELY(addr == 0)) {
        return;
    }
    secBuffer = MakeRef<ByteBuffer>(reinterpret_cast<uint8_t *>(addr), keyLen, memManager);
    if (UNLIKELY(secBuffer == nullptr)) {
        memManager->ReleaseMemory(addr);
        LOG_ERROR("Make buffer failed, because of out of memory.");
        return;
    }
    auto ret = inputView->ReadBuffer(secBuffer->Data(), keyLen);
    if (UNLIKELY(ret != BSS_OK)) {
        LOG_ERROR("Read secondary key from file failed, ret:" << ret << ", length:" << keyLen);
        return;
    }
    secKey.Init(secondaryUserKeyHash, secBuffer->Data(), keyLen);
}

BResult FullKeyUtil::BuildValue(Value &value, const ByteBufferRef &buffer, uint32_t secondaryKeyOffset,
                                uint32_t secondaryKeyLen)
{
    uint64_t seqId = 0;
    buffer->ReadUint64(seqId, secondaryKeyOffset + secondaryKeyLen);
    uint8_t valueType = INVALID_U8;
    buffer->ReadUint8(valueType, secondaryKeyOffset + secondaryKeyLen + NO_8);
    uint32_t valueOffset = secondaryKeyOffset + secondaryKeyLen + FullKeyUtil::ComputeSeqIdAndValueTypeLen();
    uint64_t decodedResult = VarEncodingUtil::DecodeUnsignedInt(buffer, valueOffset);
    uint32_t encodedSize = VarEncodingUtil::GetNumberOfEncodedBytes(decodedResult);
    uint32_t valueLen = VarEncodingUtil::GetDecodedValue(decodedResult);
    value.Init(valueType, valueLen, buffer->Data() + valueOffset + encodedSize, seqId, buffer);
    return BSS_OK;
}

}  // namespace bss
}  // namespace ock