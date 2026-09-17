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

#ifndef BOOST_SS_BLOCK_META_H
#define BOOST_SS_BLOCK_META_H

#include "binary/key/full_key.h"
#include "block_handle.h"

namespace ock {
namespace bss {
class BlockMeta {
public:
    BlockMeta() = default;
    ~BlockMeta() = default;

    inline void Reset(const BlockHandleRef &blockHandle, uint32_t rawSize, uint32_t numKeys, const FullKeyRef &startKey,
                      const FullKeyRef &endKey)
    {
        mBlockHandle = blockHandle;
        mRawSize = rawSize;
        mNumKeys = numKeys;
        mStartKey = startKey;
        mEndKey = endKey;
    }

    inline BlockHandleRef GetBlockHandle() const
    {
        return mBlockHandle;
    }

    inline uint32_t GetRawSize() const
    {
        return mRawSize;
    }

    inline uint32_t GetNumKeys() const
    {
        return mNumKeys;
    }

    inline FullKeyRef GetStartKey() const
    {
        return mStartKey;
    }

    inline FullKeyRef GetEndKey() const
    {
        return mEndKey;
    }

    inline uint32_t GetSize()
    {
        if (mBlockHandle == nullptr) {
            LOG_ERROR("blockHandle is null!");
            return 0;
        }
        return mBlockHandle->GetSize();
    }

private:
    BlockHandleRef mBlockHandle = nullptr;
    uint32_t mRawSize = 0;
    uint32_t mNumKeys = 0;
    FullKeyRef mStartKey = nullptr;
    FullKeyRef mEndKey = nullptr;
};
using BlockMetaRef = std::shared_ptr<BlockMeta>;

}  // namespace bss
}  // namespace ock
#endif  // BOOST_SS_BLOCK_META_H