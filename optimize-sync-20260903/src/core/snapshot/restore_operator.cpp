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

#include "restore_operator.h"

#include "slice_table_restore_operation.h"

namespace ock {
namespace bss {
BResult RestoreOperator::Restore(std::vector<PathRef> &restoredMetaPaths,
                                 std::unordered_map<std::string, std::string> &lazyPathMapping, uint64_t &seqId,
                                 bool isLazyDownload)
{
    if (UNLIKELY(restoredMetaPaths.empty())) {
        LOG_INFO("Nothing needed to restore snapshot.");
        return BSS_OK;
    }
    auto start = std::chrono::high_resolution_clock::now();
    // 1. 解析resort db meta.
    std::vector<RestoredDbMetaRef> restoredDbMetas;
    for (const auto &path : restoredMetaPaths) {
        auto dbMeta = SnapshotRestoreUtils::ReadDbMeta(path);
        if (UNLIKELY(dbMeta == nullptr)) {
            LOG_WARN("Read db meta failed, path:" << path->ExtractFileName());
            continue;
        }
        seqId = std::max(seqId, dbMeta->GetSeqId());
        restoredDbMetas.push_back(dbMeta);
    }
    if (UNLIKELY(restoredDbMetas.empty())) {
        LOG_INFO("Restore all meta is empty.");
        return BSS_OK;
    }

    uint64_t restoredSnapshotId = restoredDbMetas[0]->GetSnapshotId();  // 本次恢复的snapshotId.
    LOG_INFO("Receive restore operator, metaSize:" << restoredDbMetas.size() << ", snapshotId:" << restoredSnapshotId);

    // 2. 恢复statIdProvider.
    std::vector<TableDescriptionRef> tableDescriptions;
    for (const auto &table : mTables) {
        auto tableDescription = table.second->GetTableDescription();
        tableDescriptions.push_back(tableDescription);
    }
    for (const auto &item : mPQTables) {
        auto des = std::make_shared<TableDescription>(PQ, item.second->GetStateName(), -1, TableSerializer(), *mConfig);
        tableDescriptions.push_back(des);
    }
    RETURN_ERROR_AS_NULLPTR(mStateIdProvider);
    for (const RestoredDbMetaRef &dbMeta : restoredDbMetas) {
        FileInputViewRef inputView = dbMeta->GetSnapshotMetaInputView();
        inputView->Seek(dbMeta->GetStateIdOffset());
        RETURN_NOT_OK(mStateIdProvider->Restore(inputView, tableDescriptions));
    }

    // 3. 重新生成fileId.
    std::vector<SnapshotFileMappingRef> restoredLocalFileMappings;
    restoredLocalFileMappings.reserve(restoredDbMetas.size());
    for (const auto &restoredDbMeta : restoredDbMetas) {
        restoredLocalFileMappings.emplace_back(restoredDbMeta->GetLocalFileMapping());
    }

    std::unordered_map<std::string, uint32_t> restorePathFileIdMap;
    for (const auto &item : restoredLocalFileMappings) {
        CONTINUE_LOOP_AS_NULLPTR(item);
        std::vector<SnapshotFileInfoRef> fileMapping = item->GetFileMapping();
        for (const auto &fileInfo : fileMapping) {
            CONTINUE_LOOP_AS_NULLPTR(fileInfo);
            auto fileIdObj = mFileCacheFactory->GetLocalFileIdGenerator()->Generate();
            RETURN_ERROR_AS_NULLPTR(fileIdObj);
            fileInfo->SetFileId(fileIdObj->GetUniqueId());
            restorePathFileIdMap.emplace(fileInfo->GetFileName(), fileIdObj->GetUniqueId());
        }
    }

    // 4. 恢复Local file manager.
    std::vector<SnapshotFileMappingRef> relocatedLocalFileMappings;
    std::vector<SnapshotFileMappingRef> relocatedRemoteFileMappings;
    // 获取fileMapping
    bool isExcludeSSTFiles = isLazyDownload;
    relocatedLocalFileMappings = SnapshotRestoreUtils::RelocateLocalFileMappings(mLocalFileManager->GetBasePath(),
                                                                                 restoredLocalFileMappings);
    RETURN_NOT_OK(CreateHardLinkForRestoredLocalFile(isExcludeSSTFiles, restoredLocalFileMappings,
                                                     mLocalFileManager->GetBasePath()));

    auto remoteFileMapping = OrganizeRemoteFileInfo(restoredLocalFileMappings, lazyPathMapping);
    RETURN_INVALID_PARAM_AS_NULLPTR(remoteFileMapping);
    relocatedRemoteFileMappings.emplace_back(remoteFileMapping);
    RETURN_NOT_OK(mLocalFileManager->StartRestore(relocatedLocalFileMappings, isExcludeSSTFiles));
    RETURN_NOT_OK(mRemoteFileManager->StartRestore(relocatedRemoteFileMappings, !isExcludeSSTFiles));

    // 5. 恢复Slice table.
    auto sliceTableRestoreOp = std::make_shared<SliceTableRestoreOperation>(mConfig, mSliceTable);
    std::vector<SliceTableRestoreMetaRef> restoreMetaList;
    RETURN_NOT_OK(sliceTableRestoreOp->RestoreSliceBucketIndex(restoredDbMetas, restoreMetaList));
    // 恢复Blob store
    RETURN_NOT_OK(sliceTableRestoreOp->RestoreBlobStore(restoreMetaList, restorePathFileIdMap));
    // 6. 恢复File store.
    RETURN_NOT_OK(sliceTableRestoreOp->RestoreFileStoreOperator(restoreMetaList, lazyPathMapping, restorePathFileIdMap,
                                                                isLazyDownload));
    // 7. 加载sliceTable的DataSlice.
    RETURN_NOT_OK(sliceTableRestoreOp->LoadSlicesIntoSliceTable(restoredDbMetas[0]->GetSnapshotVersion(), true));

    // 8. 恢复Fresh table, 注意需要在下游组件恢复完成后方可恢复Fresh table.
    RETURN_NOT_OK(mFreshTable->Restore(restoredDbMetas));

    // 9. 恢复快照管理器.
    mSnapshotManager->Restore(restoredSnapshotId);
    mFileCacheFactory->GetFileCache()->NotifyLazyDownload();
    RETURN_NOT_OK(mLocalFileManager->EndRestore());
    RETURN_NOT_OK(mRemoteFileManager->EndRestore());

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    double elapsed = duration.count() / 1e3;  // 转换为ms
    LOG_INFO("Restore process finish, checkpointId:" << restoredSnapshotId << ", cost time:" << elapsed << " ms.");
    return BSS_OK;
}

BResult RestoreOperator::CreateHardLinkForRestoredLocalFile(
    bool isExcludeSSTFiles, const std::vector<SnapshotFileMappingRef> &restoredLocalFileMappings,
    const PathRef &currentBasePath)
{
    RETURN_INVALID_PARAM_AS_NULLPTR(currentBasePath);
    if (access(currentBasePath->Name().c_str(), F_OK) != 0) {
        LOG_ERROR("Local working directory " << currentBasePath->ExtractFileName() << " does not exist");
        return BSS_ERR;
    }
    PathRef currentBlobPath = std::make_shared<Path>(currentBasePath->Name() + "/blobFile");
    for (const auto &item : restoredLocalFileMappings) {
        PathRef restoredBasePath = item->GetBasePath();
        std::vector<SnapshotFileInfoRef> restoredFileInos = item->GetFileMapping();

        PathRef targetPath;
        for (SnapshotFileInfoRef &restoredFileIno : restoredFileInos) {
            std::string fileName = PathTransform::ExtractFileName(restoredFileIno->GetFileName());
            if (isExcludeSSTFiles && fileName.find(BLOB_FILE_NAME_PREFIX) == std::string::npos) {
                continue;
            }
            if (fileName.find(BLOB_FILE_NAME_PREFIX) != std::string::npos) {
                targetPath = currentBlobPath;
            } else {
                targetPath = currentBasePath;
            }
            PathRef srcFile = std::make_shared<Path>(restoredBasePath, fileName);
            PathRef targetFile = std::make_shared<Path>(targetPath, fileName);
            // 如果目标文件存在，先删除
            if (access(targetFile->Name().c_str(), F_OK) == 0) {
                auto ret = unlink(targetFile->Name().c_str());
                if (UNLIKELY(ret != 0)) {
                    LOG_ERROR("Delete file failed, path:" << targetFile->ExtractFileName() << ", ret: " << ret);
                }
            }
            // 创建硬链接
            if (link(srcFile->Name().c_str(), targetFile->Name().c_str()) != 0) {
                LOG_ERROR("Create hard link failed, from " << srcFile->ExtractFileName() << " to "
                                                           << targetFile->ExtractFileName());
                return BSS_ERR;
            }
        }
    }
    return BSS_OK;
}

SnapshotFileMappingRef RestoreOperator::OrganizeRemoteFileInfo(
    std::vector<SnapshotFileMappingRef> &restoredLocalFileMappings,
    std::unordered_map<std::string, std::string> &pathMap)
{
    if (UNLIKELY(restoredLocalFileMappings.empty())) {
        LOG_ERROR("restoredLocalFileMappings is empty.");
        return nullptr;
    }
    LOG_INFO("restoredLocalFileMappings size:" << restoredLocalFileMappings.size());

    for (auto &restoredLocalFileMapping : restoredLocalFileMappings) {
        for (auto &fileInfo : restoredLocalFileMapping->GetFileMapping()) {
            CONTINUE_LOOP_AS_NULLPTR(fileInfo);
            LOG_DEBUG("local file path:" << PathTransform::ExtractFileName(fileInfo->GetFileName()));
        }
    }

    std::vector<SnapshotFileInfoRef> fileMapping;
    for (auto &item : pathMap) {
        auto remotePath = item.second;
        auto localPath = item.first;

        for (auto &restoredLocalFileMapping : restoredLocalFileMappings) {
            for (auto &fileInfo : restoredLocalFileMapping->GetFileMapping()) {
                CONTINUE_LOOP_AS_NULLPTR(fileInfo);
                if (fileInfo->GetFileName() == PathTransform::ExtractFileName(localPath)) {
                    LOG_INFO("matched file path:" << PathTransform::ExtractFileName(fileInfo->GetFileName()));
                    auto remoteFIleInfo = std::make_shared<SnapshotFileInfo>(remotePath, fileInfo->GetFileId(), 0);
                    fileMapping.emplace_back(remoteFIleInfo);
                }
            }
        }
    }

    LOG_INFO("Expect file mapping size:" << pathMap.size() << ", real file mapping size:" << fileMapping.size());
    return std::make_shared<SnapshotFileMapping>(std::make_shared<Path>("null"), fileMapping);
}

}  // namespace bss
}  // namespace ock
