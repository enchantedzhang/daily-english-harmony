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

#include "mem_manager.h"

#include <unistd.h>

namespace ock {
namespace bss {
MemManager *MemManager::mInstance = nullptr;
std::mutex MemManager::mLock;
std::atomic<uint64_t> MemManager::mHeapAvailableSize{ 0 };
std::atomic<uint64_t> MemManager::mHeapUsedSize{ 0 };

const char *g_memoryTypeNames[] = { "File", "Fresh", "Slice", "Snapshot", "Restore", "Heap", "Temp" };

std::ostream &operator<<(std::ostream &os, MemoryType type)
{
    auto index = static_cast<uint32_t>(type);
    if (index >= 0 && index < sizeof(g_memoryTypeNames) / sizeof(g_memoryTypeNames[0])) {
        os << g_memoryTypeNames[index];
    } else {
        os << index;
    }
    return os;
}

BResult MemManager::Initialize(ConfigRef config, bool alignAddress)
{
    std::lock_guard<std::mutex> lk(mLock);
    if (mInitialized) {
        LOG_INFO("MemManager already initialized");
        return BSS_OK;
    }
    mTaskSlotFlag = config->GetTaskSlotFlag();
    RETURN_NOT_OK(AddDbRefCount());

    mConfig = config;
    mAlignAddress = alignAddress;
    if (!CalMemoryTypeSize(config->GetTotalDBSize())) {
        LOG_ERROR("Calculate memory type size failed.");
        return BSS_ERR;
    }
    mHeapAvailableSize.store(config->GetHeapAvailableSize());
    for (auto &item : mTypeCurrentSize) {
        item.store(0, std::memory_order_relaxed);
    }

    RETURN_NOT_OK(InitAllocator());
    mInitialized = true;
    LOG_INFO("Memory pool type:" << static_cast<uint32_t>(mAllocatorType) << ", parameter is: " << ToString()
                                 << ", heapAvailableSize: " << mHeapAvailableSize.load());
    return BSS_OK;
}

void MemManager::Revoke(MemoryType type, uint64_t size)
{
    if (type == MemoryType::FILE_STORE && mFileStoreMemRevoke != nullptr) {
        mFileStoreMemRevoke(size);
    }
}

BResult MemManager::GetMemory(MemoryType type, uint64_t size, uintptr_t &mrAddress, bool force, uint32_t timeout)
{
    auto startTime = std::chrono::high_resolution_clock::now();
    std::chrono::milliseconds duration;
    BResult result;
    do {
        result = GetMemoryDirect(type, size, mrAddress);
        if (LIKELY(result == BSS_OK)) {
            break;
        }

        // 参数错误无需重试
        if (UNLIKELY(result == BSS_INVALID_PARAM)) {
            break;
        }

        Revoke(type, size);  // 通知各个模块准备释放内存.
        usleep(NO_100);      // 当前设置延迟100us进行重试.
        // 非强制申请内存判断是否达到重试时间.
        auto endTime = std::chrono::high_resolution_clock::now();
        duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
    } while (force || duration.count() <= timeout);
    return result;
}

BResult MemManager::GetMemoryDirect(MemoryType type, uint64_t size, uintptr_t &mrAddress)
{
    if (UNLIKELY(size == 0 || size > mMemoryLimit)) {
        LOG_ERROR("Size is invalid, type:" << type << ", size:" << size << ", memory limit:" << mMemoryLimit);
        return BSS_INVALID_PARAM;
    }

    uint64_t realSize = CalcSize(size);
    uint64_t headSize = realSize - size;

    if (UNLIKELY(type == MemoryType::BORROW_HEAP)) {
        return GetHeapMemory(type, size, mrAddress);
    }

    auto index = GetIndex(type);
    uint64_t currentSize = mTypeCurrentSize[index].load();
    if (UNLIKELY(currentSize + realSize > mTypeMaxSize[index] &&
                 (type != MemoryType::FILE_STORE || mHeapUsedSize.load() + realSize > mHeapAvailableSize.load()))) {
        LOG_LIMIT_WARN("memPool[" << type << "] exceed limit! [used:" << currentSize << ",alloc:" << realSize
                                  << ",capacity:" << mTypeMaxSize[index] << ", Current memory info: [File used:"
                                  << mTypeCurrentSize[GetIndex(MemoryType::FILE_STORE)].load()
                                  << ",Fresh used:" << mTypeCurrentSize[GetIndex(MemoryType::FRESH_TABLE)].load()
                                  << ",Slice used:" << mTypeCurrentSize[GetIndex(MemoryType::SLICE_TABLE)].load()
                                  << ", mHeapTotalSize: " << mHeapAvailableSize.load()
                                  << " ,mHeapUsedSize: " << mHeapUsedSize.load() << "]");
        return BSS_ALLOC_FAIL;
    }

    BResult hr = mAllocator->Allocate(realSize, mrAddress);
    if (UNLIKELY(hr != BSS_OK)) {
        LOG_ERROR("allocator memory failed, because of out of memory.");
        return hr;
    }
    mrAddress = mrAddress + headSize;
    auto *heads = reinterpret_cast<struct SegmentHeads *>(mrAddress - sizeof(struct SegmentHeads));
    heads->type = currentSize + size > mTypeMaxSize[index] ? static_cast<size_t>(MemoryType::BORROW_HEAP) :
                                                             static_cast<size_t>(type);
    heads->size = size;
    heads->head = headSize;
    if (static_cast<MemoryType>(heads->type) == MemoryType::BORROW_HEAP) {
        mHeapUsedSize.fetch_add(realSize, std::memory_order_relaxed);
    } else {
        mTypeCurrentSize[index].fetch_add(realSize, std::memory_order_relaxed);
    }
#if ENABLE_MEMORY_TRACKER
    {
        std::lock_guard<std::mutex> lk(mmAllocatedLock);
        mAllocated.push_back(mrAddress);
    };
#endif
    return BSS_OK;
}

BResult MemManager::GetHeapMemory(const MemoryType &type, uint64_t size, uintptr_t &mrAddress)
{
    uint64_t realSize = CalcSize(size);
    uint64_t headSize = realSize - size;

    if (UNLIKELY(mHeapUsedSize.load() + realSize > mHeapAvailableSize.load())) {
        LOG_LIMIT_WARN("memPool[" << type << "] exceed limit! [used:" << mHeapUsedSize.load() << ",alloc:" << realSize
                                  << ",capacity:" << mHeapAvailableSize.load() << "]");
        return BSS_ALLOC_FAIL;
    }

    BResult hr = mAllocator->Allocate(realSize, mrAddress);
    if (UNLIKELY(hr != BSS_OK)) {
        LOG_ERROR("allocator memory failed, because of out of memory.");
        return hr;
    }

    mrAddress = mrAddress + headSize;
    auto *heads = reinterpret_cast<struct SegmentHeads *>(mrAddress - sizeof(struct SegmentHeads));
    heads->type = static_cast<size_t>(type);
    heads->size = size;
    heads->head = headSize;
    mHeapUsedSize.fetch_add(realSize, std::memory_order_relaxed);
    return BSS_OK;
}

uint64_t MemManager::GetMemoryUseSize(MemoryType type)
{
    if (UNLIKELY(static_cast<uint8_t>(type) >= static_cast<uint8_t>(MemoryType::MEM_TYPE_BUTT))) {
        return 0;
    }
    if (type == MemoryType::BORROW_HEAP) {
        return mHeapUsedSize.load();
    }
    return mTypeCurrentSize[static_cast<uint8_t>(type)].load();
}

uint64_t MemManager::GetMemoryTypeMaxSize(MemoryType type)
{
    if (UNLIKELY(static_cast<uint8_t>(type) >= static_cast<uint8_t>(MemoryType::MEM_TYPE_BUTT))) {
        return 0;
    }
    if (type == MemoryType::BORROW_HEAP) {
        return mHeapAvailableSize.load();
    }
    return mTypeMaxSize[static_cast<uint8_t>(type)];
}

BResult MemManager::ReleaseMemory(uintptr_t &mrAddress)
{
    if (UNLIKELY(mrAddress == 0)) {
        LOG_ERROR("mrAddress is invalid!");
        return BSS_INVALID_PARAM;
    }
#if ENABLE_MEMORY_TRACKER
    {
        std::lock_guard<std::mutex> lk(mmAllocatedLock);
        mAllocated.remove(mrAddress);
    };
#endif

    struct SegmentHeads *heads = reinterpret_cast<struct SegmentHeads *>(mrAddress - sizeof(struct SegmentHeads));
    uint64_t realSize = CalcSize(heads->size);
    if (static_cast<MemoryType>(heads->type) == MemoryType::BORROW_HEAP) {
        mHeapUsedSize.fetch_sub(realSize, std::memory_order_relaxed);
    } else {
        mTypeCurrentSize[static_cast<size_t>(heads->type)].fetch_sub(realSize, std::memory_order_relaxed);
    }

    uintptr_t realAddress = mrAddress - heads->head;
    BResult hr = mAllocator->Free(realAddress);
    if (UNLIKELY(hr != BSS_OK)) {
        LOG_ERROR("free memory failed");
        return hr;
    }

    return BSS_OK;
}

std::string MemManager::ToString()
{
    std::string info = "TaskSlotID:" + std::to_string(mTaskSlotFlag) + ", ";
    info = info + "MemoryLimit Size:" + std::to_string(mMemoryLimit) + ", ";
    uint32_t index = 0;
    for (auto &item : mTypeMaxSize) {
        if (std::strcmp(g_memoryTypeNames[index], "Restore") == 0 ||
            std::strcmp(g_memoryTypeNames[index], "Heap") == 0) {
            index++;
            continue;
        }
        info = info + "MemoryType is " + g_memoryTypeNames[index] + ", MemoryMaxSize is " + std::to_string(item) + ", ";
        index++;
    }
    return info;
}

void MemManager::PrintUsage()
{
    LOG_WARN("ID:" << this << " [File used:" << mTypeCurrentSize[NO_0].load()
                   << ",Fresh used:" << mTypeCurrentSize[NO_1].load() << ",Slice used:" << mTypeCurrentSize[NO_2].load()
                   << ",Heap Used:" << mHeapUsedSize.load() << "]"
                   << " Slot:" << mTaskSlotFlag);
}

BResult MemManager::AddDbRefCount()
{
    mDbRefCount.fetch_add(NO_1, std::memory_order_relaxed);
    LOG_INFO("TaskSlot:" << mTaskSlotFlag << ", DbRefCount:" << mDbRefCount.load());

    // 单Slot多DB场景，需要更新内存分配方案
    if (mDbRefCount.load() > NO_1) {
        if (!UpdateMemoryTypeSize()) {
            LOG_ERROR("update memory type size of slot:" << mTaskSlotFlag);
            return BSS_ERR;
        }
    }

    return BSS_OK;
}

void MemManager::DecDbRefCount()
{
    mDbRefCount.fetch_sub(NO_1, std::memory_order_relaxed);
    // 已经开始释放DB，说明前面有SubTask已经执行完成，整个Job已经进入退出阶段
    inReleasePhase = true;
    LOG_INFO("TaskSlot:" << mTaskSlotFlag << ", DbRefCount:" << mDbRefCount.load());
}

bool MemManager::UpdateMemoryTypeSize()
{
    if (!mInitialized) {
        LOG_ERROR("Memory manager has not initialized");
        return false;
    }

    std::lock_guard<std::mutex> lk(mLock);
    if (CalMemoryTypeSize(mMemoryLimit)) {
        LOG_INFO("Update memory manager, parameter is: " << ToString());
        return true;
    }
    return false;
}

bool MemManager::CalMemoryTypeSize(uint64_t memoryLimit)
{
    LOG_INFO("begin to cal memory manager, memoryLimit: " << memoryLimit);
    mMemoryLimit = memoryLimit;
    uint64_t availableMem = mMemoryLimit;

    // 计算FreshTable大小
    uint64_t totalFreshTypeSize = CalcFreshTableSize();
    LOG_INFO("Calc fresh table size is " << totalFreshTypeSize);
    if (totalFreshTypeSize > (mMemoryLimit / NO_2)) {
        LOG_ERROR("Fresh table size: " << totalFreshTypeSize << "exceed half of memory limit: " << mMemoryLimit);
        return false;
    }

    // 设置FreshTable的可用内存限制.
    mTypeMaxSize[static_cast<size_t>(MemoryType::FRESH_TABLE)] = totalFreshTypeSize;

    // 设置Checkpoint的可用内存限制(预留一个segment大小用于checkpoint时拷贝segment)
    mTypeMaxSize[static_cast<size_t>(MemoryType::SNAPSHOT)] = CalcSize(mConfig->GetMemorySegmentSize());

    // Restore临时内存，不计入内存管理(用于应对Restore时，并行度变更等原因导致SNAPSHOT内存改变，不足以容纳segment文件恢复的突发情况)
    mTypeMaxSize[static_cast<size_t>(MemoryType::RESTORE)] = CalcSize(IO_SIZE_64M);

    // 设置FileStore的可用内存限制.
    mTypeMaxSize[static_cast<size_t>(MemoryType::FILE_STORE)] = memoryLimit * mConfig->GetFileStoreMemoryFraction();

    uint64_t allocatedSize = totalFreshTypeSize + mTypeMaxSize[static_cast<size_t>(MemoryType::SNAPSHOT)] +
                             mTypeMaxSize[static_cast<size_t>(MemoryType::FILE_STORE)];

    if (availableMem <= allocatedSize) {
        LOG_ERROR("current db memory limit: " << memoryLimit << " is not enough.");
        return false;
    }

    // 设置SliceTable的可用内存限制.
    mTypeMaxSize[static_cast<size_t>(MemoryType::SLICE_TABLE)] = availableMem - allocatedSize;
    return true;
}

uint32_t MemManager::CalcFreshTableSize()
{
    uint32_t segmentCount = mDbRefCount.load() + 1;
    uint32_t segmentSize;

    if (mMemoryLimit >= IO_SIZE_640M) {
        segmentSize = IO_SIZE_64M;
    } else if (mMemoryLimit >= IO_SIZE_320M) {
        segmentSize = IO_SIZE_32M;
    } else {
        segmentSize = IO_SIZE_16M;
    }

    // 如果是首次初始化，且有配置Segment大小，优先使用配置值(for UT)
    if (!mInitialized && mConfig->GetMemorySegmentSize() != 0) {
        segmentSize = mConfig->GetMemorySegmentSize();
        LOG_INFO("Fresh memory segment size has been set to:" << segmentSize);
    }

    LOG_INFO("Calc memory segment size:" << segmentSize << ", count:" << segmentCount);
    mConfig->SetMemorySegmentSize(segmentSize);
    return segmentCount * CalcSize(segmentSize);
}

BResult MemManager::InitAllocator()
{
    uint32_t segmentSize = mConfig->GetMemorySegmentSize();
    if (mAllocatorType == AllocatorType::MEM_MULTI) {
        uint64_t realSegSize = segmentSize + sizeof(struct SegmentHeads);
        uint64_t realTotalSize = realSegSize * NO_2;  // 仅预留2个，防止占用系统内存，不够时走malloc/free
        mBaseAddr = malloc(realTotalSize);
        RETURN_FALSE_AS_NULLPTR(mBaseAddr);
        mAllocator = MakeRef<MultiAllocator>();
        if (mAllocator.Get() == nullptr) {
            LOG_ERROR("create MultiAllocator failed.");
            free(mBaseAddr);
            mBaseAddr = nullptr;
            return BSS_ERR;
        }
        mAllocator->Initialize(reinterpret_cast<uintptr_t>(mBaseAddr), realTotalSize, realSegSize, mAlignAddress);
    } else {
        mAllocator = MakeRef<DirectAllocator>();
        RETURN_FALSE_AS_NULLPTR(mAllocator.Get());
    }
    return BSS_OK;
}
}  // namespace bss
}  // namespace ock