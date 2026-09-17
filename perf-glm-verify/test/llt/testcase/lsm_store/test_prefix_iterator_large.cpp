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

#include "test_lsm_store.h"

#include <chrono>
#include <cstdio>

#include "binary/query_binary.h"
#include "include/bss_types.h"
#include "include/config.h"

using namespace ock::bss;

namespace {
const std::vector<std::string> gCompressionPolicy = { "lz4", "lz4", "lz4" };
const std::string gLsmCompressionPolicy = "lz4";
const uint16_t MAP_STATE_ID = MAP << NO_13;
const uint16_t VALUE_STATE_ID = VALUE << NO_13;

struct MultiSecGroup {
    SliceKey prefixKey;
    std::vector<Value> values;
};

std::vector<std::pair<SliceKey, Value>> BuildMultiSecEntries(Generator &gen, uint32_t numGroups,
                                                             uint32_t secPerGroup, uint32_t priKeyLen,
                                                             uint32_t secKeyLen, uint32_t valueLen,
                                                             std::vector<MultiSecGroup> &groups,
                                                             SeqGenerator &seqGen, uint32_t seedBase)
{
    std::vector<std::pair<SliceKey, Value>> entries;
    entries.reserve(static_cast<size_t>(numGroups) * secPerGroup);
    uint32_t seed = seedBase;
    for (uint32_t g = 0; g < numGroups; ++g) {
        uint8_t *priData = new uint8_t[priKeyLen];
        gen.GenerateRandomData(priData, priKeyLen, seed++);
        MultiSecGroup group;
        group.prefixKey = gen.GenerateSglKey(priData, priKeyLen, MAP_STATE_ID);
        for (uint32_t s = 0; s < secPerGroup; ++s) {
            uint8_t *secData = new uint8_t[secKeyLen];
            gen.GenerateRandomData(secData, secKeyLen, seed++);
            SliceKey dualKey = gen.GenerateDualKey(priData, priKeyLen, secData, secKeyLen);
            Value value = gen.GenerateValue(valueLen, seqGen.Next());
            group.values.emplace_back(value);
            entries.emplace_back(dualKey, value);
            delete[] secData;
        }
        groups.emplace_back(std::move(group));
        delete[] priData;
    }
    return entries;
}
}  // namespace

// 大数据量单secondary key前缀迭代: 触发多data block, 验证正确性与吞吐
TEST_F(TestLsmStore, LargePrefixIteratorSingleSecKey)
{
    ConfigRef config = std::make_shared<Config>();
    config->Init(NO_0, NO_15, NO_16);
    config->SetLsmStoreCompactionSwitch(1);
    config->SetZeroCopySwitch(true);
    config->SetLsmStoreCompressionPolicy(gLsmCompressionPolicy);
    config->SetCompressionLevelPolicy(gCompressionPolicy);
    config->SetFileStoreL0NumTrigger(NO_4);
    InitEnv(config);

    const uint32_t total = 20000;
    std::vector<std::pair<SliceKey, Value>> putEntries;
    Generator gen(7);
    std::set<uint32_t> seen;
    for (uint32_t i = 0; i < total; ++i) {
        SliceKey key = gen.GenerateSglKey(NO_10, VALUE_STATE_ID);
        uint32_t hash = key.KeyHashCode();
        if (seen.count(hash) > 0) {
            continue;
        }
        seen.emplace(hash);
        Value value = gen.GenerateValue(NO_256, mSeqGenerator->Next());
        putEntries.emplace_back(key, value);
    }

    FlushLevel0Table(putEntries);
    while (!mLsmStore->CheckCompactionCompleted()) {
        sleep(1);
    }

    auto t0 = std::chrono::steady_clock::now();
    for (auto &entry : putEntries) {
        GetAndCheckPrefixIterator(entry.first, entry.second);
    }
    auto t1 = std::chrono::steady_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();
    LOG_INFO("LargePrefixIteratorSingleSecKey scanned " << putEntries.size() << " keys in " << ms << " ms");
}

// 多secondary key前缀迭代: 同一主键多sec key, 验证数量、值与顺序(正序)
TEST_F(TestLsmStore, LargePrefixIteratorMultiSecKey)
{
    ConfigRef config = std::make_shared<Config>();
    config->Init(NO_0, NO_15, NO_16);
    config->SetLsmStoreCompactionSwitch(1);
    config->SetZeroCopySwitch(true);
    config->SetLsmStoreCompressionPolicy(gLsmCompressionPolicy);
    config->SetCompressionLevelPolicy(gCompressionPolicy);
    config->SetFileStoreL0NumTrigger(NO_4);
    InitEnv(config);

    const uint32_t numGroups = 300;
    const uint32_t secPerGroup = 40;
    Generator gen(31);
    std::vector<MultiSecGroup> groups;
    std::vector<std::pair<SliceKey, Value>> entries =
        BuildMultiSecEntries(gen, numGroups, secPerGroup, NO_10, NO_16, NO_128, groups, *mSeqGenerator, 1);

    FlushLevel0Table(entries);
    while (!mLsmStore->CheckCompactionCompleted()) {
        sleep(1);
    }

    auto t0 = std::chrono::steady_clock::now();
    for (auto &group : groups) {
        auto iterator = mLsmStore->PrefixIterator(group.prefixKey, false);
        ASSERT_NE(iterator, nullptr);
        uint32_t count = 0;
        Key prevKey;
        bool hasPrev = false;
        while (iterator->HasNext()) {
            auto keyValue = iterator->Next();
            ASSERT_NE(keyValue, nullptr);
            bool found = false;
            for (auto &v : group.values) {
                if (IsTheSameValue(v, keyValue->value)) {
                    found = true;
                    break;
                }
            }
            ASSERT_TRUE(found);
            if (hasPrev) {
                ASSERT_LE(prevKey.Compare(keyValue->key), 0);
            }
            prevKey = keyValue->key;
            hasPrev = true;
            ++count;
        }
        ASSERT_EQ(count, group.values.size());
        iterator->Close();
    }
    auto t1 = std::chrono::steady_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();
    LOG_INFO("LargePrefixIteratorMultiSecKey scanned " << groups.size() << " groups x " << secPerGroup
                                                       << " secKeys in " << ms << " ms");
}

// 多secondary key逆序前缀迭代: 验证逆序顺序正确
TEST_F(TestLsmStore, LargePrefixIteratorMultiSecKeyReverse)
{
    ConfigRef config = std::make_shared<Config>();
    config->Init(NO_0, NO_15, NO_16);
    config->SetLsmStoreCompactionSwitch(1);
    config->SetZeroCopySwitch(true);
    config->SetLsmStoreCompressionPolicy(gLsmCompressionPolicy);
    config->SetCompressionLevelPolicy(gCompressionPolicy);
    config->SetFileStoreL0NumTrigger(NO_4);
    InitEnv(config);

    const uint32_t numGroups = 150;
    const uint32_t secPerGroup = 40;
    Generator gen(57);
    std::vector<MultiSecGroup> groups;
    std::vector<std::pair<SliceKey, Value>> entries =
        BuildMultiSecEntries(gen, numGroups, secPerGroup, NO_10, NO_16, NO_128, groups, *mSeqGenerator, 1);

    FlushLevel0Table(entries);
    while (!mLsmStore->CheckCompactionCompleted()) {
        sleep(1);
    }

    for (auto &group : groups) {
        auto iterator = mLsmStore->PrefixIterator(group.prefixKey, true);
        ASSERT_NE(iterator, nullptr);
        uint32_t count = 0;
        Key prevKey;
        bool hasPrev = false;
        while (iterator->HasNext()) {
            auto keyValue = iterator->Next();
            ASSERT_NE(keyValue, nullptr);
            if (hasPrev) {
                ASSERT_GE(prevKey.Compare(keyValue->key), 0);
            }
            prevKey = keyValue->key;
            hasPrev = true;
            ++count;
        }
        ASSERT_EQ(count, group.values.size());
        iterator->Close();
    }
}

// 多文件多level前缀迭代: 多次flush触发compaction后跨层扫描验证
TEST_F(TestLsmStore, LargePrefixIteratorMultiLevel)
{
    ConfigRef config = std::make_shared<Config>();
    config->Init(NO_0, NO_15, NO_16);
    config->SetLsmStoreCompactionSwitch(1);
    config->SetZeroCopySwitch(true);
    config->SetLsmStoreCompressionPolicy(gLsmCompressionPolicy);
    config->SetCompressionLevelPolicy(gCompressionPolicy);
    config->SetFileStoreL0NumTrigger(NO_2);
    InitEnv(config);

    const uint32_t numGroups = 80;
    const uint32_t secPerGroup = 25;
    Generator gen(91);
    std::vector<MultiSecGroup> allGroups;
    std::set<uint32_t> seenPriHash;
    uint32_t fileCount = NO_4;
    for (uint32_t f = 0; f < fileCount; ++f) {
        std::vector<MultiSecGroup> groups;
        std::vector<std::pair<SliceKey, Value>> entries =
            BuildMultiSecEntries(gen, numGroups, secPerGroup, NO_10, NO_16, NO_128, groups, *mSeqGenerator,
                                 f * 100000U + 1);
        FlushLevel0Table(entries);
        allGroups.insert(allGroups.end(), groups.begin(), groups.end());
    }

    while (!mLsmStore->CheckCompactionCompleted()) {
        sleep(1);
    }

    uint32_t checked = 0;
    for (auto &group : allGroups) {
        if (seenPriHash.count(group.prefixKey.KeyHashCode()) > 0) {
            continue;
        }
        seenPriHash.emplace(group.prefixKey.KeyHashCode());
        auto iterator = mLsmStore->PrefixIterator(group.prefixKey, false);
        if (iterator == nullptr) {
            continue;
        }
        uint32_t count = 0;
        while (iterator->HasNext()) {
            auto keyValue = iterator->Next();
            ASSERT_NE(keyValue, nullptr);
            bool found = false;
            for (auto &v : group.values) {
                if (IsTheSameValue(v, keyValue->value)) {
                    found = true;
                    break;
                }
            }
            ASSERT_TRUE(found);
            ++count;
        }
        ASSERT_EQ(count, group.values.size());
        iterator->Close();
        ++checked;
    }
    ASSERT_GT(checked, 0u);
}

// 性能基准: 大block cache(命中), 多sec key, 重复扫描隔离decode开销, stderr输出扫描耗时
TEST_F(TestLsmStore, LargePrefixIteratorPerfWarmCache)
{
    ConfigRef config = std::make_shared<Config>();
    config->Init(NO_0, NO_15, NO_16);
    config->SetLsmStoreCompactionSwitch(1);
    config->SetZeroCopySwitch(true);
    config->SetLsmStoreCompressionPolicy(gLsmCompressionPolicy);
    config->SetCompressionLevelPolicy(gCompressionPolicy);
    config->SetFileStoreL0NumTrigger(NO_4);
    const uint32_t bigCache = NO_64 * NO_1024 * NO_1024;  // 64MB, 足以缓存所有data block
    InitEnv(config, bigCache);

    const uint32_t numGroups = 200;
    const uint32_t secPerGroup = 100;
    Generator gen(13);
    std::vector<MultiSecGroup> groups;
    std::vector<std::pair<SliceKey, Value>> entries =
        BuildMultiSecEntries(gen, numGroups, secPerGroup, NO_10, NO_16, NO_64, groups, *mSeqGenerator, 1);

    FlushLevel0Table(entries);
    while (!mLsmStore->CheckCompactionCompleted()) {
        sleep(1);
    }

    // warm up: fill the block cache.
    for (auto &group : groups) {
        auto it = mLsmStore->PrefixIterator(group.prefixKey, false);
        while (it != nullptr && it->HasNext()) {
            it->Next();
        }
        if (it != nullptr) {
            it->Close();
        }
    }

    const uint32_t iters = 20;
    auto t0 = std::chrono::steady_clock::now();
    uint64_t totalKv = 0;
    for (uint32_t iter = 0; iter < iters; ++iter) {
        for (auto &group : groups) {
            auto it = mLsmStore->PrefixIterator(group.prefixKey, false);
            while (it != nullptr && it->HasNext()) {
                it->Next();
                ++totalKv;
            }
            if (it != nullptr) {
                it->Close();
            }
        }
    }
    auto t1 = std::chrono::steady_clock::now();
    auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count();
    fprintf(stderr,
            "\n[PERF] LargePrefixIteratorPerfWarmCache: groups=%u secPerGroup=%u iters=%u totalKv=%llu scanTime=%.3f ms "
            "(%.1f ns/kv)\n",
            numGroups, secPerGroup, iters, (unsigned long long)totalKv, ns / 1e6,
            totalKv > 0 ? (double)ns / totalKv : 0.0);
    ASSERT_GT(totalKv, 0u);
}

// 性能基准: 极小block cache(每次miss→pread), 重复扫描隔离I/O开销, 验证pread路径无回退
TEST_F(TestLsmStore, LargePrefixIteratorPerfColdCache)
{
    ConfigRef config = std::make_shared<Config>();
    config->Init(NO_0, NO_15, NO_16);
    config->SetLsmStoreCompactionSwitch(1);
    config->SetZeroCopySwitch(true);
    config->SetLsmStoreCompressionPolicy(gLsmCompressionPolicy);
    config->SetCompressionLevelPolicy(gCompressionPolicy);
    config->SetFileStoreL0NumTrigger(NO_4);
    InitEnv(config, NO_10000);  // tiny cache: every block read is a pread (page-cache backed)

    const uint32_t numGroups = 100;
    const uint32_t secPerGroup = 50;
    Generator gen(19);
    std::vector<MultiSecGroup> groups;
    std::vector<std::pair<SliceKey, Value>> entries =
        BuildMultiSecEntries(gen, numGroups, secPerGroup, NO_10, NO_16, NO_64, groups, *mSeqGenerator, 1);

    FlushLevel0Table(entries);
    while (!mLsmStore->CheckCompactionCompleted()) {
        sleep(1);
    }

    const uint32_t iters = 10;
    auto t0 = std::chrono::steady_clock::now();
    uint64_t totalKv = 0;
    for (uint32_t iter = 0; iter < iters; ++iter) {
        for (auto &group : groups) {
            auto it = mLsmStore->PrefixIterator(group.prefixKey, false);
            while (it != nullptr && it->HasNext()) {
                it->Next();
                ++totalKv;
            }
            if (it != nullptr) {
                it->Close();
            }
        }
    }
    auto t1 = std::chrono::steady_clock::now();
    auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count();
    fprintf(stderr,
            "\n[PERF] LargePrefixIteratorPerfColdCache: groups=%u secPerGroup=%u iters=%u totalKv=%llu scanTime=%.3f ms "
            "(%.1f ns/kv)\n",
            numGroups, secPerGroup, iters, (unsigned long long)totalKv, ns / 1e6,
            totalKv > 0 ? (double)ns / totalKv : 0.0);
    ASSERT_GT(totalKv, 0u);
}
