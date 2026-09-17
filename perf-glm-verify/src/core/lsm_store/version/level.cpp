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

#include "level.h"

#include <unordered_map>

#include "binary/key/full_key.h"
#include "binary/key/key.h"
#include "include/bss_types.h"
#include "key/full_key_util.h"
#include "lsm_store/file/file_meta_data.h"
#include "lsm_store/file/file_meta_data_group.h"

namespace ock {
namespace bss {
void Level::GetFilesForKey(const Key &key, int32_t keyGroup, bool checkOrderRange, std::vector<FileMetaDataRef> &result)
{
    FileMetaDataGroupRef curGroup = mFileMetaDataGroups.empty() ? nullptr : mFileMetaDataGroups.at(0);
    while (curGroup != nullptr) {
        if (!curGroup->GetGroupRange()->ContainsGroup(keyGroup)) {
            curGroup = NextFileMetaDataGroup(curGroup);
            continue;
        }

        std::vector<FileMetaDataRef> files = curGroup->GetFiles();
        if (curGroup->IsOverlapping()) {
            GetOverlappingFiles(files, key, checkOrderRange, result);
            curGroup = curGroup->GetDown();
            continue;
        }

        uint32_t index = FindFile(files, key);
        while (index < files.size()) {
            FileMetaDataRef fileMetaData = files.at(index);
            int32_t compare = FullKeyUtil::CompareKeyWithInternalKey(key, fileMetaData->GetSmallest());
            bool containsHash = fileMetaData->GetOrderRange()->Contains(key.PriKey().KeyHashCode());
            if (compare >= 0 && (!checkOrderRange || containsHash)) {
                result.push_back(fileMetaData);
            }
            compare = FullKeyUtil::CompareKeyWithInternalKey(key, fileMetaData->GetLargest());
            if (compare == 0) {
                index++;
                continue;
            }
            break;
        }
        curGroup = curGroup->GetDown();
    }

    // 从新到老排序
    if (LIKELY(result.size() > 1)) {
        sort(result.begin(), result.end(), Level::Compare);
    }
}

std::vector<FileMetaDataRef> Level::GetFilesContainingPrefixKey(const Key &prefixKey, uint32_t keyGroup,
                                                                bool checkOrderRange)
{
    uint32_t isFileMetaDataNull = 0;
    uint32_t isOverlapping = 0;
    std::vector<FileMetaDataRef> result;
    FileMetaDataGroupRef curGroup = mFileMetaDataGroups.empty() ? nullptr : mFileMetaDataGroups.at(0);
    if (curGroup == nullptr) {
        isFileMetaDataNull = 1;
    }
    while (curGroup != nullptr) {
        if (curGroup->GetGroupRange()->ContainsGroup(static_cast<int32_t>(keyGroup))) {
            auto &files = curGroup->GetFiles();
            if (curGroup->IsOverlapping()) {
                isOverlapping = 1;
                auto overlappingFiles = GetOverlappingFilesMightContainPrefix(files, prefixKey, checkOrderRange);
                result.insert(result.end(), overlappingFiles.begin(), overlappingFiles.end());
            } else {
                uint32_t index = FindFile(files, prefixKey);
                for (; index < files.size(); index++) {
                    FileMetaDataRef fileMetaData = files.at(index);
                    if (!checkOrderRange || fileMetaData->GetOrderRange()->Contains(prefixKey.PriKey().KeyHashCode())) {
                        auto containsStateId = fileMetaData->GetStateIdInterval().Contains(prefixKey.StateId());
                        if (containsStateId && IsPrefixKeyWithinFile(fileMetaData, prefixKey)) {
                            result.push_back(fileMetaData);
                        } else {
                            break;
                        }
                    }
                }
            }
            curGroup = curGroup->GetDown();
            continue;
        } else {
            LOG_DEBUG("Key group is not within group range, keyGroup："
                      << keyGroup << ", startGroup:" << curGroup->GetGroupRange()->GetStartGroup()
                      << ", endGroup:" << curGroup->GetGroupRange()->GetEndGroup()
                      << ", priKeyHash:" << prefixKey.PriKey().KeyHashCode() << ".");
        }
        curGroup = NextFileMetaDataGroup(curGroup);
    }

    if (result.empty()) {
        LOG_DEBUG("Not Found, levelId:" << mLevelId << ", priKeyHash:" << prefixKey.PriKey().KeyHashCode()
                                        << ", isFileMetaDataNull:" << isFileMetaDataNull
                                        << ", isOverlapping:" << isOverlapping << ".");
    }
    return result;
}

std::vector<FileMetaDataRef> Level::GetFilesContainingStateId(uint32_t stateId)
{
    std::vector<FileMetaDataRef> result;
    FileMetaDataGroupRef curGroup = mFileMetaDataGroups.empty() ? nullptr : mFileMetaDataGroups.at(0);
    while (curGroup != nullptr) {
        auto &files = curGroup->GetFiles();
        for (const auto &item : files) {
            if (item->GetStateIdInterval().Contains(stateId)) {
                result.push_back(item);
            }
        }
        curGroup = NextFileMetaDataGroup(curGroup);
    }
    return result;
}

bool Level::IsPrefixKeyWithinFile(const FileMetaDataRef &fileMetaData, const Key &prefixKey)
{
    FullKeyRef smallest = fileMetaData->GetSmallest();
    int32_t result = smallest->ComparePrefixKey(prefixKey);
    if (result == 0) {
        return true;
    } else if (result < 0) {
        FullKeyRef largest = fileMetaData->GetLargest();
        return largest->ComparePrefixKey(prefixKey) >= 0;
    }
    return false;
}

std::vector<FileMetaDataRef> Level::GetOverlappingFilesMightContainPrefix(const std::vector<FileMetaDataRef> &files,
                                                                          const Key &prefixKey, bool checkOrderRange)
{
    std::vector<FileMetaDataRef> result;
    for (auto &file : files) {
        if (checkOrderRange && !file->GetOrderRange()->Contains(prefixKey.PriKey().KeyHashCode())) {
            continue;
        }
        auto containsStateId = file->GetStateIdInterval().Contains(prefixKey.StateId());
        if (containsStateId && IsPrefixKeyWithinFile(file, prefixKey)) {
            result.push_back(file);
        }
    }
    return result;
}

uint32_t Level::FindFile(const std::vector<FileMetaDataRef> &files, const Key &key)
{
    uint32_t left = 0;
    uint32_t right = files.size();
    while (left < right) {
        uint32_t mid = left + ((right - left) >> NO_1);
        if (FullKeyUtil::CompareKeyWithInternalKey(key, files.at(mid)->GetLargest()) > 0) {
            left = mid + 1;
            continue;
        }
        right = mid;
    }
    return right;
}

void LevelBuilder::MergeSameFileMetaDataGroups()
{
    struct PersonHash {
        size_t operator()(const GroupRangeRef &groupRange) const
        {
            return groupRange->HashCode();
        }
    };
    struct PersonEqual {
        bool operator()(const GroupRangeRef &lhs, const GroupRangeRef &rhs) const
        {
            return lhs->Equals(rhs);
        }
    };
    std::unordered_map<GroupRangeRef, FileMetaDataGroupRef, PersonHash, PersonEqual> groupRanges;

    auto iter = mFileMetaDataGroups.begin();
    while (iter != mFileMetaDataGroups.end()) {
        FileMetaDataGroupRef fileMetaDataGroup = *iter;
        GroupRangeRef groupRange = fileMetaDataGroup->GetGroupRange();
        // 1. 在groupRanges中查找, 不存在该groupRange则放入到groupRanges中, 再查找下一个.
        if (groupRanges.find(groupRange) == groupRanges.end()) {
            groupRanges.insert({ groupRange, fileMetaDataGroup });
            ++iter;
            continue;
        }

        // 2. 在groupRanges中查找, 存在该groupRange则merge则将两个groupRange进行合并.
        if (mFileMetaDataComparator == nullptr) {
            mFileMetaDataComparator = FileMetaDataGroup::CreateFileMetaDataComparator(false);
        }
        groupRanges.at(groupRange)->Merge(fileMetaDataGroup, (mLevelId == 0), mFileMetaDataComparator);
        ++iter;
    }

    // 3. 将merge过的groupRange写到mFileMetaDataGroups中.
    mFileMetaDataGroups.clear();
    for (auto &item : groupRanges) {
        mFileMetaDataGroups.push_back(item.second);
    }
}

}  // namespace bss
}  // namespace ock