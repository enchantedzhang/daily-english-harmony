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

#ifndef BOOST_SS_STATE_TYPE_H
#define BOOST_SS_STATE_TYPE_H

namespace ock {
namespace bss {
enum StateType : uint8_t {
    PQ = 0,
    VALUE = 1,
    LIST = 2,
    MAP = 3,
    SUB_VALUE = 4,
    SUB_LIST = 5,
    SUB_MAP = 6,
};

class StateTypeUtil {
public:
    inline static bool HasSecKey(StateType type)
    {
        return (type == MAP || type == SUB_VALUE || type == SUB_MAP);
    }

    inline static bool HasNameSpace(StateType type)
    {
        return type > SUB_VALUE;
    }
};
}  // namespace bss
}  // namespace ock

#endif
