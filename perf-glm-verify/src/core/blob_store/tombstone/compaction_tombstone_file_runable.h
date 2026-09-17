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

#ifndef COMPACTION_TOMBSTONE_FILE_RUNABLE_H
#define COMPACTION_TOMBSTONE_FILE_RUNABLE_H
#include "executor_service.h"

namespace ock {
namespace bss {

class TombstoneFileCompactionRunable : public Runnable {
public:
    explicit TombstoneFileCompactionRunable(const BlobCleanerRef &blobCleaner) : mBlobCleaner(blobCleaner)
    {
    }

    ~TombstoneFileCompactionRunable() override
    {
        LOG_DEBUG("Delete TombstoneFileCompactionRunable success");
    }

    void Run() override
    {
        mBlobCleaner->StartTombstoneCompaction();
        while (!mBlobCleaner->IsClosed()) {
            mBlobCleaner->TriggerCompaction();
            if (mBlobCleaner->IsClosed()) {
                break;
            }
            mBlobCleaner->ToSleeping();
            sleep(mCompactionIntervalInSecond);
        }
        mBlobCleaner->SetCompacting(false);
        LOG_INFO("Blob cleaner compaction processor stop.");
    }

private:
    BlobCleanerRef mBlobCleaner = nullptr;
    uint32_t mCompactionIntervalInSecond = 10;
};

}  // namespace bss
}  // namespace ock
#endif  // COMPACTION_TOMBSTONE_FILE_RUNABLE_H