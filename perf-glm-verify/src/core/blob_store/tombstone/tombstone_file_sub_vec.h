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

#ifndef BOOST_SS_TOMBSTONE_FILE_SUB_VEC_H
#define BOOST_SS_TOMBSTONE_FILE_SUB_VEC_H

#include <memory>
#include <vector>

#include "tombstone_file.h"

namespace ock {
namespace bss {
class TombstoneFileSubVec {
public:
    TombstoneFileSubVec(const std::vector<TombstoneFileRef> &fileVec, uint32_t startIndex)
        : mFileVec(fileVec), mStartIndex(startIndex)
    {
    }

    inline const std::vector<TombstoneFileRef> &GetFileVec()
    {
        return mFileVec;
    }

    inline uint32_t GetStartIndex() const
    {
        return mStartIndex;
    }

    inline uint32_t GetEndIndex() const
    {
        auto end = mStartIndex + mFileVec.size();
        if (end < 1) {
            return 0;
        }
        return end - 1;
    }

    inline bool Empty() const
    {
        return mFileVec.empty();
    }

private:
    std::vector<TombstoneFileRef> mFileVec;
    uint32_t mStartIndex;
};
using TombstoneFileSubVecRef = std::shared_ptr<TombstoneFileSubVec>;
}  // namespace bss
}  // namespace ock

#endif  // BOOST_SS_TOMBSTONE_FILE_SUB_VEC_H
