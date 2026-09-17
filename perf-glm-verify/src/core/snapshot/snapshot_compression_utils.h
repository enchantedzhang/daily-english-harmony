/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#ifndef BOOST_SS_SNAPSHOT_COMPRESSION_UTILS_H
#define BOOST_SS_SNAPSHOT_COMPRESSION_UTILS_H

#include <cstdint>

#include "include/bss_err.h"
#include "include/compress_algo.h"

namespace ock {
namespace bss {
class SnapshotCompressionUtils {
public:
    static BResult TryCompressInto(CompressAlgo policy, const uint8_t *rawData, uint32_t rawLength, uint8_t *storedData,
                                   uint32_t storedCapacity, CompressAlgo &storedAlgo, uint32_t &storedLength);

    static BResult Decompress(CompressAlgo storedAlgo, const uint8_t *storedData, uint32_t storedLength,
                              uint8_t *rawData, uint32_t rawLength);

private:
    static BResult CopyRaw(const uint8_t *rawData, uint32_t rawLength, uint8_t *storedData, uint32_t storedCapacity,
                           CompressAlgo &storedAlgo, uint32_t &storedLength);
};
}  // namespace bss
}  // namespace ock

#endif  // BOOST_SS_SNAPSHOT_COMPRESSION_UTILS_H
