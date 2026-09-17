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

#ifndef BOOST_SS_TOMBSTONE_FILE_H
#define BOOST_SS_TOMBSTONE_FILE_H

#include <memory>

#include "io/file_output_view.h"
#include "lsm_store/file/file_cache_manager.h"
#include "tombstone_file_meta.h"

namespace ock {
namespace bss {
class TombstoneFile {
public:
    TombstoneFile(const ConfigRef &config, const FileCacheManagerRef &fileCacheManager,
                  const TombstoneFileMetaRef &fileMeta, bool initFileOutView)
        : mConfig(config), mFileCacheManager(fileCacheManager), mFileMeta(fileMeta), mInitFileOutView(initFileOutView)
    {
    }

    BResult InitFileOutPutView(const FileInfoRef &fileInfo)
    {
        if (!mInitFileOutView) {
            LOG_ERROR("Tombstone file not need init file output View.");
            return BSS_ERR;
        }
        mFileInfo = fileInfo;
        mFileOutPutView = std::make_shared<FileOutputView>();
        return mFileOutPutView->Init(mFileInfo->GetFilePath(), mConfig);
    }

    inline void CountBlobId(uint64_t blobId)
    {
        mFileMeta->CountBlobId(blobId);
    }

    inline FileOutputViewRef GetFileOutPutView() const
    {
        return mFileOutPutView;
    }

    inline TombstoneFileMetaRef GetFileMeta() const
    {
        return mFileMeta;
    }

    inline void CloseFileOutPutView()
    {
        if (mInitFileOutView && mFileOutPutView != nullptr) {
            mFileOutPutView->Close();
        }
    }

    void SetFileVersion(uint64_t version)
    {
        mFileMeta->SetVersion(version);
    }

    inline uint32_t DecRef()
    {
        return mFileMeta->DecRef();
    }

    inline uint32_t IncRef()
    {
        return mFileMeta->IncRef();
    }

    inline uint64_t GetFileAddress() const
    {
        return mFileMeta->GetFileAddress();
    }

    inline uint64_t GetVersion() const
    {
        return mFileMeta->GetVersion();
    }

    inline uint64_t GetFileSize() const
    {
        return mFileMeta->GetFileSize();
    }

    BResult OpenFile(FileInputViewRef &inputView)
    {
        RETURN_ERROR_AS_NULLPTR(inputView);
        FileInfoRef fileInfo = mFileCacheManager->GetPrimaryFileInfo(mFileMeta->GetFileAddress());
        RETURN_ERROR_AS_NULLPTR(fileInfo);
        return inputView->Init(FileSystemType::LOCAL, fileInfo->GetFilePath());
    }

    std::shared_ptr<TombstoneFile> Snapshot()
    {
        return std::make_shared<TombstoneFile>(mConfig, mFileCacheManager, mFileMeta, false);
    }

private:
    ConfigRef mConfig;
    FileCacheManagerRef mFileCacheManager;
    TombstoneFileMetaRef mFileMeta = nullptr;
    FileOutputViewRef mFileOutPutView;
    bool mInitFileOutView = false;
    FileInfoRef mFileInfo;
};
using TombstoneFileRef = std::shared_ptr<TombstoneFile>;
}  // namespace bss
}  // namespace ock

#endif  // BOOST_SS_TOMBSTONE_FILE_H
