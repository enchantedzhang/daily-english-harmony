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

#include "fuzz_main.h"

#include "include/boost_state_table.h"

using namespace ock::bss;

int32_t g_testSeed = 24;
int main(int argc, char *argv[])
{
    testing::InitGoogleTest(&argc, argv);

    // 1. 初始化日志模块.
    LoggerOptions options = { 1, 2, 50, 20, "./test_log" };
    Logger *pLogger = Logger::Instance(options);
    int32_t ret = pLogger->Init();
    if (ret != 0) {
        printf("Init test logger failed, ret:%d", ret);
        return -1;
    }

    // 2. 执行测试用例集.
    ret = RUN_ALL_TESTS();

    Logger::Destroy();
    return ret;
}

int ExampleReturnZero()
{
    return 0;
}