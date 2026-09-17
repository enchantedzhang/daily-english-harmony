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

#ifndef TEST_SLICE_COMPACTION_H
#define TEST_SLICE_COMPACTION_H
#include <unistd.h>

#include <cmath>
#include <memory>
#include <random>

#include "boost_state_db_impl.h"
#include "common/test_bss_log.h"

using namespace ock::bss;
class TestSliceCompaction : public testing::Test {
public:
    TestSliceCompaction(){};
    ~TestSliceCompaction(){};
    // TestCase only enter once
    static void SetUpTestCase();
    static void TearDownTestCase();
    // every TEST_F macro will enter one
    void SetUp() const;
    void TearDown() const;
    static BoostStateDBImpl *mBoostStateDb;
    static ConfigRef mConfig;
    static inline void GenerateValueStateData(uint8_t *&data, uint32_t &length)
    {
        std::random_device rd;
        std::mt19937 gen(rd());

        uint32_t byteBegin = NO_1;
        uint32_t byteEnd = NO_255;

        std::uniform_int_distribution<> byteDist(byteBegin, byteEnd);

        data = new (std::nothrow) uint8_t[length];
        ASSERT_NE(data, nullptr);

        for (uint32_t i = 0; i < length; ++i) {
            data[i] = static_cast<uint8_t>(byteDist(gen));
        }
        // 保证valueData中的第一位：ValueType = PUT = 2,
        data[NO_0] = NO_2;
        // KeyData中的StateId= 00，保证算出来的state是valueState
        data[NO_4] = 0;
        data[NO_5] = 0;
    }

    static inline void GenerateKeyStateData(uint8_t *&data, uint32_t &length)
    {
        std::random_device rd;
        std::mt19937 gen(rd());

        uint32_t byteBegin = NO_1;
        uint32_t byteEnd = NO_255;

        std::uniform_int_distribution<> byteDist(byteBegin, byteEnd);

        data = new (std::nothrow) uint8_t[length];
        ASSERT_NE(data, nullptr);

        for (uint32_t i = 0; i < length; ++i) {
            data[i] = static_cast<uint8_t>(byteDist(gen));
        }
        // KeyData中的StateId= 00，保证算出来的state是valueState
        data[NO_4] = 0;
        data[NO_5] = 0;
    }

    static void ReleaseData(uint8_t *data)
    {
        delete[] data;
    }
};
#endif  // TEST_SLICE_COMPACTION_H
