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

#include "include/boost_state_db.h"
#include "include/boost_state_table.h"
#include "snapshot/savepoint_data_view.h"
#include "test_utils.h"

namespace ock {
namespace bss {

BoostStateDB *mDB;

inline std::vector<uint8_t> GetRandomData()
{
    std::random_device rd;
    std::mt19937 gen(rd());
    uint32_t lenBegin = 1;
    uint32_t lenEnd = 300;
    uint32_t byteBegin = 0;
    uint32_t byteEnd = 255;

    std::uniform_int_distribution<> LenDist(lenBegin, lenEnd);
    std::uniform_int_distribution<> byteDist(byteBegin, byteEnd);

    uint32_t length = LenDist(gen);
    std::vector<uint8_t> data(length);

    for (uint32_t i = 0; i < length; ++i) {
        data[i] = static_cast<uint8_t>(byteDist(gen));
    }

    return data;
}

TableRef CreateFromDB(StateType keyedStateType, std::string tableName, BoostStateDB *db)
{
    auto tblDesc = std::make_shared<TableDescription>(keyedStateType, tableName, -1, TableSerializer{},
                                                      db->GetConfig());
    return db->GetTableOrCreate(tblDesc);
}

class TestSavepoint : public ::testing::Test {
public:
    TestSavepoint() = default;
    void SetUp() override;
    void TearDown() override;
};

class TestSavepointMaxParallelism : public ::testing::Test {
public:
    void SetUp() override
    {
        ConfigRef config = std::make_shared<Config>();
        config->mMemorySegmentSize = IO_SIZE_4M;
        config->SetEvictMinSize(IO_SIZE_4M);
        config->Init(NO_0, NO_31, 32768);
        mDb = BoostStateDBFactory::Create();
        ASSERT_EQ(mDb->Open(config), BSS_OK);
    }

    void TearDown() override
    {
        mDb->Close();
        delete mDb;
        mDb = nullptr;
    }

    BoostStateDB *mDb = nullptr;
};

void TestSavepoint::SetUp()
{
    ConfigRef config = std::make_shared<Config>();
    config->mMemorySegmentSize = IO_SIZE_4M;
    config->SetEvictMinSize(IO_SIZE_4M);
    config->Init(NO_0, NO_15, NO_16);

    mDB = BoostStateDBFactory::Create();
    mDB->Open(config);
}

void TestSavepoint::TearDown()
{
    mDB->Close();
    delete mDB;
}

TEST_F(TestSavepoint, TestSavePoint)
{
    std::map<std::vector<uint8_t>, std::vector<uint8_t>> expectedEntrySet;
    KVTableRef kVTable = std::dynamic_pointer_cast<KVTable>(CreateFromDB(StateType::VALUE, "kVTable", mDB));
    uint32_t size = NO_10000;
    for (uint32_t j = 0; j < size; j++) {
        std::vector<uint8_t> key = GetRandomData();
        std::vector<uint8_t> value = GetRandomData();
        auto tempEntry = std::make_pair(key, value);
        if (expectedEntrySet.find(key) != expectedEntrySet.end()) {
            continue;
        }
        expectedEntrySet.emplace(key, value);
        uint32_t keyHashCode = test::HashForTest(key.data(), key.size());
        BinaryData priKey(key.data(), key.size());
        BinaryData val(value.data(), value.size());
        kVTable->Put(keyHashCode, priKey, val);
    }
    sleep(NO_10);
    SavepointDataView *dataView = mDB->TriggerSavepoint();
    auto iterator = dataView->SavepointIterator();
    ASSERT_EQ(dataView != nullptr, true);
    std::set<std::pair<std::vector<uint8_t>, std::vector<uint8_t>>> tempEntrySet;
    uint32_t count = 0;
    while (iterator->HasNext()) {
        count++;
        BinaryKeyValueItemRef item = iterator->Next();
        auto serializedKey = item->mKey;
        auto serializedValue = item->mValue;

        std::vector<uint8_t> key(serializedKey, serializedKey + item->mKeyLength);
        std::vector<uint8_t> value(serializedValue, serializedValue + item->mValueLength);

        auto it = expectedEntrySet.find(key);
        ASSERT_EQ(it != expectedEntrySet.end(), true);
        ASSERT_EQ(it->second == value, true);
    }
    ASSERT_EQ(count, expectedEntrySet.size());
}

TEST_F(TestSavepointMaxParallelism, KvOutputIsMonotonicByKeyGroup)
{
    auto table = std::dynamic_pointer_cast<KVTable>(CreateFromDB(StateType::VALUE, "kg-order-table", mDb));
    constexpr uint32_t maxParallelism = 32768;
    constexpr uint32_t groups = 32;
    constexpr uint32_t perGroup = 128;

    for (uint32_t group = 0; group < groups; ++group) {
        for (uint32_t quotient = 0; quotient < perGroup; ++quotient) {
            uint32_t rawHash = quotient * maxParallelism + group;
            std::string key = std::to_string(group) + "-" + std::to_string(quotient);
            std::string value = "value-" + key;
            BinaryData binaryKey(reinterpret_cast<const uint8_t *>(key.data()), key.size());
            BinaryData binaryValue(reinterpret_cast<const uint8_t *>(value.data()), value.size());
            ASSERT_EQ(table->Put(rawHash, binaryKey, binaryValue), BSS_OK);
        }
    }

    SavepointDataView *dataView = mDb->TriggerSavepoint();
    ASSERT_NE(dataView, nullptr);
    auto iterator = dataView->SavepointIterator();
    uint32_t previousGroup = 0;
    uint32_t count = 0;
    std::set<std::vector<uint8_t>> uniqueKeys;
    while (iterator->HasNext()) {
        auto item = iterator->Next();
        ASSERT_NE(item, nullptr);
        if (count > 0) {
            EXPECT_LE(previousGroup, item->mKeyGroup);
        }
        previousGroup = item->mKeyGroup;
        std::vector<uint8_t> serializedKey(item->mKey, item->mKey + item->mKeyLength);
        EXPECT_TRUE(uniqueKeys.insert(serializedKey).second);
        ++count;
    }
    EXPECT_EQ(count, groups * perGroup);
    dataView->Close();
    delete dataView;
}

}  // namespace bss
}  // namespace ock
