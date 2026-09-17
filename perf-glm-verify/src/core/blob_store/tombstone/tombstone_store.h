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
#ifndef BOOST_SS_TOMBSTONE_STORE_H
#define BOOST_SS_TOMBSTONE_STORE_H

#include <memory>
#include <vector>

#include "include/config.h"
#include "tombstone.h"
#include "tombstone_file_manager.h"
#include "tombstone_file_writer.h"
#include "tombstone_structure.h"

namespace ock {
namespace bss {

class TombstoneStore {
public:
    TombstoneStore(const ConfigRef &config, const TombstoneFileManagerRef &fileManager, const MemManagerRef &memManager)
        : mConfig(config), mFileManager(fileManager), mMemManager(memManager)
    {
    }

    void DeleteBlob(uint64_t blobId, uint16_t keyGroup)
    {
        mMemCache.emplace_back(std::make_shared<Tombstone>(blobId, keyGroup));
        if (mMemCache.size() < mConfig->mMaxBlobNumInMemCache) {
            return;
        }
        TombstoneFileWriterRef fileWriter = std::make_shared<TombstoneFileWriter>(mConfig, mFileManager, 0,
                                                                                  mMemManager);
        std::set<TombstoneRef, CompareTombstone> flushSet;
        flushSet.insert(mMemCache.begin(), mMemCache.end());
        std::vector<TombstoneFileRef> fileVec;
        BResult ret = fileWriter->Flush(flushSet, fileVec);
        if (UNLIKELY(ret != BSS_OK)) {
            LOG_ERROR("flush memcache failed, ret:" << ret);
            return;
        }
        {
            if (!fileVec.empty()) {
                WriteLocker<ReadWriteLock> lock(&mLock);
                mWrittenFile.insert(mWrittenFile.end(), fileVec.begin(), fileVec.end());
            }
        }
        mMemCache.clear();
    }

    inline const std::vector<TombstoneRef> &GetMemCache()
    {
        return mMemCache;
    }

    inline const std::vector<TombstoneFileRef> &GetWrittenFiles()
    {
        ReadLocker<ReadWriteLock> lock(&mLock);
        return mWrittenFile;
    }

    inline bool Empty()
    {
        ReadLocker<ReadWriteLock> lock(&mLock);
        return mMemCache.empty() && mWrittenFile.empty();
    }

    inline void Clear()
    {
        WriteLocker<ReadWriteLock> lock(&mLock);
        mWrittenFile.clear();
        mMemCache.clear();
    }

    void Discard()
    {
        {
            ReadLocker<ReadWriteLock> lock(&mLock);
            if (!mWrittenFile.empty()) {
                mFileManager->DiscardFile(mWrittenFile);
            }
        }
        Clear();
    }

private:
    std::vector<TombstoneRef> mMemCache;
    ConfigRef mConfig = nullptr;
    TombstoneFileManagerRef mFileManager = nullptr;
    std::vector<TombstoneFileRef> mWrittenFile;
    MemManagerRef mMemManager = nullptr;
    ReadWriteLock mLock;
};
using TombstoneStoreRef = std::shared_ptr<TombstoneStore>;
}  // namespace bss
}  // namespace ock

#endif  // BOOST_SS_TOMBSTONE_STORE_H
