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

#ifndef BOOST_STATE_STORE_DEF_H
#define BOOST_STATE_STORE_DEF_H

#include <memory>

#include "include/bss_err.h"
#include "include/bss_types.h"

namespace ock {
namespace bss {
#ifndef ROUND_UP
#define ROUND_UP(x, align) (((x) + (align)-1) & ~((align)-1))
#endif

#ifndef ROUND_DOWN
#define ROUND_DOWN(x, align) ((x) & ~((align)-1))
#endif

#ifndef LIKELY
#define LIKELY(x) (__builtin_expect(!!(x), 1) != 0)
#endif

#ifndef UNLIKELY
#define UNLIKELY(x) (__builtin_expect(!!(x), 0) != 0)
#endif

#ifndef UNREFERENCE_PARAM
#define UNREFERENCE_PARAM(x) ((void)(x))
#endif

#ifndef COMPARE_PARAM_WITH_NULLPTR
#define COMPARE_PARAM_WITH_NULLPTR(k1, k2)                  \
    do {                                                    \
        if (UNLIKELY((k1) == nullptr && (k2) == nullptr)) { \
            return 0;                                       \
        }                                                   \
        if (UNLIKELY((k1) == nullptr && (k2) != nullptr)) { \
            return -1;                                      \
        }                                                   \
        if (UNLIKELY((k1) != nullptr && (k2) == nullptr)) { \
            return 1;                                       \
        }                                                   \
    } while (0)
#endif

#define KB_UNIT (1024)

#define POWER_OF_2(x) ((((x)-1) & (x)) == 0)

#define FLOAT_MAX_VALUE std::numeric_limits<float>::max()
#define DOUBLE_MAX_VALUE std::numeric_limits<double>::max()

#define MA_LEN_LEN (NO_4)
#define MA_CANARY_LEN (NO_5)
#define MA_META_DATA_RESERVE_LEN                                                  \
    (NO_16) /* total reserve length, for memcpy's performance, 16 bytes alignment \
is best */

#define SUPPORT_SLICE_SORT true
#define ENABLE_MEMORY_TRACKER false
#define ENABLE_MEMORY_MONITOR false
}  // namespace bss
}  // namespace ock
#endif  // BOOST_STATE_STORE_DEF_H