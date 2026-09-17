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

#ifndef BOOST_SS_FILE_HOLDER_H
#define BOOST_SS_FILE_HOLDER_H

#include <cstdint>

#include "memory/mem_manager.h"

namespace ock {
namespace bss {
enum class FileProcHolder : uint32_t {
    FILE_STORE_FLUSH = 0,
    FILE_STORE_GET = 1,
    FILE_STORE_ITERATOR = 2,
    FILE_STORE_COMPACTION = 3,
    FILE_STORE_SNAPSHOT = 4,
    FILE_STORE_COMPRESS = 5,
    FILE_STORE_SAVEPOINT = 6,
};

class FileMemAllocator {
public:
    static const uint32_t DEFAULT_TIMEOUT = 5000;  // 5s

    static const uint32_t SAVEPOINT_TIMEOUT = 5 * 60 * 1000;  // 5min

    static void AllocStrategy(FileProcHolder holder, bool &force, uint32_t &ts);

    static uintptr_t Alloc(const MemManagerRef &memMgr, FileProcHolder holder, uint64_t size, const char *func);

    inline static bool IsForceType(FileProcHolder holder)
    {
        return holder == FileProcHolder::FILE_STORE_GET || holder == FileProcHolder::FILE_STORE_ITERATOR ||
               holder == FileProcHolder::FILE_STORE_SNAPSHOT || holder == FileProcHolder::FILE_STORE_FLUSH;
    }
};

}  // namespace bss
}  // namespace ock
#endif  // BOOST_SS_FILE_HOLDER_H