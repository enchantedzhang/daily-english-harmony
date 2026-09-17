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
#ifndef BOOST_SS_NODETYPE_H
#define BOOST_SS_NODETYPE_H

#include <cstdint>

namespace ock {
namespace bss {
enum class NodeType : uint8_t {
    SERIALIZED = 0,
    HASHMAP = 1,
    COMPOSITE = 2,
    NONE = 4,
};
}
}  // namespace ock
#endif  // BOOST_SS_NODETYPE_H