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

#ifndef BOOST_SS_SERIALIZED_DATA_H
#define BOOST_SS_SERIALIZED_DATA_H
#include <cstdint>

#include "include/ref.h"
#include "slice_table/binary/byte_buffer.h"

namespace ock {
namespace bss {

class SerializedDataWrapper : public Referable {
public:
    SerializedDataWrapper(BufferRef buffer, uint32_t offset, uint32_t length)
        : mBuffer(buffer), mOffset(offset), mData(buffer->Data() + offset), mLength(length)
    {
    }

    SerializedDataWrapper(const uint8_t *data, uint32_t length, const BufferRef &buffer)
        : mBuffer(buffer), mOffset(buffer == nullptr ? 0 : data - buffer->Data()), mData(data), mLength(length)
    {
    }

    ~SerializedDataWrapper() override = default;

    inline const uint8_t *Data()
    {
        return mData;
    }

    inline uint32_t Length()
    {
        return mLength;
    }

    inline const uint8_t *Data() const
    {
        return mData;
    }

    inline uint32_t Length() const
    {
        return mLength;
    }

    std::string ToString() const;

    BufferRef mBuffer;
    uint32_t mOffset;
    const uint8_t *mData;
    uint32_t mLength;
};
using DataWrapper = SerializedDataWrapper;
using DataWrapperRef = SerializedDataWrapper *;

}  // namespace bss
}  // namespace ock
#endif  // BOOST_SS_SERIALIZED_DATA_H