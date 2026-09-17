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

#ifndef BOOST_SS_FILE_META_DATA_GROUP_H
#define BOOST_SS_FILE_META_DATA_GROUP_H

#include <algorithm>
#include <unordered_set>
#include <utility>
#include <vector>

#include "file_meta_data.h"
#include "key/full_key_util.h"

namespace ock {
namespace bss {
class FileMetaDataGroup;
using FileMetaDataGroupRef = std::shared_ptr<FileMetaDataGroup>;
class FileDataGroupBuilder;
using FileDataGroupBuilderRef = std::shared_ptr<FileDataGroupBuilder>;

class FileMetaDataGroup {
public:
    FileMetaDataGroup(std::vector<FileMetaDataRef> &files, std::shared_ptr<GroupRange> &groupRange,
                      uint64_t totalFileSize, bool overlapping)
        : mFiles(files), mGroupRange(groupRange), mTotalFileSize(totalFileSize), mOverlapping(overlapping)
    {
    }

    inline void SetDown(const FileMetaDataGroupRef &down)
    {
        mDown = down;
    }

    inline void SetRight(const FileMetaDataGroupRef &right)
    {
        mRight = right;
    }

    inline FileMetaDataGroupRef GetDown() const
    {
        return mDown;
    }

    inline FileMetaDataGroupRef GetRight() const
    {
        return mRight;
    }

    void Merge(const FileMetaDataGroupRef &fileMetaDataGroup, bool overlapping,
               FileMetaData::FileMetaDataComparator fileMetaDataComparator)
    {
        if (UNLIKELY(fileMetaDataGroup == nullptr)) {
            LOG_ERROR("File meta data group is nullptr.");
            return;
        }
        // 1. 检查文件元数据组的范围是否与当前组的范围相等.
        if (!mGroupRange->Equals(fileMetaDataGroup->GetGroupRange())) {
            return;
        }

        // 2. 将文件元数据组中的所有文件添加到当前组中.
        for (auto &file : fileMetaDataGroup->GetFiles()) {
            CONTINUE_LOOP_AS_NULLPTR(file);
            mFiles.push_back(file);
        }

        // 3. 如果待合并文件元数据组中存在文件，则根据是否存在重叠和文件元数据比较器对文件进行排序.
        if (!fileMetaDataGroup->GetFiles().empty()) {
            SortFilesIfNecessary(mFiles, overlapping, fileMetaDataComparator);
        }
    }

    inline bool Equals(const FileMetaDataGroupRef &other)
    {
        if (mOverlapping == other->IsOverlapping() || mFiles.size() != other->GetFiles().size()) {
            return false;
        }
        for (uint32_t idx = 0; idx < mFiles.size(); idx++) {
            if (!mFiles[idx]->Equals(other->GetFiles()[idx])) {
                return false;
            }
        }
        return mGroupRange->Equals(other->GetGroupRange());
    }

    inline bool CompareTo(FileMetaDataGroupRef &other)
    {
        int64_t cmp = other->GetGroupRange()->GetEpoch() - mGroupRange->GetEpoch();
        if (cmp != 0) {
            return cmp < 0;
        }
        return mGroupRange->GetStartGroup() - other->GetGroupRange()->GetStartGroup() < 0;
    }

    inline int32_t HashCode() const
    {
        return static_cast<int32_t>(std::hash<uint64_t>()(mTotalFileSize) + std::hash<bool>()(mOverlapping));
    }

    inline uint32_t GetNumberOfFiles() const
    {
        return static_cast<uint32_t>(mFiles.size());
    }

    inline GroupRangeRef GetGroupRange() const
    {
        return mGroupRange;
    }

    inline uint64_t GetTotalFileSize() const
    {
        return mTotalFileSize;
    }

    inline bool IsOverlapping() const
    {
        return mOverlapping;
    }

    inline std::vector<FileMetaDataRef> &GetFiles()
    {
        return mFiles;
    }

    std::vector<FileMetaDataRef> GetFilesOldestFirst()
    {
        std::vector<FileMetaDataRef> result;
        for (auto &file : mFiles) {
            result.push_back(file);
        }
        std::sort(result.begin(), result.end(), FileMetaData::Compare);
        return result;
    }

    static FileMetaData::FileMetaDataComparator CreateFileMetaDataComparator(bool reverseOrder)
    {
        auto compare = [reverseOrder](const FileMetaDataRef &o1, const FileMetaDataRef &o2) -> bool {
            int32_t smallestCompare = o1->GetSmallest()->CompareFullKey(*o2->GetSmallest(), o2->GetSmallest()->SeqId(),
                                                                        reverseOrder);
            if (smallestCompare != 0) {
                return smallestCompare < 0;
            }

            auto largestCompare = o1->GetLargest()->CompareFullKey(*o2->GetLargest(), o2->GetLargest()->SeqId(),
                                                                   reverseOrder);
            return largestCompare < 0;
        };
        return compare;
    }

    inline static void SortFilesIfNecessary(std::vector<FileMetaDataRef> &files, bool overlapping,
                                            FileMetaData::FileMetaDataComparator fileMetaDataComparator)
    {
        if (files.size() > 1) {
            if (overlapping) {
                std::sort(files.begin(), files.end(), FileMetaData::Compare);
            } else {
                std::sort(files.begin(), files.end(), fileMetaDataComparator);
            }
        }
    }

private:
    std::vector<FileMetaDataRef> mFiles;
    GroupRangeRef mGroupRange = nullptr;
    uint64_t mTotalFileSize = 0;
    bool mOverlapping = false;
    FileMetaDataGroupRef mDown = nullptr;
    FileMetaDataGroupRef mRight = nullptr;
};

class FileDataGroupBuilder {
public:
    explicit FileDataGroupBuilder()
    {
    }

    static FileDataGroupBuilderRef NewBuilder()
    {
        return std::make_shared<FileDataGroupBuilder>();
    }

    inline void SetFileMetaDataComparator(FileMetaData::FileMetaDataComparator fileMetaDataComparator)
    {
        mFileMetaDataComparator = std::move(fileMetaDataComparator);
    }

    inline void SetGroupRange(const GroupRangeRef &groupRange)
    {
        mGroupRange = groupRange;
    }

    inline bool IsEmpty() const
    {
        return mFiles.empty();
    }

    inline void SetOverlapping(bool overlapping)
    {
        mOverlapping = overlapping;
    }

    void AddFileMeta(const FileMetaDataRef &fileMetaData)
    {
        if (UNLIKELY(fileMetaData == nullptr)) {
            LOG_ERROR("File meta data is nullptr.");
            return;
        }

        if (!mOverlapping) {  // 如果当前已有文件无重叠，则检查待添加的文件是否与已有的文件有重叠.
            for (auto &file : mFiles) {
                FullKeyRef largestKey = file->GetLargest();
                FullKeyRef smallestKey = file->GetSmallest();
                if (fileMetaData->GetSmallest()->CompareFullKey(*largestKey, largestKey->SeqId()) <= 0 &&
                    fileMetaData->GetLargest()->CompareFullKey(*smallestKey, smallestKey->SeqId()) >= 0) {
                    mOverlapping = true;
                    break;
                }
            }
        }
        mFiles.push_back(fileMetaData);
    }

    inline void AddFileMetas(const std::unordered_set<FileMetaDataRef> &fileMetaDataList)
    {
        for (auto &fileMetaData : fileMetaDataList) {
            AddFileMeta(fileMetaData);
        }
    }

    FileMetaDataGroupRef Build()
    {
        if (mGroupRange == nullptr) {
            return nullptr;
        }
        if (mFileMetaDataComparator == nullptr) {
            mFileMetaDataComparator = FileMetaDataGroup::CreateFileMetaDataComparator(false);
        }
        FileMetaDataGroup::SortFilesIfNecessary(mFiles, mOverlapping, mFileMetaDataComparator);

        uint64_t totalFileSize = 0L;
        for (auto &file : mFiles) {
            totalFileSize += file->GetFileSize();
        }
        return std::make_shared<FileMetaDataGroup>(mFiles, mGroupRange, totalFileSize, mOverlapping);
    }

private:
    FileMetaData::FileMetaDataComparator mFileMetaDataComparator;
    GroupRangeRef mGroupRange = nullptr;
    std::vector<FileMetaDataRef> mFiles;
    bool mOverlapping = false;
};
}  // namespace bss
}  // namespace ock
#endif  // BOOST_SS_FILE_META_DATA_GROUP_H