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

#include "lz4_interface.h"

namespace ock {
namespace bss {

uint32_t Lz4Interface::Compress(void *dst, size_t dstCapacity, const void *src, size_t srcSize, int compressionLevel)
{
    if (UNLIKELY(src == nullptr || srcSize == 0 || srcSize > INT32_MAX)) {
        LOG_ERROR("Invalid parameter src for lz4 compress, srcSize: " << srcSize);
        return 0;
    }
    if (UNLIKELY(dst == nullptr || dstCapacity == 0 || dstCapacity > INT32_MAX)) {
        LOG_ERROR("Invalid parameter dst for lz4 compress, dstCapacity: " << dstCapacity);
        return 0;
    }

    int result = LZ4_compress_fast((const char *)src, (char *)dst, (int)srcSize, (int)dstCapacity, compressionLevel);
    if (result < 1) {
        LOG_WARN("Failed to compress data with lz4, result: " << result);
        return 0;
    }

    return result;
}

uint32_t Lz4Interface::Decompress(void *dst, size_t dstCapacity, const void *src, size_t srcSize)
{
    if (UNLIKELY(src == nullptr || srcSize == 0 || srcSize > INT32_MAX)) {
        LOG_ERROR("Invalid parameter src for lz4 decompress, srcSize: " << srcSize);
        return 0;
    }
    if (UNLIKELY(dst == nullptr || dstCapacity == 0 || dstCapacity > INT32_MAX)) {
        LOG_ERROR("Invalid parameter dst for lz4 decompress, dstCapacity: " << dstCapacity);
        return 0;
    }

    int result = LZ4_decompress_safe((const char *)src, (char *)dst, (int)srcSize, (int)dstCapacity);
    if (result < 1) {
        LOG_ERROR("Failed to decompress data with lz4, result: " << result);
        return 0;
    }

    return result;
}
}  // namespace bss
}  // namespace ock
