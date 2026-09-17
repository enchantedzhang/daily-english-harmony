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

#ifndef BOOST_SS_FILE_CACHE_FACTORY_H
#define BOOST_SS_FILE_CACHE_FACTORY_H

#include <memory>

#include "file_cache_manager.h"
#include "file_id_generator.h"
#include "file_manager.h"

namespace ock {
namespace bss {
class FileCacheFactory {
public:
    FileCacheFactory(const ConfigRef &conf, const ExecutorServiceRef &ioExecutor, BoostNativeMetricPtr *metricPtrAddr)
    {
        PathRef localBasePath = std::make_shared<Path>(Uri(conf->GetLocalPath()));
        PathRef dfsBasePath = std::make_shared<Path>(Uri(conf->GetRemotePath()));
        std::string identifier = conf->GetBackendUID();
        FileModeInfo fileModeInfo;
        mLocalFileIdGenerator = std::make_shared<FileIdGenerator>(fileModeInfo);
        mLocalFileManager = std::make_shared<FileManager>(localBasePath, identifier, mLocalFileIdGenerator, ioExecutor,
                                                          false);
        mDfsFileManager = std::make_shared<FileManager>(dfsBasePath, identifier, mLocalFileIdGenerator, ioExecutor,
                                                        true);
        mInfiniteFileCache = std::make_shared<FileCacheManager>(conf, mLocalFileManager, mDfsFileManager, ioExecutor);
        mInfiniteFileCache->Init(metricPtrAddr);
    }

    ~FileCacheFactory()
    {
        mInfiniteFileCache->Close();
    }

    inline FileManagerRef GetLocalFileManager() const
    {
        return mLocalFileManager;
    }

    inline FileManagerRef GetDfsFileManager() const
    {
        return mDfsFileManager;
    }

    inline FileIdGeneratorRef GetLocalFileIdGenerator() const
    {
        return mLocalFileIdGenerator;
    }

    inline FileCacheManagerRef GetFileCache() const
    {
        return mInfiniteFileCache;
    }

private:
    FileIdGeneratorRef mLocalFileIdGenerator = nullptr;
    FileIdGeneratorRef mDfsFileIdGenerator = nullptr;
    FileManagerRef mLocalFileManager = nullptr;
    FileManagerRef mDfsFileManager = nullptr;
    FileCacheManagerRef mInfiniteFileCache = nullptr;
};
using FileCacheFactoryRef = std::shared_ptr<FileCacheFactory>;

}  // namespace bss
}  // namespace ock
#endif  // BOOST_SS_FILE_CACHE_FACTORY_H