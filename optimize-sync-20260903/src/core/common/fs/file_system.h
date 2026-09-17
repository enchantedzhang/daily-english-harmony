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

#ifndef BOOST_SS_FILE_SYSTEM_H
#define BOOST_SS_FILE_SYSTEM_H

#include <string>
#include <vector>

#include "include/bss_err.h"
#include "path.h"

namespace ock {
namespace bss {

enum class FileSystemType { LOCAL = 1, HDFS = 2 };

class FileSystem;
using FileSystemRef = std::shared_ptr<FileSystem>;
class FileSystem {
public:
    virtual ~FileSystem() = default;
    static FileSystemRef CreateFileSystem(FileSystemType type, const PathRef &path);

    explicit FileSystem(const PathRef &path) : mOffset(0), mFilePath(path)
    {
    }

    virtual BResult Open(int flags) = 0;

    virtual BResult Read(uint8_t *buffer, uint64_t count, int64_t offset) = 0;

    virtual BResult Read(uint8_t *buffer, uint64_t count) = 0;

    virtual BResult Write(const uint8_t *buffer, uint64_t count, int64_t offset) = 0;

    virtual BResult Write(const uint8_t *buffer, uint64_t count) = 0;

    virtual BResult Sync() = 0;

    virtual BResult Flush() = 0;

    virtual void Close() = 0;

    virtual void Seek(int64_t offset) = 0;

    virtual void Remove() = 0;

public:
    int64_t mOffset;
    PathRef mFilePath = nullptr;
};

}  // namespace bss
}  // namespace ock
#endif  // BOOST_SS_FILE_SYSTEM_H