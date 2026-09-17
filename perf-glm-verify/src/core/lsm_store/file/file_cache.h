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

#ifndef BOOST_SS_FILE_CACHE_H
#define BOOST_SS_FILE_CACHE_H

#include <map>

#include "common/util/bss_lock.h"
#include "common/util/string_util.h"
#include "compressor_utils.h"
#include "file_cache_manager.h"
#include "file_factory.h"
#include "file_writer.h"
#include "include/config.h"
#include "lsm_store/block/file_access.h"
#include "lsm_store/key/full_key_filter.h"
#include "lsm_store/version/version.h"

namespace ock {
namespace bss {
class FileCache {
public:
    FileCache(const ConfigRef &config, const FileFactoryRef &factory, const FileCacheManagerRef &fileCache,
              const MemManagerRef &memManager)
        : mConfig(config), mFileFactory(factory), mFileCache(fileCache), mMemManager(memManager), mFileStoreSeqId(0)
    {
        mCompressionLevelPolicy = config->GetCompressionLevelPolicy();
        mLsmStoreCompressionPolicy = config->GetLsmStoreCompressionPolicy();
        mZeroCopySwitch = config->GetZeroCopySwitch();
        PrintCompressionPolicy();
    }

    ~FileCache()
    {
        std::map<uint64_t, FileAccessRef>().swap(mAccessMap);
        LOG_DEBUG("Delete FileCache success.");
    }

    inline FileWriterRef CreateBuilder(const PathRef &path, uint32_t levelId, FileProcHolder holder)
    {
        CompressAlgo compressPolicy;
        if (mCompressionLevelPolicy.size() > levelId) {
            compressPolicy = mCompressionLevelPolicy.at(levelId);
        } else {
            compressPolicy = mLsmStoreCompressionPolicy;
        }
        LOG_DEBUG("Write file using compression policy: "
                  << CompressAlgoUtil::ReverseCompressAlgoTransform(compressPolicy) << ", level: " << levelId << ".");
        // The current reuse planner only participates in L0-to-L1 compaction.
        bool maintainRecordMeta = mZeroCopySwitch && levelId <= 1;
        return mFileFactory->CreateFileWriter(path, mConfig, compressPolicy, mMemManager, holder, maintainRecordMeta);
    }

    BResult Get(uint64_t fileAddress, const Key &key, Value &value);

    inline void FinishBuilder(uint32_t fileId, FileMetaDataRef &fileMetaData)
    {
        WriteLocker<ReadWriteLock> lk(&mRwLock);
        uint64_t fileAddress = FileAddressUtil::GetFileAddressWithZeroOffset(fileId);
        auto fileAccess = std::make_shared<FileAccess>(fileMetaData, mFileFactory, mFileCache, mMemManager);
        mAccessMap.emplace(fileAddress, fileAccess);
        LOG_DEBUG("Generated new file fileId: " << fileId << " with " << fileMetaData->GetFileSize() << " bytes.");
    }

    void RemoveFile(uint64_t fileAddress)
    {
        WriteLocker<ReadWriteLock> lk(&mRwLock);
        LOG_DEBUG("Begin to remove file from access map, file address:" << fileAddress);
        auto iter = mAccessMap.find(fileAddress);
        if (UNLIKELY(iter == mAccessMap.end())) {
            return;
        }
        mAccessMap.erase(fileAddress);
    }

    inline FileFactoryRef GetFileFactory() const
    {
        return mFileFactory;
    }

    KeyValueIteratorRef PrefixIterator(FullKeyFilterRef keyFilter, uint64_t fileAddress, const Key &prefixKey,
                                       bool reverseOrder, FileProcHolder holder)
    {
        FileAccessRef fileAccess = nullptr;
        {
            ReadLocker<ReadWriteLock> lk(&mRwLock);
            auto iter = mAccessMap.find(fileAddress);
            if (iter == mAccessMap.end()) {
                LOG_ERROR(fileAddress << " not found at mAccessMap.");
                return nullptr;
            }
            fileAccess = iter->second;
        }
        RETURN_NULLPTR_AS_NULLPTR(fileAccess);
        auto fileReader = fileAccess->GetFileReader(holder, mBoostNativeMetric);
        if (fileReader == nullptr) {
            LOG_ERROR("Failed to GetFileReader in PrefixIterator.");
            return nullptr;
        }
        return fileReader->PrefixIterator(keyFilter, prefixKey, reverseOrder);
    }

    KeyValueIteratorRef IteratorAll(FullKeyFilterRef keyFilter, uint64_t fileAddress, FileProcHolder holder)
    {
        return IteratorAll(keyFilter, fileAddress, true, holder);
    }

    KeyValueIteratorRef IteratorAll(FullKeyFilterRef keyFilter, uint64_t fileAddress, bool shareFileReader,
                                    FileProcHolder holder)
    {
        FileAccessRef fileAccess = nullptr;
        {
            ReadLocker<ReadWriteLock> lk(&mRwLock);
            fileAccess = mAccessMap.at(fileAddress);
        }
        if (UNLIKELY(fileAccess == nullptr)) {
            LOG_ERROR("Invalid fileAddress :" << fileAddress << ".");
            return nullptr;
        }
        FileReaderBaseRef fileReader = nullptr;
        if (shareFileReader) {
            fileReader = fileAccess->GetFileReader(holder, mBoostNativeMetric);
        } else {
            fileReader = fileAccess->CreateFileReader(holder, mBoostNativeMetric);
        }
        if (UNLIKELY(fileReader == nullptr)) {
            if (FileMemAllocator::IsForceType(holder)) {
                LOG_ERROR("New iterator failed, sharedFlag:" << shareFileReader << ", fileAddress:" << fileAddress
                                                             << ", holder:" << static_cast<uint32_t>(holder));
            } else {
                LOG_WARN("New iterator failed, sharedFlag:" << shareFileReader << ", fileAddress:" << fileAddress
                                                            << ", holder:" << static_cast<uint32_t>(holder));
            }
            return nullptr;
        }
        return fileReader->IteratorAll(keyFilter, shareFileReader);
    }

    inline void SetFileStoreSeqId(uint32_t fileStoreSeqId)
    {
        mFileStoreSeqId = fileStoreSeqId;
    }

    inline void PrintCompressionPolicy()
    {
        std::vector<std::string> strVecter;
        strVecter.reserve(mCompressionLevelPolicy.size());
        for (auto &compressionItem : mCompressionLevelPolicy) {
            strVecter.emplace_back(CompressAlgoUtil::ReverseCompressAlgoTransform(compressionItem));
        }
        LOG_INFO("Lsm compressionPolicy: "
                 << CompressAlgoUtil::ReverseCompressAlgoTransform(mLsmStoreCompressionPolicy)
                 << ", compressionLevelPolicy: "
                 << StringUtil::MergeVectorToString(strVecter, mConfig->GetFileStoreNumLevels()) << ".");
    }

    inline void RegisterMetric(BoostNativeMetricPtr metricPtr)
    {
        mBoostNativeMetric = metricPtr;
    }

    void Restore(const VersionPtr &restoreVersion);

private:
    ConfigRef mConfig;
    FileFactoryRef mFileFactory;
    std::map<uint64_t, FileAccessRef> mAccessMap;
    FileCacheManagerRef mFileCache;
    MemManagerRef mMemManager;
    std::vector<CompressAlgo> mCompressionLevelPolicy;
    CompressAlgo mLsmStoreCompressionPolicy;
    bool mZeroCopySwitch = false;
    uint32_t mFileStoreSeqId;
    ReadWriteLock mRwLock;
    BoostNativeMetricPtr mBoostNativeMetric = nullptr;
};
using FileCacheRef = std::shared_ptr<FileCache>;

}  // namespace bss
}  // namespace ock
#endif  // BOOST_SS_FILE_CACHE_H
