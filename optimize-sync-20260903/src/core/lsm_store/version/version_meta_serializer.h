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

#ifndef BOOST_SS_VERSION_META_SERIALIZER_H
#define BOOST_SS_VERSION_META_SERIALIZER_H

#include "include/bss_err.h"
#include "lsm_store/file/file_address_util.h"
#include "lsm_store/file/file_cache_type.h"
#include "lsm_store/file/file_factory.h"
#include "snapshot/snapshot_meta.h"
#include "version.h"
#include "version_set.h"

namespace ock {
namespace bss {
struct RestoreFilePath {
    std::string checkpointPath;
    std::string restorePath;
    std::string remotePath;
    FileStatus fileStatus;
};

class VersionMetaSerializer {
public:
    static SnapshotMetaRef Serialize(VersionPtr version, FileFactoryRef fileFactory, OutputViewRef &primaryOutputView,
                                     uint64_t fileSeqId)
    {
        std::vector<uint32_t> localFiles;
        uint64_t localFullSize = 0;
        auto function = [](OutputViewRef &outputView, uint8_t *data, uint32_t len) {
            return outputView->Write(data, len);
        };
        std::vector<OutputViewRef> outputViews = { primaryOutputView };
        int32_t magicNumber = MAGIC_NUMBER;
        RETURN_NULLPTR_AS_NOT_OK(
            WriteFunc(outputViews, function, reinterpret_cast<uint8_t *>(&magicNumber), sizeof(magicNumber)));

        uint32_t primaryFileStatusVersion = PRIMARY_FILE_STATUS_VERSION;
        RETURN_NULLPTR_AS_NOT_OK(WriteFunc(outputViews, function,
                                           reinterpret_cast<uint8_t *>(&primaryFileStatusVersion),
                                           sizeof(primaryFileStatusVersion)));

        RETURN_NULLPTR_AS_NOT_OK(
            WriteFunc(outputViews, function, reinterpret_cast<uint8_t *>(&fileSeqId), sizeof(fileSeqId)));

        auto factoryType = static_cast<uint8_t>(fileFactory->GetFileFactoryType());
        RETURN_NULLPTR_AS_NOT_OK(
            WriteFunc(outputViews, function, reinterpret_cast<uint8_t *>(&factoryType), sizeof(factoryType)));

        SerializeGroupRange(version->GetGroupRange(), outputViews, function);

        std::vector<Level> levels = version->GetLevels();
        WriteLevel(levels, outputViews, function, localFiles, localFullSize);
        return std::make_shared<SnapshotMeta>(localFiles, localFullSize, localFullSize);
    }

    static BResult WriteFunc(std::vector<OutputViewRef> &outputViews,
                             const std::function<BResult(OutputViewRef &, uint8_t *, uint32_t)> &func, uint8_t *data,
                             uint32_t len)
    {
        BResult ret = BSS_OK;
        for (auto &outputView : outputViews) {
            ret = func(outputView, data, len);
            if (ret != BSS_OK) {
                LOG_ERROR("Write output view failed, ret:" << ret);
                return ret;
            }
        }
        return ret;
    }

    static BResult SerializeGroupRange(GroupRangeRef groupRange, std::vector<OutputViewRef> &outputViews,
                                       std::function<BResult(OutputViewRef &, uint8_t *, uint32_t)> function)
    {
        int64_t epoch = groupRange->GetEpoch();
        int32_t startGroup = groupRange->GetStartGroup();
        int32_t endGroup = groupRange->GetEndGroup();
        RETURN_NOT_OK(WriteFunc(outputViews, function, reinterpret_cast<uint8_t *>(&epoch), sizeof(epoch)));
        RETURN_NOT_OK(WriteFunc(outputViews, function, reinterpret_cast<uint8_t *>(&startGroup), sizeof(startGroup)));
        RETURN_NOT_OK(WriteFunc(outputViews, function, reinterpret_cast<uint8_t *>(&endGroup), sizeof(endGroup)));
        return BSS_OK;
    }

    static BResult WriteLevel(std::vector<Level> levels, std::vector<OutputViewRef> &outputViews,
                              std::function<BResult(OutputViewRef &, uint8_t *, uint32_t)> function,
                              std::vector<uint32_t> &localFiles, uint64_t &localFullSize)
    {
        uint32_t numLevels = levels.size();
        RETURN_NOT_OK(WriteFunc(outputViews, function, reinterpret_cast<uint8_t *>(&numLevels), sizeof(numLevels)));

        for (Level level : levels) {
            auto levelId = level.GetLevelId();
            RETURN_NOT_OK(WriteFunc(outputViews, function, reinterpret_cast<uint8_t *>(&levelId), sizeof(levelId)));
            std::vector<FileMetaDataGroupRef> fileMetaGroups = level.GetFileMetaDataGroups();
            RETURN_NOT_OK(WriteFileMetaGroup(fileMetaGroups, outputViews, function, localFiles, localFullSize));
        }
        return BSS_OK;
    }

    static BResult WriteFileMetaGroup(std::vector<FileMetaDataGroupRef> fileMetaGroups,
                                      std::vector<OutputViewRef> &outputViews,
                                      std::function<BResult(OutputViewRef &, uint8_t *, uint32_t)> function,
                                      std::vector<uint32_t> &localFiles, uint64_t &localFullSize)
    {
        uint32_t fileMetaSize = fileMetaGroups.size();
        RETURN_NOT_OK(
            WriteFunc(outputViews, function, reinterpret_cast<uint8_t *>(&fileMetaSize), sizeof(fileMetaSize)));
        for (auto &fileMetaGroup : fileMetaGroups) {
            bool isOverlapping = fileMetaGroup->IsOverlapping();
            RETURN_NOT_OK(
                WriteFunc(outputViews, function, reinterpret_cast<uint8_t *>(&isOverlapping), sizeof(isOverlapping)));

            RETURN_NOT_OK(SerializeGroupRange(fileMetaGroup->GetGroupRange(), outputViews, function));
            std::vector<FileMetaDataRef> fileMetaData = fileMetaGroup->GetFiles();
            RETURN_NOT_OK(WriteFileMetaData(fileMetaData, outputViews, function, localFiles, localFullSize));
        }
        return BSS_OK;
    }

    static BResult WriteFileMetaData(std::vector<FileMetaDataRef> fileMetaData, std::vector<OutputViewRef> &outputViews,
                                     std::function<BResult(OutputViewRef &, uint8_t *, uint32_t)> function,
                                     std::vector<uint32_t> &localFiles, uint64_t &localFullSize)
    {
        uint32_t fileMetaDataSize = fileMetaData.size();
        RETURN_NOT_OK(
            WriteFunc(outputViews, function, reinterpret_cast<uint8_t *>(&fileMetaDataSize), sizeof(fileMetaDataSize)));
        for (auto &fileMeta : fileMetaData) {
            uint64_t fileAddress = fileMeta->GetFileAddress();

            uint8_t fileStatus = fileMeta->GetFileStatus();
            localFullSize += fileMeta->GetFileSize();
            if (fileStatus == FileStatus::LOCAL) {
                localFiles.push_back(FileAddressUtil::GetFileId(fileAddress));
            }
            RETURN_NOT_OK(
                WriteFunc(outputViews, function, reinterpret_cast<uint8_t *>(&fileAddress), sizeof(fileAddress)));

            RETURN_NOT_OK(
                WriteFunc(outputViews, function, reinterpret_cast<uint8_t *>(&fileStatus), sizeof(fileStatus)));

            uint64_t seqId = static_cast<uint64_t>(fileMeta->GetSeqId());
            RETURN_NOT_OK(WriteFunc(outputViews, function, reinterpret_cast<uint8_t *>(&seqId), sizeof(seqId)));

            uint64_t fileSize = fileMeta->GetFileSize();
            RETURN_NOT_OK(WriteFunc(outputViews, function, reinterpret_cast<uint8_t *>(&fileSize), sizeof(fileSize)));

            FullKeyRef smallest = fileMeta->GetSmallest();
            RETURN_NOT_OK(SerializeInternalKey(smallest, outputViews));

            FullKeyRef largest = fileMeta->GetLargest();
            RETURN_NOT_OK(SerializeInternalKey(largest, outputViews));

            RETURN_NOT_OK(SerializeUTF(fileMeta->GetIdentifier(), outputViews));

            RETURN_NOT_OK(SerializeOrderRange(fileMeta->GetOrderRange(), outputViews, function));

            auto stateIdInterval = fileMeta->GetStateIdInterval();
            RETURN_NOT_OK(WriteFunc(outputViews, function, reinterpret_cast<uint8_t *>(&stateIdInterval),
                                    sizeof(stateIdInterval)));

            uint32_t recordCount = 0;
            uint64_t minRecordSeqId = 0;
            uint64_t maxRecordSeqId = 0;
            uint8_t singleValueType = static_cast<uint8_t>(ValueType::TYPE_BUTT);
            uint32_t singleValueLength = 0;
            BuildPersistentRecordMeta(fileMeta->GetRecordMeta(), smallest, largest, recordCount, minRecordSeqId,
                                      maxRecordSeqId, singleValueType, singleValueLength);
            RETURN_NOT_OK(
                WriteFunc(outputViews, function, reinterpret_cast<uint8_t *>(&recordCount), sizeof(recordCount)));
            RETURN_NOT_OK(
                WriteFunc(outputViews, function, reinterpret_cast<uint8_t *>(&minRecordSeqId), sizeof(minRecordSeqId)));
            RETURN_NOT_OK(
                WriteFunc(outputViews, function, reinterpret_cast<uint8_t *>(&maxRecordSeqId), sizeof(maxRecordSeqId)));
            RETURN_NOT_OK(WriteFunc(outputViews, function, reinterpret_cast<uint8_t *>(&singleValueType),
                                    sizeof(singleValueType)));
            RETURN_NOT_OK(WriteFunc(outputViews, function, reinterpret_cast<uint8_t *>(&singleValueLength),
                                    sizeof(singleValueLength)));
        }
        return BSS_OK;
    }

    static void BuildPersistentRecordMeta(const FileRecordMeta &recordMeta, const FullKeyRef &smallest,
                                          const FullKeyRef &largest, uint32_t &recordCount, uint64_t &minRecordSeqId,
                                          uint64_t &maxRecordSeqId, uint8_t &singleValueType,
                                          uint32_t &singleValueLength)
    {
        // Keep the version-7 wire layout. Fields no longer retained at runtime
        // are derived for a single record and use conservative placeholders for
        // multi-record or unavailable summaries.
        recordCount = 0;
        minRecordSeqId = 0;
        maxRecordSeqId = 0;
        singleValueType = static_cast<uint8_t>(ValueType::TYPE_BUTT);
        singleValueLength = 0;
        if (!IsRuntimeRecordMetaValid(recordMeta, smallest, largest)) {
            return;
        }

        recordCount = recordMeta.GetRecordCount();
        maxRecordSeqId = recordMeta.GetMaxSeqId();
        if (recordMeta.IsSingleRecord()) {
            minRecordSeqId = maxRecordSeqId;
            singleValueType = static_cast<uint8_t>(smallest->ValueType());
            singleValueLength = recordMeta.GetSingleValueLength();
        }
    }

    static bool IsRuntimeRecordMetaValid(const FileRecordMeta &recordMeta, const FullKeyRef &smallest,
                                         const FullKeyRef &largest)
    {
        if (!recordMeta.IsAvailable() || smallest == nullptr || largest == nullptr ||
            recordMeta.GetMaxSeqId() < smallest->SeqId() || recordMeta.GetMaxSeqId() < largest->SeqId()) {
            return false;
        }
        return !recordMeta.IsSingleRecord() ||
               (smallest->EqualsFullKey(largest) && recordMeta.GetMaxSeqId() == smallest->SeqId());
    }

    static FileRecordMeta RestoreRecordMeta(uint32_t recordCount, uint64_t minRecordSeqId, uint64_t maxRecordSeqId,
                                            uint8_t singleValueType, uint32_t singleValueLength,
                                            const FullKeyRef &smallest, const FullKeyRef &largest)
    {
        if (recordCount == 0) {
            return FileRecordMeta();
        }

        bool valid = smallest != nullptr && largest != nullptr && minRecordSeqId <= maxRecordSeqId &&
                     minRecordSeqId <= smallest->SeqId() && minRecordSeqId <= largest->SeqId() &&
                     maxRecordSeqId >= smallest->SeqId() && maxRecordSeqId >= largest->SeqId();
        if (recordCount == 1) {
            valid = valid && smallest->EqualsFullKey(largest) && minRecordSeqId == maxRecordSeqId &&
                    maxRecordSeqId == smallest->SeqId() && singleValueType == smallest->ValueType();
        } else {
            valid = valid && singleValueType == static_cast<uint8_t>(ValueType::TYPE_BUTT) && singleValueLength == 0;
        }
        if (!valid) {
            // The summary is an optional optimization hint. Invalid hints must
            // not weaken adoption checks or prevent an otherwise valid restore.
            LOG_WARN("Restored file record summary is inconsistent; disable file reuse for this file.");
            return FileRecordMeta();
        }
        return FileRecordMeta(recordCount, maxRecordSeqId, recordCount == 1 ? singleValueLength : 0);
    }

    static BResult SerializeInternalKey(const FullKeyRef &key, std::vector<OutputViewRef> &outputViews)
    {
        for (auto &outputView : outputViews) {
            RETURN_NOT_OK(FullKeyUtil::WriteInternalKey(key, outputView));
        }
        return BSS_OK;
    }

    static BResult SerializeUTF(std::string str, std::vector<OutputViewRef> &outputViews)
    {
        for (auto &outputView : outputViews) {
            RETURN_NOT_OK(outputView->WriteUTF(str));
        }
        return BSS_OK;
    }

    static BResult SerializeOrderRange(HashCodeOrderRangeRef orderRange, std::vector<OutputViewRef> &outputViews,
                                       std::function<BResult(OutputViewRef &, uint8_t *, uint32_t)> function)
    {
        auto orderRangeType = static_cast<uint8_t>(orderRange->GetType());
        RETURN_NOT_OK(
            WriteFunc(outputViews, function, reinterpret_cast<uint8_t *>(&orderRangeType), sizeof(orderRangeType)));

        uint32_t startHashCode = orderRange->GetStartHashCode();
        RETURN_NOT_OK(
            WriteFunc(outputViews, function, reinterpret_cast<uint8_t *>(&startHashCode), sizeof(startHashCode)));

        uint32_t endHashCode = orderRange->GetEndHashCode();
        RETURN_NOT_OK(WriteFunc(outputViews, function, reinterpret_cast<uint8_t *>(&endHashCode), sizeof(endHashCode)));

        uint8_t hasRedundantData = orderRange->HasRedundantData() ? 1 : 0;
        RETURN_NOT_OK(
            WriteFunc(outputViews, function, reinterpret_cast<uint8_t *>(&hasRedundantData), sizeof(hasRedundantData)));
        return BSS_OK;
    }

    static BResult Deserialize(const FileInputViewRef &inputView, const VersionSetRef &versionSet,
                               const HashCodeOrderRangeRef &hashCodeOrderRange, VersionPtr &version,
                               uint64_t &fileSeqId, std::unordered_map<std::string, RestoreFileInfo> &fileMapping,
                               const FileFactoryType &fileFactoryType, const MemManagerRef &memManager,
                               const std::unordered_map<std::string, std::string> &lazyPathMapping,
                               std::unordered_map<std::string, uint32_t> &restorePathFileIdMap, PathRef &basePath,
                               bool isLazyDownload)
    {
        int32_t magicNum = 0;
        RETURN_NOT_OK_AS_READ_ERROR(inputView->Read(magicNum));
        if (magicNum != MAGIC_NUMBER) {
            LOG_ERROR("Bad magic num, expect:" << MAGIC_NUMBER << ", actual:" << magicNum);
            return BSS_ERR;
        }

        uint32_t restoreVersion = 0;
        RETURN_NOT_OK_AS_READ_ERROR(inputView->Read(restoreVersion));
        if (restoreVersion != PRIMARY_FILE_STATUS_VERSION) {
            LOG_ERROR("Bad version, expect version:" << PRIMARY_FILE_STATUS_VERSION << ", actual:" << restoreVersion);
            return BSS_ERR;
        }

        RETURN_NOT_OK_AS_READ_ERROR(inputView->Read(fileSeqId));
        uint8_t type = 0;
        RETURN_NOT_OK_AS_READ_ERROR(inputView->Read(type));
        if (type != (uint8_t)fileFactoryType) {
            LOG_ERROR("Bad file factory type, expect:" << (uint8_t)fileFactoryType << ", actual:" << type);
            return BSS_ERR;
        }

        GroupRangeRef curGroupRange = DeserializeGroupRange(inputView);
        VersionBuilderRef builder = VersionBuilder::NewBuilder(versionSet.get());
        RETURN_NOT_OK(DeserializeLevel(builder, fileMapping, inputView, memManager, lazyPathMapping,
                                       restorePathFileIdMap, basePath, isLazyDownload));
        builder->SetCurGroupRange(curGroupRange)->SetCurOrderRange(hashCodeOrderRange);
        version = builder->Build();
        return BSS_OK;
    }

    static GroupRangeRef DeserializeGroupRange(const FileInputViewRef &inputView)
    {
        int64_t epoch = 0;
        RETURN_NULLPTR_AS_READ_ERROR(inputView->Read(epoch));
        int32_t staGroup = 0;
        RETURN_NULLPTR_AS_READ_ERROR(inputView->Read(staGroup));
        int32_t endGroup = 0;
        RETURN_NULLPTR_AS_READ_ERROR(inputView->Read(endGroup));
        return std::make_shared<GroupRange>(staGroup, endGroup, epoch);
    }

    static BResult DeserializeLevel(VersionBuilderRef &builder,
                                    std::unordered_map<std::string, RestoreFileInfo> &fileMapping,
                                    const FileInputViewRef &inputView, const MemManagerRef &memManager,
                                    const std::unordered_map<std::string, std::string> &lazyPathMapping,
                                    std::unordered_map<std::string, uint32_t> &restorePathFileIdMap, PathRef &basePath,
                                    bool isLazyDownload)
    {
        uint32_t levelSize = 0;
        RETURN_NOT_OK(inputView->Read(levelSize));
        for (uint32_t i = 0; i < levelSize; i++) {
            int32_t levelId = 0;
            RETURN_NOT_OK(inputView->Read(levelId));
            auto levelBuilder = LevelBuilder::NewBuilder(levelId);
            RETURN_NOT_OK(DeserializeFileMetaGroup(levelBuilder, fileMapping, inputView, memManager, lazyPathMapping,
                                                   restorePathFileIdMap, basePath, isLazyDownload));
            builder->AddLevel(levelId, levelBuilder->Build());
        }
        return BSS_OK;
    }

    static BResult DeserializeFileMetaGroup(const LevelBuilderRef &builder,
                                            std::unordered_map<std::string, RestoreFileInfo> &fileMapping,
                                            const FileInputViewRef &inputView, const MemManagerRef &memManager,
                                            const std::unordered_map<std::string, std::string> &lazyPathMapping,
                                            std::unordered_map<std::string, uint32_t> &restorePathFileIdMap,
                                            PathRef &basePath, bool isLazyDownload)
    {
        uint32_t fileMetaDataGroupSize = 0;
        RETURN_NOT_OK(inputView->Read(fileMetaDataGroupSize));
        for (uint32_t i = 0; i < fileMetaDataGroupSize; i++) {
            bool isOverlapping = false;
            RETURN_NOT_OK(inputView->Read(isOverlapping));
            GroupRangeRef groupRange = DeserializeGroupRange(inputView);
            auto fileMetaGroupBuilder = FileDataGroupBuilder::NewBuilder();
            fileMetaGroupBuilder->SetGroupRange(groupRange);
            fileMetaGroupBuilder->SetOverlapping(isOverlapping);
            RETURN_NOT_OK(DeserializeFile(fileMetaGroupBuilder, fileMapping, inputView, groupRange, memManager,
                                          lazyPathMapping, restorePathFileIdMap, basePath, isLazyDownload));
            FileMetaDataGroupRef fileMetaDataGroup = fileMetaGroupBuilder->Build();
            builder->AddFileMetaDataGroup(fileMetaDataGroup);
        }
        return BSS_OK;
    }

    static BResult DeserializeFile(FileDataGroupBuilderRef &builder,
                                   std::unordered_map<std::string, RestoreFileInfo> &fileMapping,
                                   const FileInputViewRef &inputView, GroupRangeRef groupRange,
                                   const MemManagerRef &memManager,
                                   const std::unordered_map<std::string, std::string> &lazyPathMapping,
                                   std::unordered_map<std::string, uint32_t> &restorePathFileIdMap, PathRef &basePath,
                                   bool isLazyDownload)
    {
        uint32_t fileCount = 0;
        RETURN_NOT_OK(inputView->Read(fileCount));
        for (uint32_t i = 0; i < fileCount; i++) {
            uint64_t fileAddress = 0;
            RETURN_NOT_OK(inputView->Read(fileAddress));
            uint8_t fileStatus = FileStatus::LOCAL;
            RETURN_NOT_OK(inputView->Read(fileStatus));
            uint64_t seqId = 0;
            RETURN_NOT_OK(inputView->Read(seqId));
            uint64_t fileSize = 0;
            RETURN_NOT_OK(inputView->Read(fileSize));
            FullKeyRef smallest = FullKeyUtil::ReadInternalKey(inputView, memManager,
                                                               FileProcHolder::FILE_STORE_ITERATOR);
            FullKeyRef largest = FullKeyUtil::ReadInternalKey(inputView, memManager,
                                                              FileProcHolder::FILE_STORE_ITERATOR);
            std::string fileName;
            RETURN_NOT_OK(inputView->ReadUTF(fileName));
            HashCodeOrderRangeRef orderRange = DeserializeOrderRange(inputView);
            RETURN_ERROR_AS_NULLPTR(orderRange);

            StateIdInterval stateIdInterval;
            RETURN_NOT_OK(inputView->Read(stateIdInterval));
            uint32_t recordCount = 0;
            RETURN_NOT_OK(inputView->Read(recordCount));
            uint64_t minRecordSeqId = 0;
            RETURN_NOT_OK(inputView->Read(minRecordSeqId));
            uint64_t maxRecordSeqId = 0;
            RETURN_NOT_OK(inputView->Read(maxRecordSeqId));
            uint8_t singleValueType = static_cast<uint8_t>(ValueType::TYPE_BUTT);
            RETURN_NOT_OK(inputView->Read(singleValueType));
            uint32_t singleValueLength = 0;
            RETURN_NOT_OK(inputView->Read(singleValueLength));
            FileRecordMeta recordMeta = RestoreRecordMeta(recordCount, minRecordSeqId, maxRecordSeqId, singleValueType,
                                                          singleValueLength, smallest, largest);
            auto restoreFilePath = FindFileMapping(fileName, lazyPathMapping, static_cast<FileStatus>(fileStatus),
                                                   isLazyDownload);
            auto it = restorePathFileIdMap.find(PathTransform::ExtractFileName(fileName));
            if (it == restorePathFileIdMap.end()) {
                LOG_ERROR("Deserialize file from meta not find in restore path fileId path.");
                return BSS_ERR;
            }
            // 如果是remote文件并且还未恢复，先在本次工作目录创建同名空文件，在cp时上传同名文件保证remote文件的引用计数+1
            if (restoreFilePath.fileStatus == FileStatus::DFS) {
                PathRef localPath = std::make_shared<Path>(basePath, PathTransform::ExtractFileName(fileName));
                restoreFilePath.restorePath = localPath->Name();
                if (!localPath->Existed()) {
                    FileInputViewRef fileInputView = std::make_shared<FileInputView>();
                    fileInputView->Init(FileSystemType::LOCAL, localPath);
                }
            }
            uint32_t fileId = it->second;
            fileAddress = FileAddressUtil::GetFileAddressWithZeroOffset(fileId);
            FileMetaDataRef fileMetaData = std::make_shared<FileMetaData>(fileAddress, seqId, fileSize, smallest,
                                                                          largest, groupRange, orderRange,
                                                                          restoreFilePath.remotePath, stateIdInterval,
                                                                          restoreFilePath.fileStatus, recordMeta);
            builder->AddFileMeta(fileMetaData);
            RestoreFileInfo restoreFileInfo = {
                fileName, fileAddress, restoreFilePath.fileStatus, fileSize,
                1,        fileAddress, restoreFilePath.remotePath, restoreFilePath.restorePath
            };
            LOG_INFO("restore info, file name:" << PathTransform::ExtractFileName(fileName) << ", remote file:"
                                                << PathTransform::ExtractFileName(restoreFilePath.remotePath)
                                                << ", file status:" << static_cast<uint32_t>(restoreFilePath.fileStatus)
                                                << ", smallestKey:" << smallest->ToString()
                                                << ", largestKey:" << largest->ToString() << ", fileId:" << fileId
                                                << ", fileAddress:" << fileAddress << ", fileSize:" << fileSize);

            fileMapping.emplace(restoreFilePath.remotePath, restoreFileInfo);
        }
        return BSS_OK;
    }

    static HashCodeOrderRangeRef DeserializeOrderRange(const FileInputViewRef &inputView)
    {
        uint8_t orderRangeType;
        RETURN_NULLPTR_AS_READ_ERROR(inputView->Read(orderRangeType));
        uint32_t startHashCode = 0;
        RETURN_NULLPTR_AS_READ_ERROR(inputView->Read(startHashCode));
        uint32_t endHashCode = 0;
        RETURN_NULLPTR_AS_READ_ERROR(inputView->Read(endHashCode));
        uint8_t changed = 0;
        RETURN_NULLPTR_AS_READ_ERROR(inputView->Read(changed));
        return std::make_shared<HashCodeOrderRange>(startHashCode, endHashCode, changed);
    }

    static RestoreFilePath FindFileMapping(std::string &originPath,
                                           const std::unordered_map<std::string, std::string> &lazyPathMapping,
                                           FileStatus fileStatus, bool isLazyDownload)
    {
        LOG_DEBUG("Find file mapping, path:" << PathTransform::ExtractFileName(originPath)
                                             << ", fileStatus:" << static_cast<uint32_t>(fileStatus));
        if (lazyPathMapping.empty()) {
            return { originPath, originPath, originPath, fileStatus };
        }

        std::string shortName = PathTransform::ExtractFileName(originPath);
        for (auto &item : lazyPathMapping) {
            if (fileStatus == FileStatus::LOCAL) {
                // 恢复后的本地文件名与snapshot时相同，但是目录不同
                std::string restorePath = PathTransform::ExtractFileName(item.first);
                if (shortName != restorePath) {
                    continue;
                }
            } else {
                if (originPath != item.second) {
                    continue;
                }
            }
            Uri uri{ item.first };
            PathRef path = std::make_shared<Path>(uri);
            if (path->Existed()) {
                if (!isLazyDownload) {
                    // 本地文件已经存在，已经在java侧恢复过
                    return { originPath, item.first, originPath, FileStatus::LOCAL };
                }
                // 懒加载模式，本地文件不可信，删除本地文件
                auto ret = unlink(path->Name().c_str());
                if (UNLIKELY(ret != 0)) {
                    LOG_ERROR("Delete file failed, path:" << path->ExtractFileName() << ", ret: " << ret);
                }
            }
            // 本地文件不存在，使用懒加载恢复;  ck时本地地址\restore时本地地址\远端地址
            return { originPath, item.first, item.second, FileStatus::DFS };
        }
        LOG_WARN("Restore version expect local path but not find:" << shortName);
        return { originPath, originPath, originPath, fileStatus };
    }
};

}  // namespace bss
}  // namespace ock
#endif  // BOOST_SS_VERSION_META_SERIALIZER_H
