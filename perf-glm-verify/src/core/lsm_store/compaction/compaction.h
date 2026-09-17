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

#ifndef BOOST_SS_COMPACTION_H
#define BOOST_SS_COMPACTION_H

#include <cstdint>
#include <vector>

#include "binary/key_value.h"
#include "compaction_comm.h"
#include "lsm_store/file/basic_utils.h"
#include "lsm_store/file/file_meta_data.h"
#include "lsm_store/file/file_meta_data_group.h"
#include "version/level.h"
#include "version/version.h"
#include "version/version_edit.h"

namespace ock {
namespace bss {
class Compaction {
public:
    enum class Result {
        NON_COMPACTION,
        CONCURRENT_ABORT,
        TRIVIAL_MOVE,
        NORMAL,
        ABORT,
    };

    Compaction(const VersionPtr &inputVersion, uint32_t inputLevelId, const GroupRangeRef &groupRange,
               uint64_t maxFileOutputSize, const FullKeyRef &smallest, const FullKeyRef &largest,
               const std::vector<FileMetaDataRef> &levelInputs, const std::vector<FileMetaDataRef> &outputLevelInputs,
               const std::vector<FileMetaDataRef> &grandparents)
        : mInputVersion(inputVersion),
          mCompactionScore(inputVersion->GetCompactionScore()),
          mCompactionReason(inputVersion->GetCompactionReason()),
          mLevelInputs(levelInputs),
          mOutputLevelInputs(outputLevelInputs),
          mMergeLevelInputs(levelInputs),
          mMergeOutputLevelInputs(outputLevelInputs),
          mGrandparents(grandparents),
          mMaxFileOutputSize(maxFileOutputSize),
          mInputLevelId(inputLevelId),
          mNumLevels(inputVersion->GetConf()->GetFileStoreNumLevels()),
          mGroupRange(groupRange),
          mSmallest(smallest),
          mLargest(largest)
    {
        mEditBuilder = VersionEdit::Builder::NewBuilder();
        mLevelPointers.resize(mNumLevels);
        mInputVersion->Retain();
    }

    virtual ~Compaction()
    {
        delete mEditBuilder;
        mEditBuilder = nullptr;
    }

    virtual VersionEdit::Builder *GetEditBuilder() const
    {
        return mEditBuilder;
    }

    virtual std::vector<FileMetaDataRef> &GetLevelInputs()
    {
        return mLevelInputs;
    }

    virtual std::vector<FileMetaDataRef> &GetOutputLevelInputs()
    {
        return mOutputLevelInputs;
    }

    virtual const std::vector<FileMetaDataRef> &GetMergeLevelInputs() const
    {
        return mMergeLevelInputs;
    }

    virtual const std::vector<FileMetaDataRef> &GetMergeOutputLevelInputs() const
    {
        return mMergeOutputLevelInputs;
    }

    virtual const std::vector<FileMetaDataRef> &GetAdoptedFiles() const
    {
        return mAdoptedFiles;
    }

    virtual void SetListStateFileReusePlan(const std::vector<FileMetaDataRef> &mergeLevelInputs,
                                           const std::vector<FileMetaDataRef> &mergeOutputLevelInputs,
                                           const std::vector<FileMetaDataRef> &adoptedFiles)
    {
        mMergeLevelInputs = mergeLevelInputs;
        mMergeOutputLevelInputs = mergeOutputLevelInputs;
        mAdoptedFiles = adoptedFiles;
    }

    virtual VersionPtr GetInputVersion() const
    {
        return mInputVersion;
    }

    virtual uint32_t GetInputLevelId()
    {
        return mInputLevelId;
    }

    virtual uint32_t GetOutputLevelId()
    {
        return mInputLevelId + 1;
    }

    virtual GroupRangeRef GetGroupRange() const
    {
        return mGroupRange;
    }

    virtual double GetCompactionScore() const
    {
        return mCompactionScore;
    }

    virtual bool IsTrivialMove()
    {
        // 简单移动满足的条件：1）输入文件的数量为1; 2）输出文件为为空; 3）父的文件大小小于或等于配置中的最大重叠字节.
        return (mLevelInputs.size() == 1) && (mOutputLevelInputs.empty()) &&
               (BasicUtils::TotalFileSize(mGrandparents) <=
                mInputVersion->GetConf()->GetFileStoreMaxGrandParentOverlapBytes());
    }

    // 判断targetKey是否在output文件中不存在.
    virtual bool KeyNotExistBeyondOutputLevels(const Key &key)
    {
        for (uint32_t level = mInputLevelId + NO_2; level < mNumLevels; level++) {
            if (level >= mInputVersion->GetLevels().size()) {
                LOG_ERROR("Invalid level form mInputVersion, levelId:" << level << ".");
                return false;
            }

            auto fileMetaDataGroups = mInputVersion->GetLevels().at(level).GetFileMetaDataGroups();
            for (auto &fileMetaDataGroup : fileMetaDataGroups) {
                if (!fileMetaDataGroup->GetGroupRange()->Equals(mGroupRange)) {
                    continue;
                }

                std::vector<FileMetaDataRef> files = fileMetaDataGroup->GetFiles();
                while (mLevelPointers[level] < static_cast<int32_t>(files.size())) {
                    FileMetaDataRef f = files.at(mLevelPointers[level]);
                    if (f == nullptr) {
                        LOG_ERROR("FileMetaData is null.");
                        continue;
                    }

                    auto cmpLargest = f->GetLargest()->CompareKey(key);
                    if (cmpLargest >= 0 && f->GetSmallest()->CompareKey(key) <= 0) {
                        return false;
                    }

                    if (cmpLargest > 0) {
                        break;
                    }
                    mLevelPointers[level] = mLevelPointers[level] + 1;
                }
            }
        }
        return true;
    }

    virtual bool ShouldStopBefore(const KeyValueRef &keyValue)
    {
        if (mGrandparents.empty()) {
            return false;
        }

        while (CompareWithLargest(keyValue)) {
            if (mSeenKey && mGrandparents.at(mGrandparentIndex) != nullptr) {
                mOverlappedBytes += mGrandparents.at(mGrandparentIndex)->GetFileSize();
            }
            mGrandparentIndex++;
        }
        mSeenKey = true;
        if (mOverlappedBytes > mInputVersion->GetConf()->GetFileStoreMaxGrandParentOverlapBytes()) {
            mOverlappedBytes = 0L;
            return true;
        }
        return false;
    }

    inline bool CompareWithLargest(const KeyValueRef &keyValue)
    {
        if (mGrandparentIndex >= mGrandparents.size() || mGrandparents.at(mGrandparentIndex) == nullptr) {
            return false;
        }
        auto fullKey = mGrandparents.at(mGrandparentIndex)->GetLargest();
        return fullKey->CompareFullKey(keyValue->key, keyValue->value.SeqId()) < 0;
    }

    virtual VersionPtr Dispose()
    {
        VersionPtr oldVersion = nullptr;
        if (mInputVersion != nullptr) {
            oldVersion = mInputVersion->Release();
            mInputVersion = nullptr;
        }
        return oldVersion;
    }

protected:
    VersionPtr mInputVersion = nullptr;                     // 输入版本
    double mCompactionScore = 0;                            // compaction得分
    Reason mCompactionReason = Reason::NO_NEED_COMPACTION;  // compaction原因
    std::vector<FileMetaDataRef> mLevelInputs;              // 本层的文件
    std::vector<FileMetaDataRef> mOutputLevelInputs;        // 下一层与本层Key范围有重叠的文件
    std::vector<FileMetaDataRef> mMergeLevelInputs;         // 本层仍需逐记录归并的文件
    std::vector<FileMetaDataRef> mMergeOutputLevelInputs;   // 下一层仍需逐记录归并的文件
    std::vector<FileMetaDataRef> mAdoptedFiles;             // 通过元数据直接迁移到下一层的文件
    std::vector<FileMetaDataRef> mGrandparents;             // 父的文件元数据
    uint64_t mMaxFileOutputSize = 0;                        // 最大文件输出大小
    uint32_t mInputLevelId = 0;                             // 输入的Level的ID
    uint32_t mNumLevels = 0;                                // Level数量
    GroupRangeRef mGroupRange = nullptr;                    // 组范围
    VersionEdit::Builder *mEditBuilder = nullptr;           // 版本编辑的构建器
    FullKeyRef mSmallest = nullptr;                         // 最小的内部键
    FullKeyRef mLargest = nullptr;                          // 最大的内部键
    uint32_t mGrandparentIndex = 0;                         // 父的索引
    uint64_t mSeenKey = 0;                                  // 已定位的键
    uint64_t mOverlappedBytes = 0;                          // 重叠的字节数
    std::vector<int32_t> mLevelPointers;                    // Level指针
};
using CompactionRef = std::shared_ptr<Compaction>;

}  // namespace bss
}  // namespace ock
#endif  // BOOST_SS_COMPACTION_H
