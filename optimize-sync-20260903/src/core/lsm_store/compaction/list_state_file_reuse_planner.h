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

#ifndef BOOST_SS_LIST_STATE_FILE_REUSE_PLANNER_H
#define BOOST_SS_LIST_STATE_FILE_REUSE_PLANNER_H

#include <functional>

#include "compaction.h"

namespace ock {
namespace bss {
/**
 * Builds a conservative ADOPT/DROP/MERGE plan for single-record large
 * ListState PUT files. The planner only uses persisted file metadata.
 */
class ListStateFileReusePlanner {
public:
    using AdoptableFilter = std::function<bool(uint16_t stateId, uint64_t seqId)>;

    static void Build(const CompactionRef &compaction, const AdoptableFilter &adoptableFilter);
};
}  // namespace bss
}  // namespace ock

#endif  // BOOST_SS_LIST_STATE_FILE_REUSE_PLANNER_H
