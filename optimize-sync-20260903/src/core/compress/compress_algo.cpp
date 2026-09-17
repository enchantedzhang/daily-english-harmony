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

#include "include/compress_algo.h"

#include "common/bss_log.h"

namespace ock {
namespace bss {
CompressAlgo CompressAlgoUtil::CompressAlgoTransform(const std::string &compressAlgo)
{
    if (compressAlgo == "lz4") {
        return CompressAlgo::LZ4;
    } else if (compressAlgo == "none") {
        return CompressAlgo::NONE;
    } else {
        LOG_ERROR("Invalid compress algo, compressAlgo: " << compressAlgo);
        return CompressAlgo::NONE;
    }
}
std::string CompressAlgoUtil::ReverseCompressAlgoTransform(CompressAlgo &compressAlgo)
{
    switch (compressAlgo) {
        case CompressAlgo::LZ4:
            return "lz4";
        case CompressAlgo::NONE:
            return "none";
        default:
            LOG_ERROR("Invalid compress algo, compressAlgo: " << compressAlgo);
            return "none";
    }
}
}  // namespace bss
}  // namespace ock