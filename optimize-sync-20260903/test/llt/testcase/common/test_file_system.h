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

#ifndef TEST_FILE_SYSTEM_H
#define TEST_FILE_SYSTEM_H
#include "fs/file_system.h"
#include "gtest/gtest.h"
#include "path.h"

namespace ock {
namespace bss {
namespace test {

class TestFileSystem : public testing::Test {
public:
    void SetUp() override
    {
        const std::string testFilePath = "test.txt";
        Uri uri(testFilePath);
        PathRef path = std::make_shared<Path>(uri);
        fileSystem = FileSystem::CreateFileSystem(FileSystemType::HDFS, path);
    }

    void TearDown() override
    {
        fileSystem->Remove();
        fileSystem->Close();
        fileSystem = nullptr;
    }

public:
    FileSystemRef fileSystem = nullptr;
};
}  // namespace test
}  // namespace bss
}  // namespace ock

#endif
