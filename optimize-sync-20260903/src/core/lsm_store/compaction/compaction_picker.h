/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#ifndef BOOST_SS_COMPACTION_PICKER_H
#define BOOST_SS_COMPACTION_PICKER_H

#include <utility>

#include "compaction.h"
#include "include/config.h"
#include "lsm_store/file/file_meta_data.h"
#include "lsm_store/file/group_range.h"
#include "version/version.h"

namespace ock {
namespace bss {
class CompactionPicker {
    struct FileMetaDataHash {
        size_t operator()(const FileMetaDataRef &fileMetaData) const
        {
            return fileMetaData->HashCode();
        }
    };
    struct FileMetaDataEqual {
        bool operator()(const FileMetaDataRef &lFileMetaData, const FileMetaDataRef &rFileMetaData) const
        {
            return lFileMetaData->Equals(rFileMetaData);
        }
    };

public:
    static const uint32_t maxFilePosition = NO_50;

    explicit CompactionPicker(const ConfigRef &conf) : mConf(conf)
    {
        uint32_t numLevels = conf->GetFileStoreNumLevels();
        if (numLevels > maxFilePosition) {
            LOG_ERROR("File store level num exceeds limits, level num:" << numLevels << ", limit num:" << NO_50);
            numLevels = maxFilePosition;
        }
        for (uint32_t i = 0; i < numLevels; i++) {
            mNextFilePositions[i].store(0, std::memory_order_release);
        }
    }

    ~CompactionPicker() = default;

    std::vector<FileMetaDataRef> GetInputFiles(const VersionPtr &current, const GroupRangeRef &curGroupRange);

    CompactionRef GenerateCompactionBaseOnInputs(const VersionPtr &current, const GroupRangeRef &curGroupRange,
                                                 std::vector<FileMetaDataRef> &levelInputs);

private:
    std::pair<FileMetaDataRef, bool> GetInitialInputFile(const VersionPtr &current, uint32_t levelId,
                                                         const GroupRangeRef &curGroupRange);

    std::vector<FileMetaDataRef> ExpandInputFilesIfNeeded(const VersionPtr &current, uint32_t levelId,
                                                          const GroupRangeRef &curGroupRange,
                                                          std::vector<FileMetaDataRef> levelInputs);

    void FilterFileMetaList(std::vector<FileMetaDataRef> &fileMetaDataList);

    std::string GetFileNames(const std::vector<FileMetaDataRef> &fileMetaList);

    inline uint64_t GetTotalFileSize(const std::vector<FileMetaDataRef> &fileMetaList)
    {
        uint64_t size = 0UL;
        for (auto &fileMetaData : fileMetaList) {
            size += fileMetaData->GetFileSize();
        }
        return size;
    }

    std::vector<FileMetaDataRef> GetOverlappingInputs(const VersionPtr &current, uint32_t levelId,
                                                      GroupRangeRef queryGroupRange, FullKeyRef begin, FullKeyRef end);

    std::pair<FullKeyRef, FullKeyRef> GetKeyRange(const std::vector<std::vector<FileMetaDataRef>> &inputs);

    void ExpandInputsToCleanCut(const std::vector<FileMetaDataRef> &levelFiles,
                                std::vector<FileMetaDataRef> &toCompactionFiles);

    FullKeyRef FindLargestKey(const std::vector<FileMetaDataRef> &files);

    FileMetaDataRef FindSmallestBoundaryFile(const std::vector<FileMetaDataRef> &levelFiles,
                                             const FullKeyRef &largestKey);

private:
    ConfigRef mConf = nullptr;
    std::atomic<uint64_t> mNextFilePositions[maxFilePosition] = { { 0 } };
    void findAllSmallestAndLargest(const VersionPtr &current, uint32_t levelId,
                                   std::vector<GroupRangeRef> &inputLevelGroupRanges,
                                   std::vector<FileMetaDataRef> &levelInputs, FullKeyRef &largest,
                                   std::vector<FileMetaDataRef> &outputLevelInputs, FullKeyRef &allSmallest,
                                   FullKeyRef &allLargest);
};
using CompactionPickerRef = std::shared_ptr<CompactionPicker>;

}  // namespace bss
}  // namespace ock
#endif  // BOOST_SS_COMPACTION_PICKER_H