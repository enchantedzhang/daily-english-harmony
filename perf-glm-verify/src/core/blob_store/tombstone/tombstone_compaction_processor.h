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

#ifndef BOOST_SS_TOMBSTONE_COMPACTION_PROCESSOR_H
#define BOOST_SS_TOMBSTONE_COMPACTION_PROCESSOR_H

#include "blob_file_manager.h"
#include "tombstone_file_group.h"
#include "tombstone_file_manager.h"

namespace ock {
namespace bss {

class TombstoneFileManager;
class TombstoneCompactionProcessor {
public:
    TombstoneCompactionProcessor(const std::vector<TombstoneFileRef> &input,
                                 const std::vector<TombstoneFileRef> &nextLevel, const ConfigRef &config,
                                 const std::shared_ptr<TombstoneFileManager> &fileManager, bool isTopLevel,
                                 const MemManagerRef &memManager, const BlobFileGroupManagerRef &blobFileGroupManager)
        : mInputFiles(input),
          mNextLevelFiles(nextLevel),
          mConfig(config),
          mFileManager(fileManager),
          mIsTopLevel(isTopLevel),
          mMemManager(memManager),
          mBlobFileGroupManager(blobFileGroupManager)
    {
    }

    BResult Process(std::vector<TombstoneFileRef> &ret);

private:
    std::vector<TombstoneFileRef> mInputFiles;
    std::vector<TombstoneFileRef> mNextLevelFiles;
    ConfigRef mConfig = nullptr;
    std::shared_ptr<TombstoneFileManager> mFileManager = nullptr;
    bool mIsTopLevel;
    MemManagerRef mMemManager = nullptr;
    BlobFileGroupManagerRef mBlobFileGroupManager = nullptr;
};
using TombstoneCompactionProcessorRef = std::shared_ptr<TombstoneCompactionProcessor>;
}  // namespace bss
}  // namespace ock

#endif  // BOOST_SS_TOMBSTONE_COMPACTION_PROCESSOR_H
