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

#ifndef BLOB_STORE_MANAGER_H
#define BLOB_STORE_MANAGER_H
#include <atomic>
#include <cstdint>
#include <memory>

namespace ock {
namespace bss {

class BlobStoreStat {
public:
    inline void AddTotalBlobNum(int64_t size)
    {
        mTotalBlobNum.fetch_add(size, std::memory_order_seq_cst);
    }

    inline void SubTotalBlobNum(int64_t size)
    {
        mTotalBlobNum.fetch_sub(size, std::memory_order_seq_cst);
    }

    inline void AddTotalBlobSize(int64_t size)
    {
        mTotalBlobSize.fetch_add(size, std::memory_order_seq_cst);
    }

    inline void SubTotalBlobSize(int64_t size)
    {
        mTotalBlobSize.fetch_sub(size, std::memory_order_seq_cst);
    }

    inline void ReportWriteBlobValue(uint32_t blobNum, uint32_t blobValueSize)
    {
        AddTotalBlobNum(blobNum);
        AddTotalBlobSize(blobValueSize);
    }

private:
    std::atomic<int64_t> mTotalBlobNum{ 0 };
    std::atomic<int64_t> mTotalBlobSize{ 0 };
};
using BlobStoreStatRef = std::shared_ptr<BlobStoreStat>;
}  // namespace bss
}  // namespace ock

#endif