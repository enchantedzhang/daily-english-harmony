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

#ifndef BOOST_SS_VERSION_SET_H
#define BOOST_SS_VERSION_SET_H

#include <unordered_set>

#include "compaction/compaction.h"
#include "compaction/compaction_picker.h"
#include "include/bss_err.h"
#include "include/config.h"
#include "lsm_store/file/file_cache.h"
#include "lsm_store/file/file_meta_data_group.h"
#include "lsm_store/file/input_sorted_run.h"
#include "lsm_store/file/merging_iterator.h"
#include "lsm_store/file/order_range.h"
#include "tombstone/tombstone_service.h"
#include "version.h"
#include "version_edit.h"

namespace ock {
namespace bss {
class VersionSet {
public:
    VersionSet() = default;

    VersionSet(const ConfigRef &config, const GroupRangeRef &range, const HashCodeOrderRangeRef &curOrderRange,
               const FileCacheRef &fileCache)
        : mConfig(config), mOrderRange(curOrderRange), mGroupRange(range)
    {
        mFileMetaDataComparator = FileMetaDataGroup::CreateFileMetaDataComparator(false);
        mFileMetaDataReverseComparator = FileMetaDataGroup::CreateFileMetaDataComparator(true);

        mMaxBytesForLevels = new (std::nothrow) uint64_t[mConfig->GetFileStoreNumLevels()];
        uint64_t maxBytesForLevels = mConfig->GetFileStoreMaxBaseLevelBytes();
        mMaxBytesForLevels[0] = maxBytesForLevels;
        mMaxBytesForLevels[1] = maxBytesForLevels;
        for (uint32_t i = NO_2; i < mConfig->GetFileStoreNumLevels(); i++) {
            maxBytesForLevels = mConfig->GetFileStoreMultiple() * maxBytesForLevels;
            mMaxBytesForLevels[i] = maxBytesForLevels;
        }

        mCompactionPicker = std::make_shared<CompactionPicker>(mConfig);
        VersionBuilderRef builder = VersionBuilder::NewBuilder(this);
        VersionPtr versionPtr = builder->SetCurGroupRange(mGroupRange)->SetCurOrderRange(mOrderRange)->Build();
        AppendVersion(versionPtr);
        LOG_INFO("Construct versionSet success, max " << mConfig->GetFileStoreNumLevels() << " levels (max base size "
                                                      << mConfig->GetFileStoreMaxBaseLevelBytes() << "bytes).");
    }

    ~VersionSet()
    {
        if (mMaxBytesForLevels != nullptr) {
            delete[] mMaxBytesForLevels;
            mMaxBytesForLevels = nullptr;
        }
    }

    inline bool NeedCompaction()
    {
        return (mCurrent->GetCompactionScore() >= 1.0);
    }

    double GetCompactionScore() const
    {
        return mCurrent->GetCompactionScore();
    }

    uint64_t MaxBytesForLevel(uint64_t levelId)
    {
        if (levelId >= mConfig->GetFileStoreNumLevels()) {
            LOG_ERROR("Invalid level :" << levelId);
            return 0;
        }
        return mMaxBytesForLevels[levelId];
    }

    ConfigRef GetConf()
    {
        return mConfig;
    }

    GroupRangeRef GetGroupRange() const
    {
        return mGroupRange;
    }

    HashCodeOrderRangeRef GetOrderRange() const
    {
        return mOrderRange;
    }

    FileMetaData::FileMetaDataComparator &GetFileMetaDataComparator()
    {
        return mFileMetaDataComparator;
    }

    FileMetaData::FileMetaDataComparator &GetFileMetaDataReverseComparator()
    {
        return mFileMetaDataReverseComparator;
    }

    int64_t GetLastSeqId() const
    {
        return mLastSeqId;
    }

    // 获取versionSet中activeVersion中的所有fileMetaData.
    std::vector<FileMetaDataRef> GetLiveFileMetaDatas()
    {
        std::vector<FileMetaDataRef> results;
        for (const auto &activeVersion : mActiveVersions) {
            for (Level level : activeVersion->GetLevels()) {
                for (auto &fileMetaDataGroup : level.GetFileMetaDataGroups()) {
                    for (auto &fileMetaData : fileMetaDataGroup->GetFiles()) {
                        results.push_back(fileMetaData);
                    }
                }
            }
        }
        return results;
    }

    BResult SetLastSeqId(int64_t newLastSeqId)
    {
        if (newLastSeqId <= mLastSeqId) {
            return BSS_INVALID_PARAM;
        }
        mLastSeqId = newLastSeqId;
        return BSS_OK;
    }

    inline std::unordered_set<VersionPtr> GetActiveVersions() const
    {
        return mActiveVersions;
    }

    inline VersionPtr GetCurrent() const
    {
        return mCurrent;
    }

    VersionPtr AppendVersion(const VersionPtr &version)
    {
        if (version == nullptr) {
            return nullptr;
        }
        // 替换当前current的version.
        VersionPtr previous = mCurrent;
        mCurrent = version;
        mActiveVersions.emplace(version);
        if (previous != nullptr) {
            return previous->Release();
        }
        return nullptr;
    }

    BResult ClearInitVersion()
    {
        if (mCurrent->IsEmpty()) {
            LOG_WARN("current is Empty");
        }
        mCurrent = nullptr;
        mActiveVersions.clear();
        return BSS_OK;
    }

    bool RemoveVersion(VersionPtr version)
    {
        if (version == nullptr) {
            return false;
        }
        if (version == mCurrent) {
            LOG_ERROR("Remove version is current version.");
            return false;
        }
        return mActiveVersions.erase(version);
    }

    CompactionRef PickCompaction(const GroupRangeRef &currentGroupRange)
    {
        RETURN_NULLPTR_AS_NULLPTR(currentGroupRange);
        if (UNLIKELY(mCurrent == nullptr)) {
            LOG_ERROR("current version is null, startGroup:" << currentGroupRange->GetStartGroup()
                                                             << ", endGroup:" << currentGroupRange->GetEndGroup());
            return nullptr;
        }
        if (mCurrent->GetCompactionScore() >= 1.0) {
            std::vector<FileMetaDataRef> inputFiles = mCompactionPicker->GetInputFiles(mCurrent, currentGroupRange);
            if (inputFiles.empty()) {
                LOG_WARN("Pick compaction input files is nullptr from current version.");
                return nullptr;
            }
            return mCompactionPicker->GenerateCompactionBaseOnInputs(mCurrent, currentGroupRange, inputFiles);
        }
        return nullptr;
    }

    MergingIteratorRef MakeInputIterator(const CompactionRef &compaction, const MemManagerRef &memManager,
                                         FileProcHolder mHolder, TombstoneServiceRef &tombstoneService)
    {
        uint32_t levelId = compaction->GetInputLevelId();
        std::vector<KeyValueIteratorRef> iterators;
        if (levelId == 0) {
            for (auto &fileMetaData : compaction->GetMergeLevelInputs()) {
                iterators.emplace_back(InputSortedRun::BuildInputSortedRunIterator(fileMetaData, mFileIteratorBuilder));
            }
        } else {
            std::vector<InputSortedRunRef> inputSortedRunList =
                InputSortedRun::BuildInputSortedRun(compaction->GetMergeLevelInputs(), GetFileMetaDataComparator());
            for (auto &inputSortedRun : inputSortedRunList) {
                iterators.emplace_back(inputSortedRun->GetIterator(mFileIteratorBuilder));
            }
        }

        if (!compaction->GetMergeOutputLevelInputs().empty()) {
            std::vector<InputSortedRunRef> inputSortedRunList =
                InputSortedRun::BuildInputSortedRun(compaction->GetMergeOutputLevelInputs(),
                                                    GetFileMetaDataComparator());
            for (auto &inputSortedRun : inputSortedRunList) {
                iterators.emplace_back(inputSortedRun->GetIterator(mFileIteratorBuilder));
            }
        }

        return std::make_shared<MergingIterator>(iterators, memManager, mHolder, true, tombstoneService);
    }

    void SetFileIteratorBuilder(InputSortedRun::FileIteratorWriterRef fileIteratorBuilder)
    {
        mFileIteratorBuilder = fileIteratorBuilder;
    }

    static VersionPtr RestoreVersions(VersionSet *parentVersionSet, std::vector<VersionPtr> versions,
                                      GroupRangeRef curGroupRange, HashCodeOrderRangeRef curOrderRange)
    {
        uint32_t numOfLevels = parentVersionSet->GetConf()->GetFileStoreNumLevels();

        RestoreBuilderRef versionBuilder = RestoreBuilder::NewRestoredBuilder(static_cast<void *>(parentVersionSet));
        versionBuilder->SetCurGroupRange(curGroupRange);
        versionBuilder->SetCurOrderRange(curOrderRange);
        for (const VersionPtr &version : versions) {
            if (numOfLevels < version->GetNumLevels()) {
                LOG_ERROR("Current version must have more levels than restored.");
                return nullptr;
            }

            for (Level level : version->GetLevels()) {
                for (const FileMetaDataGroupRef &fileMetaDataGroup : level.GetFileMetaDataGroups()) {
                    GroupRangeRef groupRange = fileMetaDataGroup->GetGroupRange();
                    GroupRange intersectionGroupRange = groupRange->Intersection(curGroupRange);
                    if (intersectionGroupRange.IsEmpty()) {
                        continue;
                    }
                    RestoreFileMetaDataGroup(curOrderRange, versionBuilder, level, fileMetaDataGroup,
                                             intersectionGroupRange);
                }
            }
        }
        return versionBuilder->Build();
    }

    static void RestoreFileMetaDataGroup(const HashCodeOrderRangeRef &curOrderRange, RestoreBuilderRef &versionBuilder,
                                         Level &level, const FileMetaDataGroupRef &fileMetaDataGroup,
                                         const GroupRange &intersectionGroupRange)
    {
        FileDataGroupBuilderRef builder = FileDataGroupBuilder::NewBuilder();
        builder->SetGroupRange(std::make_shared<GroupRange>(intersectionGroupRange));
        for (const FileMetaDataRef &file : fileMetaDataGroup->GetFiles()) {
            HashCodeOrderRange intersectionOrderRange = file->GetOrderRange()->Intersection(curOrderRange);
            FileMetaData::BuilderRef fileBuilder = FileMetaData::NewBuilder();
            fileBuilder->Fill(file->GetSmallest(), file->GetLargest(), file->GetFileSize(), file->GetFileAddress(),
                              file->GetSeqId(), std::make_shared<GroupRange>(intersectionGroupRange),
                              std::make_shared<HashCodeOrderRange>(intersectionOrderRange), file->GetIdentifier(),
                              file->GetStateIdInterval(), file->GetFileStatus(), file->GetRecordMeta());
            FileMetaDataRef newFile = fileBuilder->Build();
            builder->AddFileMeta(newFile);
        }
        if (!builder->IsEmpty()) {
            versionBuilder->AddLevel(level.GetLevelId(), builder->Build());
        }
    }

private:
    ConfigRef mConfig = nullptr;
    uint64_t *mMaxBytesForLevels = nullptr;
    std::unordered_set<VersionPtr> mActiveVersions;
    FileMetaData::FileMetaDataComparator mFileMetaDataComparator = nullptr;
    FileMetaData::FileMetaDataComparator mFileMetaDataReverseComparator = nullptr;
    HashCodeOrderRangeRef mOrderRange = nullptr;
    GroupRangeRef mGroupRange = nullptr;
    CompactionPickerRef mCompactionPicker = nullptr;
    InputSortedRun::FileIteratorWriterRef mFileIteratorBuilder;

    int64_t mLastSeqId = 0;
    VersionPtr mCurrent = nullptr;
};
using VersionSetRef = std::shared_ptr<VersionSet>;

class VersionInnerBuilder;
using VersionInnerBuilderRef = std::shared_ptr<VersionInnerBuilder>;
class VersionInnerBuilder {
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

public:
    VersionInnerBuilder(VersionSet *versionSet, const VersionPtr &baseVersion)
        : mVersionSet(versionSet), mBaseVersion(baseVersion)
    {
        mBaseVersion->Retain();
    }

    static VersionInnerBuilderRef NewVersionInnerBuilder(VersionSet *versionSet, const VersionPtr &currentVersion,
                                                         const GroupRangeRef &groupRange,
                                                         const HashCodeOrderRangeRef &orderRange)
    {
        VersionInnerBuilderRef builder = std::make_shared<VersionInnerBuilder>(versionSet, currentVersion);
        builder->SetGroupRange(groupRange);
        builder->SetOrderRange(orderRange);
        return builder;
    }

    VersionInnerBuilder *SetGroupRange(const GroupRangeRef &groupRange)
    {
        mGroupRange = groupRange;
        return this;
    }

    VersionInnerBuilder *SetOrderRange(const HashCodeOrderRangeRef &orderRange)
    {
        mOrderRange = orderRange;
        return this;
    }

    VersionInnerBuilder *Apply(VersionEditRef edit)
    {
        mEdits.emplace_back(edit);
        return this;
    }

    FileMetaData::FileMetaDataComparator GetFileMetaDataComparator()
    {
        return mVersionSet->GetFileMetaDataComparator();
    }

    void InternalRelease()
    {
        if (mBaseVersion != nullptr) {
            mBaseVersion->Release();
            mBaseVersion = nullptr;
        }
    }

    VersionPtr Build()
    {
        DelFiles allDeletedFiles;
        NewFiles allAddedFiles;
        // 遍历所有的版本编辑, 将新增和删除的文件添加到对应的map中.
        for (VersionEditRef &edit : mEdits) {
            allAddedFiles.insert(edit->GetNewFiles().begin(), edit->GetNewFiles().end());
            allDeletedFiles.insert(edit->GetDeletedFiles().begin(), edit->GetDeletedFiles().end());
        }
        // 创建一个新的版本构建器.
        VersionBuilderRef versionBuilder = VersionBuilder::NewBuilder(static_cast<void *>(mVersionSet));
        std::vector<Level> levels = mBaseVersion->GetLevels();

        std::unordered_map<uint32_t, std::unordered_map<GroupRangeRef, FileDataGroupBuilderRef, PersonHash, PersonEqual>>
            fileMetaDataGroupBuilders;
        // 遍历所有新增的文件，按照级别和组范围进行分组，并创建对应的文件元数据组构建器.
        for (auto &item : allAddedFiles) {
            GroupRangeRef groupRange = item.first;
            for (auto &levelFileMeta : item.second) {
                FileDataGroupBuilderRef fileMetaBuilder = FileDataGroupBuilder::NewBuilder();
                fileMetaBuilder->SetFileMetaDataComparator(GetFileMetaDataComparator());
                fileMetaBuilder->SetGroupRange(groupRange);
                fileMetaBuilder->AddFileMetas(levelFileMeta.second);
                uint32_t levelId = levelFileMeta.first;
                fileMetaDataGroupBuilders[levelId][groupRange] = fileMetaBuilder;
            }
        }
        // 遍历所有的级别，对每个级别进行处理
        for (Level level : levels) {
            int32_t levelId = level.GetLevelId();
            LevelBuilderRef levelBuilder = LevelBuilder::NewBuilder(levelId);
            levelBuilder->SetFileMetaDataComparator(GetFileMetaDataComparator());
            auto map = fileMetaDataGroupBuilders.find(levelId);
            for (FileMetaDataGroupRef &fileMetaDataGroup : level.GetFileMetaDataGroups()) {
                GroupRangeRef groupRange = fileMetaDataGroup->GetGroupRange();
                FileDataGroupBuilderRef metaGroupBuilder = nullptr;
                if (map != fileMetaDataGroupBuilders.end()) {
                    auto builder = map->second.find(groupRange);
                    if (builder != map->second.end()) {
                        metaGroupBuilder = builder->second;
                        map->second.erase(builder);
                    } else {
                        metaGroupBuilder = FileDataGroupBuilder::NewBuilder();
                        metaGroupBuilder->SetFileMetaDataComparator(GetFileMetaDataComparator());
                        metaGroupBuilder->SetGroupRange(groupRange);
                    }
                } else {
                    metaGroupBuilder = FileDataGroupBuilder::NewBuilder();
                    metaGroupBuilder->SetFileMetaDataComparator(GetFileMetaDataComparator());
                    metaGroupBuilder->SetGroupRange(groupRange);
                }
                std::vector<FileMetaDataRef> files = fileMetaDataGroup->GetFiles();

                std::unordered_set<std::string> deletedFiles;
                auto allDeletedFilesIt = allDeletedFiles.find(groupRange);
                if (allDeletedFilesIt != allDeletedFiles.end()) {
                    auto deletedFilesIt = allDeletedFilesIt->second.find(levelId);
                    if (deletedFilesIt != allDeletedFilesIt->second.end()) {
                        deletedFiles = deletedFilesIt->second;
                    }
                }

                for (FileMetaDataRef &file : files) {
                    if (deletedFiles.find(file->GetIdentifier()) != deletedFiles.end()) {
                        continue;
                    }
                    metaGroupBuilder->AddFileMeta(file);
                }
                if (!metaGroupBuilder->IsEmpty()) {
                    levelBuilder->AddFileMetaDataGroup(metaGroupBuilder->Build());
                }
            }
            auto map1 = fileMetaDataGroupBuilders.find(levelId);
            if (map1 != fileMetaDataGroupBuilders.end()) {
                for (auto &entry : map1->second) {
                    auto fileMetaGroup = entry.second->Build();
                    CONTINUE_LOOP_AS_NULLPTR(fileMetaGroup);
                    levelBuilder->AddFileMetaDataGroup(fileMetaGroup);
                }
            }
            auto levelPtr = levelBuilder->Build();
            LOG_DEBUG("Add level to version, levelId:" << levelId << ", totalFiles:" << levelPtr.TotalFileSize()
                                                       << ", fileDataGroup:" << levelPtr.GetFileMetaDataGroups().size()
                                                       << ".");
            versionBuilder->AddLevel(levelId, levelPtr);
        }

        VersionPtr result = versionBuilder->SetCurGroupRange(mGroupRange)->SetCurOrderRange(mOrderRange)->Build();
        result->SetVersionSeqId();
        InternalRelease();
        return result;
    }

private:
    VersionSet *mVersionSet;
    std::vector<VersionEditRef> mEdits;
    VersionPtr mBaseVersion;
    GroupRangeRef mGroupRange;
    HashCodeOrderRangeRef mOrderRange;
};

}  // namespace bss
}  // namespace ock
#endif
