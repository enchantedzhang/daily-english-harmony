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

#ifndef BOOST_SS_BYTE_BUFFER_H
#define BOOST_SS_BYTE_BUFFER_H

#include <cstdint>
#include <memory>

#include "binary/binary_reader.h"
#include "include/bss_err.h"
#include "include/bss_types.h"
#include "include/buffer.h"
#include "memory/mem_manager.h"
#include "securec.h"
#include "util/bss_math.h"

namespace ock {
namespace bss {

class ByteBuffer;
using ByteBufferRef = Ref<ByteBuffer>;

class ByteBuffer : public Buffer {
public:
    /* *
     * @brief   构造空的字节buffer对象，mData内存不需要释放
     */
    explicit ByteBuffer() : mData{ nullptr }, mCapacity(0), mOffset{ 0 }
    {
    }

    /* *
     * @brief   通过memManager分配mData内存，析构对象时候通过memManager释放mData内存
     */
    ByteBuffer(uint32_t cap, MemoryType type, const MemManagerRef &memManager)
    {
        if (UNLIKELY(cap == 0 || type >= MemoryType::MEM_TYPE_BUTT || memManager == nullptr)) {
            return;
        }
        mMemManager = memManager;
        uintptr_t addr = 0;
        mMemManager->GetMemory(type, cap, addr);
        mData = reinterpret_cast<uint8_t *>(addr);
        mCapacity = cap;
        mMemManagerFree = true;
    }

    /* *
     * @brief   通过外部Data内存构造Buffer对象，mData由外部对象释放
     */
    ByteBuffer(uint8_t *p, uint32_t len) : mData{ p }, mCapacity(len), mOffset{ 0 }
    {
    }

    /* *
     * @brief   通过外部new的mData内存构造Buffer对象，对象销毁后通过delete释放mData的内存
     */
    ByteBuffer(uint8_t *p, uint32_t len, bool dataFree)
        : mData{ p }, mCapacity(len), mOffset{ 0 }, mDataFree{ dataFree }
    {
    }

    /* *
     * @brief   通过memManager分配mData内存，mData内存地址由外部传入，析构对象时候通过memManager释放mData内存
     */
    ByteBuffer(uint8_t *p, uint32_t len, const MemManagerRef &memManager)
        : mData{ p }, mCapacity(len), mOffset{ 0 }, mMemManagerFree{ true }, mMemManager{ memManager }
    {
    }

    ~ByteBuffer() noexcept override
    {
        if (mMemManagerFree && mMemManager != nullptr) {
            auto addr = reinterpret_cast<uintptr_t>(mData - mMemFreeOffset);
            mMemManager->ReleaseMemory(addr);
        }
        if (mDataFree) {
            delete[](mData - mMemFreeOffset);
        }
        mData = nullptr;
        mCapacity = mOffset = 0;
    }

    explicit ByteBuffer(const ByteBuffer &buf) = default;

    ByteBuffer(ByteBuffer &&buf) noexcept
    {
        mData = buf.mData;
        mCapacity = buf.mCapacity;
        mOffset = buf.mOffset;
        mMemFreeOffset = buf.mMemFreeOffset;
        mDataFree = buf.mDataFree;
        mMemManagerFree = buf.mMemManagerFree;
        buf.mData = nullptr;
        buf.mCapacity = buf.mOffset = 0;
    }

    ByteBuffer &operator=(const ByteBuffer &buf) = delete;

    ByteBuffer &operator=(ByteBuffer &&buf) noexcept
    {
        if (mMemManagerFree) {
            uintptr_t addr = reinterpret_cast<uintptr_t>(mData - mMemFreeOffset);
            mMemManager->ReleaseMemory(addr);
        }
        if (mDataFree) {
            delete[](mData - mMemFreeOffset);
        }
        mData = buf.mData;
        mCapacity = buf.mCapacity;
        mOffset = buf.mOffset;
        mMemFreeOffset = buf.mMemFreeOffset;
        mDataFree = buf.mDataFree;
        mMemManagerFree = buf.mMemManagerFree;

        buf.mData = nullptr;
        buf.mCapacity = buf.mOffset = 0;
        return *this;
    }

    static uint32_t CopyFromBufferToBuffer(ByteBufferRef in, ByteBufferRef out, uint32_t sourceOffset,
                                           uint32_t destinationOffset, uint32_t length)
    {
        out->WriteAt(in->Data() + sourceOffset, length, destinationOffset);
        return destinationOffset + length;
    }

    bool Valid() const noexcept
    {
        return mData != nullptr && mCapacity != 0;
    }

    BResult Write(const uint8_t *buf, uint32_t len) noexcept
    {
        if (UNLIKELY(!CheckParam(buf, len, mOffset))) {
            return BSS_INVALID_PARAM;
        }

        auto ret = memcpy_s(mData + mOffset, mCapacity - mOffset, buf, len);
        if (UNLIKELY(ret != BSS_OK)) {
            return BSS_INNER_ERR;
        }

        mOffset += len;
        return BSS_OK;
    }

    BResult WriteAt(const uint8_t *buf, uint32_t len, uint32_t pos) noexcept
    {
        if (UNLIKELY(!CheckParam(buf, len, pos))) {
            return BSS_INVALID_PARAM;
        }
        auto ret = memcpy_s(mData + pos, mCapacity - pos, buf, len);
        if (UNLIKELY(ret != BSS_OK)) {
            LOG_ERROR("Memory copy failed, ret:" << ret << ", errno:" << errno);
        }
        return ret;
    }

    BResult Read(uint8_t *buf, uint32_t len) noexcept
    {
        if (UNLIKELY(!CheckParam(buf, len, mOffset))) {
            return BSS_INVALID_PARAM;
        }

        auto ret = memcpy_s(buf, mCapacity - mOffset, mData + mOffset, len);
        if (UNLIKELY(ret != BSS_OK)) {
            return BSS_INNER_ERR;
        }

        mOffset += len;
        return BSS_OK;
    }

    virtual BResult ReadAt(uint8_t *buf, uint32_t len, uint32_t pos) noexcept
    {
        if (UNLIKELY(!CheckParam(buf, len, pos))) {
            return BSS_INVALID_PARAM;
        }

        auto ret = memcpy_s(buf, mCapacity - pos, mData + pos, len);
        if (UNLIKELY(ret != BSS_OK)) {
            return BSS_INNER_ERR;
        }
        return BSS_OK;
    }

    BResult WriteFromBuffer(ByteBufferRef source, uint32_t sourceOffset, uint32_t len, uint32_t pos)
    {
        if (UNLIKELY(source == nullptr)) {
            return BSS_INVALID_PARAM;
        }
        if (UNLIKELY(!CheckParam(source->Data(), len, pos))) {
            return BSS_INVALID_PARAM;
        }

        auto ret = memcpy_s(mData + pos, mCapacity - pos, source->Data() + sourceOffset, len);
        if (UNLIKELY(ret != BSS_OK)) {
            return BSS_INNER_ERR;
        }
        return BSS_OK;
    }

    inline BResult WriteUint32(uint32_t source, uint32_t pos)
    {
        if (UNLIKELY(!CheckParam(sizeof(uint32_t), pos))) {
            return BSS_INVALID_PARAM;
        }

        *(reinterpret_cast<uint32_t *>(mData + pos)) = source;
        return BSS_OK;
    }

    inline BResult WriteUint16(uint16_t source, uint32_t pos)
    {
        if (UNLIKELY(!CheckParam(sizeof(uint16_t), pos))) {
            return BSS_INVALID_PARAM;
        }
        *(reinterpret_cast<uint16_t *>(mData + pos)) = source;
        return BSS_OK;
    }

    inline BResult WriteUint64(uint64_t source, uint32_t pos)
    {
        if (UNLIKELY(!CheckParam(sizeof(uint64_t), pos))) {
            return BSS_INVALID_PARAM;
        }
        *(reinterpret_cast<uint64_t *>(mData + pos)) = source;
        return BSS_OK;
    }

    virtual inline BResult ReadUint8(uint8_t &source, uint32_t pos)
    {
        if (UNLIKELY(!CheckParam(sizeof(source), pos))) {
            return BSS_INVALID_PARAM;
        }
        source = *(mData + pos);
        return BSS_OK;
    }

    virtual inline BResult ReadInt8(int8_t &source, uint32_t pos)
    {
        if (UNLIKELY(!CheckParam(sizeof(source), pos))) {
            return BSS_INVALID_PARAM;
        }
        source = *(reinterpret_cast<int8_t *>(mData + pos));
        return BSS_OK;
    }

    virtual inline BResult ReadUint16(uint16_t &source, uint32_t pos)
    {
        if (UNLIKELY(!CheckParam(sizeof(source), pos))) {
            return BSS_INVALID_PARAM;
        }
        source = *(reinterpret_cast<uint16_t *>(mData + pos));
        return BSS_OK;
    }

    virtual inline BResult ReadInt32(int32_t &source, uint32_t pos)
    {
        if (UNLIKELY(!CheckParam(sizeof(source), pos))) {
            return BSS_INVALID_PARAM;
        }
        source = *(reinterpret_cast<int32_t *>(mData + pos));
        return BSS_OK;
    }

    virtual inline BResult ReadUint32(uint32_t &source, uint32_t pos)
    {
        if (UNLIKELY(!CheckParam(sizeof(source), pos))) {
            return BSS_INVALID_PARAM;
        }
        source = *(reinterpret_cast<uint32_t *>(mData + pos));
        return BSS_OK;
    }

    virtual inline BResult ReadUint64(uint64_t &source, uint32_t pos)
    {
        if (UNLIKELY(!CheckParam(sizeof(source), pos))) {
            return BSS_INVALID_PARAM;
        }
        source = *(reinterpret_cast<uint64_t *>(mData + pos));
        return BSS_OK;
    }

    inline uint64_t UnsafeReadLessThen8Bytes(uint32_t pos, uint32_t byteNum, uint64_t &value)
    {
        if (UNLIKELY(!CheckParam(sizeof(uint64_t), pos))) {
            return 0;
        }

        return BinaryReader::ReadBytes(mData + pos, byteNum, value);
    }
    /**
     *
     * @param buf 对比的buffer
     * @param start1 自身的起始
     * @param start2 buf的起始
     * @param len1 自身的长度
     * @param len2  buf的长度
     * @return 负数表示自身较小，0表示相等 ，正数表示buf较小
     */
    virtual int32_t Compare(ByteBufferRef buf, uint32_t start1, uint32_t start2, uint32_t len1, uint32_t len2)
    {
        if (UNLIKELY(buf == nullptr)) {
            LOG_WARN("Unexpected: other buffer is null");
            return -1;
        }

        if (UNLIKELY(mCapacity < (start1 + len1))) {
            LOG_ERROR("buffer off:" << start1 << ", len:" << len1 << " exceed capacity:" << mCapacity);
            return -1;
        }

        if (UNLIKELY(buf->mCapacity < (start2 + len2))) {
            LOG_ERROR("other buffer len: " << start2 << ", len:" << len2 << " exceed capacity: " << buf->mCapacity);
            return 1;
        }

        uint32_t minLen = std::min(len1, len2);
        int32_t cmp = memcmp(mData + start1, buf->Data() + start2, minLen);
        if (cmp == 0) {
            return BssMath::IntegerCompare<uint32_t>(len1, len2);
        }
        return cmp;
    }

public:
    virtual inline const uint8_t *Data() const noexcept
    {
        return mData;
    }

    uint8_t *Data() noexcept override
    {
        return mData;
    }

    inline BResult UpdateDataWithFreeOffset(uint32_t freeOffset)
    {
        if (UNLIKELY(freeOffset >= mCapacity || mData == nullptr)) {
            LOG_ERROR("The input offset is greater than the actual memory capacity, freeOffset: "
                      << freeOffset << "capacity: " << mCapacity);
            return BSS_INVALID_PARAM;
        }
        mMemFreeOffset = freeOffset;
        mCapacity = mCapacity - mMemFreeOffset;
        mData = mData + mMemFreeOffset;
        return BSS_OK;
    }

    virtual inline uint32_t Capacity() const noexcept
    {
        return mCapacity;
    }

    inline uint32_t Offset() const noexcept
    {
        return mOffset;
    }

    inline void Offset(uint32_t off) noexcept
    {
        if (off > mCapacity) {
            mOffset = mCapacity;
        } else {
            mOffset = off;
        }
    }

    inline void AddOffset(uint32_t delta) noexcept
    {
        if (mOffset + delta > mCapacity) {
            mOffset = mCapacity;
        } else {
            mOffset += delta;
        }
    }

    inline void Reset() noexcept
    {
        mOffset = 0;
    }

    inline bool CheckParam(const uint8_t *buf, uint32_t len, uint32_t pos)
    {
        if (UNLIKELY(buf == nullptr)) {
            return false;
        }
        return CheckParam(len, pos);
    }

    inline bool CheckParam(uint32_t len, uint32_t pos)
    {
        if (!Valid()) {
            return false;
        }
        if (UNLIKELY(len == 0)) {
            return false;
        }
        if (UNLIKELY(UINT32_MAX - pos < len)) {
            return false;
        }
        if (UNLIKELY(pos + len > mCapacity)) {
            return false;
        }
        return true;
    }

    inline void UpdateCapacity(uint32_t capacity) noexcept
    {
        if (UNLIKELY(mOffset > capacity || capacity > mCapacity)) {
            LOG_ERROR("Update buffer capacity failed, mOffset: " << mOffset << ", mCapacity: " << mCapacity
                                                                 << ", new capacity: " << capacity);
            return;
        }
        mCapacity = capacity;
    }

public:
    uint8_t *mData = nullptr;
    uint32_t mCapacity = 0;
    uint32_t mOffset = 0;
    uint32_t mMemFreeOffset = 0;
    bool mDataFree = false;
    bool mMemManagerFree = false;
    MemManagerRef mMemManager = nullptr;
};

class CompositeBuffer : public Buffer {
public:
    CompositeBuffer(ByteBufferRef &priBuffer, ByteBufferRef &secBuffer) : mPriBuffer(priBuffer), mSecBuffer(secBuffer)
    {
    }
    ~CompositeBuffer() noexcept override = default;
    uint8_t *Data() override
    {
        return nullptr;
    }

    uint32_t Length() override
    {
        return mPriBuffer->Length() + mSecBuffer->Length();
    }

private:
    ByteBufferRef mPriBuffer;
    ByteBufferRef mSecBuffer;
};
using CompositeBufferRef = Ref<CompositeBuffer>;
}  // namespace bss
}  // namespace ock

#endif  // BOOST_SS_BYTE_BUFFER_H
