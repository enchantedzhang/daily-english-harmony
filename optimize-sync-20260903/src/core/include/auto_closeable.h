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

#ifndef BOOST_SS_AUTO_CLOSEABLE_H
#define BOOST_SS_AUTO_CLOSEABLE_H

#include <malloc.h>

#include <atomic>

namespace ock {
namespace bss {
class AutoCloseable {
public:
    AutoCloseable()
    {
        ++GLOBAL_COUNT;
    }

    virtual ~AutoCloseable()
    {
        if (--GLOBAL_COUNT == 0) {
            malloc_trim(0);
        }
    }
    virtual void Close()
    {
        return;
    }

    static std::atomic<uint64_t> GLOBAL_COUNT;
};
}  // namespace bss
}  // namespace ock

#endif