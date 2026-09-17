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

#include "test_file_manager.h"

#include <iostream>
#include <random>
#include <vector>

#include "include/bss_types.h"
#include "include/config.h"
#include "include/ref.h"
#include "lsm_store/file/file_cache_factory.h"
#include "lsm_store/file/file_factory.h"
#include "lsm_store/file/file_name.h"

using namespace ock::bss;

TEST_F(TestFileManager, test_allocate_file_return_ok)
{
    std::string localBasePath = "./workspace/file";
    std::string fileName = "test";
    ConfigRef config = std::make_shared<Config>();
    config->mLocalPath = localBasePath;
    config->mBackendUID = fileName;
    BoostNativeMetricPtr *metric = nullptr;
    FileCacheFactoryRef cacheFactory = std::make_shared<FileCacheFactory>(config, nullptr, metric);
    FileDirectoryRef fileDirectory =
        std::make_shared<FileDirectory>(cacheFactory->GetLocalFileManager()->GetBasePath());
    FileInfoRef fileInfo = cacheFactory->GetFileCache()->AllocateFile(fileDirectory, FileName::CreateFileName);
    ASSERT_TRUE(fileInfo->GetFilePath()->Name().find("test_0.sst") != std::string::npos);
}

TEST_F(TestFileManager, test_write_to_file_store_and_read_return_ok)
{
    uint32_t loopCount = 1000;
    uint32_t fileCount = NO_5;
    std::vector<std::pair<SliceKey, Value>> queryEntries;
    for (uint32_t idx = 0; idx < fileCount; ++idx) {
        std::vector<std::pair<SliceKey, Value>> putEntries;
        uint16_t stateId = VALUE << NO_13;  // 00 010 000 = PUTValue
        uint64_t keyStart1 = 20000;
        uint64_t valueStart = 50000;

        for (uint32_t i = 0; i < loopCount; i++) {
            uint64_t key1 = keyStart1 + i;
            ByteBufferRef keyBuffer = MakeRef<ByteBuffer>(NO_8, MemoryType::FILE_STORE, mMemManager);
            keyBuffer->WriteUint64(key1, 0);
            SliceKey key = mGenerator->GenerateSglKey(keyBuffer->Data(), sizeof(key1), stateId);
            uint64_t v = valueStart + i;
            ByteBufferRef valueBuffer = MakeRef<ByteBuffer>(NO_8, MemoryType::FILE_STORE, mMemManager);
            valueBuffer->WriteUint64(v, 0);
            Value value;
            value.Init(PUT, sizeof(value), reinterpret_cast<uint8_t *>(&v), mSeqGenerator->Next(), valueBuffer);
            putEntries.emplace_back(std::pair<SliceKey, Value>(key, value));
            queryEntries.emplace_back(std::pair<SliceKey, Value>(key, value));
        }

        FlushLevel0Table(putEntries);
    }

    while (!mLsmStore->CheckCompactionCompleted()) {
        sleep(1);
    }

    for (const auto &entry : queryEntries) {
        GetAndCheckValue(entry.first, entry.second);
    }
}

TEST_F(TestFileManager, test_get_not_exist_entry_return_not_exist)
{
    uint64_t key1 = 1;
    uint16_t stateId = VALUE << NO_13;
    ByteBufferRef keyBuffer = MakeRef<ByteBuffer>(NO_8, MemoryType::FILE_STORE, mMemManager);
    keyBuffer->WriteUint64(key1, 0);
    SliceKey key = mGenerator->GenerateSglKey(keyBuffer->Data(), sizeof(key1), stateId);
    Value value;
    auto ret = mLsmStore->Get(key, value);
    // return false if key does not exist.
    ASSERT_FALSE(ret);
}

TEST_F(TestFileManager, test_file_iterator_return_ok)
{
    uint32_t loopCount = 20;
    for (uint32_t idx = 0; idx < NO_1; ++idx) {  // 文件数
        std::vector<std::pair<SliceKey, Value>> entries;
        uint16_t stateId = VALUE << NO_13;
        uint64_t keyStart1 = 20000;
        uint64_t valueStart = 50000;
        std::vector<SliceKey> keys;
        std::vector<uint32_t> hashVector;
        for (uint32_t i = 0; i < loopCount; i++) {  // key的数量
            auto addr = malloc(8);
            uint64_t key1 = keyStart1 + i;
            *reinterpret_cast<uint64_t *>(addr) = key1;
            SliceKey key = mGenerator->GenerateSglKey(reinterpret_cast<uint8_t *>(addr), sizeof(key1), stateId);
            free(addr);
            uint64_t v = valueStart + i;
            ByteBufferRef valueBuffer = MakeRef<ByteBuffer>(NO_8, MemoryType::FILE_STORE, mMemManager);
            valueBuffer->WriteUint64(v, 0);
            Value value;
            value.Init(PUT, sizeof(v), valueBuffer->Data(), mSeqGenerator->Next(), valueBuffer);
            keys.emplace_back(key);
            entries.emplace_back(std::pair<SliceKey, Value>(key, value));
        }

        FlushLevel0Table(entries);

        for (const auto &entry : entries) {
            GetAndCheckValue(entry.first, entry.second);
        }
        for (const auto &entry : entries) {
            GetAndCheckPrefixIterator(entry.first, entry.second);
        }
    }
}

TEST_F(TestFileManager, test_file_prefix_iterator_return_ok)
{
    uint32_t loopCount = 40;
    uint64_t keyStart1 = 20000;
    uint16_t stateId = VALUE << NO_13;
    std::vector<std::pair<SliceKey, Value>> putEntries;
    uint64_t valueStart = 50000;
    std::vector<SliceKey> keys;
    std::vector<uint32_t> hashVector;
    for (uint32_t i = 0; i < loopCount; i++) {  // key的数量
        auto addr = malloc(8);
        uint64_t key1 = keyStart1 + i;
        *reinterpret_cast<uint64_t *>(addr) = key1;
        SliceKey key = mGenerator->GenerateSglKey(reinterpret_cast<uint8_t *>(addr), sizeof(key1), stateId);
        free(addr);
        uint64_t v = valueStart + i;
        ByteBufferRef valueBuffer = MakeRef<ByteBuffer>(NO_8, MemoryType::FILE_STORE, mMemManager);
        valueBuffer->WriteUint64(v, 0);
        Value value;
        value.Init(PUT, sizeof(v), valueBuffer->Data(), mSeqGenerator->Next(), valueBuffer);
        keys.emplace_back(key);
        putEntries.emplace_back(std::pair<SliceKey, Value>(key, value));
    }

    FlushLevel0Table(putEntries);

    for (const auto &entry : putEntries) {
        GetAndCheckPrefixIterator(entry.first, entry.second);
    }
}

TEST_F(TestFileManager, test_write_read_secondary_key_return_ok)
{
    uint32_t primaryKeyCount = 10;
    uint32_t loopCount = 10;
    uint64_t valueStart = 20000;
    uint64_t keyStart1 = 20000;
    uint64_t keyStart2 = 30000;
    std::vector<std::pair<SliceKey, Value>> queryEntries;
    std::vector<SliceKey> queryKeys;
    for (uint32_t idx = 0; idx < primaryKeyCount; idx++) {
        uint64_t key1 = keyStart1 + idx;
        ByteBufferRef keyBuffer = MakeRef<ByteBuffer>(NO_8, MemoryType::FILE_STORE, mMemManager);
        keyBuffer->WriteUint64(key1, 0);
        for (uint32_t i = 0; i < loopCount; ++i) {
            // 1、生成second key
            uint64_t key2 = keyStart2 + i;
            ByteBufferRef secKeyBuffer = MakeRef<ByteBuffer>(NO_8, MemoryType::FILE_STORE, mMemManager);
            secKeyBuffer->WriteUint64(key2, 0);
            SliceKey key = mGenerator->GenerateDualKey(keyBuffer->Data(), sizeof(key1), secKeyBuffer->Data(),
                                                       sizeof(key2));
            uint64_t v = valueStart + i;
            ByteBufferRef valueBuffer = MakeRef<ByteBuffer>(NO_8, MemoryType::FILE_STORE, mMemManager);
            valueBuffer->WriteUint64(v, 0);
            Value value;
            value.Init(PUT, sizeof(value), reinterpret_cast<uint8_t *>(&v), mSeqGenerator->Next(), valueBuffer);
            queryKeys.emplace_back(key);

            queryEntries.emplace_back(std::pair<SliceKey, Value>(key, value));
        }
    }
    FlushLevel0Table(queryEntries);

    for (const auto &entry : queryEntries) {
        GetAndCheckValue(entry.first, entry.second);
    }
}

TEST_F(TestFileManager, test_write_read_secondary_key_iterator_return_ok)
{
    uint32_t primaryKeyCount = 10;
    uint32_t secKeyCount = 10;
    uint64_t valueStart = 20000;
    uint64_t keyStart1 = 20000;
    uint64_t keyStart2 = 30000;
    uint16_t stateId = MAP << NO_13;
    uint32_t fileCount = 3;
    std::vector<std::pair<SliceKey, Value>> kvPairs;
    std::vector<SliceKey> keys;
    std::unordered_map<SliceKey, SliceKVMap, SliceKeyHash, SliceKeyEqual> prefixEntries;
    uint64_t key1;
    uint64_t key2;
    for (uint32_t fileIdx = 0; fileIdx < fileCount; fileIdx++) {
        for (uint32_t idx = 0; idx < primaryKeyCount; idx++) {
            key1 = keyStart1 + idx;
            ByteBufferRef keyBuffer = MakeRef<ByteBuffer>(NO_8, MemoryType::FILE_STORE, mMemManager);
            keyBuffer->WriteUint64(key1, 0);
            SliceKey priKey = mGenerator->GenerateSglKey(keyBuffer->Data(), sizeof(key1), stateId);
            SliceKVMap secKeyValueMap;
            for (uint32_t i = 0; i < secKeyCount; ++i) {
                // 1、生成second key
                key2 = keyStart2 + i;
                ByteBufferRef secKeyBuffer = MakeRef<ByteBuffer>(NO_8, MemoryType::FILE_STORE, mMemManager);
                secKeyBuffer->WriteUint64(key2, 0);
                SliceKey key = mGenerator->GenerateDualKey(keyBuffer->Data(), sizeof(key1), secKeyBuffer->Data(),
                                                           sizeof(key2));
                // 2、生成value
                uint64_t v = valueStart + i;
                ByteBufferRef valueBuffer = MakeRef<ByteBuffer>(NO_8, MemoryType::FILE_STORE, mMemManager);
                valueBuffer->WriteUint64(v, 0);
                Value value;
                value.Init(PUT, sizeof(v), reinterpret_cast<uint8_t *>(&v), mSeqGenerator->Next(), valueBuffer);
                keys.emplace_back(key);
                kvPairs.emplace_back(std::pair<SliceKey, Value>(key, value));
                std::pair<SliceKey, Value> pair = { key, value };
                secKeyValueMap.emplace(pair);
            }
            prefixEntries[priKey] = secKeyValueMap;
        }
        FlushLevel0Table(kvPairs);
    }

    while (!mLsmStore->CheckCompactionCompleted()) {
        sleep(1);
    }

    for (auto &item : prefixEntries) {
        GetAndCheckPrefixIterator(item.first, item.second);
    }
}

TEST_F(TestFileManager, test_delete_file_return_ok)
{
    // 删除生成的文件，最后调用
    BResult ret = BSS_OK;
    auto current = mLsmStore->GetVersionSet()->GetCurrent();
    if (current != nullptr) {
        for (auto levle : current->GetLevels()) {
            auto fileMetaDataGroups = levle.GetFileMetaDataGroups();
            for (auto fileMetaDataGroup : fileMetaDataGroups) {
                for (auto fileMetaData : fileMetaDataGroup->GetFiles()) {
                    ret = unlink(fileMetaData->GetIdentifier().c_str());
                }
            }
        }
    }
    ASSERT_EQ(ret, BSS_OK);
}

// todo: add multi slice test.
