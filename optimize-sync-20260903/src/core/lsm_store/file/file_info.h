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

#ifndef BOOST_SS_FILE_INFO_H
#define BOOST_SS_FILE_INFO_H

#include "common/path.h"
#include "file_cache_type.h"
#include "file_id.h"

namespace ock {
namespace bss {
class FileInfo {
public:
    FileInfo(const PathRef &path, const FileIdRef &fileId) : mFilePath(path), mFileId(fileId)
    {
    }

    inline PathRef GetFilePath() const
    {
        return mFilePath;
    }

    inline FileIdRef GetFileId() const
    {
        return mFileId;
    }

    inline void SetFileStatus(FileStatus fileStatus)
    {
        mFileStatus = fileStatus;
    }

    inline FileStatus GetFileStatus() const
    {
        return mFileStatus;
    }

private:
    PathRef mFilePath = nullptr;
    FileIdRef mFileId = nullptr;
    FileStatus mFileStatus = LOCAL;
};
using FileInfoRef = std::shared_ptr<FileInfo>;

}  // namespace bss
}  // namespace ock
#endif  // BOOST_SS_FILE_INFO_H