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

#ifndef FIXED_SIZE_MEMORY_POOL_H
#define FIXED_SIZE_MEMORY_POOL_H
#include <algorithm>
#include <cstdint>
#include <mutex>
#include <vector>

#include "binary/byte_buffer.h"
#include "mem_manager.h"

namespace ock {
namespace bss {

class FixedSizeMemoryPool : public Buffer {
public:
    FixedSizeMemoryPool(uint8_t *baseAddr, uint32_t totalSize, const MemManagerRef &memManager)
        : mBaseAddr(baseAddr), mTotalMemSize(totalSize), mMemManager(memManager)
    {
    }

    FixedSizeMemoryPool(uint8_t *baseAddr, uint32_t totalSize, bool toFree)
        : mBaseAddr(baseAddr), mTotalMemSize(totalSize), mFree(toFree)
    {
    }

    ~FixedSizeMemoryPool() override
    {
        if (mFree) {
            free(mBaseAddr);
            mBaseAddr = nullptr;
        } else if (mBaseAddr != nullptr && mMemManager != nullptr) {
            auto address = reinterpret_cast<uintptr_t>(mBaseAddr);
            mMemManager->ReleaseMemory(address);
        }
    }

    uint8_t *Data() override
    {
        return mBaseAddr;
    }

    BResult Init(uint32_t fixedBlockSize)
    {
        if (UNLIKELY(fixedBlockSize == 0 || mTotalMemSize < fixedBlockSize)) {
            return BSS_INNER_ERR;
        }

        mBlockSize = fixedBlockSize;
        uint32_t totalBlocks = mTotalMemSize / fixedBlockSize;

        // 预分配所有块到空闲列表（栈式分配）
        mFreeList.reserve(totalBlocks);
        for (uint32_t i = 0; i < totalBlocks; ++i) {
            uint8_t *blockAddr = mBaseAddr + i * fixedBlockSize;
            mFreeList.emplace_back(blockAddr);
        }
        mIinitialized.store(true);
        return BSS_OK;
    }

    BResult Allocate(uint32_t size, uint8_t *&addr)
    {
        std::lock_guard<std::mutex> lock(mMutex);
        if (UNLIKELY(mFreeList.empty() || size > mBlockSize)) {
            if (!mIinitialized.load()) {
                RETURN_NOT_OK(Init(size));
            } else {
                return BSS_ALLOC_FAIL;
            }
        }

        addr = mFreeList.back();
        mFreeList.pop_back();
        return BSS_OK;
    }

    void Free(uint8_t *ptr)
    {
        std::lock_guard<std::mutex> lock(mMutex);
        mFreeList.push_back(ptr);
    }

private:
    uint8_t *mBaseAddr = nullptr;
    uint32_t mTotalMemSize = 0;
    uint32_t mBlockSize = 0;
    std::vector<uint8_t *> mFreeList;
    std::mutex mMutex;
    std::atomic<bool> mIinitialized{ false };
    bool mFree = false;
    MemManagerRef mMemManager = nullptr;
};
using FixedSizeMemoryPoolRef = Ref<FixedSizeMemoryPool>;
}  // namespace bss
}  // namespace ock

#endif