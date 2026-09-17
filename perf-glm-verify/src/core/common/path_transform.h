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

#ifndef PATH_TRANSFORM_H
#define PATH_TRANSFORM_H

#include <string>

namespace ock {
namespace bss {

class PathTransform {
public:
    // 取文件名或者当前目录名 (/home/user → user 或 /home/user/ → user/)
    static std::string ExtractFileName(const std::string &path)
    {
        if (path.empty()) {
            return path;
        }

        // 检测原始路径是否以斜杠结尾
        const bool isDirectory = (path.back() == '/');

        // 去除末尾连续斜杠
        size_t end = path.find_last_not_of('/');
        if (end == std::string::npos) {
            return "/";  // 根目录直接返回/
        }

        // 截取有效路径段
        const std::string trimmed = path.substr(0, end + 1);

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

    // 去除取文件名后缀 (/data/local/tmp.txt → /data/local)
    static std::string ExtractDirectory(const std::string &fullPath)
    {
        if (fullPath.empty()) {
            return fullPath;
        }

        const size_t lastSlashPos = fullPath.find_last_of('/');
        if (lastSlashPos == std::string::npos) {
            return ".";
        }
        if (lastSlashPos == 0) {
            return "/";
        }

        return fullPath.substr(0, lastSlashPos);
    }
};

}  // namespace bss
}  // namespace ock
#endif  // PATH_TRANSFORM_H