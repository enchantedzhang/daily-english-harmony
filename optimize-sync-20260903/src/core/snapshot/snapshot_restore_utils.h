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

#ifndef BOOST_SS_SNAPSHOT_RESTORE_UTILS_H
#define BOOST_SS_SNAPSHOT_RESTORE_UTILS_H

#include <sys/stat.h>

#include <cerrno>
#include <cstring>
#include <iostream>
#include <string>

#include "abstract_snapshot_operator.h"
#include "common/io/file_input_view.h"
#include "common/io/file_output_view.h"
#include "common/util/string_util.h"
#include "kv_table/stateId_provider.h"
#include "lsm_store/file/file_manager.h"
#include "lsm_store/file/file_store_id.h"
#include "lsm_store/file/group_range.h"
#include "restore_db_meta.h"
#include "snapshot_meta.h"
#include "snapshot_stat.h"

namespace ock {
namespace bss {
class SliceTableRestoreMeta {
public:
    SliceTableRestoreMeta(const FileInputViewRef &inputView, const GroupRangeRef &keyGroupRange,
                          int32_t restoredDBMetaVersion)
        : mMetaInputView(inputView), mKeyGroupRange(keyGroupRange), mRestoredDBMetaVersion(restoredDBMetaVersion)
    {
    }

    inline int64_t GetSliceTableMetaOffset() const
    {
        return mSliceTableMetaOffset;
    }

    void GetOverlapFileStoreMetaOffset(FileStoreIDRef &fileStoreID, std::vector<uint64_t> &metaOffsetList)
    {
        std::vector<uint32_t> rescaleMappingFileStoreId = mRescaleMappingInfo[fileStoreID->GetInternalIndex()];
        if (rescaleMappingFileStoreId.empty()) {
            return;
        }
        for (const auto &id : rescaleMappingFileStoreId) {
            int32_t idx = static_cast<int32_t>(id);
            auto iter = mFileStoreMetaMap.find(idx);
            if (iter == mFileStoreMetaMap.end()) {
                LOG_ERROR(idx << " not found at mFileStoreMetaMap.");
                continue;
            }
            uint64_t offset = iter->second;
            metaOffsetList.push_back(offset);
        }
    }

    inline FileInputViewRef GetMetaInputView() const
    {
        return mMetaInputView;
    }

    inline void SetSliceTableMetaOffset(int64_t offset)
    {
        if (mSliceTableMetaOffset != -1L) {
            LOG_ERROR("SliceTableRestoreMeta Bug.");
            return;
        }
        mSliceTableMetaOffset = offset;
    }

    inline void AddFileStoreMeta(const FileStoreIDRef &fileStoreID, uint64_t metaOffset)
    {
        auto iter = mFileStoreMetaMap.find(fileStoreID->GetInternalIndex());
        if (UNLIKELY(iter != mFileStoreMetaMap.end())) {
            LOG_ERROR("Repeated add file store meta, file internal index:" << fileStoreID->GetInternalIndex());
            return;
        }
        mFileStoreMetaMap.emplace(fileStoreID->GetInternalIndex(), metaOffset);
    }

    inline std::unordered_map<uint32_t, std::vector<uint32_t>> &GetRescaleMappingInfo()
    {
        return mRescaleMappingInfo;
    }

    inline void SetBlobServiceMetaOffset(int64_t offset)
    {
        if (mBlobServiceMetaOffset != -1L) {
            LOG_ERROR("BlobServiceMetaOffset Bug.");
            return;
        }
        mBlobServiceMetaOffset = offset;
    }

    inline int64_t GetBlobServiceMetaOffset() const
    {
        return mBlobServiceMetaOffset;
    }

private:
    int64_t mSliceTableMetaOffset = -1;
    std::unordered_map<int32_t, uint64_t> mFileStoreMetaMap;
    int64_t mBlobServiceMetaOffset = -1;
    FileInputViewRef mMetaInputView = nullptr;
    std::unordered_map<uint32_t, std::vector<uint32_t>> mRescaleMappingInfo;
    GroupRangeRef mKeyGroupRange = nullptr;
    int32_t mRestoredDBMetaVersion = 0;
};
using SliceTableRestoreMetaRef = std::shared_ptr<SliceTableRestoreMeta>;

class SnapshotMetaTail {
public:
    SnapshotMetaTail(uint32_t snapshotVersion, uint64_t snapshotId, uint32_t startKeyGroup, uint32_t endKeyGroup,
                     uint64_t seqId, uint64_t snapshotOperatorInfoOffset, uint32_t numberOfSnapshotOperators,
                     uint64_t localFileMappingOffset, uint64_t remoteFileMappingOffset, uint64_t stateIdOffset)
        : mSnapshotVersion(snapshotVersion),
          mSnapshotId(snapshotId),
          mStartKeyGroup(startKeyGroup),
          mEndKeyGroup(endKeyGroup),
          mSeqId(seqId),
          mSnapshotOperatorInfoOffset(snapshotOperatorInfoOffset),
          mNumberOfSnapshotOperators(numberOfSnapshotOperators),
          mLocalFileMappingOffset(localFileMappingOffset),
          mRemoteFileMappingOffset(remoteFileMappingOffset),
          mStateIdOffset(stateIdOffset)
    {
    }

    inline uint32_t GetSnapshotVersion() const
    {
        return mSnapshotVersion;
    }

    inline uint64_t GetSnapshotId() const
    {
        return mSnapshotId;
    }

    inline uint32_t GetStartKeyGroup() const
    {
        return mStartKeyGroup;
    }

    inline uint32_t GetEndKeyGroup() const
    {
        return mEndKeyGroup;
    }

    inline uint64_t GetSeqId() const
    {
        return mSeqId;
    }

    inline uint64_t GetSnapshotOperatorInfoOffset() const
    {
        return mSnapshotOperatorInfoOffset;
    }

    inline uint32_t GetNumberOfSnapshotOperators() const
    {
        return mNumberOfSnapshotOperators;
    }

    inline uint64_t GetLocalFileMappingOffset() const
    {
        return mLocalFileMappingOffset;
    }

    inline uint64_t GetStateIdOffset() const
    {
        return mStateIdOffset;
    }

private:
    uint32_t mSnapshotVersion;
    uint64_t mSnapshotId;
    uint32_t mStartKeyGroup;
    uint32_t mEndKeyGroup;
    uint64_t mSeqId;
    uint64_t mSnapshotOperatorInfoOffset;
    uint32_t mNumberOfSnapshotOperators;
    uint64_t mLocalFileMappingOffset;
    uint64_t mRemoteFileMappingOffset;
    uint64_t mStateIdOffset;
};

class SnapshotMetaFileInfo {
public:
    SnapshotMetaFileInfo(const PathRef &filePath, uint64_t snapshotId) : mFilePath(filePath), mSnapshotId(snapshotId)
    {
    }

    inline PathRef GetFilePath() const
    {
        return mFilePath;
    }

    inline uint64_t GetSnapshotId() const
    {
        return mSnapshotId;
    }

private:
    PathRef mFilePath = nullptr;
    uint64_t mSnapshotId = 0;
};
using SnapshotMetaFileInfoRef = std::shared_ptr<SnapshotMetaFileInfo>;

using SnapshotMetaTailRef = std::shared_ptr<SnapshotMetaTail>;

class RestoredDbMeta {
public:
    class RestoredSnapshotOperatorInfo {
    public:
        RestoredSnapshotOperatorInfo(const SnapshotOperatorInfoRef &snapshotOperatorInfo,
                                     uint64_t snapshotOperatorMetaOffset)
            : mSnapshotOperatorInfo(snapshotOperatorInfo), mSnapshotOperatorMetaOffset(snapshotOperatorMetaOffset)
        {
        }

        inline SnapshotOperatorInfoRef GetSnapshotOperatorInfo() const
        {
            return mSnapshotOperatorInfo;
        }

        inline uint64_t GetSnapshotOperatorMetaOffset() const
        {
            return mSnapshotOperatorMetaOffset;
        }

    private:
        SnapshotOperatorInfoRef mSnapshotOperatorInfo = nullptr;
        uint64_t mSnapshotOperatorMetaOffset = 0;
    };
    using RestoredSnapshotOperatorInfoRef = std::shared_ptr<RestoredSnapshotOperatorInfo>;

    RestoredDbMeta(const PathRef &snapshotMetaPath, const FileInputViewRef &snapshotMetaInputView,
                   const SnapshotMetaTailRef &mSnapshotMetaTail,
                   std::vector<RestoredSnapshotOperatorInfoRef> &restoredSnapshotOperatorsInfos,
                   const SnapshotFileMappingRef localFileMapping, const SnapshotFileMappingRef &remoteFileMapping)
        : mSnapshotMetaPath(snapshotMetaPath),
          mSnapshotMetaInputView(snapshotMetaInputView),
          mSnapshotMetaTail(mSnapshotMetaTail),
          mRestoredSnapshotOperatorsInfos(restoredSnapshotOperatorsInfos),
          mLocalFileMapping(localFileMapping),
          mRemoteFileMapping(remoteFileMapping)
    {
    }

    inline FileInputViewRef GetSnapshotMetaInputView() const
    {
        return mSnapshotMetaInputView;
    }

    inline uint32_t GetSnapshotVersion() const
    {
        return mSnapshotMetaTail->GetSnapshotVersion();
    }

    inline uint64_t GetSnapshotId() const
    {
        return mSnapshotMetaTail->GetSnapshotId();
    }

    inline uint32_t GetStartKeyGroup() const
    {
        return mSnapshotMetaTail->GetStartKeyGroup();
    }

    inline uint32_t GetEndKeyGroup() const
    {
        return mSnapshotMetaTail->GetEndKeyGroup();
    }

    inline uint64_t GetSeqId() const
    {
        return mSnapshotMetaTail->GetSeqId();
    }

    inline uint64_t GetStateIdOffset() const
    {
        return mSnapshotMetaTail->GetStateIdOffset();
    }

    inline std::vector<RestoredSnapshotOperatorInfoRef> GetRestoredSnapshotOperatorInfos() const
    {
        return mRestoredSnapshotOperatorsInfos;
    }

    inline SnapshotFileMappingRef GetLocalFileMapping() const
    {
        return mLocalFileMapping;
    }

private:
    PathRef mSnapshotMetaPath = nullptr;
    FileInputViewRef mSnapshotMetaInputView = nullptr;
    SnapshotMetaTailRef mSnapshotMetaTail = nullptr;
    std::vector<RestoredSnapshotOperatorInfoRef> mRestoredSnapshotOperatorsInfos;
    SnapshotFileMappingRef mLocalFileMapping = nullptr;
    SnapshotFileMappingRef mRemoteFileMapping = nullptr;
};
using RestoredDbMetaRef = std::shared_ptr<RestoredDbMeta>;

class SnapshotOperatorMeta {
public:
    AbstractSnapshotOperatorRef mSnapshotOperator = nullptr;
    int64_t mLocalMetaOffset;
    int64_t mRemoteMetaOffset;
    SnapshotMetaRef mSnapshotMeta = nullptr;
};
using SnapshotOperatorMetaRef = std::shared_ptr<SnapshotOperatorMeta>;

class FileMappingInfo {
public:
    uint64_t mLocalFileMappingOffset = 0;
    uint64_t mRemoteFileMappingOffset = 0;
};
using FileMappingInfoRef = std::shared_ptr<FileMappingInfo>;

class SnapshotMetaAndRemoteFileMapping {
public:
    SnapshotMetaAndRemoteFileMapping(const PathRef &snapshotMetaPath, const SnapshotMetaTailRef &snapshotMetaTail,
                                     const SnapshotFileMappingRef &remoteFileMapping)
        : mSnapshotMetaPath(snapshotMetaPath),
          mSnapshotMetaTail(snapshotMetaTail),
          mRemoteFileMapping(remoteFileMapping)
    {
    }

private:
    PathRef mSnapshotMetaPath;
    SnapshotMetaTailRef mSnapshotMetaTail;
    SnapshotFileMappingRef mRemoteFileMapping;
};

class SnapshotMetaFileParser;
using SnapshotMetaFileParserRef = std::shared_ptr<SnapshotMetaFileParser>;
class SnapshotMetaFileParser {
public:
    SnapshotMetaFileParser(std::string filePath, bool isSnapshotMetaFile, int64_t snapshotId, bool isTemp)
        : mFilePath(filePath), mIsSnapshotMetaFile(isSnapshotMetaFile), mSnapshotId(snapshotId), mIsTemp(isTemp)
    {
    }

    static SnapshotMetaFileParserRef Build(const std::string &snapshotMetaPath)
    {
        auto lastSlashPos = snapshotMetaPath.rfind("/");
        if (lastSlashPos == std::string::npos) {
            // 这里打印的异常值，字符串中都不包含斜线，所以不涉及打印文件全路径
            LOG_ERROR("Unexpected: can not find slash in invalid param: " << snapshotMetaPath);
            return nullptr;
        }
        auto fileName = snapshotMetaPath.substr(lastSlashPos + 1);
        std::vector<std::string> splits = StringUtil::Split(fileName, "-");
        if (splits.empty()) {
            LOG_ERROR("Unexpected: the split result is empty, file name:" << fileName);
            return nullptr;
        }
        if (splits.size() == NO_3 && "_tmp" == splits[0] && "snapshot" == splits[1] && isValidInt64(splits[NO_2])) {
            int64_t snapshotId = std::stoll(splits[NO_2]);
            snapshotId = snapshotId >= 0 ? snapshotId : -1L;
            if (snapshotId >= 0L) {
                return std::make_shared<SnapshotMetaFileParser>(snapshotMetaPath, true, snapshotId, true);
            }
        }

        if (splits.size() == NO_2 && "snapshot" == splits[0] && isValidInt64(splits[1])) {
            int64_t snapshotId = std::stoll(splits[1]);
            snapshotId = snapshotId >= 0 ? snapshotId : -1L;
            if (snapshotId >= 0L) {
                return std::make_shared<SnapshotMetaFileParser>(snapshotMetaPath, true, snapshotId, true);
            }
        }
        return std::make_shared<SnapshotMetaFileParser>(snapshotMetaPath, false, -1L, false);
    }

    static bool isValidInt64(const std::string &str)
    {
        if (str.empty()) {
            return false;
        }

        size_t start = 0;
        if (str[0] == '-' || str[0] == '+') {
            if (str.size() == 1) {
                return false;
            }
            start = 1;
        }

        for (size_t i = start; i < str.size(); ++i) {
            if (!std::isdigit(str[i])) {
                return false;
            }
        }

        try {
            std::stoll(str);
        } catch (...) {
            return false;
        }

        return true;
    }

    inline std::string GetFilePath() const
    {
        return mFilePath;
    }

    inline int64_t GetSnapshotId() const
    {
        return mSnapshotId;
    }

private:
    std::string mFilePath;
    bool mIsSnapshotMetaFile;
    int64_t mSnapshotId;
    bool mIsTemp;
};
using SnapshotMetaAndRemoteFileMappingRef = std::shared_ptr<SnapshotMetaAndRemoteFileMapping>;

class SnapshotRestoreUtils {
public:
    static void WriteSnapshotOperatorInfo(const FileOutputViewRef &localOutputView,
                                          const std::vector<SnapshotOperatorMetaRef> &snapshotOperatorMetas);

    static SnapshotOperatorInfoRef Deserialize(const FileInputViewRef &inputView, SnapshotOperatorType operatorType);

    static std::vector<RestoredDbMeta::RestoredSnapshotOperatorInfoRef> ReadSnapshotOperatorInfo(
        const FileInputViewRef &inputView, uint64_t snapshotOperatorInfoOffset, uint32_t numberOfSnapshotOperators);

    static inline FileMappingInfoRef WriteFileMappingMeta(
        const FileOutputViewRef &outputView, const PathRef &localSnapshotPath, const FileManagerRef &localFileManager,
        std::unordered_set<PathRef, PathHash, PathEqual> &localFilePaths, std::vector<uint32_t> localFileIds)
    {
        uint64_t localFileMappingOffset = WriteFileMapping(outputView, localSnapshotPath, localFileManager,
                                                           localFilePaths, localFileIds);
        FileMappingInfoRef fileMappingInfo = std::make_shared<FileMappingInfo>();
        fileMappingInfo->mLocalFileMappingOffset = localFileMappingOffset;
        return fileMappingInfo;
    }

    static uint64_t WriteFileMapping(const FileOutputViewRef &outputView, const PathRef &snapshotBasePath,
                                     const FileManagerRef &fileManager,
                                     std::unordered_set<PathRef, PathHash, PathEqual> &filePaths,
                                     std::vector<uint32_t> &localFileIds);

    static SnapshotFileMappingRef ReadFileMapping(const FileInputViewRef &inputView, uint64_t fileMappingOffset);

    static void WriteSnapshotMetaTail(const FileOutputViewRef &outputView, uint64_t snapshotId, uint32_t startKeyGroup,
                                      uint32_t endKeyGroup, uint32_t seqId, uint32_t snapshotOperatorInfoOffset,
                                      uint32_t numberOfSnapshotOperators, const FileMappingInfoRef &fileMappingInfo,
                                      uint64_t stateIdProviderOffset);

    static SnapshotMetaTailRef ReadSnapshotMetaTail(const PathRef &snapshotMetaPath,
                                                    const FileInputViewRef &snapshotMetaInputView);

    static SnapshotMetaRef WriteDbMeta(uint64_t snapshotId, uint64_t startKeyGroup, uint64_t endKeyGroup,
                                       uint64_t seqId, const StateIdProviderRef &mStateIdProvider,
                                       const PathRef &localSnapshotPath, const FileOutputViewRef &localOutputView,
                                       const FileManagerRef &localFileManager,
                                       std::vector<AbstractSnapshotOperatorRef> &snapshotOperators,
                                       const SnapshotStatRef &snapshotStat);

    static RestoredDbMetaRef ReadDbMeta(const PathRef &restoredMetaPath);

    static std::vector<SnapshotFileMappingRef> RelocateLocalFileMappings(
        const PathRef &relocateBasePath, std::vector<SnapshotFileMappingRef> &restoredLocalFileMappings);
};

}  // namespace bss
}  // namespace ock
#endif  // BOOST_SS_SNAPSHOT_RESTORE_UTILS_H