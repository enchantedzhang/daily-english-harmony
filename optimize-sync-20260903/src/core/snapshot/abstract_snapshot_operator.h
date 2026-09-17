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

#ifndef BOOST_SS_ABSTRACTSNAPSHOTOPERATOR_H
#define BOOST_SS_ABSTRACTSNAPSHOTOPERATOR_H

#include <atomic>
#include <cstdint>

#include "common/io/file_output_view.h"
#include "common/path.h"
#include "fresh_table/memory/memory_segment.h"
#include "include/bss_err.h"
#include "snapshot_meta.h"

namespace ock {
namespace bss {
enum class SnapshotOperatorType : uint8_t {
    DUMMY_SNAPSHOT_OPERATOR = 0,
    FRESH_TABLE = 1,
    SLICE_TABLE = 2,
    FILE_STORE = 3,
    BLOB_STORE = 4
};

// <PathString, <FileAddress, FileSize, count>>
using FileMetaInfoMap = std::unordered_map<std::string, std::tuple<uint64_t, uint32_t, uint32_t>>;

class AbstractSnapshotOperator : public Referable {
public:
    enum class State { CREATED, WAITING_BARRIER, SYNC, ASYNC, FAILED, CANCELED };

    AbstractSnapshotOperator() = default;

    explicit AbstractSnapshotOperator(uint32_t operatorId) : mOperatorId(operatorId)
    {
    }

    ~AbstractSnapshotOperator() override = default;

    void Start();

    virtual BResult SyncSnapshot(bool isSavepoint)
    {
        return BSS_ERR;
    }

    virtual BResult AsyncSnapshot(uint64_t snapshotId, const PathRef &snapshotPath, bool isIncremental,
                                  bool enableLocalRecovery, const PathRef &backupPath,
                                  std::unordered_map<std::string, uint32_t> &sliceRefCounts)
    {
        return BSS_ERR;
    }

    virtual SnapshotMetaRef OutputMeta(uint64_t snapshotId, const FileOutputViewRef &localOutputView)
    {
        return nullptr;
    }

    virtual SnapshotOperatorType GetType()
    {
        return SnapshotOperatorType::DUMMY_SNAPSHOT_OPERATOR;
    }

    virtual BResult WriteInfo(const FileOutputViewRef &localOutputView)
    {
        return BSS_OK;
    }

    virtual void InternalRelease()
    {
        std::lock_guard<std::mutex> lk(mResourceMutex);
        if (mIsReleased.load()) {
            return;
        }
        mIsReleased.store(true);
    }

    inline uint32_t GetOperatorId() const
    {
        return mOperatorId;
    }

    virtual inline void Cancel()
    {
        bool shouldRelease = false;
        std::lock_guard<std::mutex> lock(mMutex);
        if (mState != State::FAILED && mState != State::CANCELED) {
            this->mState = State::CANCELED;
            shouldRelease = true;
        }
        if (shouldRelease) {
            InternalRelease();
        }
    }

    inline std::atomic<bool> &GetIsReleased()
    {
        return mIsReleased;
    }

    virtual inline void Success()
    {
        LOG_DEBUG("Snapshot success!");
    }

protected:
    uint32_t mOperatorId = 0;
    std::mutex mMutex;
    State mState = State::CREATED;
    std::atomic<bool> mIsReleased{ false };
    std::mutex mResourceMutex;
};
using AbstractSnapshotOperatorRef = std::shared_ptr<AbstractSnapshotOperator>;

class SnapshotOperatorInfo {
public:
    explicit SnapshotOperatorInfo(SnapshotOperatorType type) : mOperatorType(type)
    {
    }

    virtual ~SnapshotOperatorInfo() = default;

    SnapshotOperatorType GetSnapshotOperatorType() const
    {
        return mOperatorType;
    }

    virtual BResult Serialize(const FileOutputViewRef &outputView)
    {
        return BSS_OK;
    }

private:
    SnapshotOperatorType mOperatorType = SnapshotOperatorType::DUMMY_SNAPSHOT_OPERATOR;
};
using SnapshotOperatorInfoRef = std::shared_ptr<SnapshotOperatorInfo>;

}  // namespace bss
}  // namespace ock
#endif  // BOOST_SS_ABSTRACTSNAPSHOTOPERATOR_H
