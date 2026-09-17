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

#ifndef BOOST_SS_ALLOCATOR_H
#define BOOST_SS_ALLOCATOR_H

#include <list>

#include "common/bss_log.h"
#include "include/bss_err.h"
#include "include/bss_types.h"
#include "include/ref.h"
#include "util/bss_lock.h"

namespace ock {
namespace bss {
/**
 * @brief Type of allocator
 */
enum class AllocatorType {
    DIRECT = 0,
    MEM_MULTI = 1,
};

/**
 * @brief Allocator cache tier policy
 */
enum class AllocatorCacheTierPolicy : int16_t {
    TIER_TIMES = 0, /* tier by times of min-block-size */
    TIER_POWER = 1, /* tier by power of min-block-size */
};

/**
 * @brief Allocator options
 */
struct AllocatorOptions {
    uintptr_t address = 0;                   /* base address of large range of memory for allocator */
    uint64_t size = 0;                       /* size of large memory chuck */
    uint32_t minBlockSize = 0;               /* min size of block can be allocated from allocator */
    uint32_t bucketCount = NO_8192;          /* default size of hash bucket */
    bool alignedAddress = false;             /* force to align the memory block allocated */
    uint16_t cacheTierCount = NO_8;          /* for DYNAMIC_SIZE_WITH_CACHE only */
    uint16_t cacheBlockCountPerTier = NO_16; /* for DYNAMIC_SIZE_WITH_CACHE only */
    AllocatorCacheTierPolicy cacheTierPolicy = AllocatorCacheTierPolicy::TIER_TIMES; /* tier policy */
};

/**
 * @brief Segment Head
 */
struct SegmentHeads {
    uint32_t type;
    uint32_t size;
    uint64_t head;  // Find the real ptr head
};

/**
 * @brief Allocator to alloc memory area from a large mount of memory.
 *
 * For example, we have RDMA memory region, which already registered to NIC,
 * and we need to reuse memory on this region, so we need to alloc sub part
 * of memory from the large memory region, use it and return it.
 */
class Allocator : public Referable {
public:
    Allocator() = default;
    ~Allocator() override;

    virtual BResult Initialize(uintptr_t mrAddress, uint64_t mrSize, uint32_t minBlockSize, bool alignAddress) = 0;

    /**
     * @brief Get the memory offset based on base address
     *
     * @param address      [in] memory address
     *
     * @return offset comparing to base address
     */
    virtual uintptr_t MemOffset(uintptr_t address) const = 0;

    /**
     * @brief Get free memory size
     *
     * @return Free memory size
     */
    virtual uint64_t FreeSize() const = 0;

    /**
     * @brief Allocate memory area
     *
     * @param size         [in] size of memory of demand
     * @param outAddress   [out] allocated memory address
     *
     * @return 0 if successful
     */
    virtual BResult Allocate(uint64_t &size, uintptr_t &outAddress) = 0;

    /**
     * @brief Free the address allocated by #Allocate function
     *
     * @param address      [in] address to be freed
     *
     * @param 0 if successful
     */
    virtual BResult Free(uintptr_t address) = 0;

    /**
     * @brief function should be called before managed memory freeing
     *
     * Remove memory protection if enabled(cmake -DBUILD_WITH_ALLOCATOR_PROTECTION=ON),
     * should be called before freeing the memory passed in, otherwise sigsegv will raise by free(),
     * It's suggested to be called even if you are not using memory protection currently,
     * in case you may miss this once you turn memory protection on in the future.
     */
    virtual void Destroy(){};
};

using AllocatorPtr = Ref<Allocator>;

class DirectAllocator : public Allocator {
public:
    ~DirectAllocator() override = default;
    DirectAllocator() = default;

    BResult Initialize(uintptr_t mrAddress, uint64_t mrSize, uint32_t minBlockSize, bool alignAddress) override;

    uintptr_t MemOffset(uintptr_t address) const override;

    uint64_t FreeSize() const override;

    BResult Allocate(uint64_t &size, uintptr_t &outAddress) override;

    BResult Free(uintptr_t address) override;

    void Destroy() override{};
};

class MultiAllocator : public Allocator {
public:
    ~MultiAllocator() override = default;
    MultiAllocator() = default;

    BResult Initialize(uintptr_t mrAddress, uint64_t mrSize, uint32_t minBlockSize, bool alignAddress) override;

    uintptr_t MemOffset(uintptr_t address) const override;

    uint64_t FreeSize() const override;

    BResult Allocate(uint64_t &size, uintptr_t &outAddress) override;

    BResult Free(uintptr_t address) override;

    inline void Destroy() override
    {
        mBigList.clear();
    }

private:
    ReadWriteLock mBigLock;
    uintptr_t mBigStart;
    uintptr_t mBigEnd;
    uint64_t mBigBlockSize;
    std::list<uintptr_t> mBigList;
};
}  // namespace bss
}  // namespace ock
#endif  // BOOST_SS_ALLOCATOR_H
