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

#ifndef BOOST_SS_SLICE_STATUS_H
#define BOOST_SS_SLICE_STATUS_H
#include <cstdint>
namespace ock {
namespace bss {

enum class SliceStatus : int8_t {
    NORMAL = 1,
    FLUSHING = 2,
    EVICTED = 3,
    COMPACTING = 4,
};

enum class SliceEvent {
    FLUSH = 1,
    COMPACT = 2,
    EVICT = 3,
    FLUSH_BACK = 4,
    COMPACT_BACK = 5,
};

enum class SliceTableCompaction : int8_t {
    CLOSED = 0,
    OPEN = 1,
};

}  // namespace bss
}  // namespace ock
#endif  // BOOST_SS_SLICE_STATUS_H
