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
#ifndef BOOST_SS_CONST_H
#define BOOST_SS_CONST_H

#include <cstdint>

namespace ock {
namespace bss {
constexpr uint32_t INDEX_NODE_SIZE_OFFSET = 0;
constexpr uint32_t BUCKET_COUNT_OFFSET = 4;
constexpr uint32_t SECTION_INDEX_BASE_OFFSET = 8;
constexpr uint32_t HASHMAP_INIT_SIZE = 12;

constexpr uint32_t BUCKET_COUNT_PER_SECTION = 8;

constexpr uint32_t SECTION_INDEX_SIZE = 4;
constexpr uint32_t BUCKET_SIZE = 4;

constexpr uint32_t NODE_POINTER_SIZE = 4;
// indexNode相关常量
constexpr uint32_t INDEX_NODE_SIZE = 16;
constexpr uint32_t LIST_INDEX_NODE_SIZE = 20;
constexpr uint32_t HASHCODE_OFFSET = 0;
constexpr uint32_t KEY_NODE_OFFSET = 4;
constexpr uint32_t VALUE_NODE_OFFSET = 8;
constexpr uint32_t NEXT_INDEX_NODE_OFFSET = 12;
constexpr uint32_t LIST_TOTAL_VALUE_LEN_OFFSET = 16;

// valueNode相关常量
constexpr uint32_t VALUE_LENGTH_OFFSET = 0;
constexpr uint32_t VALUE_TYPE_OFFSET = 4;
constexpr uint32_t VALUE_DATA_OFFSET = 5;
constexpr uint32_t HASH_MAP_NODE_SIZE = 12;

// keyNode相关常量
constexpr uint32_t KEY_LENGTH_OFFSET = 0;
constexpr uint32_t KEY_DATA_OFFSET = 4;
}  // namespace bss
}  // namespace ock

#endif  // BOOST_SS_CONST_H