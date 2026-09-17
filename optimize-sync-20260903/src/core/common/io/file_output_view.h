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

#ifndef BSS_DEV_FILE_OUTPUT_VIEW_H
#define BSS_DEV_FILE_OUTPUT_VIEW_H

#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>

#include <cstdint>
#include <cstdio>
#include <mutex>

#include "common/bss_log.h"
#include "common/fs/file_system.h"
#include "common/path.h"
#include "include/config.h"
#include "slice_table/binary/byte_buffer.h"

namespace ock {
namespace bss {

class FileOutputView {
public:
    enum class WriteMode { NO_OVERWRITE, OVERWRITE };

    FileOutputView() = default;

    BResult Init(const PathRef &filePath, WriteMode writeMode = WriteMode::NO_OVERWRITE)
    {
        RETURN_INVALID_PARAM_AS_NULLPTR(filePath);
        mFilePath = filePath;
        fileSystem = FileSystem::CreateFileSystem(FileSystemType::LOCAL, mFilePath);
        RETURN_ERROR_AS_NULLPTR(fileSystem);
        int flags = O_WRONLY | O_CREAT | O_CLOEXEC;
        if (writeMode == WriteMode::NO_OVERWRITE) {
            flags |= O_APPEND;
        } else {
            flags |= O_TRUNC;
        }
        return fileSystem->Open(flags);
    }

    BResult Init(const PathRef &filePath, const ConfigRef &conf, WriteMode writeMode = WriteMode::NO_OVERWRITE)
    {
        RETURN_INVALID_PARAM_AS_NULLPTR(filePath);
        mFilePath = filePath;
        fileSystem = FileSystem::CreateFileSystem(FileSystemType::LOCAL, mFilePath);
        if (UNLIKELY(fileSystem == nullptr)) {
            return BSS_ERR;
        }
        int flags = O_WRONLY | O_CREAT | O_CLOEXEC;
        if (writeMode == WriteMode::NO_OVERWRITE) {
            flags |= O_APPEND;
        } else {
            flags |= O_TRUNC;
        }
        return fileSystem->Open(flags);
    }

    ~FileOutputView()
    {
        if (fileSystem != nullptr) {
            fileSystem->Close();
            fileSystem = nullptr;
        }
    }

    inline PathRef GetFilePath()
    {
        return mFilePath;
    }

    inline BResult WriteUint8(const uint8_t &value)
    {
        return WriteImpl(&value, sizeof(uint8_t));
    }

    inline BResult WriteUint16(const uint16_t &value)
    {
        return WriteImpl(reinterpret_cast<const uint8_t *>(&value), sizeof(uint16_t));
    }

    inline BResult WriteInt16(const int16_t &value)
    {
        return WriteImpl(reinterpret_cast<const uint8_t *>(&value), sizeof(uint16_t));
    }

    inline BResult WriteUint32(const uint32_t &value)
    {
        return WriteImpl(reinterpret_cast<const uint8_t *>(&value), sizeof(uint32_t));
    }

    inline BResult WriteInt32(const int32_t &value)
    {
        return WriteImpl(reinterpret_cast<const uint8_t *>(&value), sizeof(int32_t));
    }

    inline BResult WriteUint64(const uint64_t &value)
    {
        return WriteImpl(reinterpret_cast<const uint8_t *>(&value), sizeof(uint64_t));
    }

    inline BResult WriteInt64(const int64_t &value)
    {
        return WriteImpl(reinterpret_cast<const uint8_t *>(&value), sizeof(int64_t));
    }

    inline BResult WriteUTF(const std::string &value)
    {
        // 1. 写入UTF value的长度.
        uint64_t utfLen = value.size();  // UTF长度统一用uint64_t, 请勿改动.
        if (UNLIKELY(WriteUint64(utfLen) != BSS_OK)) {
            LOG_ERROR("Write utf len failed, errno:" << strerror(errno) << ", file:" << mFilePath->ExtractFileName());
            return BSS_IO_ERR;
        }
        // 2. 写入value的数据.
        return WriteImpl(reinterpret_cast<const uint8_t *>(value.c_str()), utfLen);
    }

    inline BResult WriteByteBuffer(const ByteBufferRef &buffer, int64_t off, uint64_t len)
    {
        RETURN_INVALID_PARAM_AS_NULLPTR(buffer);
        auto ret = fileSystem->Write(buffer->Data(), len, off);
        if (LIKELY(ret == BSS_OK)) {
            std::lock_guard<std::mutex> lock(mLock);
            mWritten += len;
        }
        return ret;
    }

    inline BResult WriteBuffer(const uint8_t *buffer, int64_t off, uint64_t len)
    {
        RETURN_INVALID_PARAM_AS_NULLPTR(buffer);
        RETURN_INVALID_PARAM_AS_NULLPTR(fileSystem);
        auto ret = fileSystem->Write(buffer, len, off);
        if (LIKELY(ret == BSS_OK)) {
            std::lock_guard<std::mutex> lock(mLock);
            mWritten += len;
        }
        return ret;
    }

    inline BResult Sync() const
    {
        return fileSystem->Sync();
    }

    inline BResult Flush()
    {
        return fileSystem->Flush();
    }

    inline void Close()
    {
        if (fileSystem != nullptr) {
            fileSystem->Close();
            fileSystem = nullptr;
        }
    }

    inline uint64_t Size() const
    {
        return mWritten;
    }

private:
    inline BResult WriteImpl(const uint8_t *data, size_t size)
    {
        RETURN_INVALID_PARAM_AS_NULLPTR(data);
        auto ret = fileSystem->Write(data, size);
        if (LIKELY(ret == BSS_OK)) {
            std::lock_guard<std::mutex> lock(mLock);
            mWritten += size;
        }
        return ret;
    }

private:
    PathRef mFilePath = nullptr;
    FileSystemRef fileSystem = nullptr;
    uint64_t mWritten = 0;
    std::mutex mLock;
};
using FileOutputViewRef = std::shared_ptr<FileOutputView>;

}  // namespace bss
}  // namespace ock
#endif  // BSS_DEV_FILE_OUTPUT_VIEW_H
