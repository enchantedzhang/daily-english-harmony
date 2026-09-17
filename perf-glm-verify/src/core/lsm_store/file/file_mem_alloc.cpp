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

#include "file_mem_allocator.h"

namespace ock {
namespace bss {
const char *g_holderNames[] = { "Flush", "Get", "Iterator", "Compaction", "Snapshot", "Compress", "Savepoint" };

void FileMemAllocator::AllocStrategy(FileProcHolder holder, bool &force, uint32_t &ts)
{
    /*
     * 1. SliceTable调用FlushLevel0Table流程必须保证内存申请成功,
     * 避免SliceTable内存水位高导致FreshTable淘汰memorySegment失败.
     * 2. SliceTable调用Get流程必须保证内存申请成功, 避免返回上层失败, 进而导致业务读请求失败.
     * 3. SliceTable调用Iterator流程必须保证内存申请成功, 避免返回上层失败, 进而导致业务读请求失败.
     */
    switch (holder) {
        case FileProcHolder::FILE_STORE_GET:
        case FileProcHolder::FILE_STORE_ITERATOR:
        case FileProcHolder::FILE_STORE_SNAPSHOT:
        case FileProcHolder::FILE_STORE_FLUSH:
            force = true;  // 强制申请内存.
            ts = 0;
            break;
        case FileProcHolder::FILE_STORE_COMPACTION:
        case FileProcHolder::FILE_STORE_COMPRESS:
            force = false;  // 尝试申请内存.
            ts = DEFAULT_TIMEOUT;
            break;
        case FileProcHolder::FILE_STORE_SAVEPOINT:
            force = false;  // 尝试申请内存.
            ts = SAVEPOINT_TIMEOUT;
            break;
        default:
            LOG_ERROR("Invalid file proc holder:" << static_cast<uint32_t>(holder) << ".");
            break;
    }
}

uintptr_t FileMemAllocator::Alloc(const MemManagerRef &memMgr, FileProcHolder holder, uint64_t size, const char *func)
{
    if (UNLIKELY(memMgr == nullptr)) {
        LOG_ERROR("memMgr is null, holder:" << g_holderNames[static_cast<uint32_t>(holder)]);
        return 0;
    }

    // 1. 根据holder决定内存分配策略.
    bool isForce = false;
    uint32_t timeout = NO_3000;  // 3s
    AllocStrategy(holder, isForce, timeout);

    // 2. 申请内存资源.
    uintptr_t addr = 0;
    auto ret = memMgr->GetMemory(MemoryType::FILE_STORE, size, addr, isForce, timeout);
    if (UNLIKELY(ret != BSS_OK)) {
        LOG_WARN("Get fileStore memory failed, ret:"
                 << ret << ", size:" << size << ", holder:" << g_holderNames[static_cast<uint32_t>(holder)]
                 << ", isForce:" << isForce << ", timeout:" << timeout << ", call func:" << func);
        return 0;
    }
    return addr;
}
}  // namespace bss
}  // namespace ock