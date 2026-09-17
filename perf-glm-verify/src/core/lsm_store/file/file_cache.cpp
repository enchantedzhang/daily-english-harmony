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

#include "file_cache.h"

namespace ock {
namespace bss {
BResult FileCache::Get(uint64_t fileAddress, const Key &key, Value &value)
{
    FileProcHolder holder = FileProcHolder::FILE_STORE_GET;  // 标记该流程是Get读流程
    FileAccessRef fileAccess;
    {
        ReadLocker<ReadWriteLock> lk(&mRwLock);
        auto item = mAccessMap.find(fileAddress);
        if (UNLIKELY(item == mAccessMap.end())) {
            return BSS_NOT_EXISTS;
        }
        fileAccess = item->second;
    }
    auto fileReader = fileAccess->GetFileReader(holder, mBoostNativeMetric);
    if (UNLIKELY(fileReader == nullptr)) {
        LOG_ERROR("Get file reader failed, holder:" << static_cast<uint32_t>(holder));
        return BSS_ERR;
    }
    return fileReader->Get(key, value);
}

void FileCache::Restore(const VersionPtr &restoreVersion)
{
    RETURN_AS_NULLPTR(restoreVersion);
    for (auto level : restoreVersion->GetLevels()) {
        for (const auto &fileMetaDataGroup : level.GetFileMetaDataGroups()) {
            for (const auto &file : fileMetaDataGroup->GetFiles()) {
                ReadLocker<ReadWriteLock> lk(&mRwLock);
                if (mAccessMap.count(file->GetFileAddress()) == 0) {
                    auto fileAccess = std::make_shared<FileAccess>(file, mFileFactory, mFileCache, mMemManager);
                    mAccessMap.emplace(file->GetFileAddress(), fileAccess);
                }
            }
        }
    }
}

}  // namespace bss
}  // namespace ock