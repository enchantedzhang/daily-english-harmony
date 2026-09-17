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

#include "allocator.h"

namespace ock {
namespace bss {

Allocator::~Allocator() = default;

BResult DirectAllocator::Initialize(uintptr_t mrAddress, uint64_t mrSize, uint32_t minBlockSize, bool alignAddress)
{
    return BSS_OK;
}

uintptr_t DirectAllocator::MemOffset(uintptr_t address) const
{
    return 0;
}

uint64_t DirectAllocator::FreeSize() const
{
    return 0;
}

BResult DirectAllocator::Allocate(uint64_t &size, uintptr_t &outAddress)
{
    void *addr = malloc(size);
    if (addr == nullptr) {
        LOG_ERROR("mem is unavailable!");
        return BSS_ALLOC_FAIL;
    }
    outAddress = reinterpret_cast<uintptr_t>(addr);
    return BSS_OK;
}

BResult DirectAllocator::Free(uintptr_t address)
{
    void *add = reinterpret_cast<void *>(address);
    if (address == 0 || add == nullptr) {
        LOG_ERROR("wrong address, address is null.");
        return BSS_INVALID_PARAM;
    }
    free(add);
    return BSS_OK;
}

BResult MultiAllocator::Initialize(uintptr_t address, uint64_t totalSize, uint32_t blockSize, bool alignAddress)
{
    if (blockSize == 0) {
        LOG_ERROR("Invalid para, blockSize is 0.");
        return BSS_INVALID_PARAM;
    }
    mBigBlockSize = blockSize;
    mBigStart = address;
    mBigEnd = mBigStart + totalSize;
    uint64_t bigTotalNum = (mBigEnd - mBigStart) / mBigBlockSize;
    for (uint64_t i = 0; i < bigTotalNum; i++) {
        mBigList.push_back(mBigStart + i * mBigBlockSize);
    }
    return BSS_OK;
}

uintptr_t MultiAllocator::MemOffset(uintptr_t address) const
{
    return 0;
}

uint64_t MultiAllocator::FreeSize() const
{
    return 0;
}

BResult MultiAllocator::Allocate(uint64_t &size, uintptr_t &outAddress)
{
    do {
        if (size == mBigBlockSize && !mBigList.empty()) {
            WriteLocker<ReadWriteLock> lock(&mBigLock);
            if (mBigList.empty()) {
                break;
            }
            outAddress = mBigList.front();
            mBigList.pop_front();
            return BSS_OK;
        }
    } while (false);

    void *addr = malloc(size);
    if (addr == nullptr) {
        LOG_ERROR("Malloc memory failed, because of the system memory is insufficient.");
        return BSS_ALLOC_FAIL;
    }
    outAddress = reinterpret_cast<uintptr_t>(addr);
    return BSS_OK;
}

BResult MultiAllocator::Free(uintptr_t address)
{
    if (address == 0) {
        LOG_ERROR("wrong address, address is null.");
        return BSS_INVALID_PARAM;
    }

    if (address >= mBigStart && address < mBigEnd) {
        WriteLocker<ReadWriteLock> lock(&mBigLock);
        mBigList.push_front(address);
        return BSS_OK;
    } else {
        free(reinterpret_cast<void *>(address));
        return BSS_OK;
    }
}
}  // namespace bss
}  // namespace ock