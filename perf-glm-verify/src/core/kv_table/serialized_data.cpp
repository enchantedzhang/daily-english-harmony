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

#include "serialized_data.h"

#include "include/bss_err.h"
#include "include/bss_types.h"
#include "securec.h"

namespace ock {
namespace bss {

// 用于调试时打印日志
std::string SerializedDataWrapper::ToString() const
{
    if (mLength < 1) {
        LOG_ERROR("Invalid mlength value: " << mLength << ".");
        return "";
    }
    std::string str;
    str += "[";

    if (mBuffer != nullptr && mBuffer->Data() != nullptr) {
        uint8_t *data = mBuffer->Data() + mOffset;
        for (uint32_t i = 0; i < mLength - 1; i++) {
            str += std::to_string(data[i]) + ",";
        }
        str += std::to_string(data[mLength - 1]);
    }
    str += "]";
    return str;
}
}  // namespace bss
}  // namespace ock
