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

#ifndef BOOST_SS_TEST_KV_TABLE_H
#define BOOST_SS_TEST_KV_TABLE_H

#include <dirent.h>
#include <ftw.h>

#include <cerrno>
#include <filesystem>
#include <iostream>

#include "../fuzz_main.h"
#include "common/bss_log.h"
#include "gtest/gtest.h"
#include "hash.h"
#include "include/boost_state_db.h"
#include "include/boost_state_table.h"
#include "slice_table/slice_table.h"
#include "test_utils.h"

namespace ock {
namespace bss {
namespace test {
namespace test_table {
BoostStateDB *mDB;
BoostStateDB *mDB1;
KVTableRef kVTable;
NsKVTableRef nsKVTable;
KMapTableRef kMapTable;
NsKMapTableRef nsKMapTable;
KListTableRef kListTable;
NsKListTableRef nsKListTable;

std::vector<std::vector<uint8_t>> sourceKey;
std::vector<std::vector<uint8_t>> sourceSubKey;
std::vector<std::vector<uint8_t>> sourceNS;

class TestTable : public ::testing::Test {
public:
    TestTable() = default;
    void SetUp() override;
    void TearDown() override;
    static void SetUpTestCase();

    void CloseAllDb()
    {
        std::cout << "inside CloseAllDb" << std::endl;
        if (mDB != nullptr) {
            mDB->Close();
            delete mDB;
            mDB = nullptr;
        }
        if (mDB1 != nullptr) {
            mDB1->Close();
            delete mDB1;
            mDB1 = nullptr;
        }
    }

    inline void PutKv(size_t segmentSize)
    {
        int index = 0;
        uint32_t keyLength = *(uint32_t *)DT_SetGetNumberRange(&g_Element[index++], 0x1, 0x1, segmentSize / 2);
        uint8_t keyByte = *(uint8_t *)DT_SetGetNumberRange(&g_Element[index++], 0x9, 0x1, UINT8_MAX);
        std::vector<uint8_t> rawKey = GetRandomData(keyLength, keyByte);

        uint32_t valueLength = *(uint32_t *)DT_SetGetNumberRange(&g_Element[index++], 0x1, 0x1, segmentSize / 2);
        uint8_t valueByte = *(uint8_t *)DT_SetGetNumberRange(&g_Element[index++], 0x9, 0x1, UINT8_MAX);
        std::vector<uint8_t> rawValue = GetRandomData(valueLength, valueByte);

        const BinaryData priKey(rawKey.data(), rawKey.size());
        const BinaryData value(rawValue.data(), rawValue.size());
        uint32_t keyHashCode = HashCode::Hash(rawKey.data(), rawKey.size());
        kVTable->Put(keyHashCode, priKey, value);
    }

    inline void GetKv(size_t segmentSize)
    {
        int index = 0;
        uint32_t keyLength = *(uint32_t *)DT_SetGetNumberRange(&g_Element[index++], 0x1, 0x1, segmentSize - 1);
        uint8_t keyByte = *(uint8_t *)DT_SetGetNumberRange(&g_Element[index++], 0x9, 0x1, UINT8_MAX);
        std::vector<uint8_t> rawKey = GetRandomData(keyLength, keyByte);

        const BinaryData priKey(rawKey.data(), rawKey.size());
        BinaryData value;
        uint32_t keyHashCode = HashCode::Hash(rawKey.data(), rawKey.size());
        kVTable->Get(keyHashCode, priKey, value);
    }

    inline void ContainKv(size_t segmentSize)
    {
        int index = 0;
        uint32_t keyLength = *(uint32_t *)DT_SetGetNumberRange(&g_Element[index++], 0x1, 0x1, segmentSize - 1);
        uint8_t keyByte = *(uint8_t *)DT_SetGetNumberRange(&g_Element[index++], 0x9, 0x1, UINT8_MAX);
        std::vector<uint8_t> rawKey = GetRandomData(keyLength, keyByte);

        const BinaryData priKey(rawKey.data(), rawKey.size());
        uint32_t keyHashCode = HashCode::Hash(rawKey.data(), rawKey.size());
        kVTable->Contain(keyHashCode, priKey);
    }

    inline void RemoveKv(size_t segmentSize)
    {
        int index = 0;
        uint32_t keyLength = *(uint32_t *)DT_SetGetNumberRange(&g_Element[index++], 0x1, 0x1, segmentSize - 1);
        uint8_t keyByte = *(uint8_t *)DT_SetGetNumberRange(&g_Element[index++], 0x9, 0x1, UINT8_MAX);
        std::vector<uint8_t> rawKey = GetRandomData(keyLength, keyByte);

        const BinaryData priKey(rawKey.data(), rawKey.size());
        uint32_t keyHashCode = HashCode::Hash(rawKey.data(), rawKey.size());
        kVTable->Remove(keyHashCode, priKey);
    }

    inline void PutKList(size_t segmentSize)
    {
        int index = 0;
        uint32_t keyLength = *(uint32_t *)DT_SetGetNumberRange(&g_Element[index++], 0x1, 0x1, segmentSize / 2);
        uint8_t keyByte = *(uint8_t *)DT_SetGetNumberRange(&g_Element[index++], 0x9, 0x1, UINT8_MAX);
        std::vector<uint8_t> rawKey = GetRandomData(keyLength, keyByte);

        uint32_t valueLength = *(uint32_t *)DT_SetGetNumberRange(&g_Element[index++], 0x1, 0x1, segmentSize / 2);
        uint8_t valueByte = *(uint8_t *)DT_SetGetNumberRange(&g_Element[index++], 0x9, 0x1, UINT8_MAX);
        std::vector<uint8_t> rawValue = GetRandomData(valueLength, valueByte);

        const BinaryData priKey(rawKey.data(), rawKey.size());
        const BinaryData value(rawValue.data(), rawValue.size());
        uint32_t keyHashCode = HashCode::Hash(rawKey.data(), rawKey.size());
        kListTable->Put(keyHashCode, priKey, value);
    }

    inline void GetKList(size_t segmentSize)
    {
        int index = 0;
        uint32_t keyLength = *(uint32_t *)DT_SetGetNumberRange(&g_Element[index++], 0x1, 0x1, segmentSize / 2);
        uint8_t keyByte = *(uint8_t *)DT_SetGetNumberRange(&g_Element[index++], 0x9, 0x1, UINT8_MAX);
        std::vector<uint8_t> rawKey = GetRandomData(keyLength, keyByte);
        const BinaryData priKey(rawKey.data(), rawKey.size());
        BinaryData value;
        uint32_t keyHashCode = HashCode::Hash(rawKey.data(), rawKey.size());
        kListTable->Get(keyHashCode, priKey);
    }

    inline void ContainKList(size_t segmentSize)
    {
        int index = 0;
        uint32_t keyLength = *(uint32_t *)DT_SetGetNumberRange(&g_Element[index++], 0x1, 0x1, segmentSize / 2);
        uint8_t keyByte = *(uint8_t *)DT_SetGetNumberRange(&g_Element[index++], 0x9, 0x1, UINT8_MAX);
        std::vector<uint8_t> rawKey = GetRandomData(keyLength, keyByte);
        const BinaryData priKey(rawKey.data(), rawKey.size());
        uint32_t keyHashCode = HashCode::Hash(rawKey.data(), rawKey.size());
        kListTable->Contain(keyHashCode, priKey);
    }

    inline void AddKList(size_t segmentSize)
    {
        int index = 0;
        uint32_t keyLength = *(uint32_t *)DT_SetGetNumberRange(&g_Element[index++], 0x1, 0x1, segmentSize / 2);
        uint8_t keyByte = *(uint8_t *)DT_SetGetNumberRange(&g_Element[index++], 0x9, 0x1, UINT8_MAX);
        std::vector<uint8_t> rawKey = GetRandomData(keyLength, keyByte);

        uint32_t valueLength = *(uint32_t *)DT_SetGetNumberRange(&g_Element[index++], 0x1, 0x1, segmentSize / 2);
        uint8_t valueByte = *(uint8_t *)DT_SetGetNumberRange(&g_Element[index++], 0x9, 0x1, UINT8_MAX);
        std::vector<uint8_t> rawValue = GetRandomData(valueLength, valueByte);

        const BinaryData priKey(rawKey.data(), rawKey.size());
        const BinaryData value(rawValue.data(), rawValue.size());
        uint32_t keyHashCode = HashCode::Hash(rawKey.data(), rawKey.size());
        kListTable->Add(keyHashCode, priKey, value);
    }

    inline void RemoveKList(size_t segmentSize)
    {
        int index = 0;
        uint32_t keyLength = *(uint32_t *)DT_SetGetNumberRange(&g_Element[index++], 0x1, 0x1, segmentSize / 2);
        uint8_t keyByte = *(uint8_t *)DT_SetGetNumberRange(&g_Element[index++], 0x9, 0x1, UINT8_MAX);
        std::vector<uint8_t> rawKey = GetRandomData(keyLength, keyByte);

        const BinaryData priKey(rawKey.data(), rawKey.size());
        uint32_t keyHashCode = HashCode::Hash(rawKey.data(), rawKey.size());
        kListTable->Remove(keyHashCode, priKey);
    }

    inline void AddKMap(size_t segmentSize)
    {
        int index = 0;
        uint32_t keyLength = *(uint32_t *)DT_SetGetNumberRange(&g_Element[index++], 0x1, 0x1, segmentSize / 3);
        uint8_t keyByte = *(uint8_t *)DT_SetGetNumberRange(&g_Element[index++], 0x9, 0x1, UINT8_MAX);
        std::vector<uint8_t> rawKey = GetRandomData(keyLength, keyByte);

        uint32_t keyLength1 = *(uint32_t *)DT_SetGetNumberRange(&g_Element[index++], 0x1, 0x1, segmentSize / 3);
        uint8_t keyByte1 = *(uint8_t *)DT_SetGetNumberRange(&g_Element[index++], 0x9, 0x1, UINT8_MAX);
        std::vector<uint8_t> rawKey1 = GetRandomData(keyLength1, keyByte1);

        uint32_t valueLength = *(uint32_t *)DT_SetGetNumberRange(&g_Element[index++], 0x1, 0x1, segmentSize / 3);
        uint8_t valueByte = *(uint8_t *)DT_SetGetNumberRange(&g_Element[index++], 0x9, 0x1, UINT8_MAX);
        std::vector<uint8_t> rawValue = GetRandomData(valueLength, valueByte);

        const BinaryData priKey(rawKey.data(), rawKey.size());
        const BinaryData secKey(rawKey1.data(), rawKey1.size());
        const BinaryData value(rawValue.data(), rawValue.size());
        uint32_t keyHashCode = HashCode::Hash(rawKey.data(), rawKey.size());
        kMapTable->Put(keyHashCode, priKey, secKey, value);
    }

    inline void AddNSKMap(size_t segmentSize)
    {
        int index = 0;
        uint32_t keyLength = *(uint32_t *)DT_SetGetNumberRange(&g_Element[index++], 0x1, 0x1, segmentSize / 3);
        uint8_t keyByte = *(uint8_t *)DT_SetGetNumberRange(&g_Element[index++], 0x9, 0x1, UINT8_MAX);
        std::vector<uint8_t> rawKey = GetRandomData(keyLength, keyByte);

        uint32_t keyLength1 = *(uint32_t *)DT_SetGetNumberRange(&g_Element[index++], 0x1, 0x1, segmentSize / 3);
        uint8_t keyByte1 = *(uint8_t *)DT_SetGetNumberRange(&g_Element[index++], 0x9, 0x1, UINT8_MAX);
        std::vector<uint8_t> rawKey1 = GetRandomData(keyLength1, keyByte1);

        uint32_t valueLength = *(uint32_t *)DT_SetGetNumberRange(&g_Element[index++], 0x1, 0x1, segmentSize / 3);
        uint8_t valueByte = *(uint8_t *)DT_SetGetNumberRange(&g_Element[index++], 0x9, 0x1, UINT8_MAX);
        std::vector<uint8_t> rawValue = GetRandomData(valueLength, valueByte);

        const BinaryData priKey(rawKey.data(), rawKey.size());
        const BinaryData secKey(rawKey1.data(), rawKey1.size());
        const BinaryData value(rawValue.data(), rawValue.size());
        uint32_t keyHashCode = HashCode::Hash(rawKey.data(), rawKey.size());
        nsKMapTable->Put(keyHashCode, priKey, secKey, value);
    }

    inline void ContainKMap(size_t segmentSize)
    {
        int index = 0;
        uint32_t keyLength = *(uint32_t *)DT_SetGetNumberRange(&g_Element[index++], 0x1, 0x1, segmentSize / 2);
        uint8_t keyByte = *(uint8_t *)DT_SetGetNumberRange(&g_Element[index++], 0x9, 0x1, UINT8_MAX);
        std::vector<uint8_t> rawKey = GetRandomData(keyLength, keyByte);

        const BinaryData priKey(rawKey.data(), rawKey.size());
        uint32_t keyHashCode = HashCode::Hash(rawKey.data(), rawKey.size());
        kMapTable->Contain(keyHashCode, priKey);
    }

    inline void ContainNSKMap(size_t segmentSize)
    {
        int index = 0;
        uint32_t keyLength = *(uint32_t *)DT_SetGetNumberRange(&g_Element[index++], 0x1, 0x1, segmentSize / 2);
        uint8_t keyByte = *(uint8_t *)DT_SetGetNumberRange(&g_Element[index++], 0x9, 0x1, UINT8_MAX);
        std::vector<uint8_t> rawKey = GetRandomData(keyLength, keyByte);

        const BinaryData priKey(rawKey.data(), rawKey.size());
        uint32_t keyHashCode = HashCode::Hash(rawKey.data(), rawKey.size());
        nsKMapTable->Contain(keyHashCode, priKey);
    }

    inline void ContainKMap1(size_t segmentSize)
    {
        int index = 0;
        uint32_t keyLength = *(uint32_t *)DT_SetGetNumberRange(&g_Element[index++], 0x1, 0x1, segmentSize / 2);
        uint8_t keyByte = *(uint8_t *)DT_SetGetNumberRange(&g_Element[index++], 0x9, 0x1, UINT8_MAX);
        std::vector<uint8_t> rawKey = GetRandomData(keyLength, keyByte);

        uint32_t keyLength1 = *(uint32_t *)DT_SetGetNumberRange(&g_Element[index++], 0x1, 0x1, segmentSize / 3);
        uint8_t keyByte1 = *(uint8_t *)DT_SetGetNumberRange(&g_Element[index++], 0x9, 0x1, UINT8_MAX);
        std::vector<uint8_t> rawKey1 = GetRandomData(keyLength1, keyByte1);

        const BinaryData priKey(rawKey.data(), rawKey.size());
        const BinaryData secKey(rawKey1.data(), rawKey1.size());
        uint32_t keyHashCode = HashCode::Hash(rawKey.data(), rawKey.size());
        kMapTable->Contain(keyHashCode, priKey, secKey);
    }

    inline void ContainNSKMap1(size_t segmentSize)
    {
        int index = 0;
        uint32_t keyLength = *(uint32_t *)DT_SetGetNumberRange(&g_Element[index++], 0x1, 0x1, segmentSize / 2);
        uint8_t keyByte = *(uint8_t *)DT_SetGetNumberRange(&g_Element[index++], 0x9, 0x1, UINT8_MAX);
        std::vector<uint8_t> rawKey = GetRandomData(keyLength, keyByte);

        uint32_t keyLength1 = *(uint32_t *)DT_SetGetNumberRange(&g_Element[index++], 0x1, 0x1, segmentSize / 3);
        uint8_t keyByte1 = *(uint8_t *)DT_SetGetNumberRange(&g_Element[index++], 0x9, 0x1, UINT8_MAX);
        std::vector<uint8_t> rawKey1 = GetRandomData(keyLength1, keyByte1);

        const BinaryData priKey(rawKey.data(), rawKey.size());
        const BinaryData secKey(rawKey1.data(), rawKey1.size());
        uint32_t keyHashCode = HashCode::Hash(rawKey.data(), rawKey.size());
        nsKMapTable->Contain(keyHashCode, priKey, secKey);
    }

    inline void GetKMap(size_t segmentSize)
    {
        int index = 0;
        uint32_t keyLength = *(uint32_t *)DT_SetGetNumberRange(&g_Element[index++], 0x1, 0x1, segmentSize / 2);
        uint8_t keyByte = *(uint8_t *)DT_SetGetNumberRange(&g_Element[index++], 0x9, 0x1, UINT8_MAX);
        std::vector<uint8_t> rawKey = GetRandomData(keyLength, keyByte);

        uint32_t keyLength1 = *(uint32_t *)DT_SetGetNumberRange(&g_Element[index++], 0x1, 0x1, segmentSize / 3);
        uint8_t keyByte1 = *(uint8_t *)DT_SetGetNumberRange(&g_Element[index++], 0x9, 0x1, UINT8_MAX);
        std::vector<uint8_t> rawKey1 = GetRandomData(keyLength1, keyByte1);

        const BinaryData priKey(rawKey.data(), rawKey.size());
        const BinaryData secKey(rawKey1.data(), rawKey1.size());
        BinaryData value;
        uint32_t keyHashCode = HashCode::Hash(rawKey.data(), rawKey.size());
        kMapTable->Get(keyHashCode, priKey, secKey, value);
    }

    inline void RemoveKMap(size_t segmentSize)
    {
        int index = 0;
        uint32_t keyLength = *(uint32_t *)DT_SetGetNumberRange(&g_Element[index++], 0x1, 0x1, segmentSize / 2);
        uint8_t keyByte = *(uint8_t *)DT_SetGetNumberRange(&g_Element[index++], 0x9, 0x1, UINT8_MAX);
        std::vector<uint8_t> rawKey = GetRandomData(keyLength, keyByte);

        uint32_t keyLength1 = *(uint32_t *)DT_SetGetNumberRange(&g_Element[index++], 0x1, 0x1, segmentSize / 3);
        uint8_t keyByte1 = *(uint8_t *)DT_SetGetNumberRange(&g_Element[index++], 0x9, 0x1, UINT8_MAX);
        std::vector<uint8_t> rawKey1 = GetRandomData(keyLength1, keyByte1);

        const BinaryData priKey(rawKey.data(), rawKey.size());
        const BinaryData secKey(rawKey1.data(), rawKey1.size());
        uint32_t keyHashCode = HashCode::Hash(rawKey.data(), rawKey.size());
        kMapTable->Remove(keyHashCode, priKey, secKey);
    }

    inline void RemoveAllKMap(size_t segmentSize)
    {
        int index = 0;
        uint32_t keyLength = *(uint32_t *)DT_SetGetNumberRange(&g_Element[index++], 0x1, 0x1, segmentSize / 2);
        uint8_t keyByte = *(uint8_t *)DT_SetGetNumberRange(&g_Element[index++], 0x9, 0x1, UINT8_MAX);
        std::vector<uint8_t> rawKey = GetRandomData(keyLength, keyByte);

        const BinaryData priKey(rawKey.data(), rawKey.size());
        uint32_t keyHashCode = HashCode::Hash(rawKey.data(), rawKey.size());
        kMapTable->Remove(keyHashCode, priKey);
    }

    inline uint64_t SyncCheckpoint(std::string &checkpointPath)
    {
        int index = 0;
        uint64_t checkpointId = *(uint64_t *)DT_SetGetU64(&g_Element[index++], 0x0);
        std::string path = DT_SetGetString(&g_Element[index++], 41, 0, "test_sync_checkpoint_ok--checkpoint_path");
        path.erase(std::remove(path.begin(), path.end(), '.'), path.end());
        checkpointPath += path;
        mDB->CreateSyncCheckpoint(checkpointPath, checkpointId);
        return checkpointId;
    }

    inline void AsyncCheckpoint(uint64_t checkpointId)
    {
        int index = 0;
        bool isIncremental = (*(uint8_t *)DT_SetGetU8(&g_Element[index++], 0x1)) % 2;
        mDB->CreateAsyncCheckpoint(checkpointId, isIncremental);
    }

    inline ConfigRef FuzzConfig(std::string &basePath)
    {
        int index = 0;
        uint32_t maxParallelism = *(uint32_t *)DT_SetGetU32(&g_Element[index++], 0x0);
        unsigned int startKeyGroup = *(uint32_t *)DT_SetGetU32(&g_Element[index++], 0x0);
        unsigned int endKeyGroup = *(uint32_t *)DT_SetGetU32(&g_Element[index++], 0x0);
        std::string backendUID = DT_SetGetString(&g_Element[index++], 24, 0, "CreateConfig_backendUID");
        std::string localPath = DT_SetGetString(&g_Element[index++], 23, 0, "CreateConfig_localPath");
        float sliceMemWaterMark = *(float *)DT_SetGetFloat(&g_Element[index++], 0.1);
        // dbSize应当不小于120M
        uint64_t dbSize = *(uint64_t *)DT_SetGetU64(&g_Element[index++], 0x1);
        int32_t compactionSwitch = *(int32_t *)DT_SetGetS32(&g_Element[index++], 0x1);
        float fileMemoryRatio = *(float *)DT_SetGetFloat(&g_Element[index++], 0.1);
        uint64_t heapAvailableSize = *(uint64_t *)DT_SetGetU64(&g_Element[index++], 0x1);
        int32_t taskSlotFlag = *(int32_t *)DT_SetGetS32(&g_Element[index++], 0x1);
        std::string path = basePath + localPath;
        auto ret = mkdir(path.c_str(), 777);
        if (errno != EEXIST) {
            ASSERT_NE(ret, -1);
        }
        if (!CheckPathValid(path)) {
            path = "/home/workspace/w30061906/self/boost_state_store/tmp/";
        }
        if (dbSize < IO_SIZE_120M) {
            dbSize = IO_SIZE_120M;
        }
        ConfigRef config = std::make_shared<Config>();
        config->Init(startKeyGroup, endKeyGroup, maxParallelism);
        config->SetBackendUID(backendUID);
        config->SetLocalPath(path);
        config->SetTotalMemHighMarkRatio(sliceMemWaterMark);
        config->SetTotalDBSize(dbSize);
        config->SetLsmStoreCompactionSwitch(compactionSwitch);
        config->SetFileMemoryRatio(fileMemoryRatio);
        config->SetHeapAvailableSize(heapAvailableSize);
        config->SetTaskSlotFlag(taskSlotFlag);
        return config;
    }

    static inline bool CheckPathValid(const std::string &inputPath, bool allowPathNotExist = false,
                                      bool isDirectory = false)
    {
        // 检查非空
        if (UNLIKELY(inputPath.empty())) {
            LOG_ERROR("InputPath is null");
            return false;
        }
        std::string path = inputPath;
        // 去掉 'file://' 文件头
        std::string filterPrefixeStr = "file://";
        std::size_t filterSize = filterPrefixeStr.size();
        if (path.size() > filterSize && strncmp(path.c_str(), filterPrefixeStr.c_str(), filterSize) == 0) {
            path = path.substr(filterSize);
        }

        // 检查真实路径
        auto realPath = realpath(path.c_str(), nullptr);
        if (UNLIKELY(realPath == nullptr)) {
            int errorCode = errno;
            if (allowPathNotExist && LIKELY(errorCode == ENOENT)) {
                return true;
            }
            LOG_ERROR("Path transform realpath failed, path: " << PathTransform::ExtractFileName(path)
                                                               << ", error: " << strerror(errorCode));
            return false;
        }

        if (UNLIKELY(std::string(realPath).size() > PATH_MAX)) {
            LOG_ERROR("InputPath size too big, size: " << std::string(realPath).size() << ", limit: " << PATH_MAX
                                                       << ", path: " << PathTransform::ExtractFileName(inputPath));
            free(realPath);
            realPath = nullptr;
            return false;
        }
        if (access(realPath, R_OK) != 0) {  // the file is not readable
            LOG_ERROR("InputPath is not readable, path: " << PathTransform::ExtractFileName(inputPath));
            free(realPath);
            realPath = nullptr;
            return false;
        }

        struct stat s {
        };
        // The path has been checked before, it is readable and exists.
        if (lstat(realPath, &s) != 0) {
            LOG_ERROR("Failed to get the inputPath stat, path: " << PathTransform::ExtractFileName(inputPath));
            free(realPath);
            realPath = nullptr;
            return false;
        }
        if (isDirectory && !S_ISDIR(s.st_mode)) {
            LOG_ERROR("InputPath is not directory, path: " << PathTransform::ExtractFileName(inputPath));
            free(realPath);
            realPath = nullptr;
            return false;
        }

        free(realPath);
        realPath = nullptr;
        return true;
    }

    inline void UpdateTtlConfig()
    {
        int index = 0;
        KeyedStateType keyedStateType =
            static_cast<KeyedStateType>(*(uint8_t *)DT_SetGetU8(&g_Element[index++], 0x0) % 6);
        std::string tableName = DT_SetGetString(&g_Element[index++], 10, 0, "tableName");
        int64_t tableTtl = *(int64_t *)DT_SetGetS64(&g_Element[index++], 0x0);
        mDB->UpdateTtlConfig(keyedStateType, tableName, tableTtl);
    }

    inline bool GetRestoreParam(std::vector<std::string> &metaPaths,
                                std::unordered_map<std::string, std::string> &lazyPathMapping)
    {
        int index = 0;
        std::string metaPath = DT_SetGetString(&g_Element[index++], 19, 0, "RestoreDB_metaPath");
        std::string localPath = DT_SetGetString(&g_Element[index++], 20, 0, "RestoreDB_localPath");
        std::string remotePath = DT_SetGetString(&g_Element[index++], 21, 0, "RestoreDB_remotePath");
        bool isLazy = (*(uint8_t *)DT_SetGetU8(&g_Element[index++], 0x1)) % 2;
        metaPaths.emplace_back(metaPath);
        lazyPathMapping[localPath] = remotePath;
        return isLazy;
    }
};

class TestIterator : public TestTable {
public:
    inline void OpenKeysIterator()
    {
        int index = 0;
        uint32_t keyLength = *(uint32_t *)DT_SetGetU32(&g_Element[index++], 0x0);
        uint8_t keyByte = *(uint8_t *)DT_SetGetNumberRange(&g_Element[index++], 0x9, 0x1, UINT8_MAX);
        std::vector<uint8_t> rawKey = GetRandomData(keyLength, keyByte);

        const BinaryData priKey(rawKey.data(), rawKey.size());
        auto it = kVTable->KeysIterator(priKey);
        delete it;
        it = nullptr;
    }

    inline void OpenNSKVKeysIterator()
    {
        int index = 0;
        uint32_t keyLength = *(uint32_t *)DT_SetGetU32(&g_Element[index++], 0x0);
        uint8_t keyByte = *(uint8_t *)DT_SetGetNumberRange(&g_Element[index++], 0x9, 0x1, UINT8_MAX);
        std::vector<uint8_t> rawKey = GetRandomData(keyLength, keyByte);

        const BinaryData priKey(rawKey.data(), rawKey.size());
        auto it = nsKVTable->KeysIterator(priKey);
        delete it;
        it = nullptr;
    }

    inline void OpenNSKListKeysIterator()
    {
        int index = 0;
        uint32_t keyLength = *(uint32_t *)DT_SetGetU32(&g_Element[index++], 0x0);
        uint8_t keyByte = *(uint8_t *)DT_SetGetNumberRange(&g_Element[index++], 0x9, 0x1, UINT8_MAX);
        std::vector<uint8_t> rawKey = GetRandomData(keyLength, keyByte);

        const BinaryData priKey(rawKey.data(), rawKey.size());
        auto it = nsKListTable->KeysIterator(priKey);
        delete it;
        it = nullptr;
    }

    inline void OpenSubKeyIterator()
    {
        int index = 0;
        uint32_t nsLength = *(uint32_t *)DT_SetGetU32(&g_Element[index++], 0x0);
        uint8_t nsByte = *(uint8_t *)DT_SetGetNumberRange(&g_Element[index++], 0x9, 0x1, UINT8_MAX);
        std::vector<uint8_t> ns = GetRandomData(nsLength, nsByte);

        const BinaryData key(ns.data(), ns.size());
        auto it = nsKMapTable->KeysIterator(key);
        delete it;
        it = nullptr;
    }

    inline void OpenSubEntryIterator()
    {
        int index = 0;
        uint32_t keyLength = *(uint32_t *)DT_SetGetNumberRange(&g_Element[index++], 0x1, 0x1, UINT32_MAX);
        uint8_t keyByte = *(uint8_t *)DT_SetGetNumberRange(&g_Element[index++], 0x9, 0x1, UINT8_MAX);
        std::vector<uint8_t> rawKey = GetRandomData(keyLength, keyByte);

        const BinaryData priKey(rawKey.data(), rawKey.size());
        uint32_t keyHashCode = HashCode::Hash(rawKey.data(), rawKey.size());
        auto it = nsKMapTable->EntryIterator(keyHashCode, priKey);
        delete it;
        it = nullptr;
    }
};
}  // namespace test_table
}  // namespace test
}  // namespace bss
}  // namespace ock

#endif  // BOOST_SS_TEST_KV_TABLE_H