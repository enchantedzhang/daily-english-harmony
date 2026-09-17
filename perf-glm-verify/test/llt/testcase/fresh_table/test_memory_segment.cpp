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

#include "test_memory_segment.h"

#include <random>

using namespace ock::bss;
MemManagerRef TestMemorySegment::mMemManager = nullptr;
void TestMemorySegment::SetUp() const
{
}

void TestMemorySegment::TearDown() const
{
}

void TestMemorySegment::SetUpTestCase()
{
    ConfigRef config = std::make_shared<Config>();
    config->Init(NO_0, NO_15, NO_16);

    mMemManager = std::make_shared<MemManager>(AllocatorType::DIRECT);
    mMemManager->Initialize(config);
}

void TestMemorySegment::TearDownTestCase()
{
}

TEST_F(TestMemorySegment, PutAndGetUint8_t)
{
    uint32_t capacity = NO_1024 * NO_1024 * NO_32;

    uintptr_t addr = 0;
    mMemManager->GetMemory(MemoryType::SLICE_TABLE, capacity, addr);

    MemorySegmentRef memorySegment = MakeRef<MemorySegment>(capacity, reinterpret_cast<uint8_t *>(addr), mMemManager);

    std::random_device rd;
    std::mt19937 gen(rd());
    uint32_t positionBegin = 1;
    uint32_t positionEnd = capacity - 1;
    std::uniform_int_distribution<uint8_t> ValueDist(0, UINT8_MAX);
    std::uniform_int_distribution<uint32_t> PositionDist(positionBegin, positionEnd);

    uint32_t loopTimes = 100000;
    for (uint32_t i = 0; i <= loopTimes; i++) {
        uint8_t value = static_cast<uint8_t>(ValueDist(gen));
        uint32_t offset = static_cast<uint32_t>(PositionDist(gen));

        memorySegment->PutUint8_t(offset, value);
        uint8_t readValue = memorySegment->ToUint8_t(offset);
        ASSERT_TRUE(readValue == value);
    }
}

TEST_F(TestMemorySegment, PutAndGetUint16_t)
{
    uint32_t capacity = NO_1024 * NO_1024 * NO_32;

    uintptr_t addr = 0;
    mMemManager->GetMemory(MemoryType::SLICE_TABLE, capacity, addr);

    MemorySegmentRef memorySegment = MakeRef<MemorySegment>(capacity, reinterpret_cast<uint8_t *>(addr), mMemManager);

    std::random_device rd;
    std::mt19937 gen(rd());
    uint32_t positionBegin = 1;
    uint32_t positionEnd = capacity - NO_2;
    std::uniform_int_distribution<uint16_t> ValueDist(0, UINT16_MAX);
    std::uniform_int_distribution<uint32_t> PositionDist(positionBegin, positionEnd);

    uint32_t loopTimes = 100000;
    for (uint32_t i = 0; i <= loopTimes; i++) {
        uint16_t value = static_cast<uint16_t>(ValueDist(gen));
        uint32_t offset = static_cast<uint32_t>(PositionDist(gen));

        memorySegment->PutUint16_t(offset, value);
        uint16_t readValue = memorySegment->ToUint16_t(offset);
        ASSERT_TRUE(readValue == value);
    }
}

TEST_F(TestMemorySegment, PutAndGetUint32_t)
{
    uint32_t capacity = NO_1024 * NO_1024 * NO_32;

    uintptr_t addr = 0;
    mMemManager->GetMemory(MemoryType::SLICE_TABLE, capacity, addr);

    MemorySegmentRef memorySegment = MakeRef<MemorySegment>(capacity, reinterpret_cast<uint8_t *>(addr), mMemManager);

    std::random_device rd;
    std::mt19937 gen(rd());
    uint32_t positionBegin = 1;
    uint32_t positionEnd = capacity - NO_4;
    std::uniform_int_distribution<uint32_t> ValueDist(0, UINT32_MAX);
    std::uniform_int_distribution<uint32_t> PositionDist(positionBegin, positionEnd);

    uint32_t loopTimes = 100000;
    for (uint32_t i = 0; i <= loopTimes; i++) {
        uint32_t value = static_cast<uint32_t>(ValueDist(gen));
        uint32_t offset = static_cast<uint32_t>(PositionDist(gen));

        memorySegment->PutUint32_t(offset, value);
        uint32_t readValue = memorySegment->ToUint32_t(offset);
        ASSERT_TRUE(readValue == value);
    }
}

TEST_F(TestMemorySegment, PutAndGetUint64_t)
{
    uint32_t capacity = NO_1024 * NO_1024 * NO_32;
    uintptr_t addr = 0;
    mMemManager->GetMemory(MemoryType::SLICE_TABLE, capacity, addr);
    MemorySegmentRef memorySegment = MakeRef<MemorySegment>(capacity, reinterpret_cast<uint8_t *>(addr), mMemManager);

    std::random_device rd;
    std::mt19937 gen(rd());
    uint32_t positionBegin = 1;
    uint32_t positionEnd = capacity - NO_8;
    std::uniform_int_distribution<uint64_t> ValueDist(0, UINT64_MAX);
    std::uniform_int_distribution<> PositionDist(positionBegin, positionEnd);

    uint32_t loopTimes = 100000;
    for (uint32_t i = 0; i <= loopTimes; i++) {
        uint64_t value = static_cast<uint64_t>(ValueDist(gen));
        uint32_t offset = static_cast<uint32_t>(PositionDist(gen));

        memorySegment->PutUint64_t(offset, value);
        uint64_t readValue = memorySegment->ToUint64_t(offset);
        ASSERT_TRUE(readValue == value);
    }
}
