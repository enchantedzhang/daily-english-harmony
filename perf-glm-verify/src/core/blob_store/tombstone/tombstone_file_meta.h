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

#ifndef BOOST_SS_TOMBSTONE_FILE_META_H
#define BOOST_SS_TOMBSTONE_FILE_META_H

#include <memory>

#include "blob_file_meta_base.h"
#include "common/io/file_input_view.h"
#include "common/io/file_output_view.h"
#include "lsm_store/file/file_info.h"

namespace ock {
namespace bss {
class TombstoneFileMeta : public BlobFileMetaBase {
public:
    TombstoneFileMeta() = default;

    TombstoneFileMeta(FileInfoRef fileInfo, GroupRangeRef groupRange, uint64_t version)
        : BlobFileMetaBase(fileInfo->GetFilePath()->Name(), groupRange, groupRange, fileInfo->GetFileId()->Get()),
          mVersion(version)
    {
    }

    void CountBlobId(uint64_t blobId)
    {
        mBlobNum++;
        if (blobId > mMaxBlobId) {
            mMaxBlobId = blobId;
        }
        if (blobId < mMinBlobId || mMinBlobId == 0) {
            mMinBlobId = blobId;
        }
    }

    inline uint32_t GetBlobNum() const
    {
        return mBlobNum;
    }

    uint64_t CalValidBlobIDNum() const
    {
        return mValidGroupRange->GetKeyGroupRangeSize() / mCoveredGroupRange->GetKeyGroupRangeSize() * mBlobNum;
    }

    inline void SetVersion(uint64_t version)
    {
        mVersion = version;
    }

    inline uint64_t GetVersion() const
    {
        return mVersion;
    }

    BResult Snapshot(const FileOutputViewRef &fileOutputView)
    {
        RETURN_NOT_OK(fileOutputView->WriteUTF(mIdentifier));
        RETURN_NOT_OK(fileOutputView->WriteUint64(mFileAddress));
        RETURN_NOT_OK(fileOutputView->WriteUint64(mFileSize));
        RETURN_NOT_OK(fileOutputView->WriteUint32(mBlobNum));
        RETURN_NOT_OK(fileOutputView->WriteUint64(mMinBlobId));
        RETURN_NOT_OK(fileOutputView->WriteUint64(mMaxBlobId));
        RETURN_NOT_OK(fileOutputView->WriteUint64(mVersion));
        RETURN_NOT_OK(fileOutputView->WriteInt32(mValidGroupRange->GetStartGroup()));
        RETURN_NOT_OK(fileOutputView->WriteInt32(mValidGroupRange->GetEndGroup()));
        RETURN_NOT_OK(fileOutputView->WriteInt32(mCoveredGroupRange->GetStartGroup()));
        RETURN_NOT_OK(fileOutputView->WriteInt32(mCoveredGroupRange->GetEndGroup()));
        RETURN_NOT_OK(fileOutputView->WriteUint8(static_cast<uint8_t>(mFileStatus)));
        return BSS_OK;
    }

    BResult Restore(const FileInputViewRef &inputView, const PathRef &workingPath,
                    std::unordered_map<std::string, uint32_t> &restorePathFileIdMap, RestoreFileInfo &restoreFileInfo)
    {
        RETURN_NOT_OK(inputView->ReadUTF(mIdentifier));
        mIdentifier = workingPath->Name() + "/blobFile/" + PathTransform::ExtractFileName(mIdentifier);
        auto it = restorePathFileIdMap.find(PathTransform::ExtractFileName(mIdentifier));
        if (it == restorePathFileIdMap.end()) {
            LOG_ERROR("Deserialize file from meta not find in restore path fileId path.");
            return BSS_ERR;
        }
        auto fileId = it->second;
        RETURN_NOT_OK(inputView->Read(mFileAddress));
        mFileAddress = FileAddressUtils::GetFileAddressWithZeroOffset(fileId);
        RETURN_NOT_OK(inputView->Read(mFileSize));
        RETURN_NOT_OK(inputView->Read(mBlobNum));
        RETURN_NOT_OK(inputView->Read(mMinBlobId));
        RETURN_NOT_OK(inputView->Read(mMaxBlobId));
        RETURN_NOT_OK(inputView->Read(mVersion));
        int32_t validGroupRangeStart = 0;
        RETURN_NOT_OK(inputView->Read(validGroupRangeStart));
        int32_t validGroupRangeEnd = 0;
        RETURN_NOT_OK(inputView->Read(validGroupRangeEnd));
        mValidGroupRange = std::make_shared<GroupRange>(validGroupRangeStart, validGroupRangeEnd);
        int32_t coveredGroupRangeStart = 0;
        RETURN_NOT_OK(inputView->Read(coveredGroupRangeStart));
        int32_t coveredGroupRangeEnd = 0;
        RETURN_NOT_OK(inputView->Read(coveredGroupRangeEnd));
        mCoveredGroupRange = std::make_shared<GroupRange>(coveredGroupRangeStart, coveredGroupRangeEnd);
        RETURN_NOT_OK(inputView->Read(mFileStatus));
        restoreFileInfo = {
            mIdentifier, mFileAddress, mFileStatus, mFileSize, 1, mFileAddress, mIdentifier, mIdentifier
        };
        return BSS_OK;
    }

private:
    uint32_t mBlobNum = 0;
    uint64_t mVersion = 0;
};
using TombstoneFileMetaRef = std::shared_ptr<TombstoneFileMeta>;
}  // namespace bss
}  // namespace ock

#endif  // BOOST_SS_TOMBSTONE_FILE_META_H
