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

#ifndef BOOST_SS_RESTOREFILEINFO_H
#define BOOST_SS_RESTOREFILEINFO_H

#include <string>

#include "lsm_store/file/file_cache_type.h"

namespace ock {
namespace bss {
struct RestoreFileInfo {
    std::string fileIdentifier;  // snapshot时文件地址
    uint64_t fileAddress;
    FileStatus fileStatus;
    uint64_t fileLength;
    uint32_t refCount;
    uint64_t remoteFileAddress;
    std::string remoteFileName;        // 远程文件地址
    std::string restoreLocalFileName;  // restore后本地文件地址
};

}  // namespace bss
}  // namespace ock
#endif  // BOOST_SS_RESTOREFILEINFO_H