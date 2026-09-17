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

#include "test_boost_hashmap.h"

#include <random>

#include "../../ut_main.h"
#include "binary/query_binary.h"
#include "include/binary_data.h"
#include "test_utils.h"
#include "unordered_map"

using namespace ock::bss;
MemManagerRef TestBoostHashMap::mMemManager = nullptr;
inline void GenerateData(uint8_t *&data, uint32_t &length)
{
    std::random_device rd;
    std::mt19937 gen(g_testSeed++);
    uint32_t lenBegin = 1;
    uint32_t lenEnd = 500;
    uint32_t byteBegin = 1;
    uint32_t byteEnd = 255;

    std::uniform_int_distribution<> LenDist(lenBegin, lenEnd);
    std::uniform_int_distribution<> byteDist(byteBegin, byteEnd);

    length = LenDist(gen);

    data = new (std::nothrow) uint8_t[length];
    ASSERT_NE(data, nullptr);

    for (uint32_t i = 0; i < length; ++i) {
        data[i] = static_cast<uint8_t>(byteDist(gen));
    }
}

inline void ReleaseData(uint8_t *&data)
{
    delete[] data;
}

inline void GenerateFixedData(uint8_t *&data, uint32_t length)
{
    std::random_device rd;
    std::mt19937 gen(rd());
    uint32_t byteBegin = 1;
    uint32_t byteEnd = 255;

    std::uniform_int_distribution<> byteDist(byteBegin, byteEnd);

    data = new (std::nothrow) uint8_t[length];
    ASSERT_NE(data, nullptr);

    for (uint32_t i = 0; i < length; ++i) {
        data[i] = static_cast<uint8_t>(byteDist(gen));
    }
}

void TestBoostHashMap::SetUp() const
{
}

void TestBoostHashMap::TearDown() const
{
}

void TestBoostHashMap::SetUpTestCase()
{
    ConfigRef config = std::make_shared<Config>();
    config->Init(NO_0, NO_15, NO_16);
    mMemManager = std::make_shared<MemManager>(AllocatorType::DIRECT);
    mMemManager->Initialize(config);
}

void TestBoostHashMap::TearDownTestCase()
{
}

TEST_F(TestBoostHashMap, PutOneValueAndGetOneValue)
{
    uint32_t capacity = NO_1024 * NO_1024 * NO_32;
    uintptr_t addr = 0;
    mMemManager->GetMemory(MemoryType::FRESH_TABLE, capacity, addr);
    MemorySegmentRef memorySegment = MakeRef<MemorySegment>(capacity, reinterpret_cast<uint8_t *>(addr), mMemManager);
    ASSERT_TRUE(memorySegment != nullptr);

    BoostHashMap *boostHashMap = new BoostHashMap();
    ASSERT_TRUE(boostHashMap != nullptr);
    boostHashMap->Init(memorySegment, true);
    uint16_t stateId = VALUE << NO_13;
    uint64_t seq = 0;

    uint32_t keyValueNum = 10000;
    for (uint32_t i = 1; i <= keyValueNum; i++) {
        uint8_t *keyData;
        uint32_t keyLength;
        uint8_t *valueData;
        uint32_t valueLength;
        GenerateData(keyData, keyLength);
        GenerateData(valueData, valueLength);
        BinaryData prikey(keyData, keyLength);
        Value value;
        value.Init(PUT, valueLength, valueData, seq++, nullptr);
        QueryKey queryKey(stateId, test::HashForTest(keyData, keyLength), prikey);
        boostHashMap->Put(queryKey, value);
        Value value1;
        boostHashMap->Get(queryKey, value1);
        ASSERT_TRUE(value.ValueLen() == value1.ValueLen());
        ReleaseData(keyData);
        ReleaseData(valueData);
    }
    delete boostHashMap;
}

TEST_F(TestBoostHashMap, PutAllValueAndGetValue)
{
    uint32_t capacity = NO_1024 * NO_1024 * NO_32;
    uintptr_t addr = 0;
    mMemManager->GetMemory(MemoryType::FRESH_TABLE, capacity, addr);
    MemorySegmentRef memorySegment = MakeRef<MemorySegment>(capacity, reinterpret_cast<uint8_t *>(addr), mMemManager);
    ASSERT_TRUE(memorySegment != nullptr);
    BoostHashMap *boostHashMap = new BoostHashMap();
    ASSERT_TRUE(boostHashMap != nullptr);
    boostHashMap->Init(memorySegment, true);
    uint16_t stateId = VALUE << NO_13;
    uint64_t seq = 0;

    std::unordered_map<std::string, std::string> unorderedMap;

    uint32_t keyValueNum = 10000;
    for (uint32_t i = 1; i <= keyValueNum; i++) {
        uint8_t *keyData;
        uint32_t keyLength;
        uint8_t *valueData;
        uint32_t valueLength;
        GenerateData(keyData, keyLength);
        GenerateData(valueData, valueLength);
        BinaryData prikey(keyData, keyLength);
        GenerateData(valueData, valueLength);
        Value value;
        value.Init(PUT, valueLength, valueData, seq++, nullptr);
        QueryKey queryKey(stateId, test::HashForTest(keyData, keyLength), prikey);
        boostHashMap->Put(queryKey, value);

        std::string keyString = std::string(reinterpret_cast<const char *>(keyData), keyLength);
        std::string valueString = std::string(reinterpret_cast<const char *>(valueData), valueLength);
        unorderedMap[keyString] = valueString;
        ReleaseData(keyData);
        ReleaseData(valueData);
    }

    for (const auto &pair : unorderedMap) {
        uint32_t keyLength = pair.first.size();
        uint8_t *keyData = new uint8_t[keyLength];
        std::copy(pair.first.begin(), pair.first.end(), keyData);
        uint32_t valueLength = pair.second.size();
        uint8_t *valueData = new uint8_t[valueLength];
        std::copy(pair.second.begin(), pair.second.end(), valueData);
        BinaryData prikey(keyData, keyLength);
        QueryKey queryKey(stateId, test::HashForTest(keyData, keyLength), prikey);
        Value value;
        auto ret = boostHashMap->Get(queryKey, value);
        ASSERT_TRUE(ret);
        ASSERT_TRUE(value.ValueLen() == valueLength);
        delete[] keyData;
        delete[] valueData;
    }

    delete boostHashMap;
}

TEST_F(TestBoostHashMap, PutOneMapValueAndGetOneMapValue)
{
    uint32_t capacity = NO_1024 * NO_1024 * NO_32;
    uintptr_t addr = 0;
    mMemManager->GetMemory(MemoryType::SLICE_TABLE, capacity, addr);
    MemorySegmentRef memorySegment = MakeRef<MemorySegment>(capacity, reinterpret_cast<uint8_t *>(addr), mMemManager);
    ASSERT_TRUE(memorySegment != nullptr);
    BoostHashMap *boostHashMap = new BoostHashMap();
    ASSERT_TRUE(boostHashMap != nullptr);
    boostHashMap->Init(memorySegment, true);
    uint16_t stateId = MAP << NO_13;
    uint64_t seq = 0;

    uint32_t keyValueNum = 10000;
    for (uint32_t i = 1; i <= keyValueNum; i++) {
        uint8_t *firstKeyData;
        uint32_t firstKeyLength;
        uint8_t *secondKeyData;
        uint32_t secondKeyLength;
        uint8_t *valueData;
        uint32_t valueLength;
        GenerateData(firstKeyData, firstKeyLength);
        GenerateData(secondKeyData, secondKeyLength);
        GenerateData(valueData, valueLength);
        BinaryData prikey(firstKeyData, firstKeyLength);
        BinaryData secKey(secondKeyData, secondKeyLength);
        Value value;
        value.Init(PUT, valueLength, valueData, seq++, nullptr);
        QueryKey queryKey(stateId, test::HashForTest(firstKeyData, firstKeyLength), prikey, secKey);
        KeyValue keyValue;
        keyValue.key = queryKey;
        keyValue.value = value;
        boostHashMap->Put(keyValue);
        Value value1;
        bool mValue = boostHashMap->GetKMap(queryKey, value1);
        ASSERT_TRUE(mValue);
        ASSERT_TRUE(value.ValueLen() == value1.ValueLen());
        ReleaseData(firstKeyData);
        ReleaseData(secondKeyData);
        ReleaseData(valueData);
    }
    delete boostHashMap;
}

TEST_F(TestBoostHashMap, PutAllMapValueAndGetMapValue)
{
    uint32_t capacity = NO_1024 * NO_1024 * NO_32;
    uintptr_t addr = 0;
    mMemManager->GetMemory(MemoryType::SLICE_TABLE, capacity, addr);
    MemorySegmentRef memorySegment = MakeRef<MemorySegment>(capacity, reinterpret_cast<uint8_t *>(addr), mMemManager);
    ASSERT_TRUE(memorySegment != nullptr);
    BoostHashMap *boostHashMap = new BoostHashMap();
    ASSERT_TRUE(boostHashMap != nullptr);
    boostHashMap->Init(memorySegment, true);
    uint16_t stateId = MAP << NO_13;
    uint64_t seq = 0;

    std::unordered_map<std::string, std::unordered_map<std::string, std::string>> unorderedMap;

    uint32_t keyValueNum = 10000;
    uint32_t addSize = 0;
    for (uint32_t i = 1; i <= keyValueNum; i++) {
        uint8_t *firstKeyData;
        uint32_t firstKeyLength;
        uint8_t *secondKeyData;
        uint32_t secondKeyLength;
        uint8_t *valueData;
        uint32_t valueLength;
        GenerateData(firstKeyData, firstKeyLength);
        GenerateData(secondKeyData, secondKeyLength);
        GenerateData(valueData, valueLength);
        BinaryData prikey(firstKeyData, firstKeyLength);
        BinaryData secKey(secondKeyData, secondKeyLength);
        Value value;
        value.Init(PUT, valueLength, valueData, seq++, nullptr);
        QueryKey queryKey(stateId, test::HashForTest(firstKeyData, firstKeyLength), prikey, secKey);
        KeyValue keyValue;
        keyValue.key = queryKey;
        keyValue.value = value;
        boostHashMap->Put(keyValue);

        std::string firstKeyString = std::string(reinterpret_cast<const char *>(firstKeyData), firstKeyLength);
        std::string secondKeyString = std::string(reinterpret_cast<const char *>(secondKeyData), secondKeyLength);
        std::string valueString = std::string(reinterpret_cast<const char *>(valueData), valueLength);
        if (unorderedMap.find(firstKeyString) == unorderedMap.end()) {
            addSize++;
        } else {
            auto secondMap = unorderedMap.find(firstKeyString)->second;
            if (secondMap.find(secondKeyString) == secondMap.end()) {
                addSize++;
            }
        }
        unorderedMap[firstKeyString][secondKeyString] = valueString;
        ReleaseData(firstKeyData);
        ReleaseData(secondKeyData);
        ReleaseData(valueData);
    }

    for (const auto &pair : unorderedMap) {
        uint32_t firstKeyLength = pair.first.size();
        uint8_t *firstKeyData = new uint8_t[firstKeyLength];
        std::copy(pair.first.begin(), pair.first.end(), firstKeyData);
        BinaryData prikey(firstKeyData, firstKeyLength);

        std::unordered_map<std::string, std::string> secondMap = pair.second;
        for (const auto &secondPair : secondMap) {
            uint32_t secondKeyLength = secondPair.first.size();
            uint8_t *secondKeyData = new uint8_t[secondKeyLength];
            std::copy(secondPair.first.begin(), secondPair.first.end(), secondKeyData);
            uint32_t valueLength = secondPair.second.size();
            uint8_t *valueData = new uint8_t[valueLength];
            std::copy(secondPair.second.begin(), secondPair.second.end(), valueData);

            BinaryData secKey(secondKeyData, secondKeyLength);
            QueryKey queryKey(stateId, test::HashForTest(firstKeyData, firstKeyLength), prikey, secKey);
            Value value1;
            auto ret = boostHashMap->GetKMap(queryKey, value1);
            ASSERT_TRUE(ret);
            ASSERT_TRUE(value1.ValueLen() == valueLength);
            ASSERT_TRUE(memcmp(value1.ValueData(), valueData, valueLength) == 0);
            delete[] secondKeyData;
            delete[] valueData;
        }
        delete[] firstKeyData;
    }
    delete boostHashMap;
}

TEST_F(TestBoostHashMap, PutMapValueAndGetFromIterator)
{
    uint32_t capacity = NO_1024 * NO_1024 * NO_32;
    uintptr_t addr = 0;
    mMemManager->GetMemory(MemoryType::SLICE_TABLE, capacity, addr);
    MemorySegmentRef memorySegment = MakeRef<MemorySegment>(capacity, reinterpret_cast<uint8_t *>(addr), mMemManager);
    ASSERT_TRUE(memorySegment != nullptr);
    BoostHashMapRef boostHashMap = MakeRef<BoostHashMap>();
    ASSERT_TRUE(boostHashMap != nullptr);
    boostHashMap->Init(memorySegment, true);

    std::unordered_map<std::string, std::unordered_map<std::string, std::string>> unorderedMap;
    uint16_t stateId = MAP << NO_13;
    uint64_t seq = 0;

    uint32_t keyValueNum = 10000;
    uint32_t addSize = 0;
    for (uint32_t i = 1; i <= keyValueNum; i++) {
        uint8_t *firstKeyData;
        uint32_t firstKeyLength;
        uint8_t *secondKeyData;
        uint32_t secondKeyLength;
        uint8_t *valueData;
        uint32_t valueLength;
        GenerateData(firstKeyData, firstKeyLength);
        GenerateData(secondKeyData, secondKeyLength);
        GenerateData(valueData, valueLength);
        BinaryData prikey(firstKeyData, firstKeyLength);
        BinaryData secKey(secondKeyData, secondKeyLength);
        Value value;
        value.Init(PUT, valueLength, valueData, seq++, nullptr);
        QueryKey queryKey(stateId, test::HashForTest(firstKeyData, firstKeyLength), prikey, secKey);
        KeyValue keyValue;
        keyValue.key = queryKey;
        keyValue.value = value;
        boostHashMap->Put(keyValue);

        std::string firstKeyString = std::string(reinterpret_cast<const char *>(firstKeyData), firstKeyLength);
        std::string secondKeyString = std::string(reinterpret_cast<const char *>(secondKeyData), secondKeyLength);
        std::string valueString = std::string(reinterpret_cast<const char *>(valueData), valueLength);
        if (unorderedMap.find(firstKeyString) == unorderedMap.end()) {
            addSize++;
        } else {
            auto secondMap = unorderedMap.find(firstKeyString)->second;
            if (secondMap.find(secondKeyString) == secondMap.end()) {
                addSize++;
            }
        }
        unorderedMap[firstKeyString][secondKeyString] = valueString;
        ReleaseData(firstKeyData);
        ReleaseData(secondKeyData);
        ReleaseData(valueData);
    }

    uint32_t sizeFromIterator = 0;
    IteratorRef<std::pair<FreshKeyNodePtr, FreshValueNodePtr>> firstIterator = boostHashMap->KVIterator();

    while (firstIterator->HasNext()) {
        auto pair = firstIterator->Next();
        FreshKeyNodePtr firstKey = pair.first;
        FreshValueNodePtr secondMap = pair.second;
        ASSERT_TRUE(firstKey != nullptr);
        ASSERT_TRUE(secondMap != nullptr);
        std::string readFirstKeyString = std::string(reinterpret_cast<const char *>(firstKey->PriKeyData()),
                                                     firstKey->PriKeyDataLen());

        BoostHashMapRef hashMap = MakeRef<BoostHashMap>();
        ASSERT_TRUE(hashMap != nullptr);
        uint32_t offset = secondMap->MapData() - memorySegment->GetSegment();
        hashMap->Init(memorySegment, offset, false);
        auto secondIterator = hashMap->KVIterator();
        while (secondIterator->HasNext()) {
            sizeFromIterator++;
            auto pair = secondIterator->Next();
            auto secondKey = pair.first;
            auto value = pair.second;

            std::string readSecondString = std::string(reinterpret_cast<const char *>(secondKey->SecKeyData()),
                                                       secondKey->SecKeyDataLen());
            std::string readValueString = std::string(reinterpret_cast<const char *>(value->Value()),
                                                      value->ValueDataLen());
            auto valueInMap = unorderedMap[readFirstKeyString][readSecondString];
            ASSERT_TRUE(valueInMap == readValueString);
        }
    }
    ASSERT_TRUE(sizeFromIterator == addSize);
}