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

#ifndef BUFFER_H
#define BUFFER_H

#include "ref.h"

namespace ock {
namespace bss {
class Buffer : public Referable {
public:
    virtual ~Buffer()
    {
    }

    virtual uint8_t *Data()
    {
        return nullptr;
    }

    virtual uint32_t Length()
    {
        return 0;
    }
};
using BufferRef = Ref<Buffer>;
}  // namespace bss
}  // namespace ock

#endif
