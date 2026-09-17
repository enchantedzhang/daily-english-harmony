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

#ifndef BOOST_SS_SNAPSHOT_STAT_H
#define BOOST_SS_SNAPSHOT_STAT_H

#include <memory>

#include "snapshot_meta.h"

namespace ock {
namespace bss {

class SnapshotStat {
public:
    inline void SetEndTime(uint64_t endTime)
    {
        mEndTime = endTime;
    }

    inline void AddSliceTableSnapshotMeta(const SnapshotMetaRef &snapshotMeta)
    {
        mSliceTableSnapshotMeta->AddSnapshotMeta(snapshotMeta);
    }

    inline void AddFileStoreSnapshotMeta(const SnapshotMetaRef &snapshotMeta)
    {
        mFileStoreSnapshotMeta->AddSnapshotMeta(snapshotMeta);
    }

    inline void SetTotalSnapshotMeta(const SnapshotMetaRef &snapshotMeta)
    {
        mTotalSnapshotMeta = snapshotMeta;
    }

private:
    uint64_t mStartTime = 0;
    uint64_t mEndTime = 0;
    SnapshotMetaRef mSliceTableSnapshotMeta = std::make_shared<SnapshotMeta>();
    SnapshotMetaRef mFileStoreSnapshotMeta = std::make_shared<SnapshotMeta>();
    SnapshotMetaRef mTotalSnapshotMeta = nullptr;
};
using SnapshotStatRef = std::shared_ptr<SnapshotStat>;

}  // namespace bss
}  // namespace ock
#endif  // BOOST_SS_SNAPSHOT_STAT_H