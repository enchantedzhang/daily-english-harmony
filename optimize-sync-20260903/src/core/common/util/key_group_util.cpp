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

#include "key_group_util.h"

#include <new>

#include "common/bss_log.h"

namespace ock {
namespace bss {

constexpr uint32_t KeyGroupUtil::VALID_HASH_BITS;
constexpr uint64_t KeyGroupUtil::HASH_SPACE;

KeyGroupUtil::KeyGroupUtil(uint32_t maxParallelism, uint64_t base, uint64_t extra)
    : mMaxParallelism(maxParallelism),
      mGroupBits([maxParallelism]() {
          uint32_t bits = 0;
          for (uint32_t value = maxParallelism; value > 1; value >>= 1) {
              ++bits;
          }
          return bits;
      }()),
      mQuotientBits(VALID_HASH_BITS - mGroupBits),
      mBase(base),
      mExtra(extra),
      mLongSpan(mExtra * (mBase + 1)),
      mPowerOfTwo((maxParallelism & (maxParallelism - 1)) == 0)
{
}

BResult KeyGroupUtil::Create(uint32_t maxParallelism, KeyGroupUtilRef &result)
{
    result = nullptr;
    if (UNLIKELY(maxParallelism == 0 || maxParallelism > MAX_PARALLELISM)) {
        LOG_ERROR("Invalid maxParallelism: " << maxParallelism << ", valid range: [1, " << MAX_PARALLELISM << "]");
        return BSS_INVALID_PARAM;
    }
    uint64_t base = HASH_SPACE / maxParallelism;
    uint64_t extra = HASH_SPACE % maxParallelism;
    auto *keyGroupUtil = new (std::nothrow) KeyGroupUtil(maxParallelism, base, extra);
    if (UNLIKELY(keyGroupUtil == nullptr)) {
        return BSS_ERR;
    }
    result = KeyGroupUtilRef(keyGroupUtil);
    return BSS_OK;
}

}  // namespace bss
}  // namespace ock
