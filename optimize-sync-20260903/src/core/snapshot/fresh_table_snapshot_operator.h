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

#ifndef BOOST_SS_FRESH_TABLE_SNAPSHOT_OPERATOR_H
#define BOOST_SS_FRESH_TABLE_SNAPSHOT_OPERATOR_H

#include <cstdint>

#include "abstract_snapshot_operator.h"
#include "fresh_table/fresh_table.h"
#include "fresh_table/memory/memory_segment.h"
#include "include/bss_err.h"
#include "include/compress_algo.h"
#include "kv_table/pq_table.h"

namespace ock {
namespace bss {
class FreshTableSnapshotOperator : public AbstractSnapshotOperator {
public:
    FreshTableSnapshotOperator(uint64_t operatorId, const FreshTableRef &freshTable, const ConfigRef &config,
                               const MemManagerRef &memManager, const std::vector<PQTableRef> &pqTable)
        : AbstractSnapshotOperator(operatorId),
          mFreshTable(freshTable),
          mConfig(config),
          mMemManager(memManager),
          mPqTables(pqTable)
    {
    }

    ~FreshTableSnapshotOperator() override = default;

    BResult SyncSnapshot(bool isSavepoint) override;

    BResult AsyncSnapshot(uint64_t snapshotId, const PathRef &snapshotPath, bool isIncremental,
                          bool enableLocalRecovery, const PathRef &backupPath,
                          std::unordered_map<std::string, uint32_t> &sliceRefCounts) override;

    SnapshotMetaRef OutputMeta(uint64_t snapshotId, const FileOutputViewRef &localOutputView) override;

    SnapshotOperatorType GetType() override
    {
        return SnapshotOperatorType::FRESH_TABLE;
    }

    BResult WriteInfo(const FileOutputViewRef &localOutputView) override;

    void InternalRelease() override;

private:
    MemorySegmentRef mToUpload = nullptr;
    FreshTableRef mFreshTable = nullptr;
    ConfigRef mConfig = nullptr;
    uint32_t mByteLength = 0;
    uint32_t mCompressLength = 0;
    CompressAlgo mCompressAlgo = CompressAlgo::NONE;
    std::string mLocalAddress;
    SnapshotMetaRef mSnapshotMeta = std::make_shared<SnapshotMeta>();
    MemManagerRef mMemManager = nullptr;
    std::vector<PQTableRef> mPqTables;
};

class FreshTableSnapshotOperatorInfo : public SnapshotOperatorInfo {
public:
    explicit FreshTableSnapshotOperatorInfo(SnapshotOperatorType type) : SnapshotOperatorInfo(type)
    {
    }

    BResult Serialize(const FileOutputViewRef &outputView) override
    {
        return BSS_OK;
    }
};
using FreshTableSnapshotOperatorInfoRef = std::shared_ptr<FreshTableSnapshotOperatorInfo>;

}  // namespace bss
}  // namespace ock
#endif  // BOOST_SS_FRESH_TABLE_SNAPSHOT_OPERATOR_H
