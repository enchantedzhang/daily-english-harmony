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

#ifndef BOOST_SS_RAWDATASLICE_H
#define BOOST_SS_RAWDATASLICE_H

#include <cstdint>
#include <vector>

#include "binary/fresh_binary.h"
#include "slice_table/binary_map/binary_key.h"
#include "transform/vector_group.h"

namespace ock {
namespace bss {

class RawDataSlice {
public:
    RawDataSlice(MemorySegmentRef freshSegment, uint64_t version)
        : mSliceData(), mFreshSegment(std::move(freshSegment)), mVersion(version)
    {
        vectorGroup = std::make_shared<VectorGroup>();
    }

    inline std::vector<std::pair<BinaryKey, FreshValueNodePtr>> &GetSliceData()
    {
        return mSliceData;
    }

    inline auto &GetFreshSegment() const
    {
        return mFreshSegment;
    }

    inline VectorGroupRef &GetVectorGroup()
    {
        return vectorGroup;
    }

    inline void PutMixHashCode(uint32_t mixHashCode)
    {
        vectorGroup->PutMixHashCode(mixHashCode);
    }

    inline void PutIndexVec(uint32_t index)
    {
        vectorGroup->PutIndexVec(index);
    }

    inline void PutIndexVec()
    {
        vectorGroup->PutIndexVec(rowSize - 1);
    }

    inline uint64_t GetVersion() const
    {
        return mVersion;
    }

    inline void AddBinaryData(const std::pair<BinaryKey, FreshValueNodePtr> &pair)
    {
        mSliceData.push_back(pair);
        rowSize++;
    }

private:
    std::vector<std::pair<BinaryKey, FreshValueNodePtr>> mSliceData;
    uint64_t rowSize = 0;
    VectorGroupRef vectorGroup;
    MemorySegmentRef mFreshSegment;
    uint64_t mVersion = 0;
};
using RawDataSliceRef = std::shared_ptr<RawDataSlice>;

}  // namespace bss
}  // namespace ock
#endif  // BOOST_SS_RAWDATASLICE_H