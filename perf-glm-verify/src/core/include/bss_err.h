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

#ifndef BOOST_STATE_STORE_ERR_H
#define BOOST_STATE_STORE_ERR_H

#include <cstdint>

namespace ock {
namespace bss {
using BResult = int32_t;
enum Error : int32_t {
    BSS_OK = 0,
    BSS_ERR = 1,
    BSS_INNER_ERR = 2,
    BSS_INVALID_PARAM = 3,
    BSS_NOT_READY = 4,
    BSS_ALLOC_FAIL = 5,
    BSS_NOT_INITIALIZED = 6,
    BSS_NOT_EXISTS = 7,
    BSS_ALREADY_EXISTS = 8,
    BSS_ALREADY_DONE = 9,
    BSS_EXISTS = 10,
    BSS_INNER_RETRY = 11,
    BSS_NO_COMPRESS = 12,
    BSS_NOT_FOUND = 13,
    BSS_NOT_SUPPORTED = 14,
    BSS_BLOB_NOT_SNAPSHOT = 15,
    BSS_IO_ERR = 201,

    BSS_FRESH_TABLE_IS_FULL = 500,
    BSS_FRESH_TABLE_ALLOCATE_FAIL = 501,

    BSS_MAX,
};
}  // namespace bss
}  // namespace ock

#endif
