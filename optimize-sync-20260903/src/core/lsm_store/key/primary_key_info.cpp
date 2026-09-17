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

#include "primary_key_info.h"

#include "lsm_store/block/data_block.h"

namespace ock {
namespace bss {
bool PrimaryKeyInfo::IsSingleSecondaryKey() const
{
    return DataBlock::IsSingleSecondaryKey(mFlag);
}

uint32_t PrimaryKeyInfo::GetNumBytesForSecondaryKeyIndexElement() const
{
    return DataBlock::GetNumBytesForSecondaryKeyIndexElement(mFlag);
}

uint32_t PrimaryKeyInfo::GetSecondaryLevelOffset() const
{
    return mPrimaryKeyOffset + FullKeyUtil::GetPrimaryKeyLen(mPrimaryKeyLenInfo) + NO_1;
}

}  // namespace bss
}  // namespace ock