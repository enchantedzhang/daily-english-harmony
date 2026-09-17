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

#ifndef BSS_DEV_INPUT_LEVEL_COMPACTION_H
#define BSS_DEV_INPUT_LEVEL_COMPACTION_H

#include "compaction/compaction.h"

namespace ock {
namespace bss {
class InputLevelCompaction : public Compaction {
public:
    InputLevelCompaction(VersionPtr inputVersion, uint32_t levelId, GroupRangeRef groupRange,
                         uint64_t maxFileOutputSize, std::vector<FileMetaDataRef> &levelInputs)
        : Compaction(inputVersion, levelId, groupRange, maxFileOutputSize, nullptr, nullptr, levelInputs,
                     std::vector<FileMetaDataRef>(), std::vector<FileMetaDataRef>())
    {
    }

    inline uint32_t GetInputLevelId() override
    {
        return mInputLevelId;
    }

    inline uint32_t GetOutputLevelId() override
    {
        return GetInputLevelId();
    }

    inline bool ShouldStopBefore(const KeyValueRef &key) override
    {
        return false;
    }

    inline bool IsTrivialMove() override
    {
        return false;
    }
};

}  // namespace bss
}  // namespace ock
#endif  // BSS_DEV_INPUT_LEVEL_COMPACTION_H