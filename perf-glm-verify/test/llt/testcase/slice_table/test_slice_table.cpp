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

#include "test_slice_table.h"

using namespace ock::bss;
ConfigRef TestSliceTable::mConfig = nullptr;
BoostStateDBImpl *TestSliceTable::mBoostStateDb = nullptr;
BoostNativeMetric *TestSliceTable::mMetric = nullptr;

void TestSliceTable::SetUp()
{
    ConfigRef config = std::make_shared<Config>();
    config->Init(NO_0, NO_15, NO_16);
    mConfig = config;
    mConfig->mMemorySegmentSize = IO_SIZE_4M;
    mBoostStateDb = (BoostStateDBImpl *)BoostStateDBFactory::Create();
    mBoostStateDb->Open(config);
    mMetric = new BoostNativeMetric(NO_31);
    mMetric->Init();
    mBoostStateDb->RegisterMetric(mMetric);

    // initialize generator
    mGenerator = MakeRef<Generator>(42);
}
void TestSliceTable::TearDown()
{
    mBoostStateDb->Close();
    delete mBoostStateDb;
}

TEST_F(TestSliceTable, test_kmap_add_1_slice_and_get_return_ok)
{
    // generator kv pair and add to slice.
    std::vector<KVPair> kvList;
    AddSlice(1, kvList);

    // get value by key from slice table.
    for (const auto &kv : kvList) {
        ValidateResult(kv);
    }
}

TEST_F(TestSliceTable, test_kmap_add_1K_slice_and_get_return_ok)
{
    // generator kv pair and add to slice.
    std::vector<KVPair> kvList;
    AddSlice(1024, kvList);

    // get value by key from slice table.
    for (const auto &kv : kvList) {
        ValidateResult(kv);
    }
}

TEST_F(TestSliceTable, test_kmap_add_slice_and_get_return_not_exit)
{
    // generator kv pair and add to slice.
    std::vector<KVPair> kvList;
    AddSlice(1024, kvList);

    // prepare new kv, check it not exist.
    auto kv = PrepareKvPair(NO_1024);
    ValidateResult(kv, false);
}

TEST_F(TestSliceTable, test_kv_add_1_slice_and_get_return_ok)
{
    // generator kv pair and add to slice.
    std::vector<KVPair> kvList;
    AddSliceForKv(1, kvList);

    // get value by key from slice table.
    for (const auto &kv : kvList) {
        ValidateResultForKv(kv);
    }
}

TEST_F(TestSliceTable, test_kv_add_1K_slice_and_get_return_ok)
{
    // generator kv pair and add to slice.
    std::vector<KVPair> kvList;
    AddSliceForKv(1024, kvList);

    // get value by key from slice table.
    for (const auto &kv : kvList) {
        ValidateResultForKv(kv);
    }
}

TEST_F(TestSliceTable, test_kv_add_slice_and_get_return_not_exit)
{
    // generator kv pair and add to slice.
    std::vector<KVPair> kvList;
    AddSliceForKv(1024, kvList);

    // prepare new kv, check it not exist.
    auto kv = PrepareKvPairForKv(NO_1024);
    ValidateResultForKv(kv, false);
}

TEST_F(TestSliceTable, test_kv_add_delete_kv_return_not_exit)
{
    // generator kv pair and add to slice.
    std::vector<KVPair> kvList;
    AddSliceForKv(2, kvList);

    // generator kv pair for delete
    AddSliceForDeleteKv(kvList);

    // delete kv check it not exist.
    for (auto iter : kvList) {
        ValidateResultForKv(iter, false);
    }
}
