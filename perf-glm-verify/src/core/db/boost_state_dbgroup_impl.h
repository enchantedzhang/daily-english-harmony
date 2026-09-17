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

#ifndef BOOST_STATE_DBGROUP_IMPL_H
#define BOOST_STATE_DBGROUP_IMPL_H

#include <vector>

#include "boost_state_dbgroup.h"
#include "bucket_group.h"
#include "common/util/bss_lock.h"
#include "memory/mem_manager.h"

namespace ock {
namespace bss {
class BoostStateDbGroupImpl : public BoostStateDbGroup {
public:
    BoostStateDbGroupImpl(uint32_t dbGroupId, const ConfigRef &config, const MemManagerRef &memManager)
        : mDbGroupId(dbGroupId), mMemManager(memManager), mConfig(config), mEvictMinSize(config->mEvictMinSize)
    {
    }

    ~BoostStateDbGroupImpl() override
    {
        LOG_INFO("Delete boost state db group: " << mDbGroupId << " implement success.");
    }

    BResult Add(BoostStateDB *boostStateDb) override;

    BResult Remove(BoostStateDB *boostStateDb) override;

    BResult TryEvict(bool isSync, bool isForce, uint32_t minSize) override;

    inline bool IsEmpty() override
    {
        ReadLocker<ReadWriteLock> lock(&mRwLock);
        return mBoostStateDb.empty();
    }

    inline MemManagerRef &GetMemManager() override
    {
        return mMemManager;
    }

    inline void UpdateSliceEvictWaterMark() override
    {
        uint64_t totalSliceMem = mMemManager->GetMemoryTypeMaxSize(MemoryType::SLICE_TABLE);
        float totalMemHighMarkRatio = mConfig->GetTotalMemHighMarkRatio();
        mEvictHighWaterMark = static_cast<int64_t>(totalSliceMem * totalMemHighMarkRatio);
        mEvictLowWaterMark = static_cast<int64_t>(mEvictHighWaterMark * 0.85f);
        for (const auto &boostStateDb : mBoostStateDb) {
            boostStateDb->UpdateSliceEvictWaterMark(mEvictHighWaterMark);
        }
        auto dbNums = mBoostStateDb.size();
        if (UNLIKELY(dbNums < NO_1)) {
            return;
        }
        float normalEvictRatio = totalMemHighMarkRatio >= 0.5f ? totalMemHighMarkRatio : 0.8f;
        auto normalEvictWaterMark = static_cast<int64_t>(totalSliceMem * normalEvictRatio);
        auto avgDbSliceSize = normalEvictWaterMark / static_cast<int64_t>(dbNums);
        while (static_cast<uint64_t>(avgDbSliceSize) < mEvictMinSize) {
            mEvictMinSize = mEvictMinSize >> NO_1;
        }
        LOG_INFO("Update slice table evict water mark, highWaterMark: " << mEvictHighWaterMark
                                                                        << ", lowWaterMark: " << mEvictLowWaterMark
                                                                        << ", min evict size: " << mEvictMinSize);
    }

private:
    BResult GetEvictFlushInfo(int64_t &useTotal, int64_t &flushingTotal);
    void SelectEvictBoostDb(uint32_t fileSize, BoostStateDB *&boostDb, BucketGroupRef &minGroup);

private:
    uint32_t mDbGroupId;
    MemManagerRef mMemManager = nullptr;
    ConfigRef mConfig = nullptr;
    std::vector<BoostStateDB *> mBoostStateDb;
    ReadWriteLock mRwLock;
    uint64_t mEvictMinSize;
    int64_t mEvictHighWaterMark = 0;
    int64_t mEvictLowWaterMark = 0;
    std::atomic<bool> mIsEvicting{ false };
};
}  // namespace bss
}  // namespace ock

#endif  // BOOST_STATE_DBGROUP_IMPL_H
