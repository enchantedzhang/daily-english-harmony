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

#include "test_slice_compaction.h"

#include "test_utils.h"

ConfigRef TestSliceCompaction::mConfig = nullptr;
BoostStateDBImpl *TestSliceCompaction::mBoostStateDb = nullptr;

void TestSliceCompaction::SetUp() const
{
}

void TestSliceCompaction::TearDown() const
{
}

void TestSliceCompaction::SetUpTestCase()
{
    ConfigRef config = std::make_shared<Config>();
    config->Init(NO_0, NO_15, NO_16);
    mConfig = config;
    mConfig->mMemorySegmentSize = IO_SIZE_4M;
    mBoostStateDb = (BoostStateDBImpl *)BoostStateDBFactory::Create();
    mBoostStateDb->Open(config);
}

void TestSliceCompaction::TearDownTestCase()
{
    mBoostStateDb->Close();
    delete mBoostStateDb;
}

TEST_F(TestSliceCompaction, TestSliceCompactionTestSmallDataIsNotTriggered)
{
    // 默认初始化大小为32M
    mBoostStateDb->GetFreshTable()->Open();
    uint32_t index = 0;
    uint64_t num = 0;
    //  写满32M数据
    while (index <= NO_2 * NO_1024) {
        uint8_t *keyData;
        uint32_t keyLength = NO_1024;
        uint8_t *valueData;
        uint32_t valueLength = NO_1024;

        // 1、生成key
        GenerateKeyStateData(keyData, keyLength);
        uint32_t hashCode = test::HashForTest(keyData, keyLength);
        BinaryData priKey(keyData, keyLength);

        // 2、生成value
        GenerateValueStateData(valueData, valueLength);
        BinaryData value(valueData, valueLength);
        uint16_t stateId = MAP << NO_13;
        QueryKey queryKey(stateId, hashCode, priKey);
        Value putVal;
        putVal.Init(ValueType::PUT, keyLength, valueData, num++);

        // 3、Put进freshTable
        mBoostStateDb->GetFreshTable()->Put(queryKey, putVal);
        index++;
        ReleaseData(valueData);
        ReleaseData(keyData);
    }
    ASSERT_TRUE(mBoostStateDb->GetFreshTable()->IsAlreadyTriggerFlush());
    // 等待刷新队列中全部flush完成
    while (!mBoostStateDb->GetFreshTable()->IsSnapshotQueueEmpty()) {
        sleep(NO_2);
    }

    // 已经触发了flush
    ASSERT_TRUE(mBoostStateDb->GetFreshTable()->IsAlreadyTriggerFlush());
}

TEST_F(TestSliceCompaction, TestSliceCompactionTestEnoughDataIsTriggered)
{
    // 默认初始化大小为32M
    mBoostStateDb->GetFreshTable()->Open();
    std::vector<BinaryData> originKeyList;
    uint32_t index = 0;
    uint64_t num = 0;
    uint16_t stateId = LIST << NO_13;

    //  写满32M数据
    while (index <= NO_2 * NO_1024) {
        uint8_t *keyData;
        uint32_t keyLength = NO_1024;
        uint8_t *valueData;
        uint32_t valueLength = NO_1024;

        // 1、生成key
        GenerateKeyStateData(keyData, keyLength);
        uint32_t hashCode = test::HashForTest(keyData, keyLength);
        BinaryData priKey(keyData, keyLength);

        // 2、生成value
        GenerateValueStateData(valueData, valueLength);
        BinaryData value(valueData, valueLength);
        QueryKey queryKey(stateId, hashCode, priKey);
        Value putVal;
        putVal.Init(ValueType::PUT, valueLength, valueData, num++);

        // 4、数据暂存到对应的kv list，用于后续插入重复数据
        originKeyList.push_back(priKey);

        // 5、Put进freshTable
        mBoostStateDb->GetFreshTable()->Put(queryKey, putVal);
        index++;
        ReleaseData(valueData);
    }

    uint32_t index2 = 0;
    // 插入重复key，Slice大于6个，压缩
    while (index2 <= NO_5) {
        index = 0;
        while (index <= NO_2 * NO_1024) {
            uint8_t *valueData;
            uint32_t valueLength = NO_1024;

            // 1、取key
            auto serializedBinaryKey = originKeyList[index];

            // 2、生成value
            GenerateValueStateData(valueData, valueLength);
            BinaryData value(valueData, valueLength);
            uint32_t hashCode = test::HashForTest(serializedBinaryKey.Data(), serializedBinaryKey.Length());
            QueryKey queryKey(stateId, hashCode, serializedBinaryKey);
            Value putVal;
            putVal.Init(ValueType::PUT, valueLength, valueData, num++);
            // 5、Put进freshTable
            mBoostStateDb->GetFreshTable()->Put(queryKey, putVal);
            index++;
            ReleaseData(valueData);
        }
        index2++;
    }
    ASSERT_TRUE(mBoostStateDb->GetFreshTable()->IsAlreadyTriggerFlush());
    for (auto &iter : originKeyList) {
        delete[] iter.Data();
    }
}