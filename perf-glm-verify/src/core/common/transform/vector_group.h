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

#ifndef VECTOR_BATCH_H
#define VECTOR_BATCH_H

#include <cstdint>
#include <vector>

#include "include/bss_types.h"

namespace ock {
namespace bss {

class VectorGroup {
public:
    VectorGroup()
    {
        mMixHashCodeVec.reserve(NO_1024);
        mIndexVec.reserve(NO_1024);
    }

    ~VectorGroup() = default;

    inline void PutMixHashCode(uint32_t mixHashCode)
    {
        mMixHashCodeVec.emplace_back(mixHashCode);
    }

    inline void PutIndexVec(uint32_t index)
    {
        mIndexVec.emplace_back(index);
    }

    inline std::vector<uint32_t> &GetMixHashCodeVec()
    {
        return mMixHashCodeVec;
    }

    inline std::vector<uint32_t> &GetIndexVec()
    {
        return mIndexVec;
    }

private:
    std::vector<uint32_t> mMixHashCodeVec;
    std::vector<uint32_t> mIndexVec;
};
using VectorGroupRef = std::shared_ptr<VectorGroup>;
}  // namespace bss
}  // namespace ock

#endif  // VECTOR_BATCH_H
