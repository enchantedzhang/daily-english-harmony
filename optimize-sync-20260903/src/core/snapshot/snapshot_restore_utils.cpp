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

#include "snapshot_restore_utils.h"

#include "file_store_snapshot_operator.h"
#include "fresh_table_snapshot_operator.h"
#include "slice_table_snapshot_operator.h"

namespace ock {
namespace bss {
void SnapshotRestoreUtils::WriteSnapshotOperatorInfo(const FileOutputViewRef &localOutputView,
                                                     const std::vector<SnapshotOperatorMetaRef> &snapshotOperatorMetas)
{
    // snapshot operator的元数据内容: operatorType+对应operator在文件中的偏移+对应的operatorInfo.
    for (const auto &metaOffset : snapshotOperatorMetas) {
        AbstractSnapshotOperatorRef snapshotOperator = metaOffset->mSnapshotOperator;
        localOutputView->WriteUint8(static_cast<uint8_t>(snapshotOperator->GetType()));
        localOutputView->WriteUint64(metaOffset->mLocalMetaOffset);
        snapshotOperator->WriteInfo(localOutputView);
    }
}

SnapshotOperatorInfoRef SnapshotRestoreUtils::Deserialize(const FileInputViewRef &inputView,
                                                          SnapshotOperatorType operatorType)
{
    if (operatorType == SnapshotOperatorType::FRESH_TABLE) {
        return std::make_shared<FreshTableSnapshotOperatorInfo>(SnapshotOperatorType::FRESH_TABLE);
    } else if (operatorType == SnapshotOperatorType::SLICE_TABLE) {
        return std::make_shared<SliceTableSnapshotOperatorInfo>(SnapshotOperatorType::SLICE_TABLE);
    } else if (operatorType == SnapshotOperatorType::FILE_STORE) {
        RETURN_NULLPTR_AS_NULLPTR(inputView);
        uint32_t size = NO_0;
        inputView->Read(size);
        std::vector<FileStoreIDRef> fileStoreIds;
        std::vector<uint64_t> fileMetaOffsets;
        for (uint32_t i = 0; i < size; i++) {
            uint64_t offset = NO_0;
            inputView->Read(offset);
            fileMetaOffsets.push_back(offset);

            fileStoreIds.push_back(FileStoreID::Deserialize(inputView));
        }
        return std::make_shared<FileStoreSnapshotOperatorInfo>(SnapshotOperatorType::FILE_STORE, fileStoreIds,
                                                               fileMetaOffsets);
    } else if (operatorType == SnapshotOperatorType::BLOB_STORE) {
        return std::make_shared<SliceTableSnapshotOperatorInfo>(SnapshotOperatorType::BLOB_STORE);
    } else {
        LOG_ERROR("Illegal snapshot type, type:" << static_cast<uint32_t>(operatorType));
        return nullptr;
    }
}

std::vector<RestoredDbMeta::RestoredSnapshotOperatorInfoRef> SnapshotRestoreUtils::ReadSnapshotOperatorInfo(
    const FileInputViewRef &inputView, uint64_t snapshotOperatorInfoOffset, uint32_t numberOfSnapshotOperators)

{
    std::vector<RestoredDbMeta::RestoredSnapshotOperatorInfoRef> restoredSnapshotOperatorInfos;
    inputView->Seek(snapshotOperatorInfoOffset);
    for (uint32_t i = 0; i < numberOfSnapshotOperators; i++) {
        SnapshotOperatorType operatorType;
        inputView->Read(operatorType);

        uint64_t metaOffset = 0;
        inputView->Read(metaOffset);
        // 反序列化snapshotOperatorInfo, 主要是反序列化fileStore类别, 其余只是一个类型.
        SnapshotOperatorInfoRef snapshotOperatorInfo = Deserialize(inputView, operatorType);
        if (snapshotOperatorInfo == nullptr) {
            return {};
        }
        restoredSnapshotOperatorInfos.push_back(
            std::make_shared<RestoredDbMeta::RestoredSnapshotOperatorInfo>(snapshotOperatorInfo, metaOffset));
    }
    return restoredSnapshotOperatorInfos;
}

uint64_t SnapshotRestoreUtils::WriteFileMapping(const FileOutputViewRef &outputView, const PathRef &snapshotBasePath,
                                                const FileManagerRef &fileManager,
                                                std::unordered_set<PathRef, PathHash, PathEqual> &filePaths,
                                                std::vector<uint32_t> &localFileIds)

{
    auto fileMappingOffset = static_cast<uint64_t>(outputView->Size());
    outputView->WriteUTF(snapshotBasePath->Name());
    PathRef workingBasePath = fileManager->GetBasePath();
    outputView->WriteUint32(localFileIds.size());
    for (const auto &fileId : localFileIds) {
        FileInfoRef fileInfo = fileManager->GetFileInfo(fileId);
        if (UNLIKELY(fileInfo == nullptr)) {
            LOG_ERROR("File info is nullptr, fileId:" << fileId);
            continue;
        }
        // 写入文件完整路径和文件id，当前处理是针对fileStore中的文件元数据进行操作
        outputView->WriteUTF(fileInfo->GetFilePath()->ExtractFileName());
        outputView->WriteUint32(fileId);
    }
    return fileMappingOffset;
}

SnapshotFileMappingRef SnapshotRestoreUtils::ReadFileMapping(const FileInputViewRef &inputView,
                                                             uint64_t fileMappingOffset)
{
    RETURN_NULLPTR_AS_NULLPTR(inputView);
    inputView->Seek(fileMappingOffset);
    std::string basePathStr;
    if (UNLIKELY(inputView->ReadUTF(basePathStr) != BSS_OK)) {
        return nullptr;
    }
    // 当前获取的目录是checkpoint的时候的目录
    PathRef basePath = basePathStr.empty() ? nullptr : std::make_shared<Path>(basePathStr);
    std::vector<SnapshotFileInfoRef> fileMapping;
    uint32_t numberLsmFiles = NO_0;
    RETURN_NULLPTR_AS_READ_ERROR(inputView->Read(numberLsmFiles));
    for (uint32_t i = 0; i < numberLsmFiles; i++) {
        // 对应checkpoint，这里恢复lsm的filePathName以及fileId
        std::string fileName;
        if (UNLIKELY(inputView->ReadUTF(fileName) != BSS_OK)) {
            return nullptr;
        }
        uint32_t lsmFileId;
        RETURN_NULLPTR_AS_READ_ERROR(inputView->Read(lsmFileId));
        SnapshotFileInfoRef restoredFileIno = std::make_shared<SnapshotFileInfo>(fileName, lsmFileId, 0);
        fileMapping.push_back(restoredFileIno);
    }
    return std::make_shared<SnapshotFileMapping>(basePath, fileMapping);
}

void SnapshotRestoreUtils::WriteSnapshotMetaTail(const FileOutputViewRef &outputView, uint64_t snapshotId,
                                                 uint32_t startKeyGroup, uint32_t endKeyGroup, uint32_t seqId,
                                                 uint32_t snapshotOperatorInfoOffset,
                                                 uint32_t numberOfSnapshotOperators,
                                                 const FileMappingInfoRef &fileMappingInfo,
                                                 uint64_t stateIdProviderOffset)
{
    // 这里是对应恢复的时候的snapshotVersion版本号, 后面校验这个数字是否正确.
    outputView->WriteUint32(NO_5);  // magic
    outputView->WriteUint64(snapshotId);
    outputView->WriteUint32(startKeyGroup);
    outputView->WriteUint32(endKeyGroup);
    outputView->WriteUint64(seqId);
    outputView->WriteUint64(snapshotOperatorInfoOffset);
    outputView->WriteUint32(numberOfSnapshotOperators);
    outputView->WriteUint64(fileMappingInfo->mLocalFileMappingOffset);
    outputView->WriteUint64(fileMappingInfo->mRemoteFileMappingOffset);
    outputView->WriteUint64(stateIdProviderOffset);
}

SnapshotMetaTailRef SnapshotRestoreUtils::ReadSnapshotMetaTail(const PathRef &snapshotMetaPath,
                                                               const FileInputViewRef &snapshotMetaInputView)
{
    struct stat fileStat;
    if (stat(snapshotMetaPath->Name().c_str(), &fileStat) != 0) {
        LOG_ERROR("snapshotMetaPath read file info error");
        return nullptr;
    }
    if (!S_ISREG(fileStat.st_mode)) {
        LOG_ERROR("The specified path is not a regular file.");
        return nullptr;
    }

    auto restoredMetaSize = static_cast<uint64_t>(fileStat.st_size);
    // 减64字节直接偏移到snapshotVersion那里, 跳过migration这部分.
    if (restoredMetaSize < NO_64) {
        LOG_ERROR("Read restore meta tail fail, meta size:" << restoredMetaSize);
        return nullptr;
    }
    RETURN_NULLPTR_AS_NULLPTR(snapshotMetaInputView);
    uint64_t tailOffset = restoredMetaSize - NO_64;
    snapshotMetaInputView->Seek(tailOffset);
    uint32_t snapshotVersion = UINT32_MAX;
    RETURN_NULLPTR_AS_READ_ERROR(snapshotMetaInputView->Read(snapshotVersion));
    if (UNLIKELY(snapshotVersion > NO_5)) {
        LOG_ERROR("Unknown version of snapshot meta, expect version: 5, actual version:"
                  << snapshotVersion << ", snapshot meta path:" << snapshotMetaPath->ExtractFileName());
        return nullptr;
    }
    uint64_t snapshotId;
    RETURN_NULLPTR_AS_READ_ERROR(snapshotMetaInputView->Read(snapshotId));
    uint32_t startKeyGroup;
    RETURN_NULLPTR_AS_READ_ERROR(snapshotMetaInputView->Read(startKeyGroup));
    uint32_t endKeyGroup;
    RETURN_NULLPTR_AS_READ_ERROR(snapshotMetaInputView->Read(endKeyGroup));
    uint64_t seqId;
    RETURN_NULLPTR_AS_READ_ERROR(snapshotMetaInputView->Read(seqId));
    uint64_t snapshotOperatorInfoOffset;
    RETURN_NULLPTR_AS_READ_ERROR(snapshotMetaInputView->Read(snapshotOperatorInfoOffset));
    uint32_t numberOfSnapshotOperators;
    RETURN_NULLPTR_AS_READ_ERROR(snapshotMetaInputView->Read(numberOfSnapshotOperators));
    uint64_t localFileMappingOffset;
    RETURN_NULLPTR_AS_READ_ERROR(snapshotMetaInputView->Read(localFileMappingOffset));
    uint64_t remoteFileMappingOffset;
    RETURN_NULLPTR_AS_READ_ERROR(snapshotMetaInputView->Read(remoteFileMappingOffset));
    uint64_t stateIdOffset;
    RETURN_NULLPTR_AS_READ_ERROR(snapshotMetaInputView->Read(stateIdOffset));

    return std::make_shared<SnapshotMetaTail>(snapshotVersion, snapshotId, startKeyGroup, endKeyGroup, seqId,
                                              snapshotOperatorInfoOffset, numberOfSnapshotOperators,
                                              localFileMappingOffset, remoteFileMappingOffset, stateIdOffset);
}

SnapshotMetaRef SnapshotRestoreUtils::WriteDbMeta(uint64_t snapshotId, uint64_t startKeyGroup, uint64_t endKeyGroup,
                                                  uint64_t seqId, const StateIdProviderRef &stateIdProvider,
                                                  const PathRef &localSnapshotPath,
                                                  const FileOutputViewRef &localOutputView,
                                                  const FileManagerRef &localFileManager,
                                                  std::vector<AbstractSnapshotOperatorRef> &snapshotOperators,
                                                  const SnapshotStatRef &snapshotStat)
{
    RETURN_NULLPTR_AS_NULLPTR(stateIdProvider);
    RETURN_NULLPTR_AS_NULLPTR(localFileManager);
    RETURN_NULLPTR_AS_NULLPTR(snapshotStat);
    RETURN_NULLPTR_AS_NULLPTR(localSnapshotPath);
    RETURN_NULLPTR_AS_NULLPTR(localOutputView);

    SnapshotMetaRef totalSnapshotMeta = std::make_shared<SnapshotMeta>();
    std::vector<SnapshotOperatorMetaRef> snapshotOperatorMetas;
    for (auto &snapshotOperator : snapshotOperators) {
        // 1. 创建各个operator的snapshot元数据.
        SnapshotOperatorMetaRef snapshotOperatorMeta = std::make_shared<SnapshotOperatorMeta>();
        snapshotOperatorMeta->mSnapshotOperator = snapshotOperator;
        // 记录localSnapshot元信息的起始offset
        snapshotOperatorMeta->mLocalMetaOffset = static_cast<int64_t>(localOutputView->Size());
        // 将各个snapshotOperator的元数据写入到文件中
        snapshotOperatorMeta->mSnapshotMeta = snapshotOperator->OutputMeta(snapshotId, localOutputView);

        // 如果因为flink侧释放了此次checkpoint而获取到空的snapshotOperatorMeta->mSnapshotMeta，不应视为error
        if (snapshotOperator->GetIsReleased().load() && snapshotOperatorMeta->mSnapshotMeta == nullptr) {
            LOG_WARN("Current snapshot is released.");
            return nullptr;
        }

        // 增加snapshotOperatorMeta->mSnapshotMeta非空校验
        RETURN_NULLPTR_AS_NULLPTR(snapshotOperatorMeta->mSnapshotMeta)

        // 添加到snapshotOperator元数据列表
        snapshotOperatorMetas.push_back(snapshotOperatorMeta);
        totalSnapshotMeta->AddSnapshotMeta(snapshotOperatorMeta->mSnapshotMeta);

        if (snapshotOperator->GetType() == SnapshotOperatorType::SLICE_TABLE) {
            snapshotStat->AddSliceTableSnapshotMeta(snapshotOperatorMeta->mSnapshotMeta);
            continue;
        }
        if (snapshotOperator->GetType() == SnapshotOperatorType::FILE_STORE) {
            snapshotStat->AddFileStoreSnapshotMeta(snapshotOperatorMeta->mSnapshotMeta);
        }
        if (snapshotOperator->GetType() == SnapshotOperatorType::BLOB_STORE) {
            snapshotStat->AddFileStoreSnapshotMeta(snapshotOperatorMeta->mSnapshotMeta);
        }
    }

    // 2. 将snapshot operator meta写入文件.
    int64_t localSnapshotOperatorInfoOffset = static_cast<int64_t>(localOutputView->Size());
    WriteSnapshotOperatorInfo(localOutputView, snapshotOperatorMetas);

    // 3. 将FileMapping写入文件.
    FileMappingInfoRef localMetaFileMappingInfo = nullptr;
    localMetaFileMappingInfo = WriteFileMappingMeta(localOutputView, localSnapshotPath, localFileManager,
                                                    totalSnapshotMeta->GetLocalFilePaths(),
                                                    totalSnapshotMeta->GetLocalFileIds());
    RETURN_NULLPTR_AS_NULLPTR(localMetaFileMappingInfo);

    // 4. 将stateIdProvider写入文件.
    uint64_t localStateIdProviderOffset = 0;
    localStateIdProviderOffset = localOutputView->Size();
    auto ret = stateIdProvider->Snapshot(localOutputView);
    if (UNLIKELY(ret != BSS_OK)) {
        return nullptr;
    }

    // 5. 写入snapshot meta tail信息.
    WriteSnapshotMetaTail(localOutputView, snapshotId, startKeyGroup, endKeyGroup, seqId,
                          localSnapshotOperatorInfoOffset, snapshotOperators.size(), localMetaFileMappingInfo,
                          localStateIdProviderOffset);
    return totalSnapshotMeta;
}

RestoredDbMetaRef SnapshotRestoreUtils::ReadDbMeta(const PathRef &restoredMetaPath)
{
    // 1. 创建一个FileInputView实例, 用于读取DB元数据信息.
    FileInputViewRef restoredMetaInputView = std::make_shared<FileInputView>();
    RETURN_NULLPTR_AS_NOT_OK(restoredMetaInputView->Init(FileSystemType::LOCAL, restoredMetaPath));

    // 2. 读取SnapshotMeta信息
    SnapshotMetaTailRef snapshotMetaTail = ReadSnapshotMetaTail(restoredMetaPath, restoredMetaInputView);
    RETURN_NULLPTR_AS_NULLPTR(snapshotMetaTail);

    // 3. 读取SnapshotOperatorInfo信息.
    auto restoredSnapshotOperatorInfos = ReadSnapshotOperatorInfo(restoredMetaInputView,
                                                                  snapshotMetaTail->GetSnapshotOperatorInfoOffset(),
                                                                  snapshotMetaTail->GetNumberOfSnapshotOperators());

    // 4. 读取本地文件映射信息
    SnapshotFileMappingRef localRestoredFileMapping = ReadFileMapping(restoredMetaInputView,
                                                                      snapshotMetaTail->GetLocalFileMappingOffset());

    // 5. 创建并返回RestoredDbMeta实例
    return std::make_shared<RestoredDbMeta>(restoredMetaPath, restoredMetaInputView, snapshotMetaTail,
                                            restoredSnapshotOperatorInfos, localRestoredFileMapping, nullptr);
}

std::vector<SnapshotFileMappingRef> SnapshotRestoreUtils::RelocateLocalFileMappings(
    const PathRef &relocateBasePath, std::vector<SnapshotFileMappingRef> &restoredLocalFileMappings)
{
    std::vector<SnapshotFileMappingRef> relocatedFileMappings;
    for (SnapshotFileMappingRef &fileMapping : restoredLocalFileMappings) {
        SnapshotFileMappingRef relocate = std::make_shared<SnapshotFileMapping>(relocateBasePath,
                                                                                fileMapping->GetFileMapping());
        relocatedFileMappings.push_back(relocate);
    }
    return relocatedFileMappings;
}

}  // namespace bss
}  // namespace ock