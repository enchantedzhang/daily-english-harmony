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

#ifndef BOOST_STATE_DBGROUP_H
#define BOOST_STATE_DBGROUP_H

#include <unordered_map>

#include "common/util/bss_lock.h"
#include "include/boost_state_db.h"
#include "include/bss_err.h"
#include "include/config.h"
#include "mem_manager.h"

namespace ock {
namespace bss {
class BoostStateDbGroup {
public:
    virtual ~BoostStateDbGroup()
    {
        LOG_INFO("Delete Boost state db group success.");
    }

    /**
     * Add boost state db to dbgroup.
     * @param boostStateDb:boost state db instance ptr
     * @return return BSS_OK if success, else return BSS_ERR.
     */
    virtual BResult Add(BoostStateDB *boostStateDb) = 0;

    /**
     * Remove boost state db from dbgroup.
     * @param boostStateDb:boost state db instance ptr
     * @return return BSS_OK if success, else return BSS_ERR.
     */
    virtual BResult Remove(BoostStateDB *boostStateDb) = 0;

    /**
     * check boost state db group is empty
     * @return return true empty, else return false not empty.
     */
    virtual bool IsEmpty() = 0;

    /**
     * try evict db group db cache
     * @param isSync: is sync evict
     * @param isForce: is force evict
     * @param minSize evict file size
     * @return return BSS_OK if success, else return BSS_ERR.
     */
    virtual BResult TryEvict(bool isSync, bool isForce, uint32_t minSize) = 0;

    /**
     * get db group memory manager
     * @return return memory manager ptr, else return nullptr.
     */
    virtual MemManagerRef &GetMemManager() = 0;

    /**
     * Update slice table evict water mark
     */
    virtual void UpdateSliceEvictWaterMark() = 0;
};

using BoostStateDbGroupPtr = BoostStateDbGroup *;

class BoostStateDbGroupMgr {
public:
    /**
     * Create boost state db group object
     * @param dbGroupId:boost state db group id(use slot id)
     * @param config:配置参数
     * @return return BSS_OK if success, else return BSS_ERR.
     */
    static BResult Create(uint32_t dbGroupId, ConfigRef config);

    /**
     * Create boost state db group object.
     * @param dbGroupId:boost state db group id(use slot id)
     * @param boostStateDb:boost state db instance
     * @return return BSS_OK if success, else return BSS_ERR.
     */
    static BResult Add(uint32_t dbGroupId, BoostStateDB *boostStateDb);

    /**
     * Destroy boost state db group object
     * @param dbGroupId:boost state db group id(use slot id)
     * @param boostStateDb:boost state db instance
     * @return return BSS_OK if success, else return BSS_ERR.
     */
    static BResult Remove(uint32_t dbGroupId, BoostStateDB *boostStateDb);

    /**
     * Delete db group
     * @param dbGroupId:boost state db group id(use slot id)
     */
    static void DeleteDbGroupPtr(uint32_t dbGroupId);

    /**
     * Destroy boost state db group object
     * @param dbGroupId:boost state db group id(use slot id)
     * @param boostStateDb:boost state db instance
     * @return return BSS_OK if success, else return BSS_ERR.
     */
    static BoostStateDbGroupPtr GetBoostStateDbGroup(uint32_t dbGroupId);

    /**
     * Destroy boost state db group object
     * @param dbGroupId:boost state db group id(use slot id)
     * @param memManager:memory manager
     * @return return BSS_OK if success, else return BSS_ERR.
     */
    static BResult GetMemManager(uint32_t dbGroupId, MemManagerRef &memManager);

    /**
     * Get or create lock of task slot
     * @param dbGroupId:boost state db group id(use slot id)
     * @return return ReadWriteLock.
     */
    static ReadWriteLock &GetOrCreateLock(uint32_t dbGroupId);

private:
    static std::unordered_map<uint32_t, BoostStateDbGroupPtr> mBoostStateDbGroup;
    static std::unordered_map<uint32_t, std::unique_ptr<ReadWriteLock>> mSlotLockMap;
    static ReadWriteLock mRwLock;
};
}  // namespace bss
}  // namespace ock

#endif  // BOOST_STATE_DBGROUP_H
