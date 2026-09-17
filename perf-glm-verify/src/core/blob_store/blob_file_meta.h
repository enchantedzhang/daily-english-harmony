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

#ifndef BLOB_FILE_META_H
#define BLOB_FILE_META_H
#include <memory>

#include "blob_file_meta_base.h"
#include "file/file_cache_type.h"
#include "file/group_range.h"
#include "io/file_input_view.h"
#include "io/file_output_view.h"
#include "lazy/restore_file_info.h"
#include "path_transform.h"

namespace ock {
namespace bss {
class BlobFileMeta : public BlobFileMetaBase {
public:
    BlobFileMeta() = default;
    BlobFileMeta(const std::string &fileName, const GroupRangeRef &validGroupRange,
                 const GroupRangeRef &coveredGroupRange, uint32_t fileId)
        : BlobFileMetaBase(fileName, validGroupRange, coveredGroupRange, fileId)
    {
    }

    void UpdateBlobId(uint64_t minBlobId, uint64_t maxBlobId)
    {
        mMinBlobId = std::min(minBlobId, mMinBlobId);
        mMaxBlobId = std::max(maxBlobId, mMaxBlobId);
    }

    void UpdateExpireTime(uint64_t minExpireTime, uint64_t maxExpireTime)
    {
        mMinExpireTime = std::min(minExpireTime, mMinExpireTime);
        mMaxExpireTime = std::max(maxExpireTime, mMaxExpireTime);
    }

    void IncDataBlockSize()
    {
        mDataBlockSize++;
    }

    uint32_t GetDataBlockSize() const
    {
        return mDataBlockSize;
    }

    void IncBlobNum(uint32_t blobNum)
    {
        mTotalBlobNum += blobNum;
    }

    uint32_t GetBlobNum()
    {
        return mTotalBlobNum;
    }

    void SetVersion(uint64_t version)
    {
        mVersion = version;
    }

    uint64_t GetVersion() const
    {
        return mVersion;
    }

    inline void IncBlobDeleteNum()
    {
        mDeletedBlobNum++;
    }

    BResult SnapshotMeta(OutputViewRef &outputViews)
    {
        RETURN_NOT_OK(outputViews->WriteUTF(mIdentifier));
        RETURN_NOT_OK(outputViews->WriteUint64(mFileAddress));
        RETURN_NOT_OK(outputViews->WriteUint64(mFileSize));
        RETURN_NOT_OK(outputViews->WriteUint64(mMinBlobId));
        RETURN_NOT_OK(outputViews->WriteUint64(mMaxBlobId));
        RETURN_NOT_OK(outputViews->WriteInt32(mValidGroupRange->GetStartGroup()));
        RETURN_NOT_OK(outputViews->WriteInt32(mValidGroupRange->GetEndGroup()));
        RETURN_NOT_OK(outputViews->WriteInt32(mCoveredGroupRange->GetStartGroup()));
        RETURN_NOT_OK(outputViews->WriteInt32(mCoveredGroupRange->GetEndGroup()));
        RETURN_NOT_OK(outputViews->WriteUint64(mMinExpireTime));
        RETURN_NOT_OK(outputViews->WriteUint64(mMaxExpireTime));
        RETURN_NOT_OK(outputViews->WriteUint32(mDataBlockSize));
        RETURN_NOT_OK(outputViews->WriteUint32(mTotalBlobNum));
        RETURN_NOT_OK(outputViews->WriteUint32(mDeletedBlobNum));
        RETURN_NOT_OK(outputViews->WriteUint64(mVersion));
        RETURN_NOT_OK(outputViews->WriteUint8(static_cast<uint8_t>(mFileStatus)));
        return BSS_OK;
    }

    BResult Restore(FileInputViewRef &inputView, RestoreFileInfo &restoreFileInfo, const PathRef &workingPath,
                    std::unordered_map<std::string, uint32_t> &restorePathFileIdMap)
    {
        RETURN_NOT_OK(inputView->ReadUTF(mIdentifier));
        mIdentifier = workingPath->Name() + "/blobFile/" + PathTransform::ExtractFileName(mIdentifier);
        auto it = restorePathFileIdMap.find(PathTransform::ExtractFileName(mIdentifier));
        if (it == restorePathFileIdMap.end()) {
            LOG_ERROR("Deserialize file from meta not find in restore path fileId path.");
            return BSS_ERR;
        }
        uint32_t fileId = it->second;
        auto fileAddress = FileAddressUtil::GetFileAddressWithZeroOffset(fileId);
        RETURN_NOT_OK(inputView->Read(mFileAddress));
        mFileAddress = fileAddress;
        RETURN_NOT_OK(inputView->Read(mFileSize));
        RETURN_NOT_OK(inputView->Read(mMinBlobId));
        RETURN_NOT_OK(inputView->Read(mMaxBlobId));
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
        RETURN_NOT_OK(inputView->Read(mMinExpireTime));
        RETURN_NOT_OK(inputView->Read(mMaxExpireTime));
        RETURN_NOT_OK(inputView->Read(mDataBlockSize));
        RETURN_NOT_OK(inputView->Read(mTotalBlobNum));
        RETURN_NOT_OK(inputView->Read(mDeletedBlobNum));
        RETURN_NOT_OK(inputView->Read(mVersion));
        RETURN_NOT_OK(inputView->Read(mFileStatus));
        restoreFileInfo = {
            mIdentifier, mFileAddress, mFileStatus, mFileSize, 1, mFileAddress, mIdentifier, mIdentifier
        };
        return BSS_OK;
    }

    double EstimateDeleteRatio() const
    {
        if (UNLIKELY(mTotalBlobNum == 0)) {
            LOG_ERROR("total blob num is zero");
            return DOUBLE_MAX_VALUE;
        }
        double keyGroupNotContainsRatio = 1.0 - mValidGroupRange->GetKeyGroupRangeSize() * 1.0 /
                                                    mCoveredGroupRange->GetKeyGroupRangeSize();
        uint64_t totalDeleteNum = mDeletedBlobNum + static_cast<uint64_t>(keyGroupNotContainsRatio * mTotalBlobNum);
        if (totalDeleteNum >= mTotalBlobNum) {
            return 1.0;
        }
        return static_cast<double>(totalDeleteNum * 1.0 / mTotalBlobNum);
    }

    inline uint64_t GetMaxExpireTime() const
    {
        return mMaxExpireTime;
    }

    inline uint64_t GetMinExpireTime() const
    {
        return mMinExpireTime;
    }

private:
    uint64_t mMinExpireTime = UINT64_MAX;
    uint64_t mMaxExpireTime = 0;
    uint32_t mDataBlockSize = 0;
    uint32_t mTotalBlobNum = 0;
    uint32_t mDeletedBlobNum = 0;
    uint64_t mVersion = 0;
    FileStatus mFileStatus = LOCAL;
};
using BlobFileMetaRef = std::shared_ptr<BlobFileMeta>;
}  // namespace bss
}  // namespace ock

#endif
