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

#ifndef BOOST_SEGMENT_H
#define BOOST_SEGMENT_H
#include "boost_hash_map.h"

namespace ock {
namespace bss {
class BoostSegment {
public:
    BoostSegment() = default;

    inline BResult Init(uint64_t segmentId, const MemorySegmentRef &memorySegment, uint64_t version)
    {
        mSegmentId = segmentId;
        mVersion = version;
        mBoostHashMap = MakeRef<BoostHashMap>();
        RETURN_ALLOC_FAIL_AS_NULLPTR(mBoostHashMap);
        return mBoostHashMap->Init(memorySegment, true);
    }

    inline BoostHashMapRef GetBinaryData()
    {
        return mBoostHashMap;
    }

    inline MemorySegmentRef GetMemorySegment()
    {
        RETURN_NULLPTR_AS_NULLPTR_WARN(mBoostHashMap);
        return mBoostHashMap->GetMemorySegment();
    }

    inline uint64_t GetSegmentId() const
    {
        return mSegmentId;
    }

    inline uint64_t GetVersion() const
    {
        return mVersion;
    }

    inline uint32_t Size() const
    {
        return mBoostHashMap->Size();
    }

private:
    uint64_t mSegmentId = 0UL;
    uint64_t mVersion = 0UL;
    BoostHashMapRef mBoostHashMap;
};
using BoostSegmentRef = std::shared_ptr<BoostSegment>;
}  // namespace bss
}  // namespace ock
#endif  // BOOST_SEGMENT_H
