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

#include "test_slice_table_manager.h"

#include <unistd.h>

#include <map>

#include "fresh_table/fresh_table.h"

using namespace ock::bss;

ConfigRef TestSliceTableManager::mConfig = nullptr;
BoostStateDBImpl *TestSliceTableManager::mBoostStateDb = nullptr;

void TestSliceTableManager::SetUp() const
{
}

void TestSliceTableManager::TearDown() const
{
}

void TestSliceTableManager::SetUpTestCase()
{
    ConfigRef config = std::make_shared<Config>();
    config->Init(NO_0, NO_15, NO_16);
    mConfig = config;
    mConfig->mMemorySegmentSize = IO_SIZE_4M;
    mBoostStateDb = (BoostStateDBImpl *)BoostStateDBFactory::Create();
    mBoostStateDb->Open(config);
}

void TestSliceTableManager::TearDownTestCase()
{
    mBoostStateDb->Close();
    delete mBoostStateDb;
}

TEST_F(TestSliceTableManager, TestSliceTableManagerInitWhenConfigIsNullThenReturnErr)
{
    std::shared_ptr<SliceTable> sliceTableManager = std::make_shared<SliceTable>();
    BResult ret = sliceTableManager->Initialize(nullptr, nullptr, mBoostStateDb->GetMemManager(),
                                                ock::bss::StateFilterManagerRef());
    ASSERT_TRUE(ret == BSS_ERR);
}
