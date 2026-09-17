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

#ifndef BOOST_SS_FILE_META_BASE_H
#define BOOST_SS_FILE_META_BASE_H

#include <iomanip>
#include <memory>
#include <string>
#include <utility>

#include "common/path_transform.h"
#include "file_address_util.h"

namespace ock {
namespace bss {
class FileMetaBase {
public:
    FileMetaBase() = default;

    explicit FileMetaBase(std::string identifier, uint64_t fileAddress = 0, uint64_t fileSize = 0)
        : mIdentifier(std::move(identifier)), mFileAddress(fileAddress), mFileSize(fileSize)
    {
    }

    inline uint64_t GetFileAddress() const
    {
        return mFileAddress;
    }

    inline const std::string &GetIdentifier() const
    {
        return mIdentifier;
    }

    inline void SetFileSize(uint64_t fileSize)
    {
        mFileSize = fileSize;
    }

    inline uint64_t GetFileSize() const
    {
        return mFileSize;
    }

    inline uint32_t GetFileId() const
    {
        return FileAddressUtil::GetFileId(mFileAddress);
    }

    inline void SetFileAddress(uint64_t fileAddress)
    {
        mFileAddress = fileAddress;
    }

    std::string ToString()
    {
        std::ostringstream oss;
        oss << "[Identifier: " << PathTransform::ExtractFileName(mIdentifier) << ", FileAddress: " << mFileAddress
            << ", FileSize: " << mFileSize << "]";
        return oss.str();
    }

protected:
    std::string mIdentifier;    // 文件标志符: 完整的文件路径+文件名.
    uint64_t mFileAddress = 0;  // 文件地址: 分配的唯一文件ID.
    uint64_t mFileSize = 0;     // 文件大小.
};

using FileMetaBaseRef = std::shared_ptr<FileMetaBase>;
}  // namespace bss
}  // namespace ock

#endif  // BOOST_SS_FILE_META_BASE_H