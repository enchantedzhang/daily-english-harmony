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

#include "test_flush_table.h"

#include "flushing_bucket_group.h"
#include "generator.h"
#include "slice/data_slice.h"

using namespace ock::bss;

void TestFlushTable::SetUp()
{
    auto config = std::make_shared<Config>();
    config->Init(NO_0, NO_15, NO_16);

    InitEnv(config);
}

TEST_F(TestFlushTable, test_flush_one_slice_with_1_kv_to_level0_table_return_ok)
{
    // prepare key and values.
    uint32_t kvCount = 1;
    std::vector<std::pair<SliceKey, Value>> kvPairList;
    auto dataSlice = CreateDataSlice(kvCount, kvPairList, 11);

    // flush data slice to level0 table.
    Ref<FlushingBucketGroupIterator> iterator = MakeRef<FlushingBucketGroupIterator>();
    std::vector<std::vector<DataSliceRef>> dataSlices = std::vector<std::vector<DataSliceRef>>{ { dataSlice } };
    iterator->Initialize(dataSlices);
    auto result = mLsmStore->Put(iterator);
    ASSERT_EQ(result, BSS_OK);

    // get value by key from file store and check result.
    for (const auto &kvPair : kvPairList) {
        GetAndCheckValue(kvPair.first, kvPair.second);
    }
}

TEST_F(TestFlushTable, test_flush_one_slice_with_1024_kv_with_namespace_to_level0_table_return_ok)
{
    // prepare key and values.
    uint32_t kvCount = 1024;
    std::vector<std::pair<SliceKey, Value>> kvPairList;
    auto dataSlice = CreateDataSlice(kvCount, kvPairList, true, false, 11);

    // flush data slice to level0 table.
    Ref<FlushingBucketGroupIterator> iterator = MakeRef<FlushingBucketGroupIterator>();
    std::vector<std::vector<DataSliceRef>> dataSlices = std::vector<std::vector<DataSliceRef>>{ { dataSlice } };
    iterator->Initialize(dataSlices);
    auto result = mLsmStore->Put(iterator);
    ASSERT_EQ(result, BSS_OK);

    // get value by key from file store and check result.
    for (const auto &kvPair : kvPairList) {
        GetAndCheckValue(kvPair.first, kvPair.second);
    }
}

TEST_F(TestFlushTable, test_flush_one_slice_with_1024_kv_with_the_same_primary_key_to_level0_table_return_ok)
{
    // prepare key and values.
    uint32_t kvCount = 1024;
    std::vector<std::pair<SliceKey, Value>> kvPairList;
    auto dataSlice = CreateDataSlice(kvCount, kvPairList, false, true, 11);

    // flush data slice to level0 table.
    Ref<FlushingBucketGroupIterator> iterator = MakeRef<FlushingBucketGroupIterator>();
    std::vector<std::vector<DataSliceRef>> dataSlices = std::vector<std::vector<DataSliceRef>>{ { dataSlice } };
    iterator->Initialize(dataSlices);
    auto result = mLsmStore->Put(iterator);
    ASSERT_EQ(result, BSS_OK);

    // get value by key from file store and check result.
    for (const auto &kvPair : kvPairList) {
        GetAndCheckValue(kvPair.first, kvPair.second);
    }
}

TEST_F(TestFlushTable,
       test_flush_one_slice_with_1024_kv_with_the_same_primary_key_and_namespace_to_level0_table_return_ok)
{
    // prepare key and values.
    uint32_t kvCount = 1024;
    std::vector<std::pair<SliceKey, Value>> kvPairList;
    auto dataSlice = CreateDataSlice(kvCount, kvPairList, true, true, 11);

    // flush data slice to level0 table.
    Ref<FlushingBucketGroupIterator> iterator = MakeRef<FlushingBucketGroupIterator>();
    std::vector<std::vector<DataSliceRef>> dataSlices = std::vector<std::vector<DataSliceRef>>{ { dataSlice } };
    iterator->Initialize(dataSlices);
    auto result = mLsmStore->Put(iterator);
    ASSERT_EQ(result, BSS_OK);

    // get value by key from file store and check result.
    for (const auto &kvPair : kvPairList) {
        GetAndCheckValue(kvPair.first, kvPair.second);
    }
}

TEST_F(TestFlushTable, test_flush_one_slice_with_1024_kv_to_level0_table_return_ok)
{
    // prepare key and values.
    uint32_t kvCount = 1024;
    std::vector<std::pair<SliceKey, Value>> kvPairList;
    auto dataSlice = CreateDataSlice(kvCount, kvPairList, 11);

    // flush data slice to level0 table.
    Ref<FlushingBucketGroupIterator> iterator = MakeRef<FlushingBucketGroupIterator>();
    std::vector<std::vector<DataSliceRef>> dataSlices = std::vector<std::vector<DataSliceRef>>{ { dataSlice } };
    iterator->Initialize(dataSlices);
    auto result = mLsmStore->Put(iterator);
    ASSERT_EQ(result, BSS_OK);

    // get value by key from file store and check result.
    for (const auto &kvPair : kvPairList) {
        GetAndCheckValue(kvPair.first, kvPair.second);
    }
}

TEST_F(TestFlushTable, test_flush_two_slice_with_the_same_1_kv_to_level0_table_return_ok)
{
    // prepare key and values.
    uint32_t kvCount = 1;
    std::vector<std::pair<SliceKey, Value>> kvPairList;
    auto dataSlice = CreateDataSlice(kvCount, kvPairList, 11);

    // flush data slice to level0 table.
    Ref<FlushingBucketGroupIterator> iterator = MakeRef<FlushingBucketGroupIterator>();
    std::vector<std::vector<DataSliceRef>> dataSlices = std::vector<std::vector<DataSliceRef>>{ { dataSlice } };
    iterator->Initialize(dataSlices);
    auto result = mLsmStore->Put(iterator);
    ASSERT_EQ(result, BSS_OK);

    // get value by key from file store and check result.
    for (const auto &kvPair : kvPairList) {
        GetAndCheckValue(kvPair.first, kvPair.second);
    }
}

TEST_F(TestFlushTable, test_flush_two_slice_with_the_same_1024_kv_to_level0_table_return_ok)
{
    // prepare key and values.
    uint32_t kvCount = 1024;
    std::vector<std::pair<SliceKey, Value>> kvPairList;
    auto dataSlice = CreateDataSlice(kvCount, kvPairList, 11);

    // flush data slice to level0 table.
    Ref<FlushingBucketGroupIterator> iterator = MakeRef<FlushingBucketGroupIterator>();
    std::vector<std::vector<DataSliceRef>> dataSlices = std::vector<std::vector<DataSliceRef>>{ { dataSlice } };
    iterator->Initialize(dataSlices);
    auto result = mLsmStore->Put(iterator);
    ASSERT_EQ(result, BSS_OK);

    // get value by key from file store and check result.
    for (const auto &kvPair : kvPairList) {
        GetAndCheckValue(kvPair.first, kvPair.second);
    }
}

TEST_F(TestFlushTable, test_flush_two_slice_with_the_diff_1024_kv_to_level0_table_return_ok)
{
    // prepare key and values.
    uint32_t kvCount = 1024;
    std::vector<std::pair<SliceKey, Value>> kvPairList1;
    auto dataSlice1 = CreateDataSlice(kvCount, kvPairList1, 11);

    std::vector<std::pair<SliceKey, Value>> kvPairList2;
    auto dataSlice2 = CreateDataSlice(kvCount, kvPairList2, 12);

    // flush data slice to level0 table.
    Ref<FlushingBucketGroupIterator> iterator = MakeRef<FlushingBucketGroupIterator>();
    std::vector<std::vector<DataSliceRef>> dataSlices =
        std::vector<std::vector<DataSliceRef>>{ { dataSlice1, dataSlice2 } };
    iterator->Initialize(dataSlices);
    auto result = mLsmStore->Put(iterator);
    ASSERT_EQ(result, BSS_OK);

    // get value by key from file store and check result.
    kvPairList1.insert(kvPairList1.end(), kvPairList2.begin(), kvPairList2.end());
    for (const auto &kvPair : kvPairList1) {
        GetAndCheckValue(kvPair.first, kvPair.second);
    }
}
