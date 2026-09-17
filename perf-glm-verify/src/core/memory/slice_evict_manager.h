/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */

#ifndef BOOST_SS_SLICEEVICTMANAGER_H
#define BOOST_SS_SLICEEVICTMANAGER_H

#include <atomic>
#include <memory>

#include "common/bss_log.h"

namespace ock {
namespace bss {

class SliceEvictManager {
public:
    inline void AddEvictingMemory(int64_t size)
    {
        mEvictingMemory.fetch_add(size, std::memory_order_seq_cst);
    }

    inline void SubEvictingMemory(int64_t size)
    {
        mEvictingMemory.fetch_sub(size, std::memory_order_seq_cst);
    }

    inline void AddSliceUsedMemory(int64_t size)
    {
        mUsedMemory.fetch_add(size, std::memory_order_seq_cst);
    }

    inline void SubSliceUsedMemory(int64_t size)
    {
        mUsedMemory.fetch_sub(size, std::memory_order_seq_cst);
    }

    inline int64_t GetMemHighMark() const
    {
        return mMemHighMark;
    }

    inline void SetMemHighMark(int64_t memHighMark)
    {
        mMemHighMark = memHighMark;
    }

    inline int64_t GetEvictingMemorySize() const
    {
        return mEvictingMemory.load(std::memory_order_seq_cst);
    }

    inline int64_t GetUsedMemorySize() const
    {
        return mUsedMemory.load(std::memory_order_seq_cst);
    }

    inline int64_t GetOverHighMark()
    {
        return mUsedMemory.load(std::memory_order_seq_cst) - mMemHighMark;
    }

private:
    int64_t mMemHighMark = 0;
    std::atomic<int64_t> mEvictingMemory{ 0 };
    std::atomic<int64_t> mUsedMemory{ 0 };
};
using SliceEvictManagerRef = std::shared_ptr<SliceEvictManager>;
}  // namespace bss
}  // namespace ock

#endif  // BOOST_SS_SLICEEVICTMANAGER_H
