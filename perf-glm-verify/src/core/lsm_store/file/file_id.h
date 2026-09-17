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

#ifndef BOOST_SS_FILE_ID_H
#define BOOST_SS_FILE_ID_H

#include <memory>

#include "include/bss_err.h"

namespace ock {
namespace bss {

struct FileModeInfo {
    uint32_t maxAllowedUniqueID = 2097151;
    uint32_t uniqueIDMask = 2097151;
    uint32_t initValue = 0;
};

class FileId {
public:
    FileId() = default;

    inline BResult Init(uint32_t fileIdValue)
    {
        mValue = fileIdValue;
        mUniqueId = fileIdValue & mFileMode.uniqueIDMask;
        return BSS_OK;
    }

    inline uint32_t Get() const
    {
        return mValue;
    }

    inline uint32_t GetUniqueId() const
    {
        return mUniqueId;
    }

private:
    FileModeInfo mFileMode;
    uint32_t mUniqueId = 0;
    uint32_t mValue = 0;
};
using FileIdRef = std::shared_ptr<FileId>;

}  // namespace bss
}  // namespace ock
#endif  // BOOST_SS_FILE_ID_H