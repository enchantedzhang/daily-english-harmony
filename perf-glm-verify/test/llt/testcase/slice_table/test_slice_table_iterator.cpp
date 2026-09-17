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

#include "test_slice_table_iterator.h"

#include <unistd.h>

#include <map>

#include "test_utils.h"

using namespace ock::bss;

ConfigRef TestSliceTableIterator::mConfig = nullptr;
BoostStateDBImpl *TestSliceTableIterator::mBoostStateDb = nullptr;

void TestSliceTableIterator::SetUp() const
{
}

void TestSliceTableIterator::TearDown() const
{
}

void TestSliceTableIterator::SetUpTestCase()
{
    ConfigRef config = std::make_shared<Config>();
    config->Init(NO_0, NO_15, NO_16);
    mConfig = config;
    mConfig->mMemorySegmentSize = IO_SIZE_4M;
    mBoostStateDb = (BoostStateDBImpl *)BoostStateDBFactory::Create();
    mBoostStateDb->Open(config);
}

void TestSliceTableIterator::TearDownTestCase()
{
    mBoostStateDb->Close();
    delete mBoostStateDb;
}

TEST_F(TestSliceTableIterator, TestSliceTablePrefixIteratorValidInMapState)
{
    mBoostStateDb->GetFreshTable()->Open();
    std::vector<BinaryData> originValueList;
    uint32_t index = 0;

    // 1、生成primary key
    uint8_t *primaryKeyData;
    uint32_t primaryKeyLength = NO_1024;
    GenerateMapKeyStateData(primaryKeyData, primaryKeyLength);
    ByteBufferRef firstBuffer = MakeRef<ByteBuffer>(primaryKeyData, primaryKeyLength);
    BinaryData priKey(primaryKeyData, primaryKeyLength);

    //  写满100M数据
    while (index <= NO_2 * NO_1024) {
        uint8_t *secondKeyData;
        uint32_t keyLength = NO_1024;
        uint8_t *valueData;
        uint32_t valueLength = NO_1024;

        // 1、生成second key
        GenerateMapValueStateData(secondKeyData, keyLength);
        BinaryData secKey(secondKeyData, keyLength);

        // 2、生成value
        GenerateMapValueStateData(valueData, valueLength);
        BinaryData value(valueData, valueLength);

        // 3、数据暂存到对应的kv list，用于后续查找比较
        originValueList.push_back(value);
        uint32_t keyHashCode = test::HashForTest(primaryKeyData, primaryKeyLength);
        KeyValue keyValue;
        uint16_t stateId = MAP << NO_13;
        QueryKey queryKey(stateId, keyHashCode, priKey, secKey);
        keyValue.key = queryKey;
        Value putVal;
        putVal.Init(ValueType::PUT, value.Length(), value.Data(), 0);
        keyValue.value = putVal;

        // 5、Add进freshTable
        mBoostStateDb->GetFreshTable()->Add(keyValue);
        index++;
    }

    // 因为写入了100M数据，所以正常情况下已经触发了flush
    ASSERT_TRUE(mBoostStateDb->GetFreshTable()->IsAlreadyTriggerFlush());
    // 等待刷新队列中全部flush完成
    while (!mBoostStateDb->GetFreshTable()->IsSnapshotQueueEmpty()) {
        sleep(NO_2);
    }
}
