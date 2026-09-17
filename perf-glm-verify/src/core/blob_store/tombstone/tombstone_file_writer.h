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

#ifndef BOOST_SS_TOMBSTONE_FILE_WRITER_H
#define BOOST_SS_TOMBSTONE_FILE_WRITER_H

#include <memory>
#include <vector>

#include "include/config.h"
#include "io/file_output_view.h"
#include "lsm_store/block/block_handle.h"
#include "tombstone.h"
#include "tombstone_file.h"
#include "tombstone_file_manager.h"
#include "tombstone_structure.h"

namespace ock {
namespace bss {

class TombstoneFileWriter {
public:
    TombstoneFileWriter(const ConfigRef &config, const TombstoneFileManagerRef &fileManager, uint64_t version,
                        const MemManagerRef &memManager)
        : mConfig(config), mFileManager(fileManager), mVersion(version), mMemManager(memManager)
    {
        mOutPutView = std::make_shared<OutputView>(mMemManager);
    }

    BResult Flush(const std::set<TombstoneRef, CompareTombstone> &flushSet, std::vector<TombstoneFileRef> &fileVec)
    {
        mOutPutView->SetOffset(0);
        for (const auto &item : flushSet) {
            RETURN_NOT_OK(WriteTombstone(item));
        }

        return FinishWrite(fileVec);
    }

    BResult WriteTombstone(const TombstoneRef &tombstone)
    {
        if (UNLIKELY(tombstone == nullptr)) {
            LOG_ERROR("Tombstone is null!");
            return BSS_INVALID_PARAM;
        }
        if (mCurFile == nullptr) {
            mCurFile = mFileManager->AllocateFile(mVersion);
            RETURN_ERROR_AS_NULLPTR(mCurFile);
        }
        if (mPreTombstone != nullptr && tombstone->GetBlobId() < mPreTombstone->GetBlobId()) {
            LOG_ERROR("tombstone blobId is smaller than previous one, pre:" << mPreTombstone->GetBlobId()
                                                                            << ", cur:" << tombstone->GetBlobId());
            return BSS_INVALID_PARAM;
        }
        mPreTombstone = tombstone;
        RETURN_NOT_OK(mOutPutView->WriteUint64(tombstone->GetBlobId()));
        RETURN_NOT_OK(mOutPutView->WriteUint16(tombstone->GetKeyGroup()));
        mCurFile->CountBlobId(tombstone->GetBlobId());
        if (mOutPutView->GetOffset() < mConfig->mTombstoneDataBlockSize) {
            return BSS_OK;
        }
        // 已经满足dataBlock大小，先刷盘
        return WriteBlock();
    }

    BResult FinishWrite(std::vector<TombstoneFileRef> &result)
    {
        if (mOutPutView->GetOffset() > 0) {
            RETURN_NOT_OK(WriteBlock());
        }
        if (!mBlockHandleVec.empty()) {
            RETURN_ERROR_AS_NULLPTR(mCurFile);
            auto fileOutPutView = mCurFile->GetFileOutPutView();
            RETURN_NOT_OK(WriteFile(fileOutPutView));
            mCurFile->CloseFileOutPutView();
            mWrittenFileVec.emplace_back(mCurFile);
            mCurFile = nullptr;
        }
        mOutPutView = nullptr;
        std::vector<TombstoneFileMetaRef> metaVec;
        for (const auto &item : mWrittenFileVec) {
            metaVec.emplace_back(item->GetFileMeta());
        }
        mFileManager->GetFileCacheManager()->ConfirmAllocationOnFlushOrCompaction(metaVec);
        mFileManager->AddFiles(mWrittenFileVec);
        result = mWrittenFileVec;
        return BSS_OK;
    }

private:
    BResult WriteBlock()
    {
        if (UNLIKELY(mCurFile == nullptr)) {
            LOG_ERROR("File is null!");
            return BSS_ERR;
        }
        auto fileOutPutView = mCurFile->GetFileOutPutView();
        RETURN_ERROR_AS_NULLPTR(fileOutPutView);
        auto offset = fileOutPutView->Size();
        auto length = mOutPutView->GetOffset();
        auto ret = fileOutPutView->WriteBuffer(mOutPutView->Data(), offset, length);
        RETURN_NOT_OK(ret);
        mBlockHandleVec.emplace_back(std::make_shared<BlockHandle>(offset, length));
        mOutPutView->SetOffset(0);
        if (fileOutPutView->Size() < mConfig->mTombstoneFileSize) {
            return BSS_OK;
        }
        ret = WriteFile(fileOutPutView);
        RETURN_NOT_OK(ret);
        mCurFile->CloseFileOutPutView();
        mWrittenFileVec.emplace_back(mCurFile);
        mCurFile = nullptr;
        mOutPutView->SetOffset(0);
        return BSS_OK;
    }

    BResult WriteFooter(FileOutputViewRef &fileOutputView, uint32_t indexBlockOffset, uint32_t indexBlockSize)
    {
        TombstoneFileMetaRef fileMeta = mCurFile->GetFileMeta();
        TombstoneFooterStructure footer{ fileMeta->GetMinBlobId(),
                                         fileMeta->GetMaxBlobId(),
                                         BLOCK_COMMON_MAGIC_NUM,
                                         fileMeta->GetBlobNum(),
                                         static_cast<uint32_t>(mBlockHandleVec.size()),
                                         indexBlockOffset,
                                         indexBlockSize };
        mBlockHandleVec.clear();
        auto ret = fileOutputView->WriteBuffer(reinterpret_cast<uint8_t *>(&footer), fileOutputView->Size(),
                                               sizeof(TombstoneFooterStructure));
        RETURN_NOT_OK(ret);

        LOG_INFO("Write tombstone file:" << PathTransform::ExtractFileName(mCurFile->GetFileMeta()->GetIdentifier())
                                         << " success. Size: " << fileOutputView->Size());
        fileMeta->SetFileSize(fileOutputView->Size());
        return BSS_OK;
    }

    BResult WriteIndexBlock(FileOutputViewRef &fileOutputView, uint32_t &indexBlockOffset, uint32_t &indexBlockSize)
    {
        mOutPutView->SetOffset(0);
        for (const auto &item : mBlockHandleVec) {
            RETURN_NOT_OK(mOutPutView->WriteUint32(item->GetOffset()));
            RETURN_NOT_OK(mOutPutView->WriteUint32(item->GetSize()));
        }
        indexBlockOffset = static_cast<uint32_t>(fileOutputView->Size());
        indexBlockSize = mOutPutView->GetOffset();
        return fileOutputView->WriteBuffer(mOutPutView->Data(), indexBlockOffset, indexBlockSize);
    }

    BResult WriteFile(FileOutputViewRef &fileOutputView)
    {
        uint32_t indexBlockOffset = 0;
        uint32_t indexBlockSize = 0;
        auto ret = WriteIndexBlock(fileOutputView, indexBlockOffset, indexBlockSize);
        RETURN_NOT_OK(ret);
        return WriteFooter(fileOutputView, indexBlockOffset, indexBlockSize);
    }

private:
    ConfigRef mConfig;
    TombstoneFileManagerRef mFileManager = nullptr;
    uint64_t mVersion;
    std::vector<BlockHandleRef> mBlockHandleVec;
    TombstoneFileRef mCurFile = nullptr;
    OutputViewRef mOutPutView = nullptr;
    std::vector<TombstoneFileRef> mWrittenFileVec;
    MemManagerRef mMemManager = nullptr;
    TombstoneRef mPreTombstone = nullptr;  // 上次写入的tombstone，后面写入的blobId不能大于该值
};
using TombstoneFileWriterRef = std::shared_ptr<TombstoneFileWriter>;
}  // namespace bss
}  // namespace ock

#endif  // BOOST_SS_TOMBSTONE_FILE_WRITER_H
