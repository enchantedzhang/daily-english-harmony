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

#ifndef BOOST_SS_TOMBSTONE_STRUCTURE_H
#define BOOST_SS_TOMBSTONE_STRUCTURE_H

#include <cstdint>

#include "bss_log.h"
#include "tombstone.h"

namespace ock {
namespace bss {
struct TombstoneFooterStructure {
    uint64_t minBlobId;
    uint64_t maxBlobId;
    uint32_t magicNum;
    uint32_t blobNum;
    uint32_t dataBlockNum;
    uint32_t indexBlockOffset;
    uint32_t indexBlockSize;
};

struct CompareTombstone {
    bool operator()(const TombstoneRef &t1, const TombstoneRef &t2) const
    {
        if (t1 == nullptr || t2 == nullptr) {
            LOG_ERROR("Invalid param!");
            return false;
        }
        return t1->GetBlobId() < t2->GetBlobId();
    }
};
}  // namespace bss
}  // namespace ock

#endif  // BOOST_SS_TOMBSTONE_STRUCTURE_H
