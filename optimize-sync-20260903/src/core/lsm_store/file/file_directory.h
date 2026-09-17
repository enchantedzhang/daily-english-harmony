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

#ifndef BOOST_SS_FILE_DIRECTORY_H
#define BOOST_SS_FILE_DIRECTORY_H

#include <sys/stat.h>
#include <sys/types.h>

#include <mutex>
#include <utility>

#include "common/path.h"
#include "common/path_transform.h"

namespace ock {
namespace bss {
class FileDirectory {
public:
    explicit FileDirectory(PathRef path) : mDirPath(std::move(path))
    {
    }

    FileDirectory(const PathRef &primaryBathPath, const std::string &subDirName)
    {
        mDirPath = std::make_shared<Path>(primaryBathPath, subDirName);
    }

    // 权限0750：S_IRWXU | S_IRGRP | S_IXGRP
    bool CreateDirectories(const std::string &path, mode_t mode = S_IRWXU | S_IRGRP | S_IXGRP)
    {
        // 如果路径为空，直接返回成功
        if (path.empty()) {
            return true;
        }

        // 检查路径是否已经存在
        if (CheckDirExists(path)) {
            return true;
        }

        // 获取递归锁
        std::lock_guard<std::recursive_mutex> lock(mRcMutex);
        // 双重检查路径是否已经存在
        if (CheckDirExists(path)) {
            return true;
        }

        // 递归创建父目录
        size_t lastSlashPos = path.find_last_of('/');
        if (lastSlashPos != std::string::npos) {
            std::string parentPath = path.substr(0, lastSlashPos);
            if (!CreateDirectories(parentPath, mode)) {
                LOG_ERROR("Create directory failed, path:" << PathTransform::ExtractFileName(path)
                                                           << ", errno:" << errno);
                return false;  // 递归创建父目录失败
            }
        }

        // 创建当前目录
        if (mkdir(path.c_str(), mode) == -1) {
            LOG_ERROR("Failed to create directory:" << PathTransform::ExtractFileName(path) << ", errno:" << errno);
            return false;
        }

        return true;
    }

    static bool CheckDirExists(const std::string &path)
    {
        struct stat st {
        };
        if (stat(path.c_str(), &st) == 0) {
            // 如果路径已经存在且是目录，返回成功
            if (S_ISDIR(st.st_mode)) {
                return true;
            }

            // 如果路径存在但不是目录，返回失败(预期不会出现)
            LOG_ERROR("Path exists but is not a directory, path:" << PathTransform::ExtractFileName(path));
        }
        return false;
    }

    PathRef GetDirectoryPath() const
    {
        return mDirPath;
    }

    bool CreateDirInFileSystem()
    {
        // 权限0750：S_IRWXU | S_IRGRP | S_IXGRP
        return CreateDirectories(mDirPath->Name(), S_IRWXU | S_IRGRP | S_IXGRP);
    }

private:
    PathRef mDirPath = nullptr;
    std::recursive_mutex mRcMutex;
};
using FileDirectoryRef = std::shared_ptr<FileDirectory>;

}  // namespace bss
}  // namespace ock
#endif  // BOOST_SS_FILE_DIRECTORY_H