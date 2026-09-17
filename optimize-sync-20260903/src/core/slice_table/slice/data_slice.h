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

#ifndef BOOST_SS_DATASLICE_H
#define BOOST_SS_DATASLICE_H

#include <cstdint>
#include <memory>

#include "slice.h"

namespace ock {
namespace bss {
class DataSlice {
public:
    DataSlice() = default;

    DataSlice(const DataSlice &other) : mChainIndex(other.GetChainIndex()), mSlice(other.GetSlice())
    {
    }

    DataSlice &operator=(const DataSlice &other)
    {
        if (this != &other) {
            mChainIndex = other.GetChainIndex();
            mSlice = other.GetSlice();  // 浅拷贝, 仅复制智能指针.
        }
        return *this;
    }

    inline BResult Get(const Key &key, Value &value)
    {
        if (UNLIKELY(mSlice == nullptr)) {
            return BSS_NOT_EXISTS;
        }
        bool result = mSlice->Get(key, value);
        if (UNLIKELY(!result)) {
            return BSS_NOT_EXISTS;
        }
        return BSS_OK;
    }

    inline uint32_t GetSize()
    {
        uint32_t size = 0;
        if (mSlice == nullptr) {
            return size;
        }
        mSlice->BytesSize(size);
        return size;
    }

    inline void SetChainIndex(uint32_t index)
    {
        mChainIndex = static_cast<int32_t>(index);
    }

    inline int32_t GetChainIndex() const
    {
        return mChainIndex;
    }

    inline SliceRef GetSlice() const
    {
        return mSlice;
    }

    inline void Init(SliceRef &slice)
    {
        mSlice = slice;
    }

    inline uint32_t GetCompactionCount()
    {
        if (mSlice == nullptr) {
            return 0;
        }

        return mSlice->GetCompactionCount();
    }

protected:
    int32_t mChainIndex = 0;
    SliceRef mSlice;
};

using DataSliceRef = std::shared_ptr<DataSlice>;

}  // namespace bss
}  // namespace ock

#endif  // BOOST_SS_DATASLICE_H