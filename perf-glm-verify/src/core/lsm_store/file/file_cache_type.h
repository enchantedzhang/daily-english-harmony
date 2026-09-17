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

#ifndef BOOST_SS_FILE_CACHE_TYPE_H
#define BOOST_SS_FILE_CACHE_TYPE_H

namespace ock {
namespace bss {
enum class FileCacheType { NONE = 0, INFINITE = 1, LIMITED = 2 };

enum FileStatus : uint8_t { LOCAL = 1, DFS = 2 };

}  // namespace bss
}  // namespace ock
#endif  // BOOST_SS_FILE_CACHE_TYPE_H