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
#ifndef BOOST_SS_MEMORYSEGMENT_H
#define BOOST_SS_MEMORYSEGMENT_H
#include <cstdint>
#include <memory>

#include "binary/byte_buffer.h"
#include "common/bss_log.h"
#include "include/bss_err.h"
#include "memory/mem_manager.h"
#include "securec.h"

namespace ock {
namespace bss {
class MemorySegment;
using MemorySegmentRef = Ref<MemorySegment>;
class MemorySegment : public Buffer {
public:
    MemorySegment(uint32_t capacity, uint8_t *data, const MemManagerRef &memManager)
        : mSegment(data), mCapacity(capacity), mMemManager(memManager)
    {
    }

    MemorySegment(uint32_t capacity, uint8_t *data, bool toFree) : mSegment(data), mCapacity(capacity), mFree(toFree)
    {
    }

    ~MemorySegment() override
    {
        if (mFree) {
            free(mSegment);
            mSegment = nullptr;
        } else if (mSegment != nullptr && mMemManager != nullptr) {
            auto address = reinterpret_cast<uintptr_t>(mSegment);
            mMemManager->ReleaseMemory(address);
        }
    }

    inline uint8_t ToUint8_t(uint32_t offset)
    {
        return *reinterpret_cast<uint8_t *>(mSegment + offset);
    }

    inline uint16_t ToUint16_t(uint32_t offset)
    {
        return *reinterpret_cast<uint16_t *>(mSegment + offset);
    }

    inline uint32_t ToUint32_t(uint32_t offset)
    {
        return *reinterpret_cast<uint32_t *>(mSegment + offset);
    }

    inline uint64_t ToUint64_t(uint32_t offset)
    {
        return *reinterpret_cast<uint64_t *>(mSegment + offset);
    }

    inline void PutUint8_t(uint32_t offset, uint8_t value)
    {
        *(mSegment + offset) = value;
    }

    inline void PutUint16_t(uint32_t offset, uint16_t value)
    {
        *(uint16_t *)(mSegment + offset) = value;
    }

    inline void PutUint32_t(uint32_t offset, uint32_t value)
    {
        *(uint32_t *)(mSegment + offset) = value;
    }

    inline void PutUint64_t(uint32_t offset, uint64_t value)
    {
        *(uint64_t *)(mSegment + offset) = value;
    }

    inline uint32_t Offset(const uint8_t *pos)
    {
        if (UNLIKELY(pos == nullptr)) {
            LOG_ERROR("pos is nullptr.");
            return UINT32_MAX;
        }
        return static_cast<uint32_t>(pos - mSegment);
    }

    BResult Copy(uint32_t from, uint32_t to, uint32_t length)
    {
        if (from + length >= mCapacity || to + length >= mCapacity) {
            return BSS_ERR;
        }
        if (from == to || length == 0) {
            return BSS_OK;
        }
        if (from + length > to || to + length > from) {
            int ret = memcpy_s(mSegment + to, mCapacity - to, mSegment + from, length);
            RETURN_NOT_OK(ret);
        }
        return BSS_OK;
    }

    inline BResult CopyFrom(uint8_t *data, uint32_t length, uint32_t destOffset)
    {
        if (UNLIKELY(destOffset + length > mCapacity)) {
            LOG_ERROR("can not copy,because the destOffset + length > capacity,destOffset:"
                      << destOffset << ",length:" << length << ",capacity:" << mCapacity);
            return BSS_ERR;
        }
        RETURN_INVALID_PARAM_AS_NULLPTR(data);
        int ret = memcpy_s(mSegment + destOffset, mCapacity - destOffset, data, length);
        if (UNLIKELY(ret != BSS_OK)) {
            LOG_ERROR("Failed to CopyFrom, error code:" << ret << ", length" << length << ", destOffset" << destOffset);
        }
        return ret;
    }

    BResult CopyTo(uint8_t *data, uint32_t length, uint32_t srcOffset)
    {
        if (UNLIKELY(srcOffset + length > mCapacity)) {
            LOG_ERROR("can not copy,because the srcOffset + length > capacity,srcOffset:"
                      << srcOffset << ",length:" << length << ",capacity:" << mCapacity);
            return BSS_ERR;
        }
        RETURN_INVALID_PARAM_AS_NULLPTR(data);
        int ret = memcpy_s(data, length, mSegment + srcOffset, length);
        if (UNLIKELY(ret != BSS_OK)) {
            LOG_ERROR("Failed to CopyTo, error code:" << ret << ", length" << length << ", srcOffset" << srcOffset);
        }
        return ret;
    }

    inline BResult CopyTo(const MemorySegmentRef &newSegment)
    {
        RETURN_INVALID_PARAM_AS_NULLPTR(newSegment);
        int32_t ret = memcpy_s(newSegment->GetSegment(), newSegment->GetLen(), GetSegment(), GetCurPos());
        if (UNLIKELY(ret != BSS_OK)) {
            LOG_ERROR("Failed to Copy from origin segment to new segment, ret:" << ret);
        } else {
            newSegment->UpdatePosition(mPosition);  // 更新newSegment的mPosition.
        }
        return ret;
    }

    inline BResult Allocate(uint32_t size, uint32_t &startOffset)
    {
        BResult res = Allocate(size);
        startOffset = mPosition - size;
        return res;
    }

    inline BResult Allocate(uint32_t size, uint8_t *&addr)
    {
        BResult res = Allocate(size);
        if (res != BSS_OK) {
            return res;
        }
        addr = mSegment + mPosition - size;
        return res;
    }

    inline BResult Allocate(uint32_t size)
    {
        if (EnsureRemaining(size) != BSS_OK) {
            return BSS_FRESH_TABLE_IS_FULL;
        }
        mPosition += size;
        return BSS_OK;
    }

    inline uint32_t GetLen()
    {
        return mCapacity;
    }

    inline uint32_t GetCurPos()
    {
        return mPosition;
    }

    uint8_t *Data() override
    {
        return mSegment;
    }

    inline uint8_t *GetSegment() const
    {
        return mSegment;
    }

    void UpdatePosition(uint32_t position)
    {
        this->mPosition = position;
    }

private:
    inline BResult EnsureRemaining(uint32_t size) const
    {
        if (UNLIKELY(mPosition + size > mCapacity || UINT32_MAX - size < mPosition)) {
            return BSS_ERR;
        }
        return BSS_OK;
    }

    uint8_t *mSegment = nullptr;
    uint32_t mCapacity = 0;
    uint32_t mPosition = 0;
    MemManagerRef mMemManager = nullptr;
    bool mFree = false;
};
}  // namespace bss
}  // namespace ock
#endif  // BOOST_SS_MEMORYSEGMENT_H