/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#ifndef BOOST_SS_VAR_ENCODING_UTIL_H
#define BOOST_SS_VAR_ENCODING_UTIL_H

#include "common/io/output_view.h"
#include "include/bss_types.h"

namespace ock {
namespace bss {
class VarEncodingUtil {
public:
    // 无符号int类型变长编码
    // 每7bit为一组，编为一个字节，第一个bit为1表示还有后续字节，为0表示没有后续
#define ENABLE_ENCODE_UINT false
    static int32_t EncodeUnsignedInt(uint32_t value, const OutputViewRef &outputView)
    {
        if (UNLIKELY(outputView == nullptr)) {
            LOG_ERROR("The outputView is invalid.");
            return BSS_INVALID_PARAM;
        }

        return outputView->WriteUint32(value);
#if ENABLE_ENCODE_UINT
        // 0xFFFFFF80: 1111,1111 1111,1111 1111,1111 1000,0000
        if ((value & 0xFFFFFF80) == 0) {
            outputView->WriteUint8(static_cast<uint8_t>(value));
            return NO_1;
        }

        // 0x80: 0000,0000 0000,0000 0000,0000 1000,0000
        auto temp = (int8_t)(value | 0x80);
        outputView->Write(reinterpret_cast<uint8_t *>(&temp), sizeof(temp));
        value >>= NO_7;
        if ((value & 0xFFFFFF80) == 0) {
            outputView->WriteUint8(static_cast<uint8_t>(value));
            return NO_2;
        }

        temp = (int8_t)(value | 0x80);
        outputView->Write(reinterpret_cast<uint8_t *>(&temp), sizeof(temp));
        value >>= NO_7;
        if ((value & 0xFFFFFF80) == 0) {
            outputView->WriteUint8(static_cast<uint8_t>(value));
            return NO_3;
        }
        temp = (int8_t)(value | 0x80);
        outputView->Write(reinterpret_cast<uint8_t *>(&temp), sizeof(temp));
        value >>= NO_7;
        if ((value & 0xFFFFFF80) == 0) {
            outputView->WriteUint8(static_cast<uint8_t>(value));
            return NO_4;
        }
        temp = (int8_t)(value | 0x80);
        outputView->Write(reinterpret_cast<uint8_t *>(&temp), sizeof(temp));
        value >>= NO_7;
        outputView->WriteUint8(static_cast<uint8_t>(value));
        return NO_5;
#endif
    }

    inline static uint64_t DecodeUnsignedInt(const ByteBufferRef &buffer, uint32_t pos)
    {
        uint32_t value = 0;
        auto ret = buffer->ReadUint32(value, pos);
        if (UNLIKELY(ret != BSS_OK)) {
            LOG_ERROR("Read fail, pos: " << pos << ", ret: " << ret);
        }
        return (static_cast<uint64_t>(NO_4) << NO_32) | ((0xFFFFFFFFL) & value);
#if ENABLE_ENCODE_UINT
        uint32_t numBytes = 0;
        int32_t value = 0;
        int8_t read = 0;
        if (buffer->ReadInt8(read, pos++) != BSS_OK) {
            LOG_ERROR("decode failed. buffer cap: " << buffer->Capacity() << ", pos: " << pos);
        }
        value = read;
        if (value >= 0) {
            numBytes = 1;
        } else if (buffer->ReadInt8(read, pos++) == BSS_OK && (value ^= static_cast<int32_t>(read) << NO_7) < 0) {
            value ^= 0xFFFFFF80;
            numBytes = NO_2;
        } else if (buffer->ReadInt8(read, pos++) == BSS_OK && (value ^= static_cast<int32_t>(read) << NO_14) >= 0) {
            value ^= 0x3F80;
            numBytes = NO_3;
        } else if (buffer->ReadInt8(read, pos++) == BSS_OK && (value ^= static_cast<int32_t>(read) << NO_21) < 0) {
            value ^= 0xFFE03F80;
            numBytes = NO_4;
        } else {
            buffer->ReadInt8(read, pos);
            value ^= static_cast<int32_t>(read) << NO_28;
            value ^= 0xFE03F80;
            numBytes = NO_5;
        }
        return (static_cast<uint64_t>(numBytes) << NO_32) | ((0xFFFFFFFFL) & value);
#endif
    }

    inline static uint32_t GetDecodedValue(uint64_t decodedResult)
    {
        return static_cast<uint32_t>(decodedResult);
    }

    inline static uint32_t GetNumberOfEncodedBytes(uint64_t decodedResult)
    {
        return static_cast<uint32_t>(decodedResult >> NO_32);
    }
};
}  // namespace bss
}  // namespace ock

#endif  // BOOST_SS_VAR_ENCODING_UTIL_H
