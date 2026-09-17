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

#include "test_mem_manager.h"

#include "include/bss_types.h"
#include "include/config.h"
#include "memory/allocator.h"
#include "securec.h"

using namespace ock::bss;
MemManagerRef TestMemManager::mDirectMemManager = nullptr;

void TestMemManager::SetUp() const
{
}

void TestMemManager::TearDown() const
{
}

void TestMemManager::SetUpTestCase()
{
}

void TestMemManager::TearDownTestCase()
{
}

TEST_F(TestMemManager, TestMemManagerInitialize1)
{
    ConfigRef config = std::make_shared<Config>();
    ASSERT_TRUE(config != nullptr);
    config->mTotalDBSize = NO_1024 * NO_1024 * NO_1024 * NO_2;

    mDirectMemManager = std::make_shared<MemManager>(AllocatorType::DIRECT);
    mDirectMemManager->Initialize(config);
}

TEST_F(TestMemManager, TestMemManagerAllocateAndFree1)
{
    ConfigRef config = std::make_shared<Config>();
    ASSERT_TRUE(config != nullptr);
    config->mTotalDBSize = NO_1024 * NO_1024 * NO_1024 * NO_2;
    mDirectMemManager->Initialize(config);
    uintptr_t compactionAddr = 0;
    BResult hr = mDirectMemManager->GetMemory(MemoryType::SLICE_TABLE, NO_4096, compactionAddr);
    ASSERT_EQ(hr, BSS_OK);
    std::string str = "do test";
    auto result = memcpy_s(reinterpret_cast<void *>(compactionAddr), 4096, str.c_str(), strlen(str.c_str()));
    ASSERT_EQ(result, BSS_OK);
    hr = mDirectMemManager->ReleaseMemory(compactionAddr);
    ASSERT_EQ(hr, BSS_OK);
    hr = mDirectMemManager->GetMemory(MemoryType::SLICE_TABLE, NO_1000, compactionAddr);
    ASSERT_EQ(hr, BSS_OK);
    uint64_t usedSize = mDirectMemManager->GetMemoryUseSize(MemoryType::SLICE_TABLE);
    uint64_t realSize = mDirectMemManager->CalcSize(NO_1000);
    ASSERT_EQ(usedSize, realSize);
    mDirectMemManager->ReleaseMemory(compactionAddr);
}

TEST_F(TestMemManager, TestMemManagerAllocateAndFree2)
{
    ConfigRef config = std::make_shared<Config>();
    ASSERT_TRUE(config != nullptr);
    config->mTotalDBSize = NO_1024 * NO_1024 * NO_1024 * NO_2;
    mDirectMemManager->Initialize(config, false);
    uintptr_t compactionAddr = 0;
    BResult hr = mDirectMemManager->GetMemory(MemoryType::SLICE_TABLE, NO_4096, compactionAddr);
    ASSERT_EQ(hr, BSS_OK);
    std::string str = "do test";
    auto result = memcpy_s(reinterpret_cast<void *>(compactionAddr), NO_4096, str.c_str(), strlen(str.c_str()));
    ASSERT_EQ(result, BSS_OK);
    hr = mDirectMemManager->ReleaseMemory(compactionAddr);
    ASSERT_EQ(hr, BSS_OK);
}

TEST_F(TestMemManager, TestPoolMemManager)
{
    ConfigRef config = std::make_shared<Config>();
    ASSERT_TRUE(config != nullptr);
    config->mTotalDBSize = NO_1024 * NO_1024 * NO_1024 * NO_2;
    mDirectMemManager->Initialize(config, false);
    std::vector<uintptr_t> list;
    for (uint32_t i = 0; i < NO_10000; ++i) {
        uintptr_t compactionAddr = 0;
        BResult hr = mDirectMemManager->GetMemory(MemoryType::SLICE_TABLE, NO_4096 * (i % 7 + 1), compactionAddr);
        ASSERT_EQ(hr, BSS_OK);
        list.emplace_back(compactionAddr);
    }

    for (const auto &item : list) {
        mDirectMemManager->ReleaseMemory(const_cast<uintptr_t &>(item));
    }
}

TEST_F(TestMemManager, TestMemManagerAllocateAndFree3)
{
    ConfigRef config = std::make_shared<Config>();
    ASSERT_TRUE(config != nullptr);
    config->mTotalDBSize = NO_1024 * NO_1024 * NO_1024 * NO_2;
    mDirectMemManager->Initialize(config);
    uintptr_t compactionAddr = 0;
    BResult hr = mDirectMemManager->ReleaseMemory(compactionAddr);
    ASSERT_NE(hr, BSS_OK);
    hr = mDirectMemManager->GetMemory(MemoryType::SLICE_TABLE, NO_4096, compactionAddr);
    ASSERT_EQ(hr, BSS_OK);
    std::string str = "do test";
    auto result = memcpy_s(reinterpret_cast<void *>(compactionAddr), 4096, str.c_str(), strlen(str.c_str()));
    ASSERT_EQ(result, BSS_OK);
    hr = mDirectMemManager->ReleaseMemory(compactionAddr);
    ASSERT_EQ(hr, BSS_OK);
}

TEST_F(TestMemManager, TestMemManagerMemoryUseSize)
{
    uint64_t usedSize = mDirectMemManager->GetMemoryUseSize(MemoryType::SLICE_TABLE);
    uint64_t expect = 0;
    ASSERT_EQ(usedSize, expect);
    uintptr_t compactionAddr = 0;
    BResult hr = mDirectMemManager->GetMemory(MemoryType::SLICE_TABLE, NO_4096, compactionAddr);
    uint64_t realSize1 = mDirectMemManager->CalcSize(NO_4096);
    ASSERT_EQ(hr, BSS_OK);
    uintptr_t compactionAddr1 = 0;
    hr = mDirectMemManager->GetMemory(MemoryType::SLICE_TABLE, NO_1024, compactionAddr1);
    ASSERT_EQ(hr, BSS_OK);
    usedSize = mDirectMemManager->GetMemoryUseSize(MemoryType::SLICE_TABLE);
    uint64_t realSize2 = mDirectMemManager->CalcSize(NO_1024);
    ASSERT_EQ(usedSize, realSize1 + realSize2);
    hr = mDirectMemManager->ReleaseMemory(compactionAddr);
    ASSERT_EQ(hr, BSS_OK);
    usedSize = mDirectMemManager->GetMemoryUseSize(MemoryType::SLICE_TABLE);
    ASSERT_EQ(usedSize, realSize2);
    mDirectMemManager->ReleaseMemory(compactionAddr1);
}

TEST_F(TestMemManager, TestAllocator1)
{
    AllocatorPtr allocator = new DirectAllocator();
    uintptr_t mrAddress = 0;
    uint64_t mrSize = 0;
    uint32_t minBlockSize = NO_4096;
    bool alignAddress = true;
    BResult hr = allocator->Initialize(mrAddress, mrSize, minBlockSize, alignAddress);
    ASSERT_EQ(hr, BSS_OK);
    allocator->MemOffset(mrAddress);
    allocator->FreeSize();
    hr = allocator->Free(mrAddress);
    ASSERT_EQ(hr, BSS_INVALID_PARAM);
    allocator->Destroy();
}