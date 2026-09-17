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

#ifndef BOOST_SS_FILE_META_H
#define BOOST_SS_FILE_META_H

#include <atomic>
#include <memory>

#include "common/path.h"
#include "file_id.h"

namespace ock {
namespace bss {
class FileMeta {
public:
    FileMeta(const PathRef &path, const FileIdRef &fileId, bool deleted)
        : mFilePath(path), mFileId(fileId), mCanDeleted(deleted)
    {
    }

    ~FileMeta() = default;

    inline PathRef GetFilePath() const
    {
        return mFilePath;
    }

    inline int32_t AddDbRef(int32_t ref)
    {
        return mDbRef.fetch_add(ref);
    }

    inline void SetLastEpochToDecDbRef(int64_t epoch)
    {
        SafeSet(mLastEpochToDecDbRef, epoch);
    }

    inline void SetLastTimestampToDecDbRef(int64_t timestamp)
    {
        SafeSet(mLastTimestampToDecDbRef, timestamp);
    }

    inline uint64_t AddDataSize(uint32_t size)
    {
        return mDataSize.fetch_add(size);
    }

    inline uint64_t SubDataSize(uint32_t size)
    {
        return mDataSize.fetch_sub(size);
    }

    inline int64_t GetLastEpochToDecDbRef()
    {
        return mLastEpochToDecDbRef.load();
    }

    inline uint32_t AddSnapshotRef(uint32_t ref)
    {
        return mSnapshotRef.fetch_add(ref);
    }

    inline FileIdRef GetFileId() const
    {
        return mFileId;
    }

    inline bool IsCanDelete() const
    {
        return mCanDeleted;
    }

    inline uint64_t GetFileSize()
    {
        return mFileSize.load();
    }

    inline void AddFileSize(uint64_t size)
    {
        mFileSize.fetch_add(size);
    }

    inline std::string ToString()
    {
        std::ostringstream oss;
        oss << "FileMeta[filePath:" << mFilePath->ExtractFileName() << ", fileId:" << mFileId->Get()
            << ", canDeleted:" << mCanDeleted << "]";
        return oss.str();
    }

private:
    template <class T> void SafeSet(std::atomic<T> &toSet, T value)
    {
        T oldValue = toSet.load();
        while (oldValue < value && !toSet.compare_exchange_weak(oldValue, value)) {
            oldValue = toSet.load();
        }
    }

private:
    PathRef mFilePath = nullptr;
    FileIdRef mFileId = nullptr;
    bool mCanDeleted = false;
    std::atomic<uint64_t> mFileSize{ 0 };  // 文件大小.
    std::atomic<uint64_t> mDataSize{ 0 };  // 文件中的数据大小.
    std::atomic<int32_t> mDbRef{ 0 };
    std::atomic<int64_t> mLastTimestampToDecDbRef{ -1 };
    std::atomic<int64_t> mLastEpochToDecDbRef{ -1 };
    std::atomic<uint32_t> mSnapshotRef{ 0 };
};
using FileMetaRef = std::shared_ptr<FileMeta>;

}  // namespace bss
}  // namespace ock
#endif  // BOOST_SS_FILE_META_H