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

#ifndef BOOST_SS_INDEX_BLOCK_STAT_H
#define BOOST_SS_INDEX_BLOCK_STAT_H

#include <memory>
#include <vector>

namespace ock {
namespace bss {
class IndexBlockStat {
public:
    inline uint32_t GetTotalIndexBlockSize() const
    {
        return mTotalIndexBlockSize;
    }

    inline uint32_t GetTotalIndexBlockRawSize() const
    {
        return mTotalIndexBlockRawSize;
    }

    inline void AddIndexBlockSize(uint32_t size)
    {
        mTotalIndexBlockSize += size;
    }

    inline void AddIndexBlockRawSize(uint32_t size)
    {
        mTotalIndexBlockRawSize += size;
    }

private:
    uint32_t mTotalIndexBlockCount = 0;
    uint32_t mTotalIndexBlockSize = 0;
    uint32_t mTotalIndexBlockRawSize = 0;
};
using IndexBlockStatRef = std::shared_ptr<IndexBlockStat>;

}  // namespace bss
}  // namespace ock
#endif  // BOOST_SS_INDEX_BLOCK_STAT_H