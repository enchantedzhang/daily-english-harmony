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

#ifndef BOOST_SS_BLOCK_H
#define BOOST_SS_BLOCK_H

#include <memory>

#include "include/bss_types.h"
#include "slice_table/binary/byte_buffer.h"

namespace ock {
namespace bss {
class Block {
public:
    explicit Block(ByteBufferRef &byteBuffer) : mBuffer(byteBuffer)
    {
    }

    virtual ~Block() = default;

    virtual ByteBufferRef GetBuffer()
    {
        return mBuffer;
    }

    virtual uint32_t BufferLen()
    {
        if (UNLIKELY(mBuffer == nullptr)) {
            LOG_ERROR("buffer is null.");
            return 0;
        }
        return mBuffer->mCapacity;
    }

    virtual uint32_t GetKeyIndexElement(uint32_t indexOffset, uint32_t numBytesForElement, uint32_t index)
    {
        return 0;
    }

    virtual BlockType GetBlockType() = 0;

protected:
    ByteBufferRef mBuffer;
};
using BlockRef = std::shared_ptr<Block>;

}  // namespace bss
}  // namespace ock
#endif  // BOOST_SS_BLOCK_H
