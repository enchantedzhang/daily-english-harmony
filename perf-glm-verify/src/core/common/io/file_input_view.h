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

#ifndef BSS_DEV_FILE_INPUT_VIEW_H
#define BSS_DEV_FILE_INPUT_VIEW_H

#include <fcntl.h>
#include <sys/epoll.h>

#include <cstdint>
#include <cstdio>

#include "common/fs/file_system.h"
#include "common/path.h"
#include "fresh_table/memory/memory_segment.h"
#include "include/bss_err.h"
#include "include/bss_types.h"
#include "slice_table/binary/byte_buffer.h"

namespace ock {
namespace bss {

class FileInputView {
public:
    FileInputView() = default;

    BResult Init(FileSystemType type, const PathRef &filePath)
    {
        mFilePath = filePath;
        mType = type;
        fileSystem = FileSystem::CreateFileSystem(type, mFilePath);
        if (UNLIKELY(fileSystem == nullptr)) {
            return BSS_ERR;
        }
        int flags = O_RDWR | O_CREAT | O_SYNC;
        return fileSystem->Open(flags);
    }

    ~FileInputView()
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

    template <typename T> BResult Read(T &value)
    {
        return fileSystem->Read(reinterpret_cast<uint8_t *>(&value), sizeof(T));
    }

    BResult ReadUTF(std::string &value)
    {
        // 1. 读取UTF value的长度.
        uint64_t utfLen = 0;  // UTF长度统一用uint64_t, 请勿改动.
        if (UNLIKELY(Read(utfLen) != BSS_OK)) {
            LOG_ERROR("Read UTF len failed, errno:" << strerror(errno) << ", file:" << mFilePath->ExtractFileName());
            return BSS_IO_ERR;
        }
        // 增加保护校验：此处主要用于读取文件名等字符串，限定读取len不超过128KB(Linux文件全路径最大4KB)，避免len有误导致new超大内存
        if (UNLIKELY(utfLen > IO_SIZE_128K)) {
            LOG_ERROR("UTF value len is exceed limit:" << utfLen << ", file:" << mFilePath->ExtractFileName());
            return BSS_IO_ERR;
        }
        // 2. 读取value的数据.
        auto *tempBuf = new (std::nothrow) uint8_t[utfLen];
        RETURN_ALLOC_FAIL_AS_NULLPTR(tempBuf);
        auto ret = ReadBuffer(tempBuf, utfLen);
        if (UNLIKELY(ret != BSS_OK)) {
            LOG_ERROR("Read buffer from file failed, ret:" << ret << ", file:" << mFilePath->ExtractFileName());
        } else {
            value.assign(reinterpret_cast<char *>(tempBuf), utfLen);
        }
        delete[] tempBuf;
        return ret;
    }

    inline BResult ReadBuffer(uint8_t *buff, uint32_t bLen)
    {
        return fileSystem->Read(buff, bLen);
    }

    inline BResult ReadBuffer(uint8_t *buff, uint32_t bLen, uint32_t offset)
    {
        return fileSystem->Read(buff, bLen, offset);
    }

    /**
     * 从文件中指定位置读取数据
     * @param position 缓冲区的长度
     * @param buff 存储读取数据的缓冲区
     * @param offset 读取数据的起始位置
     * @param length 要读取的数据的长度
     * @return 返回实际读取的数据长度，如果读取失败返回-1
     */
    inline BResult ReadByteBuffer(uint32_t position, const ByteBufferRef &buffer, uint32_t offset, uint32_t length)
    {
        RETURN_INVALID_PARAM_AS_NULLPTR(buffer);
        if (UNLIKELY(position >= buffer->Capacity() || length > buffer->Capacity() - position)) {
            LOG_ERROR("Invalid read param, buffer Capacity:" << buffer->Capacity() << " buffer position:" << position
                                                             << " read length:" << length);
            return BSS_INVALID_PARAM;
        }
        return fileSystem->Read(buffer->Data() + position, length, offset);
    }

    /**
     * 从文件中指定位置读取数据写入到memorySegment中
     * @param position 写入目标缓冲区的起始地址
     * @param buff 存储读取数据的缓冲区
     * @param offset 读取数据的起始位置
     * @param length 要读取的数据的长度
     * @return 返回实际读取的数据长度，如果读取失败返回0
     */
    inline BResult ReadMemorySegment(uint32_t position, const MemorySegmentRef &segment, uint32_t offset,
                                     uint32_t length)
    {
        RETURN_INVALID_PARAM_AS_NULLPTR(segment);
        if (UNLIKELY(position >= segment->GetLen() || length > segment->GetLen() - position)) {
            LOG_ERROR("Invalid read param, segment Capacity:" << segment->GetLen() << " segment position:" << position
                                                              << " read length:" << length);
            return BSS_INVALID_PARAM;
        }
        return fileSystem->Read(segment->GetSegment() + position, length, offset);
    }

    inline void Seek(uint64_t desired)
    {
        if (desired > INT64_MAX) {
            LOG_ERROR("desired is larger than INT64_MAX, desired: " << desired);
            return;
        }
        fileSystem->Seek(desired);
    }

    inline void Close()
    {
        if (fileSystem != nullptr) {
            fileSystem->Close();
            fileSystem = nullptr;
        }
    }

private:
    PathRef mFilePath = nullptr;
    FileSystemType mType = FileSystemType::LOCAL;
    FileSystemRef fileSystem = nullptr;
};
using FileInputViewRef = std::shared_ptr<FileInputView>;

}  // namespace bss
}  // namespace ock
#endif  // BSS_DEV_FILE_INPUT_VIEW_H