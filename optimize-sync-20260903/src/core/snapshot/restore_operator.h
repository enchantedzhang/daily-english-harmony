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

#ifndef BOOST_SS_RESTORE_OPERATOR_H
#define BOOST_SS_RESTORE_OPERATOR_H

#include <vector>

#include "include/boost_state_table.h"
#include "include/config.h"
#include "lsm_store/file/file_cache_factory.h"
#include "snapshot_manager.h"
#include "snapshot_restore_utils.h"
#include "transform/fresh_transformer.h"

namespace ock {
namespace bss {
class RestoreOperator {
public:
    RestoreOperator(ConfigRef &config, FileManagerRef &localFileManager, FileManagerRef &remoteFileManager,
                    SliceTableManagerRef &sliceTable, FreshTableRef &freshTable, StateIdProviderRef &stateIdProvider,
                    std::unordered_map<std::string, TableRef> &tables, FileCacheFactoryRef &fileCacheFactory,
                    SnapshotManagerRef &snapshotManager, std::unordered_map<std::string, PQTableRef> pqTables)
        : mConfig(config),
          mLocalFileManager(localFileManager),
          mRemoteFileManager(remoteFileManager),
          mSliceTable(sliceTable),
          mFreshTable(freshTable),
          mTables(tables),
          mPQTables(pqTables),
          mStateIdProvider(stateIdProvider),
          mFileCacheFactory(fileCacheFactory),
          mSnapshotManager(snapshotManager)
    {
    }

    BResult Restore(std::vector<PathRef> &restoredMetaPaths,
                    std::unordered_map<std::string, std::string> &lazyPathMapping, uint64_t &seqId,
                    bool isLazyDownload);

    // 将restoredMetaInfo里面的本地恢复路径中的文件链接到当前db的本地恢复路径中.
    BResult CreateHardLinkForRestoredLocalFile(bool isExcludeSSTFiles,
                                               const std::vector<SnapshotFileMappingRef> &restoredLocalFileMappings,
                                               const PathRef &currentBasePath);

private:
    SnapshotFileMappingRef OrganizeRemoteFileInfo(std::vector<SnapshotFileMappingRef> &restoredLocalFileMappings,
                                                  std::unordered_map<std::string, std::string> &pathMap);

private:
    ConfigRef mConfig = nullptr;
    FileManagerRef mLocalFileManager = nullptr;
    FileManagerRef mRemoteFileManager = nullptr;
    SliceTableManagerRef mSliceTable = nullptr;
    FreshTableRef mFreshTable = nullptr;
    std::unordered_map<std::string, TableRef> mTables;
    std::unordered_map<std::string, PQTableRef> mPQTables;
    StateIdProviderRef mStateIdProvider = nullptr;
    FileCacheFactoryRef mFileCacheFactory = nullptr;
    SnapshotManagerRef mSnapshotManager = nullptr;
};

}  // namespace bss
}  // namespace ock
#endif  // BOOST_SS_RESTORE_OPERATOR_H