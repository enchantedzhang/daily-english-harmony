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

#ifndef BOOST_SS_PATH_H
#define BOOST_SS_PATH_H

#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <memory>
#include <string>

#include "bss_log.h"

namespace ock {
namespace bss {

class Uri {
public:
    Uri() = default;

    explicit Uri(std::string path) : mPath(path)
    {
    }

    inline std::string GetPath() const
    {
        return mPath;
    }

private:
    std::string mPath;
};

class Path;
using PathRef = std::shared_ptr<Path>;
class Path {
public:
    explicit Path(Uri uri) : mUri(uri)
    {
    }

    Path(const PathRef &parent, const std::string &child) : Path(parent, std::make_shared<Path>(Uri(child)))
    {
    }

    Path(const PathRef &parent, const PathRef &child)
    {
        auto parentStr = parent->GetUri().GetPath();
        if (parentStr == "null") {
            mUri = Uri(child->GetUri().GetPath());
        } else {
            std::string netPath = parentStr + "/" + child->GetUri().GetPath();
            mUri = Uri(netPath);
        }
    }

    explicit Path(const std::string &pathStr)
    {
        mUri = Uri(pathStr);
    }

    inline Uri GetUri()
    {
        return mUri;
    }

    inline std::string Name() const
    {
        return mUri.GetPath();
    }

    inline bool IsLocalFileSystem()
    {
        return true;
    }

    std::string ExtractFileName() const
    {
        if (Name().empty()) {
            return Name();
        }

        // 检测原始路径是否以斜杠结尾
        const bool isDirectory = (Name().back() == '/');

        // 去除末尾连续斜杠
        size_t end = Name().find_last_not_of('/');
        if (end == std::string::npos) {
            return "/";  // 根目录直接返回/
        }

        // 截取有效路径段
        const std::string trimmed = Name().substr(0, end + 1);

        // 查找最后一个斜杠位置
        const size_t lastSlash = trimmed.find_last_of('/');

        std::string result;
        if (lastSlash != std::string::npos) {
            if (lastSlash == 0 && trimmed.length() > 1) {
                // 处理根目录(如 /var → var)
                result = trimmed.substr(1);
            } else {
                // 截取文件名 (如 /home/user → user)
                result = trimmed.substr(lastSlash + 1);
            }
        } else {
            result = trimmed;
        }
        // 如果是目录则追加斜杠
        if (isDirectory) {
            result += '/';
        }

        return result;
    }

    inline bool Existed()
    {
        return access(mUri.GetPath().c_str(), F_OK) == 0;
    }

private:
    Uri mUri;
};

struct PathHash {
    uint32_t operator()(const PathRef &path) const
    {
        return std::hash<std::string>()(path->Name());
    }
};

struct PathEqual {
    bool operator()(const PathRef &a, const PathRef &b) const
    {
        return a->Name() == b->Name();
    }
};

}  // namespace bss
}  // namespace ock
#endif  // BOOST_SS_PATH_H