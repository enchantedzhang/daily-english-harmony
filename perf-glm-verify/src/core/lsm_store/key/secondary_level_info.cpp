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

#include "secondary_level_info.h"

namespace ock {
namespace bss {

BResult SingleSecondaryLevelInfo::GetPair(PrimaryKeyInfo &primaryKeyInfo, const Key &key, Value &value,
                                          ByteBufferRef byteBuffer)
{
    uint32_t offset = mSecondaryKeyOffset;
    uint16_t stateId = FullKeyUtil::ReadStateId(byteBuffer, offset);
    offset += sizeof(uint16_t);

    int32_t cmp = FullKeyUtil::CompareStateId(key.StateId(), stateId);
    uint64_t secondaryKeyLenInfo = 0L;
    if (cmp == 0 && FullKeyUtil::StateHasSecondaryKey(stateId)) {
        secondaryKeyLenInfo = FullKeyUtil::ParseSecondaryKeyLen(byteBuffer, offset);
        cmp = FullKeyUtil::CompareSecondaryKeyForLookup(key, byteBuffer, offset, secondaryKeyLenInfo);
    }

    if (cmp == 0) {
        return FullKeyUtil::BuildValue(value, byteBuffer, offset, FullKeyUtil::GetSecondaryKeyLen(secondaryKeyLenInfo));
    }

    return BSS_NOT_EXISTS;
}

BResult MultiSecondaryLevelInfo::GetPair(PrimaryKeyInfo &primaryKeyInfo, const Key &key, Value &value,
                                         ByteBufferRef byteBuffer)
{
    if (UNLIKELY(mNumSecondaryKeys == 0)) {
        return BSS_NOT_EXISTS;
    }

    bool hasSecondaryKey = FullKeyUtil::StateHasSecondaryKey(key.StateId());
    uint32_t leftSecondaryKeyIndex = 0;
    uint32_t rightSecondaryKeyIndex = mNumSecondaryKeys - NO_1;
    while (leftSecondaryKeyIndex <= rightSecondaryKeyIndex) {
        uint32_t midSecondaryKeyIndex = leftSecondaryKeyIndex + (rightSecondaryKeyIndex - leftSecondaryKeyIndex) / NO_2;
        uint32_t keyIndexElement = static_cast<uint32_t>(FullKeyUtil::ReadValueWithNumberOfBytes(
            byteBuffer, mSecondaryKeyIndexOffset + mNumBytesForSecondaryKeyIndexElement * midSecondaryKeyIndex,
            mNumBytesForSecondaryKeyIndexElement));
        if (UNLIKELY(UINT32_MAX - keyIndexElement < mFirstSecondaryKeyOffset)) {
            LOG_ERROR("Read keyIndexElement is inValid:" << keyIndexElement
                                                         << ", mFirstSecondaryKeyOffset:" << mFirstSecondaryKeyOffset);
            return BSS_ERR;
        }
        uint32_t offset = mFirstSecondaryKeyOffset + keyIndexElement;

        uint16_t stateId = FullKeyUtil::ReadStateId(byteBuffer, offset);
        offset += sizeof(uint16_t);
        int32_t cmp = FullKeyUtil::CompareStateId(key.StateId(), stateId);

        uint64_t secondaryKeyLenInfo = 0L;
        if (cmp == 0 && hasSecondaryKey) {
            secondaryKeyLenInfo = FullKeyUtil::ParseSecondaryKeyLen(byteBuffer, offset);
            cmp = FullKeyUtil::CompareSecondaryKeyForLookup(key, byteBuffer, offset, secondaryKeyLenInfo);
        }

        if (cmp == 0) {
            return FullKeyUtil::BuildValue(value, byteBuffer, offset,
                                           FullKeyUtil::GetSecondaryKeyLen(secondaryKeyLenInfo));
        }

        if (cmp < 0) {
            if (midSecondaryKeyIndex == 0) {
                break;
            }
            rightSecondaryKeyIndex = midSecondaryKeyIndex - 1;
            continue;
        }
        leftSecondaryKeyIndex = midSecondaryKeyIndex + 1;
    }
    return BSS_NOT_EXISTS;
}

}  // namespace bss
}  // namespace ock