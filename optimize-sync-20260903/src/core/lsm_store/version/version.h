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

#ifndef BOOST_SS_VERSION_H
#define BOOST_SS_VERSION_H

#include <memory>
#include <unordered_map>
#include <unordered_set>

#include "common/bss_def.h"
#include "common/bss_log.h"
#include "compaction/compaction_comm.h"
#include "include/config.h"
#include "lazy/restore_file_info.h"
#include "level.h"
#include "lsm_store/file/file_meta_data_group.h"

namespace ock {
namespace bss {
static uint64_t MakeVersionSeqId()
{
    static std::atomic<uint64_t> baseSeq(1);
    auto result = baseSeq.fetch_add(1);
    return result;
}

class Version;
using VersionPtr = std::shared_ptr<Version>;
class Version : public std::enable_shared_from_this<Version> {
public:
    Version() = default;

    Version(void *versionSet, std::vector<Level> &levels, uint32_t compactionLevel, const GroupRangeRef &group,
            double compactionScore, Reason compactionReason)
        : mParentVersionSet(versionSet),
          mLevels(levels),
          mCompactionReason(compactionReason),
          mCompactionLevel(compactionLevel),
          mCompactionScore(compactionScore),
          mGroupRange(group)
    {
        mReferenceCount.store(1);
    }

    inline bool IsEmpty()
    {
        for (Level &level : mLevels) {
            if (!level.GetFileMetaDataGroups().empty()) {
                return false;
            }
        }
        return true;
    }

    inline uint32_t GetCompactionLevel() const
    {
        return mCompactionLevel;
    }

    inline double GetCompactionScore() const
    {
        return mCompactionScore;
    }

    inline Reason GetCompactionReason() const
    {
        return mCompactionReason;
    }

    inline bool IsVersionAligned() const
    {
        return fabs(mCompactionScore - DOUBLE_MAX_VALUE) != 0;
    }

    inline GroupRangeRef GetGroupRange() const
    {
        return mGroupRange;
    }

    std::vector<FileMetaDataRef> GetFileMetaDatas()
    {
        std::vector<FileMetaDataRef> files;
        for (auto &mLevel : mLevels) {
            auto fileMetaDataGroups = mLevel.GetFileMetaDataGroups();
            for (auto &fileMetaDataGroup : fileMetaDataGroups) {
                auto vec = fileMetaDataGroup->GetFiles();
                for (auto &file : vec) {
                    files.emplace_back(file);
                }
            }
        }
        return files;
    }

    std::vector<FileMetaDataRef> GetFileMetaDatas(uint32_t levelId, GroupRangeRef groupRange)
    {
        auto fileMetaDataGroups = mLevels[levelId].GetFileMetaDataGroups();
        for (auto &fileMetaDataGroup : fileMetaDataGroups) {
            if (fileMetaDataGroup->GetGroupRange()->Equals(groupRange)) {
                return fileMetaDataGroup->GetFiles();
            }
        }
        return {};
    }

    std::pair<std::vector<FileMetaDataRef>, bool> GetMayOverlappedFiles(uint32_t levelId, GroupRangeRef groupRange)
    {
        auto fileMetaDataGroups = mLevels[levelId].GetFileMetaDataGroups();
        if (fileMetaDataGroups.size() >= NO_2) {
            LOG_ERROR("Number of file meta data groups at level "
                      << levelId << " is expected to be less than 2, but is " << fileMetaDataGroups.size()
                      << " because a compaction with highest priority will be launched first"
                      << " to merge multiple FileMetaDataGroups after rescaling.");
            return { {}, false };
        }

        std::vector<FileMetaDataRef> files;
        bool isOverlapping = false;
        for (auto &fileMetaDataGroup : fileMetaDataGroups) {
            if (fileMetaDataGroup->GetGroupRange()->Overlaps(groupRange)) {
                if (fileMetaDataGroup->IsOverlapping()) {
                    isOverlapping = true;
                    for (auto &item : fileMetaDataGroup->GetFilesOldestFirst()) {
                        files.push_back(item);
                    }
                    continue;
                }
                for (auto &item : fileMetaDataGroup->GetFiles()) {
                    files.push_back(item);
                }
            }
        }

        return std::pair<std::vector<FileMetaDataRef>, bool>(files, isOverlapping);
    }

    inline uint32_t GetNumLevels() const
    {
        return mLevels.size();
    }

    ConfigRef GetConf();

    inline std::vector<Level> &GetLevels()
    {
        return mLevels;
    }

    inline Level GetLevel(uint32_t id)
    {
        if (id >= mLevels.size()) {
            LOG_ERROR("Invalid level: " << id);
            return mLevels[mLevels.size() - 1];
        }
        return mLevels[id];
    }

    inline void Retain()
    {
        mReferenceCount.fetch_add(1);
    }

    VersionPtr Release();

    std::string ToString()
    {
        std::ostringstream oss;
        oss << " seqId:" << mSeqId << ", referenceCount:" << mReferenceCount
            << ", compactionReason:" << static_cast<uint32_t>(mCompactionReason)
            << ", compactionLevel:" << mCompactionLevel << ", compactionScore:" << mCompactionScore
            << ", groupRange:" << mGroupRange->ToString() << ".";
        oss << " Level info:" << LevelSummary() << ".";
        return oss.str();
    }

    std::string LevelSummary()
    {
        std::ostringstream stream;
        stream << "{";
        for (Level level : mLevels) {
            stream << "level-" << level.GetLevelId() << ": ";
            for (auto &fileMetaDataGroup : level.GetFileMetaDataGroups()) {
                stream << fileMetaDataGroup->GetGroupRange() << ", files:";
                for (auto &file : fileMetaDataGroup->GetFiles()) {
                    stream << "[" << PathTransform::ExtractFileName(file->GetIdentifier()) << " "
                           << file->GetFileAddress() << " " << file->GetFileSize() << " "
                           << static_cast<uint32_t>(file->GetFileStatus()) << "] ";
                }
            }
            stream << ";";
        }
        stream << "}";
        return stream.str();
    }

    inline uint64_t GetVersionSeqId() const
    {
        return mSeqId;
    }

    inline void SetVersionSeqId()
    {
        mSeqId = MakeVersionSeqId();
    }

    inline static std::vector<Level> InitEmptyLevels(uint32_t numLevels)
    {
        std::vector<Level> levels;
        for (uint32_t idx = 0; idx < numLevels; idx++) {
            levels.push_back(LevelBuilder::NewBuilder(static_cast<int32_t>(idx))->Build());
        }
        return levels;
    }

    static VersionPtr MigrateVersion(
        VersionPtr currentVersion, std::unordered_map<std::string, RestoreFileInfo> &toRelocateFileMapping,
        std::unordered_map<std::string, std::tuple<uint64_t, uint32_t>> &relocatedFileMapping);

private:
    void *mParentVersionSet = nullptr;
    std::vector<Level> mLevels;
    std::atomic<uint32_t> mReferenceCount{ 0 };
    Reason mCompactionReason = Reason::NO_NEED_COMPACTION;
    uint32_t mCompactionLevel = 0;
    double mCompactionScore = -1.0;
    GroupRangeRef mGroupRange = nullptr;
    uint64_t mSeqId = 0;
};

template <typename k> class BuilderBase {
public:
    explicit BuilderBase(void *versionSet) : mParentVersionSet(versionSet)
    {
    }

    virtual ~BuilderBase() = default;

    virtual k *SetCurGroupRange(const GroupRangeRef &groupRange)
    {
        mCurGroupRange = groupRange;
        return static_cast<k *>(this);
    }

    virtual k *SetCurOrderRange(const HashCodeOrderRangeRef &curOrderRange)
    {
        mOrderRange = curOrderRange;
        return static_cast<k *>(this);
    }

    void FinalizeVersion();

public:
    void *mParentVersionSet = nullptr;
    std::vector<Level> mLevels;
    int32_t mCompactionLevel = -1;
    GroupRangeRef mCurGroupRange = nullptr;
    HashCodeOrderRangeRef mOrderRange = nullptr;
    double mCompactionScore = -1.0;
    Reason mCompactionReason = Reason::NO_NEED_COMPACTION;
};

class RestoreBuilder;
using RestoreBuilderRef = std::shared_ptr<RestoreBuilder>;
class RestoreBuilder : public BuilderBase<RestoreBuilder> {
public:
    explicit RestoreBuilder(void *parentVersionSet) : BuilderBase(parentVersionSet)
    {
        for (uint32_t i = 0; i < GetConf()->GetFileStoreNumLevels(); i++) {
            LevelBuilderRef builder = LevelBuilder::NewBuilder(static_cast<int32_t>(i));
            builder->SetFileMetaDataComparator(GetFileMetaDataComparator());
            mLevelBuilders.push_back(builder);
        }
    }

    static RestoreBuilderRef NewRestoredBuilder(void *parentVersionSet)
    {
        return std::make_shared<RestoreBuilder>(parentVersionSet);
    }

    inline void AddLevel(uint32_t levelId, const FileMetaDataGroupRef &fileMetaDataGroup)
    {
        if (levelId >= mLevelBuilders.size()) {
            LOG_ERROR("Out of level range:" << levelId << ".");
            return;
        }
        mLevelBuilders[levelId]->AddFileMetaDataGroup(fileMetaDataGroup);
    }

    ConfigRef GetConf();

    FileMetaData::FileMetaDataComparator GetFileMetaDataComparator();

    VersionPtr Build();

private:
    std::vector<LevelBuilderRef> mLevelBuilders;
};

class VersionBuilder;
using VersionBuilderRef = std::shared_ptr<VersionBuilder>;
class VersionBuilder : public BuilderBase<VersionBuilder> {
public:
    explicit VersionBuilder(void *versionSet) : BuilderBase(versionSet)
    {
    }

    static VersionBuilderRef NewBuilder(void *versionSet)
    {
        return std::make_shared<VersionBuilder>(versionSet);
    }

    void AddLevel(uint32_t levelId, Level level);

    VersionPtr Build();
};
}  // namespace bss
}  // namespace ock
#endif