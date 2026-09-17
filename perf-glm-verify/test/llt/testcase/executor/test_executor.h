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

#ifndef EXECUTOR_TEST_H
#define EXECUTOR_TEST_H

#include "executor/executor_service.h"
#include "gtest/gtest.h"

using namespace ock::bss;
class TestExecutor : public testing::Test {
public:
    TestExecutor(){};
    ~TestExecutor(){};
    // TestCase only enter once
    static void SetUpTestCase();
    static void TearDownTestCase();

    // every TEST_F macro will enter one
    void SetUp() const;
    void TearDown() const;
    static ExecutorServicePtr mService;
};
#endif
