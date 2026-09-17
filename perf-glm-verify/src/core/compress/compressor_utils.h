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

#ifndef BLOB_COMPRESSOR_H
#define BLOB_COMPRESSOR_H
#include <set>

#include "bss_log.h"
#include "compressor.h"
#include "include/compress_algo.h"
#include "include/ref.h"

namespace ock {
namespace bss {
class CompressorUtils {
public:
    static CompressorRef InitCompressor(CompressAlgo &codec);

    static inline bool IsSupportCodec(CompressAlgo &codec)
    {
        if (mSupportCodec.find(codec) != mSupportCodec.end()) {
            return true;
        }
        LOG_ERROR("Compress policy is not support, codec: " << codec);
        return false;
    }

    static inline int64_t MaxCompressedLength(int64_t length)
    {
        if (length < 0) {
            LOG_ERROR("Length must be >= 0, length: " << length);
            return 0;
        }
        if (length >= NO_MAX_INT32_VALUE_2G) {
            LOG_ERROR("Length must be less than 2G.");
            return 0;
        }
        return length + length / NO_255 + NO_16;
    }

private:
    static const CompressAlgo mDefaultCodec;
    static const std::set<CompressAlgo> mSupportCodec;
};
}  // namespace bss
}  // namespace ock

#endif
