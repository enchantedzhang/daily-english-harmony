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

#ifndef BOOST_SS_VALUE_TYPE_H
#define BOOST_SS_VALUE_TYPE_H

namespace ock {
namespace bss {
enum ValueType : unsigned char {
    DELETE = 1,
    PUT = 2,
    APPEND = 3,
    SEPARATE = 4,
    TYPE_BUTT = 5,
};
}
}  // namespace ock

#endif  // BOOST_SS_VALUE_TYPE_H