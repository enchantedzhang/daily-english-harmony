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

#ifndef BOOST_SS_OUTPUT_VIEW_H
#define BOOST_SS_OUTPUT_VIEW_H

#include <memory>

#include "common/util/bss_math.h"
#include "lsm_store/file/file_mem_allocator.h"
#include "slice_table/binary/byte_buffer.h"

namespace ock {
namespace bss {
class OutputView {
public:
    explicit OutputView(const MemManagerRef &memManager, FileProcHolder holder = FileProcHolder::FILE_STORE_FLUSH)
        : mMemManager(memManager), mHolder(holder)
    {
    }

    explicit OutputView(uint32_t cap, const MemManagerRef &memManager,
                        FileProcHolder holder = FileProcHolder::FILE_STORE_FLUSH)
        : mInitSize(cap), mMemManager(memManager), mHolder(holder)
    {
    }

    ~OutputView()
    {
        if (mMemManager != nullptr && mData != nullptr) {
            auto tmp = reinterpret_cast<uintptr_t>(mData);
            mMemManager->ReleaseMemory(tmp);
        }
    }

    BResult Write(const uint8_t *buf, uint32_t len)
    {
        if (UNLIKELY(buf == nullptr || len == 0)) {
            LOG_ERROR("The input parameter is invalid.");
            return BSS_INVALID_PARAM;
        }

        BResult ret = BSS_OK;
        RETURN_NOT_OK_AS_FALSE(mOffset > UINT32_MAX - len, BSS_INVALID_PARAM);
        if (mOffset + len > mCapacity) {
            ret = Grow(len + mOffset);  // 扩容量.
            RETURN_NOT_OK_NO_LOG(ret);
        }
        ret = memcpy_s(mData + mOffset, mCapacity - mOffset, buf, len);
        if (UNLIKELY(ret != BSS_OK)) {
            LOG_ERROR("Memory copy failed, ret:" << ret << ", capacity:" << mCapacity << ", offset:" << mOffset
                                                 << ", len:" << len << ", errno:" << errno);
            return BSS_INNER_ERR;
        }
        mOffset += len;
        mWritten += len;
        return BSS_OK;
    }

    BResult WriteBoolean(bool value)
    {
        return Write(reinterpret_cast<uint8_t *>(&value), sizeof(value));
    }

    BResult WriteUint8(uint8_t value)
    {
        return Write(&value, sizeof(value));
    }

    BResult WriteInt16(int16_t value)
    {
        return Write(reinterpret_cast<uint8_t *>(&value), sizeof(value));
    }

    BResult WriteUint16(uint16_t value)
    {
        return Write(reinterpret_cast<uint8_t *>(&value), sizeof(value));
    }

    BResult WriteUint32(uint32_t value)
    {
        return Write(reinterpret_cast<uint8_t *>(&value), sizeof(value));
    }

    BResult WriteInt32(int32_t value)
    {
        return Write(reinterpret_cast<uint8_t *>(&value), sizeof(value));
    }

    BResult WriteUint64(uint64_t value)
    {
        return Write(reinterpret_cast<uint8_t *>(&value), sizeof(value));
    }

    // UTF长度统一用uint64_t，请勿改动
    BResult WriteUTF(std::string value)
    {
        // 1. 写入value的长度.
        uint64_t len = value.size();
        auto ret = WriteUint64(len);
        if (UNLIKELY(ret != BSS_OK)) {
            LOG_ERROR("Write string value length failed.");
            return BSS_ERR;
        }
        // 2. 写入value的数据.
        ret = Write(reinterpret_cast<const uint8_t *>(value.c_str()), len);
        if (UNLIKELY(ret != BSS_OK)) {
            LOG_ERROR("write string value failed.");
            return BSS_ERR;
        }
        return BSS_OK;
    }

    BResult WriteBuffer(const ByteBufferRef &buffer, uint32_t offset, uint32_t len)
    {
        if (UNLIKELY(buffer == nullptr || offset + len > buffer->Capacity())) {
            return BSS_INVALID_PARAM;
        }
        return Write(buffer->Data() + offset, len);
    }

    inline uint32_t GetOffset() const
    {
        return mOffset;
    }

    inline uint32_t GetCapacity() const
    {
        return mCapacity;
    }

    uint8_t *Data()
    {
        return mData;
    }

    BResult SetOffset(uint32_t requiredOffset)
    {
        if (UNLIKELY(requiredOffset > mCapacity)) {
            auto ret = Grow(BssMath::RoundUpToPowerOfTwo(requiredOffset));
            if (UNLIKELY(ret != BSS_OK)) {
                return ret;
            }
        }
        mOffset = requiredOffset;
        mWritten = requiredOffset;
        return BSS_OK;
    }

    BResult Reverse(uint32_t capacity)
    {
        if (UNLIKELY(UINT32_MAX < capacity || capacity == 0)) {
            return BSS_ALLOC_FAIL;
        }

        if (mCapacity < capacity) {
            auto addr = FileMemAllocator::Alloc(mMemManager, mHolder, capacity, __FUNCTION__);
            if (UNLIKELY(addr == 0)) {
                return BSS_ALLOC_FAIL;
            }
            if (UNLIKELY(mData != nullptr)) {
                // 释放旧的内存.
                auto tmp = reinterpret_cast<uintptr_t>(mData);
                RETURN_NOT_OK(mMemManager->ReleaseMemory(tmp));
            }
            // 替换新的内存.
            mData = reinterpret_cast<uint8_t *>(addr);
            mCapacity = capacity;
        }
        mOffset = 0;
        mWritten = 0;
        return BSS_OK;
    }

    // 大块内存用完提前释放，避免长时间占用
    void ReleaseMemory()
    {
        if (mCapacity < IO_SIZE_32M) {
            SetOffset(0);
            return;
        }

        LOG_INFO("release outputView memory size:" << mCapacity);
        if (LIKELY(mMemManager != nullptr && mData != nullptr)) {
            auto tmp = reinterpret_cast<uintptr_t>(mData);
            mMemManager->ReleaseMemory(tmp);
            mData = nullptr;
        }
        mCapacity = 0;
        mOffset = 0;
        mWritten = 0;
    }

private:
    BResult Grow(uint32_t size)
    {
        RETURN_ERROR_AS_NULLPTR(mMemManager);
        if (UNLIKELY(UINT32_MAX - mCapacity < size)) {
            return BSS_ALLOC_FAIL;
        }

        uint32_t newCapacity = 0;
        if (LIKELY(mCapacity == 0)) {
            newCapacity = mInitSize;
        } else {
            newCapacity = mCapacity << 1;  // 扩容按照当前容量的2倍大小进行扩充.
        }
        while ((newCapacity - mOffset) < size) {
            if (UNLIKELY((newCapacity << 1) < newCapacity)) {
                LOG_ERROR("Integer wraparound!");
                return BSS_ERR;
            }
            newCapacity = newCapacity << 1;
        }
        while ((newCapacity - mOffset) > (size + IO_SIZE_1M)) {  // 按照1M的步长进行缩容.
            newCapacity -= IO_SIZE_1M;
        }

        auto addr = FileMemAllocator::Alloc(mMemManager, mHolder, newCapacity, __FUNCTION__);
        if (UNLIKELY(addr == 0)) {
            return BSS_ALLOC_FAIL;
        }
        if (UNLIKELY(mData != nullptr)) {
            auto ret = memcpy_s(reinterpret_cast<uint8_t *>(addr), newCapacity, mData, mCapacity);
            if (UNLIKELY(ret != BSS_OK)) {
                LOG_ERROR("Memory copy failed, ret:" << ret << ", errno:" << errno);
                mMemManager->ReleaseMemory(addr);
                return BSS_ALLOC_FAIL;
            }
            // 释放旧的内存.
            auto tmp = reinterpret_cast<uintptr_t>(mData);
            mMemManager->ReleaseMemory(tmp);
        }
        // 替换新的内存.
        mData = reinterpret_cast<uint8_t *>(addr);
        mCapacity = newCapacity;
        return BSS_OK;
    }

private:
    uint8_t *mData = nullptr;
    uint32_t mCapacity = 0;
    uint32_t mInitSize = NO_16;
    uint32_t mOffset = 0;
    uint32_t mWritten = 0;
    MemManagerRef mMemManager = nullptr;
    FileProcHolder mHolder;
};
using OutputViewRef = std::shared_ptr<OutputView>;

}  // namespace bss
}  // namespace ock
#endif  // BOOST_SS_OUTPUT_VIEW_H
