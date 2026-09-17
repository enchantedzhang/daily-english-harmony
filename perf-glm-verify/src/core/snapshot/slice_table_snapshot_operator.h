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

#ifndef BOOST_SS_SLICE_TABLE_SNAPSHOT_OPERATOR_H
#define BOOST_SS_SLICE_TABLE_SNAPSHOT_OPERATOR_H

#include <dirent.h>

#include <cerrno>

#include "abstract_snapshot_operator.h"
#include "slice_table/slice_table.h"
#include "slice_table_snapshot.h"

namespace ock {
namespace bss {
class SliceTableSnapshotOperator : public AbstractSnapshotOperator {
public:
    SliceTableSnapshotOperator(uint64_t operatorId, const SliceTableManagerRef &sliceTable, const ConfigRef &config,
                               const MemManagerRef &memManager, uint64_t snapshotId)
        : AbstractSnapshotOperator(operatorId),
          mSliceTable(sliceTable),
          mConfig(config),
          mMemManager(memManager),
          mSnapshotId(snapshotId)
    {
    }

    ~SliceTableSnapshotOperator() override = default;

    BResult SyncSnapshot(bool isSavepoint);

    BResult AsyncSnapshot(uint64_t snapshotId, const PathRef &snapshotPath, bool isIncremental,
                          bool enableLocalRecovery, const PathRef &backupPath,
                          std::unordered_map<std::string, uint32_t> &sliceRefCounts) override;

    BResult AsyncSnapshotWithoutLocalRecovery(uint64_t snapshotId, const PathRef &snapshotPath, bool isIncremental);

    SnapshotMetaRef OutputMeta(uint64_t snapshotId, const FileOutputViewRef &localOutputView) override;

    SnapshotOperatorType GetType() override
    {
        return SnapshotOperatorType::SLICE_TABLE;
    }

    BResult WriteInfo(const FileOutputViewRef &localOutputView) override;

    void InternalRelease() override;

    inline SliceKVIteratorPtr SnapshotIterator()
    {
        return mSliceTableSnapshot->Iterator();
    }

    inline uint64_t GetSnapshotId() const
    {
        return mSnapshotId;
    }

    static inline BResult createHardlinks(const PathRef &backupPath, const PathRef &snapshotPath,
                                          const std::string &sliceFile)
    {
        PathRef sourcePath = std::make_shared<Path>(backupPath, sliceFile);
        PathRef destPath = std::make_shared<Path>(snapshotPath, sliceFile);
        int ret = link(sourcePath->Name().c_str(), destPath->Name().c_str());
        if (UNLIKELY(ret != 0)) {
            if (errno == EEXIST) {
                LOG_WARN("Create hard link failed, file already exists, from " << sourcePath->ExtractFileName()
                                                                               << " to " << destPath->ExtractFileName()
                                                                               << ", errno: " << strerror(errno));
                return BSS_OK;
            }
            LOG_ERROR("Create hard link failed, from " << sourcePath->ExtractFileName() << " to "
                                                       << destPath->ExtractFileName()
                                                       << ", errno: " << strerror(errno));
            return BSS_ERR;
        }
        return BSS_OK;
    }

    static inline BResult unlinkUseless(const PathRef &backupPath, const std::string &sliceFile)
    {
        RETURN_INVALID_PARAM_AS_NULLPTR(backupPath);
        auto uselessFile = std::make_shared<Path>(backupPath, sliceFile);
        int ret = unlink(uselessFile->Name().c_str());
        if (UNLIKELY(ret != 0)) {
            LOG_WARN("Failed to unlinkUseless for " << sliceFile
                                                    << " in backupPath: " << backupPath->ExtractFileName());
            return BSS_ERR;
        }
        return BSS_OK;
    }

private:
    SliceTableSnapshotRef mSliceTableSnapshot = nullptr;
    SliceTableManagerRef mSliceTable = nullptr;
    ConfigRef mConfig = nullptr;
    MemManagerRef mMemManager = nullptr;
    uint64_t mSnapshotId = 0;
    FileOutputViewRef mFileOutputView = nullptr;
};
using SliceTableSnapshotOperatorRef = std::shared_ptr<SliceTableSnapshotOperator>;

class SliceTableSnapshotOperatorInfo : public SnapshotOperatorInfo {
public:
    explicit SliceTableSnapshotOperatorInfo(SnapshotOperatorType type) : SnapshotOperatorInfo(type)
    {
    }

    BResult Serialize(const FileOutputViewRef &outputView) override
    {
        return BSS_OK;
    }
};
using SliceTableSnapshotOperatorInfoRef = std::shared_ptr<SliceTableSnapshotOperatorInfo>;

}  // namespace bss
}  // namespace ock
#endif  // BOOST_SS_SLICE_TABLE_SNAPSHOT_OPERATOR_H