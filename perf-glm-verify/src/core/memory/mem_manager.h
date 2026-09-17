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

#ifndef BOOST_SS_MEM_MANAGER_H
#define BOOST_SS_MEM_MANAGER_H

#include <array>
#include <atomic>
#include <functional>
#include <mutex>
#include <unordered_map>
#include <utility>

#include "allocator.h"
#include "bss_metric.h"
#include "common/bss_log.h"
#include "executor_service.h"
#include "include/bss_err.h"
#include "include/bss_types.h"
#include "include/config.h"
#include "include/ref.h"

namespace ock {
namespace bss {
constexpr size_t MEM_TYPE_NUM = static_cast<size_t>(MemoryType::MEM_TYPE_BUTT);

class MemManager : public Referable {
public:
    explicit MemManager(AllocatorType type) : mAllocatorType(type), mInitialized(false){};

    ~MemManager() override
    {
        std::lock_guard<std::mutex> lk(mLock);
        LOG_INFO("Memory info when delete memory manager: [File used:"
                 << mTypeCurrentSize[GetIndex(MemoryType::FILE_STORE)].load()
                 << ",Fresh used: " << mTypeCurrentSize[GetIndex(MemoryType::FRESH_TABLE)].load()
                 << ",Slice used: " << mTypeCurrentSize[GetIndex(MemoryType::SLICE_TABLE)].load()
                 << ",Snap used: " << mTypeCurrentSize[GetIndex(MemoryType::SNAPSHOT)].load()
                 << ",Heap used: " << mHeapUsedSize.load() << "]");
        if (mInitialized && (mAllocatorType == AllocatorType::MEM_MULTI) && mBaseAddr != nullptr) {
            mAllocator->Destroy();
            free(mBaseAddr);
            mBaseAddr = nullptr;
        }
        mAllocator = nullptr;
        if (mInstance != nullptr) {
            mInstance = nullptr;
        }
#if ENABLE_MEMORY_TRACKER
        ReportLeaks();
#endif
    }

    inline ConfigRef GetConfig()
    {
        return mConfig;
    }

    inline size_t CalcSize(uint64_t size)
    {
        return size + sizeof(struct SegmentHeads);
    }

    inline size_t GetIndex(MemoryType type)
    {
        return static_cast<size_t>(type);
    }

    BResult Initialize(ConfigRef config, bool alignAddress = true);

    void Exit()
    {
    }

    // force表示是否一直重试申请内存; ts表示允许的最大重试时间, 当force为false时有效, 单位ms.
    BResult GetMemory(MemoryType type, uint64_t size, uintptr_t &mrAddress, bool force = true, uint32_t timeout = 0);

    BResult GetMemoryDirect(MemoryType type, uint64_t size, uintptr_t &mrAddress);
    BResult GetHeapMemory(const MemoryType &type, uint64_t size, uintptr_t &mrAddress);

    BResult ReleaseMemory(uintptr_t &mrAddress);

    uint64_t GetMemoryUseSize(MemoryType type);
    uint64_t GetMemoryTypeMaxSize(MemoryType type);
    std::string ToString();
    bool CalMemoryTypeSize(uint64_t memoryLimit);
    BResult InitAllocator();
    BResult AddDbRefCount();
    void DecDbRefCount();
    uint32_t CalcFreshTableSize();
    bool UpdateMemoryTypeSize();
    void PrintUsage();

    void Revoke(MemoryType type, uint64_t size);

    static inline uint64_t ChangeHeapOverSize(uint64_t newSize)
    {
        uint64_t heapUsedSize;
        if (newSize < (heapUsedSize = mHeapUsedSize.load())) {
            // heap内存使用量超过newSize，预留4m内存，防止卡主。
            newSize = heapUsedSize + IO_SIZE_4MB;
        }
        uint64_t oldSize = mHeapAvailableSize.load();
        mHeapAvailableSize.store(newSize);
        LOG_INFO("Resize heap available size from " << oldSize << " to " << newSize);
        return newSize;
    }

    void RegisterLsmStoreRevoke(std::function<void(uint64_t)> func)
    {
        if (mFileStoreMemRevoke == nullptr) {
            mFileStoreMemRevoke = func;
        }
    }

    inline void RegisterMetric(uintptr_t dbAddr, BoostNativeMetricPtr metricPtr)
    {
        if (UNLIKELY(metricPtr == nullptr)) {
            LOG_ERROR("RegisterMetric failed, metricPtr is nullptr.");
            return;
        }

        metricPtr->SetUsedMemoryGetter([this](MemoryType type) -> uint64_t { return GetMemoryUseSize(type); });
        metricPtr->SetMaxMemoryGetter([this](MemoryType type) -> uint64_t { return GetMemoryTypeMaxSize(type); });
        mDBsMetricMap.emplace(dbAddr, metricPtr);
        // update memory type max value to metric
        for (const auto &pair : mDBsMetricMap) {
            BoostNativeMetricPtr metric = pair.second;
            metric->SetMemoryTotalMax(mMemoryLimit);
            metric->SetMemoryFreshMax(mTypeMaxSize[static_cast<size_t>(MemoryType::FRESH_TABLE)]);
            metric->SetMemorySliceMax(mTypeMaxSize[static_cast<size_t>(MemoryType::SLICE_TABLE)]);
            metric->SetMemoryFileMax(mTypeMaxSize[static_cast<size_t>(MemoryType::FILE_STORE)]);
            metric->SetMemorySnapshotMax(mTypeMaxSize[static_cast<size_t>(MemoryType::SNAPSHOT)]);
            LOG_INFO("Set memory limit info to metric.");
        }
        LOG_INFO("Register metric to memory manager success.");
    }

    inline void DeregisterMetric(uintptr_t dbAddr)
    {
        auto iter = mDBsMetricMap.find(dbAddr);
        if (iter != mDBsMetricMap.end()) {
            auto metric = iter->second;
            // 主动置空，避免memManager释放后lambda表达式捕获this产生悬空指针
            metric->SetUsedMemoryGetter(nullptr);
            metric->SetMaxMemoryGetter(nullptr);
            mDBsMetricMap.erase(dbAddr);
        }
        LOG_INFO("Deregister metric from memory manager success.");
    }

#if ENABLE_MEMORY_TRACKER
    void ReportLeaks()
    {
        std::lock_guard<std::mutex> lk(mmAllocatedLock);
        if (!mAllocated.empty()) {
            LOG_ERROR("Memory leaks detected: " << mAllocated.size() << " blocks");
            for (const auto &addr : mAllocated) {
                auto head = reinterpret_cast<struct SegmentHeads *>(addr - sizeof(struct SegmentHeads));
                LOG_ERROR("Leaked:  << std::hex << addr << std::dec << , Size: " << head->size
                                                                                 << ", Type: " << head->type);
            }
        } else {
            LOG_INFO("No memory leaks detected.");
        }
    }
#endif

private:
    static MemManager *mInstance;
    static std::mutex mLock;
    uint64_t mMemoryLimit = 0;
    ConfigRef mConfig;
    AllocatorType mAllocatorType;
    std::atomic<bool> mInitialized{ false };
    AllocatorPtr mAllocator;
    void *mBaseAddr = nullptr;
    std::function<void(uint64_t)> mFileStoreMemRevoke;
    bool mAlignAddress = false;
    bool inReleasePhase = false;
    uint32_t mTaskSlotFlag = 0;
    // MemManager归属某个TaskSlot，mDbRefCount表示当前Slot下的DB数量
    std::atomic<uint32_t> mDbRefCount{ 0 };
    static std::atomic<uint64_t> mHeapAvailableSize;
    static std::atomic<uint64_t> mHeapUsedSize;

    std::array<uint64_t, MEM_TYPE_NUM> mTypeMaxSize;
    std::array<std::atomic<uint64_t>, MEM_TYPE_NUM> mTypeCurrentSize;
    // MemManager归属某个TaskSlot, 由多个DB共享，每个DB都有自己的metric实例，因此需要使用map维护DB和metric实例的对应关系
    std::unordered_map<uintptr_t, BoostNativeMetricPtr> mDBsMetricMap;
#if ENABLE_MEMORY_TRACKER
    std::mutex mmAllocatedLock;
    std::list<uintptr_t> mAllocated;
#endif
};
using MemManagerRef = std::shared_ptr<MemManager>;

#if ENABLE_MEMORY_MONITOR
class MemManagerMonitorTask : public Runnable {
public:
    MemManagerMonitorTask(MemManagerRef memManager) : mMemManager(std::move(memManager))
    {
    }

    ~MemManagerMonitorTask() override
    {
        LOG_INFO("Delete MemManagerMonitorTask success");
    }

    void Run() override
    {
        while (true) {
            if (mStopFlag) {
                break;
            }
            mMemManager->PrintUsage();
            std::this_thread::sleep_for(std::chrono::seconds(NO_3));
        }
    }

    void StopTask()
    {
        mStopFlag = true;
    }

private:
    MemManagerRef mMemManager;
    bool mStopFlag = false;
};
using MemManagerMonitorTaskRef = std::shared_ptr<MemManagerMonitorTask>;
#endif

}  // namespace bss
}  // namespace ock

#endif
