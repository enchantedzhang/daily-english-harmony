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

#ifndef BOOST_SS_FILEHOLDER_H
#define BOOST_SS_FILEHOLDER_H

#include <functional>
#include <memory>
#include <string>
#include <unordered_map>

#include "include/bss_err.h"
#include "restore_file_info.h"

namespace ock {
namespace bss {
class FileHolder {
public:
    virtual ~FileHolder() = default;
    virtual bool MigrateFileMetas(
        std::unordered_map<std::string, RestoreFileInfo> &toMigrateFileMapping,
        std::unordered_map<std::string, std::tuple<uint64_t, uint32_t>> &migratedFileMapping,
        std::function<BResult(std::unordered_map<std::string, std::tuple<uint64_t, uint32_t>>)> consumer)
    {
        return false;
    }

    virtual std::string GetName()
    {
        return "";
    }

    virtual void NotifyStop()
    {
    }
};
using FileHolderRef = std::shared_ptr<FileHolder>;

}  // namespace bss
}  // namespace ock
#endif  // BOOST_SS_FILEHOLDER_H