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

#include "compressor_utils.h"

#include "lz4_interface.h"

namespace ock {
namespace bss {

using Lz4InterfacePtr = Ref<Lz4Interface>;
const CompressAlgo CompressorUtils::mDefaultCodec = CompressAlgo::NONE;
const std::set<CompressAlgo> CompressorUtils::mSupportCodec = { CompressAlgo::NONE, CompressAlgo::LZ4 };

CompressorRef CompressorUtils::InitCompressor(CompressAlgo &codec)
{
    if (!IsSupportCodec(codec)) {
        LOG_WARN("Don't support compress codec " << codec << ", will change to default codec " << mDefaultCodec);
        codec = mDefaultCodec;
    }
    if (codec == CompressAlgo::LZ4) {
        return MakeRef<Lz4Interface>();
    }
    return nullptr;
}
}  // namespace bss
}  // namespace ock