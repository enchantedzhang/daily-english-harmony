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

#include "filter_block_writer.h"

namespace ock {
namespace bss {

BResult HashFilterBlockWriter::Finish(uint8_t *output, uint32_t outputSize)
{
    RETURN_INVALID_PARAM_AS_NULLPTR(output);
    if (UNLIKELY(mFilterDataLen == 0)) {
        LOG_ERROR("Unexpected: mFilterDataLen is zero.");
        return BSS_ERR;
    }
    auto ret = memset_s(output, outputSize, 0, mFilterDataLen);
    if (UNLIKELY(ret != EOK)) {
        LOG_ERROR("HashFilterBlockWriter Finish failed, ret: " << ret << " .");
        return BSS_ERR;
    }
    auto bits = mFilterDataLen * NO_8;
    for (uint32_t hash : mHashCodes) {
        uint32_t delta = (hash >> NO_17) | (hash << NO_15);
        for (uint32_t j = 0; j < mProbes; j++) {
            auto bitPosition = static_cast<uint32_t>((static_cast<uint64_t>(hash) % static_cast<uint32_t>(bits)));
            uint32_t i = bitPosition / NO_8;
            output[i] = static_cast<uint8_t>(output[i] | (1 << (bitPosition % NO_8)));
            hash += delta;
        }
    }
    output[mFilterDataLen] = mProbes;
    output[mFilterDataLen + 1] = mFilterType;
    return BSS_OK;
}

bool FilterBlock::KeyMayMatch(uint32_t mixHashCode)
{
    if (UNLIKELY(mLength <= 1)) {
        return false;
    }

    uint32_t bits = (mLength - 1) * NO_8;
    if (UNLIKELY(mProbes > NO_30)) {
        return true;
    }

    uint32_t hashCode = mixHashCode;
    uint32_t delta = hashCode >> NO_17 | hashCode << NO_15;
    for (uint32_t i = 0; i < mProbes; i++) {
        auto bitPosition = static_cast<uint32_t>((static_cast<uint64_t>(hashCode) % bits));
        uint8_t value = 0;
        if (UNLIKELY(mBuffer == nullptr)) {
            LOG_ERROR("buffer is null, mixHashCode: " << mixHashCode);
            return false;
        }
        mBuffer->ReadUint8(value, bitPosition / NO_8);
        if ((value & (1 << (bitPosition % NO_8))) == 0) {
            return false;
        }
        hashCode += delta;
    }
    return true;
}
}  // namespace bss
}  // namespace ock