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

#ifndef BOOST_SS_LOCAL_FILE_SYSTEM_H
#define BOOST_SS_LOCAL_FILE_SYSTEM_H

#include <fcntl.h>
#include <sys/epoll.h>
#include <sys/mman.h>
#include <unistd.h>

#include <memory>
#include <string>

#include "common/fs/file_system.h"
#include "common/path.h"

namespace ock {
namespace bss {

class LocalFileSystem : public FileSystem {
public:
    ~LocalFileSystem() override = default;
    explicit LocalFileSystem(const PathRef &path) : FileSystem(path)
    {
    }

    BResult Open(int flags) override
    {
        RETURN_ERROR_AS_NULLPTR(mFilePath);
        static const int32_t OPEN_FILE_PERMISSION = 0640;
        mFd = open(mFilePath->Name().c_str(), flags, OPEN_FILE_PERMISSION);
        if (UNLIKELY(mFd < 0)) {
            LOG_ERROR("Failed to open file:" << mFilePath->ExtractFileName() << ", errno:" << errno << ".");
            return BSS_ALLOC_FAIL;
        }
        LOG_DEBUG("Open file success, file: " << mFilePath->ExtractFileName() << ".");
        return BSS_OK;
    }

    BResult Read(uint8_t *buffer, uint64_t count, int64_t offset) override
    {
        RETURN_INVALID_PARAM_AS_NULLPTR(buffer);
        if (UNLIKELY(mFd < 0)) {
            LOG_ERROR("Read failed, file:" << mFilePath->ExtractFileName() << ", offset:" << offset
                                           << ", count:" << count);
            return BSS_ERR;
        }
        ssize_t result = pread(mFd, buffer, count, offset);
        if (UNLIKELY(result != static_cast<ssize_t>(count))) {
            LOG_ERROR("Pread failed, errno: " << strerror(errno) << ", file: " << mFilePath->ExtractFileName()
                                              << ", result:" << result << ", count:" << count << ", offset:" << offset);
            return BSS_IO_ERR;
        }
        return BSS_OK;
    }

    BResult Read(uint8_t *buffer, uint64_t count) override
    {
        RETURN_INVALID_PARAM_AS_NULLPTR(buffer);
        if (UNLIKELY(mFd < 0)) {
            LOG_ERROR("Read failed, file:" << mFilePath->ExtractFileName() << ", count:" << count);
            return BSS_ERR;
        }
        ssize_t result = read(mFd, buffer, count);
        if (UNLIKELY(result != static_cast<ssize_t>(count))) {
            LOG_ERROR("Read failed, errno: " << strerror(errno) << ", file: " << mFilePath->ExtractFileName()
                                             << ", result:" << result << ", count:" << count);
            return BSS_IO_ERR;
        }
        return BSS_OK;
    }

    BResult Write(const uint8_t *buffer, uint64_t count, int64_t offset) override
    {
        RETURN_INVALID_PARAM_AS_NULLPTR(buffer);
        if (UNLIKELY(mFd < 0)) {
            LOG_ERROR("Write failed, file:" << mFilePath->ExtractFileName() << ", offset:" << offset
                                            << ", count:" << count);
            return BSS_ERR;
        }
        ssize_t result = pwrite(mFd, buffer, count, offset);
        if (UNLIKELY(result != static_cast<ssize_t>(count))) {
            LOG_ERROR("Pwrite failed, errno:" << strerror(errno) << ", file: " << mFilePath->ExtractFileName()
                                              << ", result:" << result << ", count:" << count
                                              << ", offset: " << offset);
            return BSS_IO_ERR;
        }
        return BSS_OK;
    }

    BResult Write(const uint8_t *buffer, uint64_t count) override
    {
        RETURN_INVALID_PARAM_AS_NULLPTR(buffer);
        if (UNLIKELY(mFd < 0)) {
            LOG_ERROR("Write failed, file:" << mFilePath->ExtractFileName() << ", count:" << count);
            return BSS_ERR;
        }
        ssize_t result = write(mFd, buffer, count);
        if (UNLIKELY(result != static_cast<ssize_t>(count))) {
            LOG_ERROR("Write failed, errno:" << strerror(errno) << ", file: " << mFilePath->ExtractFileName()
                                             << ", result:" << result << ", count:" << count);
            return BSS_IO_ERR;
        }
        return BSS_OK;
    }

    BResult Sync() override
    {
        return BSS_OK;
        // 性能优化, 不用每次写文件都执行sync操作，依赖于OS自身的数据持久化机制. return fsync(mFd);
    }

    BResult Flush() override
    {
        if (UNLIKELY(mFd < 0)) {
            LOG_ERROR("Flush failed, file:" << mFilePath->ExtractFileName());
            return BSS_ERR;
        }
        if (UNLIKELY(fsync(mFd) < 0)) {
            LOG_ERROR("Flush failed, error:" << strerror(errno) << ", file:" << mFilePath->ExtractFileName());
            return BSS_IO_ERR;
        }
        return BSS_OK;
    }

    void Close() override
    {
        if (UNLIKELY(mFd < 0)) {
            return;
        }
        int result = close(mFd);
        if (result != 0) {
            LOG_ERROR("Close file failed, errno:" << strerror(errno) << ", file: " << mFilePath->ExtractFileName());
        }
        mFd = -1;
    }

    void Seek(int64_t offset) override
    {
        lseek(mFd, offset, SEEK_SET);
    }

    void Remove() override
    {
    }

private:
    int32_t mFd = -1;
};

}  // namespace bss
}  // namespace ock
#endif  // BOOST_SS_LOCAL_FILE_SYSTEM_H