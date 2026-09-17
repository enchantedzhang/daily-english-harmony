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

#ifndef BOOST_SS_BLOCK_HANDLE_H
#define BOOST_SS_BLOCK_HANDLE_H

#include <memory>

namespace ock {
namespace bss {
class BlockHandle {
public:
    BlockHandle() = default;

    BlockHandle(uint32_t offset, uint32_t size) : mOffset(offset), mSize(size)
    {
    }

    BlockHandle(const BlockHandle &other)
    {
        mOffset = other.GetOffset();
        mSize = other.GetSize();
    }

    BlockHandle &operator=(const BlockHandle &other)
    {
        if (this != &other) {
            mOffset = other.GetOffset();
            mSize = other.GetSize();
        }
        return *this;
    }

    inline void Fill(uint32_t offset, uint32_t size)
    {
        mOffset = offset;
        mSize = size;
    }

    inline uint32_t GetOffset() const
    {
        return mOffset;
    }

    inline uint32_t GetSize() const
    {
        return mSize;
    }

    inline bool IsEmpty() const
    {
        return (mOffset == 0) && (mSize == 0);
    }

private:
    uint32_t mOffset = 0;
    uint32_t mSize = 0;
};
using BlockHandleRef = std::shared_ptr<BlockHandle>;

}  // namespace bss
}  // namespace ock
#endif  // BOOST_SS_BLOCK_HANDLE_H