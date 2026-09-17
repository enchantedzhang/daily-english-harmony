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

#ifndef BOOST_SS_SNAPSHOT_SYNC_TASK_H
#define BOOST_SS_SNAPSHOT_SYNC_TASK_H

#include <condition_variable>

#include "executor/executor_service.h"
#include "snapshot_status.h"

namespace ock {
namespace bss {
class SnapshotSyncTask : public Runnable {
public:
    explicit SnapshotSyncTask(SnapshotStatusType type) : mStatusType(type)
    {
    }

    void Run() override
    {
        SnapshotStatus expect = SnapshotStatus::INIT;
        if (!mStatus.compare_exchange_strong(expect, SnapshotStatus::RUNNING)) {
            LOG_INFO("Run snapshot sync task return, current status:"
                     << static_cast<uint32_t>(mStatus.load()) << ",type:" << static_cast<uint32_t>(mStatusType));
            return;
        }
        LOG_DEBUG("Start snapshot sync task:" << static_cast<uint32_t>(mStatusType));
        while (mStatus.load() == SnapshotStatus::RUNNING) {
            usleep(NO_100000);
        }
        LOG_DEBUG("Finish snapshot sync task:" << static_cast<uint32_t>(mStatusType));
    }

    void FinishSnapshot()
    {
        mStatus.store(SnapshotStatus::FINISH);
    }

    SnapshotStatus GetStatus() const
    {
        return mStatus.load();
    }

private:
    SnapshotStatusType mStatusType;
    std::atomic<SnapshotStatus> mStatus{ SnapshotStatus::INIT };
};
using SnapshotSyncTaskRef = std::shared_ptr<SnapshotSyncTask>;

}  // namespace bss
}  // namespace ock
#endif  // BOOST_SS_SNAPSHOT_SYNC_TASK_H