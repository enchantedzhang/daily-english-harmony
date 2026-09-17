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

#ifndef BOOST_SS_BINARY_READER_H
#define BOOST_SS_BINARY_READER_H

#include "bss_def.h"
#include "bss_err.h"
#include "bss_log.h"

namespace ock {
namespace bss {
struct BinaryReader {
    inline static BResult ReadBytes(uint8_t *buffer, uint32_t byteNum, uint64_t &resOffset)
    {
        RETURN_INVALID_PARAM_AS_NULLPTR(buffer);
        if (UNLIKELY(byteNum > NO_8)) {
            LOG_ERROR("The actual capacity of the array is exceeded, byteNum: " << byteNum);
            return BSS_INNER_ERR;
        }

        const static uint64_t DATA_MASK_LESS_THEN_8BYTES[] = {
            0, 0xFF, 0xFFFF, 0xFFFFFF, 0xFFFFFFFF, 0xFFFFFFFFFF, 0xFFFFFFFFFFFF, 0xFFFFFFFFFFFFFF, 0xFFFFFFFFFFFFFFFF
        };
        resOffset = *(reinterpret_cast<uint64_t *>(buffer)) & DATA_MASK_LESS_THEN_8BYTES[byteNum];
        return BSS_OK;
    }
};

}  // namespace bss
}  // namespace ock
#endif  // BOOST_SS_BINARY_READER_H
