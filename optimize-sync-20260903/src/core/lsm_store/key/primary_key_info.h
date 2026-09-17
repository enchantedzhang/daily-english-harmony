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

#ifndef BOOST_SS_PRIMARY_KEY_INFO_H
#define BOOST_SS_PRIMARY_KEY_INFO_H

#include <cstdint>
#include <memory>

namespace ock {
namespace bss {
class PrimaryKeyInfo {
public:
    PrimaryKeyInfo() = default;

    PrimaryKeyInfo(uint32_t primaryKeyIndex, uint32_t primaryKeyOffset, uint64_t primaryKeyLenInfo, uint8_t flag)
        : mPrimaryKeyIndex(primaryKeyIndex),
          mPrimaryKeyOffset(primaryKeyOffset),
          mPrimaryKeyLenInfo(primaryKeyLenInfo),
          mFlag(flag)
    {
    }

    void Init(uint32_t primaryKeyIndex, uint32_t primaryKeyOffset, uint64_t primaryKeyLenInfo, uint8_t flag)
    {
        mPrimaryKeyIndex = primaryKeyIndex;
        mPrimaryKeyOffset = primaryKeyOffset;
        mPrimaryKeyLenInfo = primaryKeyLenInfo;
        mFlag = flag;
    }

    bool IsSingleSecondaryKey() const;

    uint32_t GetNumBytesForSecondaryKeyIndexElement() const;

    uint32_t GetSecondaryLevelOffset() const;

public:
    uint32_t mPrimaryKeyIndex = 0;
    uint32_t mPrimaryKeyOffset = 0;
    uint64_t mPrimaryKeyLenInfo = 0;
    uint8_t mFlag = 0;
};
using PrimaryKeyInfoRef = std::shared_ptr<PrimaryKeyInfo>;

}  // namespace bss
}  // namespace ock
#endif  // BOOST_SS_PRIMARY_KEY_INFO_H