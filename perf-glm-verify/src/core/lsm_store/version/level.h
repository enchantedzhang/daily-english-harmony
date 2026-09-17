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

#ifndef BOOST_SS_LEVEL_H
#define BOOST_SS_LEVEL_H

#include <cstdint>
#include <vector>

#include "common/bss_log.h"
#include "lsm_store/file/file_meta_data_group.h"
#include "lsm_store/file/group_range.h"

namespace ock {
namespace bss {
class LevelBuilder;
using LevelBuilderRef = std::shared_ptr<LevelBuilder>;

class Level {
public:
    static bool Compare(const FileMetaDataRef &f1, const FileMetaDataRef &f2)
    {
        int64_t result = f2->GetGroupRange()->GetEpoch() - f1->GetGroupRange()->GetEpoch();
        if (result != 0L) {
            return (result < 0);
        }
        return ((f2->GetSeqId() - f1->GetSeqId()) < 0);
    }

    Level(int32_t levelId, const std::vector<FileMetaDataGroupRef> &files,
          const std::vector<GroupRangeRef> &groupRanges)
        : mLevelId(levelId), mFileMetaDataGroups(files), mGroupRanges(groupRanges)
    {
    }

    inline int32_t GetLevelId() const
    {
        return mLevelId;
    }

    inline bool IsEmpty() const
    {
        return mFileMetaDataGroups.empty();
    }

    inline uint32_t TotalFileSize() const
    {
        uint32_t count = 0;
        for (auto &group : mFileMetaDataGroups) {
            count += group->GetTotalFileSize();
        }
        return count;
    }

    inline std::vector<FileMetaDataGroupRef> &GetFileMetaDataGroups()
    {
        return mFileMetaDataGroups;
    }

    bool Equals(Level other)
    {
        if (mLevelId != other.mLevelId || mFileMetaDataGroups.size() != other.mFileMetaDataGroups.size()) {
            return false;
        }

        for (uint32_t idx = 0; idx < mFileMetaDataGroups.size(); idx++) {
            if (!mFileMetaDataGroups[idx]->Equals(other.GetFileMetaDataGroups()[idx])) {
                return false;
            }
        }
        return true;
    }

    inline int32_t HashCode() const
    {
        return static_cast<int32_t>(std::hash<int32_t>()(mLevelId));
    }

    const std::vector<GroupRangeRef> &GetGroupRanges() const
    {
        return mGroupRanges;
    }

    GroupRangeRef GetBottomLeftMostGroupRange() const
    {
        if (mFileMetaDataGroups.empty()) {
            LOG_ERROR("Get Bottom Left-Most GroupRange failed, mFileMetaDataGroups is invalid.");
            return nullptr;
        }
        FileMetaDataGroupRef current = mFileMetaDataGroups.at(0);
        while (current->GetDown() != nullptr) {
            current = current->GetDown();
        }
        return current->GetGroupRange();
    }

    std::vector<FileMetaDataRef> GetFilesContainingPrefixKey(const Key &prefixKey, uint32_t keyGroup,
                                                             bool checkOrderRange);

    std::vector<FileMetaDataRef> GetFilesContainingStateId(uint32_t stateId);

    void GetFilesForKey(const Key &key, int32_t keyGroup, bool checkOrderRange, std::vector<FileMetaDataRef> &result);

    uint64_t GetTotalFileSize() const
    {
        uint64_t fileSize = 0;
        for (const auto &item : mFileMetaDataGroups) {
            CONTINUE_LOOP_AS_NULLPTR(item);
            fileSize += item->GetTotalFileSize();
        }
        return fileSize;
    }

private:
    inline FileMetaDataGroupRef NextFileMetaDataGroup(const FileMetaDataGroupRef &group)
    {
        return (group->GetRight() != nullptr) ? group->GetRight() : group->GetDown();
    }

    inline bool IsInclusiveKeyWithinFile(const FileMetaDataRef &fileMetaData, const Key &key)
    {
        return FullKeyUtil::CompareKeyWithInternalKey(key, fileMetaData->GetSmallest()) >= 0 &&
               FullKeyUtil::CompareKeyWithInternalKey(key, fileMetaData->GetLargest()) <= 0;
    }

    inline void GetOverlappingFiles(const std::vector<FileMetaDataRef> &files, const Key &key, bool checkOrderRange,
                                    std::vector<FileMetaDataRef> &result)
    {
        for (auto &file : files) {
            if (checkOrderRange && !file->GetOrderRange()->Contains(key.PriKey().KeyHashCode())) {
                continue;
            }
            if (IsInclusiveKeyWithinFile(file, key)) {
                result.push_back(file);
            }
        }
    }

    std::vector<FileMetaDataRef> GetOverlappingFilesMightContainPrefix(const std::vector<FileMetaDataRef> &files,
                                                                       const Key &prefixKey, bool checkOrderRange);

    bool IsPrefixKeyWithinFile(const FileMetaDataRef &fileMetaData, const Key &prefixKey);

    uint32_t FindFile(const std::vector<FileMetaDataRef> &files, const Key &key);

private:
    int32_t mLevelId;
    std::vector<FileMetaDataGroupRef> mFileMetaDataGroups;
    std::vector<GroupRangeRef> mGroupRanges;
};

class LevelBuilder : public std::enable_shared_from_this<LevelBuilder> {
public:
    explicit LevelBuilder(int32_t levelId) : mLevelId(levelId)
    {
    }

    static inline LevelBuilderRef NewBuilder(int32_t levelId)
    {
        return std::make_shared<LevelBuilder>(levelId);
    }

    inline void SetFileMetaDataComparator(const FileMetaData::FileMetaDataComparator &fileMetaDataComparator)
    {
        mFileMetaDataComparator = fileMetaDataComparator;
    }

    void AddFileMetaDataGroup(const FileMetaDataGroupRef &fileMetaDataGroup)
    {
        mFileMetaDataGroups.push_back(fileMetaDataGroup);
    }

    std::vector<GroupRangeRef> BuildGroupTree()
    {
        // 1. 对文件元数据组进行排序.
        auto fileMetaDataGroupComparator = [](FileMetaDataGroupRef &r1, FileMetaDataGroupRef &r2) -> bool {
            return r1->CompareTo(r2);
        };
        std::sort(mFileMetaDataGroups.begin(), mFileMetaDataGroups.end(), fileMetaDataGroupComparator);

        FileMetaDataGroupRef prev = nullptr;
        std::vector<FileMetaDataGroupRef> curHorizontalList;
        std::vector<GroupRangeRef> groupRanges;
        // 2. 遍历所有文件元数据组构建树形结构.
        for (FileMetaDataGroupRef &group : mFileMetaDataGroups) {
            if (prev != nullptr) {
                // 2.1 如果前一个文件元数据组的epoch与当前文件元数据组的epoch相同, 则放在右子树.
                if (prev->GetGroupRange()->GetEpoch() == group->GetGroupRange()->GetEpoch()) {
                    prev->SetRight(group);
                    // 2.2 如果前一个文件元数据组的epoch大于当前文件元数据组的epoch, 则放在下一层.
                } else if (prev->GetGroupRange()->GetEpoch() > group->GetGroupRange()->GetEpoch()) {
                    for (FileMetaDataGroupRef &prevList : curHorizontalList) {
                        prevList->SetDown(group);
                    }
                    curHorizontalList.clear();
                } else {
                    LOG_ERROR("Previous mEpoch should larger than current epoch.");
                    return {};
                }
            }
            prev = group;
            curHorizontalList.push_back(prev);
            groupRanges.push_back(prev->GetGroupRange());
        }
        return groupRanges;
    }

    Level Build()
    {
        if (mLevelId < 0) {
            return Level(-1, {}, {});
        }

        std::vector<GroupRangeRef> groupRanges;
        if (mFileMetaDataGroups.size() > 1) {
            MergeSameFileMetaDataGroups();
            groupRanges = BuildGroupTree();
        } else if (mFileMetaDataGroups.size() == 1) {
            groupRanges.push_back(mFileMetaDataGroups.at(0)->GetGroupRange());
        }

        return Level(mLevelId, mFileMetaDataGroups, groupRanges);
    }

private:
    void MergeSameFileMetaDataGroups();

private:
    std::vector<FileMetaDataGroupRef> mFileMetaDataGroups;
    int32_t mLevelId = -1;
    FileMetaData::FileMetaDataComparator mFileMetaDataComparator = nullptr;
};
}  // namespace bss
}  // namespace ock
#endif  // BOOST_SS_LEVEL_H