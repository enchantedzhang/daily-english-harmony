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

#ifndef LZ4_INTERFACE_H
#define LZ4_INTERFACE_H
#include "bss_log.h"
#include "compressor.h"
#include "lz4.h"
#include "util/bss_def.h"

namespace ock {
namespace bss {
class Lz4Interface : public Compressor {
public:
    Lz4Interface() = default;
    ~Lz4Interface() override = default;

    uint32_t Compress(void *dst, size_t dstCapacity, const void *src, size_t srcSize,
                      int compressionLevel = 1) override;

    uint32_t Decompress(void *dst, size_t dstCapacity, const void *src, size_t srcSize) override;
};
}  // namespace bss
}  // namespace ock
#endif