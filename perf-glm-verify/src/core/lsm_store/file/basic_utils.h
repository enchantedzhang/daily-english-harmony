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

#ifndef BSS_DEV_BASIC_UTILS_H
#define BSS_DEV_BASIC_UTILS_H

#include <cstdint>
#include <vector>

#include "lsm_store/file/file_meta_data.h"

namespace ock {
namespace bss {
class BasicUtils {
public:
    inline static uint64_t TotalFileSize(const std::vector<FileMetaDataRef> &files)
    {
        uint64_t fileSize = 0UL;
        for (auto &file : files) {
            fileSize += file->GetFileSize();
        }
        return fileSize;
    }
};

}  // namespace bss
}  // namespace ock
#endif  // BSS_DEV_BASIC_UTILS_H