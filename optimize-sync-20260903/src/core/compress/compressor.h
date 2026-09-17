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

#ifndef COMPRESSOR_H
#define COMPRESSOR_H
#include "include/ref.h"

namespace ock {
namespace bss {
class Compressor : public Referable {
public:
    ~Compressor() override = default;

    virtual uint32_t Compress(void *dst, size_t dstCapacity, const void *src, size_t srcSize,
                              int compressionLevel = 1) = 0;

    virtual uint32_t Decompress(void *dst, size_t dstCapacity, const void *src, size_t srcSize) = 0;
};

using CompressorRef = Ref<Compressor>;
}  // namespace bss
}  // namespace ock
#endif
