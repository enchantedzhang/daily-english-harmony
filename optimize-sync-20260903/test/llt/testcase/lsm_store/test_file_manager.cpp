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

#include <algorithm>
#include <chrono>
#include <cstdlib>
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

namespace {
uint32_t GetBenchmarkParam(const char *name, uint32_t defaultValue)
{
    const char *value = std::getenv(name);
    if (value == nullptr) {
        return defaultValue;
    }
    char *end = nullptr;
    unsigned long parsed = std::strtoul(value, &end, 10);
    if (end == value || *end != '\0' || parsed == 0 || parsed > UINT32_MAX) {
        return defaultValue;
    }
    return static_cast<uint32_t>(parsed);
}
}  // namespace

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

// Explicit performance UT for the PrefixIterator -> ReadBlock -> pread path. It is disabled in the regular LLT run
// because its data volume is intentionally large. Run with --gtest_also_run_disabled_tests and optionally override
// BSS_PREFIX_BENCH_ENTRIES/BSS_PREFIX_BENCH_ROUNDS/BSS_PREFIX_BENCH_CACHE_CAPACITY.
TEST_F(TestFileManager, DISABLED_test_prefix_iterator_pread_benchmark)
{
    constexpr uint64_t primaryKey = 20000;
    constexpr uint64_t secondaryKeyBase = 30000;
    constexpr uint64_t valueBase = 50000;
    const uint32_t entryCount = GetBenchmarkParam("BSS_PREFIX_BENCH_ENTRIES", 200000);
    const uint32_t rounds = GetBenchmarkParam("BSS_PREFIX_BENCH_ROUNDS", 5);
    const size_t blockCacheCapacity = GetBenchmarkParam("BSS_PREFIX_BENCH_CACHE_CAPACITY", 1);

    // Capacity 1 exercises the pread-heavy miss path. A larger override also measures repeated scans with warm blocks.
    ExitEnv();
    auto config = std::make_shared<Config>();
    config->Init(NO_0, NO_15, NO_16);
    config->SetCacheIndexAndFilterSwitch(false);
    InitEnv(config, blockCacheCapacity);

    ByteBufferRef primaryBuffer = MakeRef<ByteBuffer>(NO_8, MemoryType::FILE_STORE, mMemManager);
    primaryBuffer->WriteUint64(primaryKey, 0);
    SliceKey prefixKey = mGenerator->GenerateSglKey(primaryBuffer->Data(), sizeof(primaryKey), MAP << NO_13);

    std::vector<std::pair<SliceKey, Value>> entries;
    entries.reserve(entryCount);
    for (uint32_t i = 0; i < entryCount; ++i) {
        uint64_t secondaryKey = secondaryKeyBase + i;
        ByteBufferRef secondaryBuffer = MakeRef<ByteBuffer>(NO_8, MemoryType::FILE_STORE, mMemManager);
        secondaryBuffer->WriteUint64(secondaryKey, 0);
        SliceKey key = mGenerator->GenerateDualKey(primaryBuffer->Data(), sizeof(primaryKey), secondaryBuffer->Data(),
                                                   sizeof(secondaryKey));

        uint64_t rawValue = valueBase + i;
        ByteBufferRef valueBuffer = MakeRef<ByteBuffer>(NO_8, MemoryType::FILE_STORE, mMemManager);
        valueBuffer->WriteUint64(rawValue, 0);
        Value value;
        value.Init(PUT, sizeof(rawValue), valueBuffer->Data(), mSeqGenerator->Next(), valueBuffer);
        entries.emplace_back(key, value);
    }
    FlushLevel0Table(entries);
    entries.clear();

    std::vector<uint64_t> elapsedMicros;
    elapsedMicros.reserve(rounds);
    for (uint32_t round = 0; round < rounds; ++round) {
        auto start = std::chrono::steady_clock::now();
        auto iterator = mLsmStore->PrefixIterator(prefixKey, false);
        ASSERT_NE(iterator, nullptr);
        uint32_t actualCount = 0;
        uint64_t checksum = 0;
        while (iterator->HasNext()) {
            auto keyValue = iterator->Next();
            ASSERT_NE(keyValue, nullptr);
            ++actualCount;
            checksum += keyValue->value.ValueLen();
        }
        iterator->Close();
        auto end = std::chrono::steady_clock::now();
        ASSERT_EQ(actualCount, entryCount);
        ASSERT_EQ(checksum, static_cast<uint64_t>(entryCount) * sizeof(uint64_t));

        uint64_t elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
        elapsedMicros.emplace_back(elapsed);
        std::cout << "[PREFIX_PREAD_BENCH] entries=" << entryCount << " cache_capacity=" << blockCacheCapacity
                  << " round=" << round
                  << " elapsed_us=" << elapsed << " ns_per_entry="
                  << (elapsed * NO_1000 / entryCount) << std::endl;
    }

    std::sort(elapsedMicros.begin(), elapsedMicros.end());
    std::cout << "[PREFIX_PREAD_BENCH] entries=" << entryCount << " cache_capacity=" << blockCacheCapacity
              << " rounds=" << rounds
              << " median_us=" << elapsedMicros[elapsedMicros.size() / NO_2] << std::endl;
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
