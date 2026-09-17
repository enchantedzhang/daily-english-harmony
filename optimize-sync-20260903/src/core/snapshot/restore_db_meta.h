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

#ifndef BOOST_SS_RESTORE_DB_META_H
#define BOOST_SS_RESTORE_DB_META_H

namespace ock {
namespace bss {

class SnapshotFileInfo {
public:
    SnapshotFileInfo(std::string fileName, uint32_t fileId, uint64_t fileSize)
        : mFileName(fileName), mFileId(fileId), mFileSize(fileSize)
    {
    }

    inline std::string GetFileName() const
    {
        return mFileName;
    }

    inline uint32_t GetFileId() const
    {
        return mFileId;
    }

    inline uint64_t GetFileSize() const
    {
        return mFileSize;
    }

    inline void SetFileId(uint32_t fileId)
    {
        mFileId = fileId;
    }

    inline std::string ToString()
    {
        std::ostringstream oss;
        oss << "[fileName:" << PathTransform::ExtractFileName(mFileName) << ", fileId:" << mFileId
            << ", fileSize:" << mFileSize << "]";
        return oss.str();
    }

private:
    std::string mFileName;
    uint32_t mFileId = 0;
    uint64_t mFileSize = 0;
};
using SnapshotFileInfoRef = std::shared_ptr<SnapshotFileInfo>;

class SnapshotFileMapping {
public:
    SnapshotFileMapping(const PathRef &basePath, std::vector<SnapshotFileInfoRef> fileMapping)
    {
        mBasePath = basePath;
        mFileMapping = fileMapping;
    }

    inline PathRef GetBasePath() const
    {
        return mBasePath;
    }

    inline std::vector<SnapshotFileInfoRef> GetFileMapping() const
    {
        return mFileMapping;
    }

    inline std::string ToString()
    {
        std::ostringstream oss;
        oss << "SnapshotFileMapping ,basePath:" << mBasePath->ExtractFileName()
            << ", fileMapping size:" << mFileMapping.size() << ", ";
        for (const auto &item : mFileMapping) {
            oss << item->ToString();
        }
        return oss.str();
    }

private:
    PathRef mBasePath = nullptr;
    std::vector<SnapshotFileInfoRef> mFileMapping;
};
using SnapshotFileMappingRef = std::shared_ptr<SnapshotFileMapping>;

}  // namespace bss
}  // namespace ock
#endif  // BOOST_SS_RESTORE_DB_META_H