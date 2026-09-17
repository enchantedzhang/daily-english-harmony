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

#ifndef BOOST_SS_TEST_MEM_MANAGER_H
#define BOOST_SS_TEST_MEM_MANAGER_H

#include "gtest/gtest.h"
#include "memory/mem_manager.h"

using namespace ock::bss;
class TestMemManager : public testing::Test {
public:
    TestMemManager(){};

    ~TestMemManager(){};

    // TestCase only enter once
    static void SetUpTestCase();

    static void TearDownTestCase();

    // every TEST_F macro will enter one
    void SetUp() const;

    void TearDown() const;

    static MemManagerRef mDirectMemManager;
};
#endif  // BOOST_SS_TEST_MEM_MANAGER_H
