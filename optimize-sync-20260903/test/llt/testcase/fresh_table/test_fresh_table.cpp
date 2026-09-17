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

#include "test_fresh_table.h"

#include "boost_state_db_impl.h"
#include "boost_state_dbgroup.h"

void TestFreshTable::SetUp()
{
    ConfigRef config = std::make_shared<Config>();
    config->mMemorySegmentSize = IO_SIZE_16M;
    config->SetEvictMinSize(IO_SIZE_32M);
    config->Init(NO_0, NO_127, NO_128);
    mDB = BoostStateDBFactory::Create();
    mDB->Open(config);
    mFreshTable = dynamic_cast<BoostStateDBImpl *>(mDB)->GetFreshTable();
    auto tblDesc = std::make_shared<TableDescription>(StateType::VALUE, "kVTable", 0, TableSerializer{},
                                                      mDB->GetConfig());
    kVTable = std::dynamic_pointer_cast<KVTable>(dynamic_cast<BoostStateDBImpl *>(mDB)->GetTableOrCreate(tblDesc));
}

void TestFreshTable::TearDown()
{
    mDB->Close();
    delete mDB;
    mDB = nullptr;
}

// 单个文件kv读写
TEST_F(TestFreshTable, test_write_big_value_and_get_ok)
{
    std::string key = "test_key";
    BinaryData priKey(reinterpret_cast<const uint8_t *>(key.c_str()), key.size());
    char *data = static_cast<char *>(malloc(IO_SIZE_16M));
    ASSERT_TRUE(data != nullptr);
    memset_s(data, IO_SIZE_16M, 0xFF, IO_SIZE_16M);
    uint16_t stateId = VALUE << NO_13;
    BinaryData val(reinterpret_cast<uint8_t *>(data), IO_SIZE_16M);
    uint32_t rawHash = 1234;
    uint32_t orderHash = rawHash;
    uint32_t keyGroupIndex = rawHash % NO_128;
    // 先计算stateId，再修改hash，避免修改hash影响计算stateId
    KeyGroupUtilRef codec;
    ASSERT_EQ(KeyGroupUtil::Create(NO_128, codec), BSS_OK);
    codec->SetKeyGroup(orderHash, keyGroupIndex);
    QueryKey queryKey(stateId, orderHash, priKey);
    Value putVal;
    putVal.Init(ValueType::PUT, val.Length(), val.Data(), 1);
    BResult result = mFreshTable->Put(queryKey, putVal);
    ASSERT_TRUE(result == BSS_OK);
    BinaryData getVal;
    result = kVTable->Get(rawHash, priKey, getVal);
    ASSERT_TRUE(result == BSS_OK);
    ASSERT_TRUE(putVal.ValueLen() == getVal.Length());
    ASSERT_TRUE(memcmp(putVal.ValueData(), getVal.Data(), IO_SIZE_16M) == 0);
    free(data);
}

TEST_F(TestFreshTable, test_add)
{
    mFreshTable->SetActiveEmpty();
    KeyValue keyValue;
    std::string key = "test_key";
    BinaryData priKey(reinterpret_cast<const uint8_t *>(key.c_str()), key.size());
    std::string key1 = "test_key1";
    BinaryData secKey(reinterpret_cast<const uint8_t *>(key1.c_str()), key1.size());
    char *data = static_cast<char *>(malloc(IO_SIZE_16M));
    ASSERT_TRUE(data != nullptr);
    memset_s(data, IO_SIZE_16M, 0xFF, IO_SIZE_16M);
    uint16_t stateId = MAP << NO_13;
    BinaryData val(reinterpret_cast<uint8_t *>(data), IO_SIZE_16M);
    QueryKey queryKey(stateId, 1234, priKey, secKey);
    Value putVal;
    putVal.Init(ValueType::PUT, val.Length(), val.Data(), 1);
    keyValue.key = queryKey;
    keyValue.value = putVal;
    auto ret = mFreshTable->Add(keyValue);
    ASSERT_EQ(ret, BSS_ERR);
    free(data);
    std::string value = "value";
    BinaryData val1(reinterpret_cast<const uint8_t *>(value.c_str()), value.size());
    putVal.Init(ValueType::PUT, val1.Length(), val1.Data(), 1);
    keyValue.value = putVal;
    mFreshTable->SetActiveEmpty();
    ret = mFreshTable->Add(keyValue);
    ASSERT_EQ(ret, BSS_ERR);
}
