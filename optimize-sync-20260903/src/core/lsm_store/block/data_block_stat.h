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

#ifndef BOOST_SS_DATA_BLOCK_STAT_H
#define BOOST_SS_DATA_BLOCK_STAT_H

#include "lsm_store/block/block_meta.h"

namespace ock {
namespace bss {
class DataBlockStat {
public:
    inline FullKeyRef GetStartKey() const
    {
        return mStartKey;
    }

    inline FullKeyRef GetEndKey() const
    {
        return mEndKey;
    }

    inline void UpdateDataBlock(const BlockMetaRef &blockMeta)
    {
        mNumDataBlocks++;
        mTotalDataBlockSize += blockMeta->GetSize();
        mTotalDataBlockRawSize += blockMeta->GetRawSize();
        mTotalNumKeys += blockMeta->GetNumKeys();
        if (mStartKey == nullptr) {
            mStartKey = blockMeta->GetStartKey();
        }
        mEndKey = blockMeta->GetEndKey();
    }

    inline uint32_t GetNumDataBlock() const
    {
        return mNumDataBlocks;
    }

    inline uint32_t GetTotalDataBlockSize() const
    {
        return mTotalDataBlockSize;
    }

    inline uint32_t GetTotalDataBlockRawSize() const
    {
        return mTotalDataBlockRawSize;
    }

    inline uint32_t GetTotalNumKeys() const
    {
        return mTotalNumKeys;
    }

private:
    uint32_t mNumDataBlocks = 0;
    uint32_t mTotalDataBlockSize = 0;
    uint32_t mTotalDataBlockRawSize = 0;
    uint32_t mTotalNumKeys = 0;
    FullKeyRef mStartKey = nullptr;
    FullKeyRef mEndKey = nullptr;
};
using DataBlockStatRef = std::shared_ptr<DataBlockStat>;

}  // namespace bss
}  // namespace ock
#endif  // BOOST_SS_DATA_BLOCK_STAT_H