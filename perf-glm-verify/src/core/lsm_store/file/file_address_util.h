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

#ifndef BOOST_SS_FILE_ADDRESS_UTIL_H
#define BOOST_SS_FILE_ADDRESS_UTIL_H

#include "include/bss_types.h"

namespace ock {
namespace bss {
class FileAddressUtil {
public:
    inline static uint32_t GetFileId(uint64_t fileAddress)
    {
        return static_cast<uint32_t>(fileAddress >> NO_32);
    }

    inline static uint32_t GetFileOffset(uint64_t address)
    {
        return static_cast<uint32_t>(address & 0xFFFFFFFFL);
    }

    inline static uint64_t GetFileAddressWithZeroOffset(uint32_t fileId)
    {
        return static_cast<uint64_t>(fileId) << NO_32;
    }
};

}  // namespace bss
}  // namespace ock
#endif  // BOOST_SS_FILE_ADDRESS_UTIL_H