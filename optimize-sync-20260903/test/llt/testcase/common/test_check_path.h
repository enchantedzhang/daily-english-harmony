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

#ifndef TEST_CHECK_PATH_H
#define TEST_CHECK_PATH_H

#include <glob.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <climits>
#include <csignal>
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <sstream>
#include <string>

#include "common/bss_log.h"
#include "gtest/gtest.h"
#include "securec.h"
#include "spdlog/common.h"
#include "spdlog/spdlog.h"
#include "test_utils.h"

namespace ock {
namespace bss {
namespace test {
const std::string EXIST_FILE_STR = "exist_file";
const std::string EXIST_DIR_STR = "exist_dir";

class TestCheckPath : public ::testing::Test {
public:
    TestCheckPath() = default;

    void SetUp() override
    {
        // 创建文件
        const int flags = O_RDWR | O_CREAT | O_SYNC;
        const int32_t OPEN_FILE_PERMISSION = 0640;
        int fd = open(EXIST_FILE_STR.c_str(), flags, OPEN_FILE_PERMISSION);
        ASSERT_GE(fd, 0);
        // 创建目录
        int result = mkdir(EXIST_DIR_STR.c_str(), 0640);
        ASSERT_NE(result, -1);
    }

    void TearDown() override
    {
        ASSERT_EQ(std::remove(EXIST_FILE_STR.c_str()), 0);
        ASSERT_EQ(std::remove(EXIST_DIR_STR.c_str()), 0);
    }
};

}  // namespace test
}  // namespace bss
}  // namespace ock

#endif
