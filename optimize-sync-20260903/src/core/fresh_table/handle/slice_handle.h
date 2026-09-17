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

#ifndef BOOST_SS_SLICEHANDLE_H
#define BOOST_SS_SLICEHANDLE_H

#include <memory>

#include "include/bss_err.h"
#include "slice_table/slice_table.h"
namespace ock {
namespace bss {

struct SliceIndexContextHash {
    uint32_t operator()(const SliceIndexContextRef &sliceIndexContext) const
    {
        return std::hash<LogicalSliceChain *>()(sliceIndexContext->GetLogicalSliceChain().get());
    }
};

struct SliceIndexContextEqual {
    bool operator()(const SliceIndexContextRef &a, const SliceIndexContextRef &b) const
    {
        RETURN_FALSE_AS_NULLPTR(a);
        RETURN_FALSE_AS_NULLPTR(b);
        return a->GetSliceIndexSlot() == b->GetSliceIndexSlot();
    }
};

}  // namespace bss
}  // namespace ock

#endif  // BOOST_SS_SLICEHANDLE_H
