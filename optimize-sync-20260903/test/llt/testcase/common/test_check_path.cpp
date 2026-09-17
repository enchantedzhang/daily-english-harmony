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

#include "test_check_path.h"

#include "common/path_transform.h"
#include "jni/kv_helper.h"

namespace ock {
namespace bss {
namespace test {
TEST_F(TestCheckPath, test_path_utils_path_transform_extract_file_name)
{
    // 截取不能影响原始字符串
    std::string path = "/data/local/tmp.log";
    const auto &extractFileName = PathTransform::ExtractFileName(path);
    ASSERT_EQ(path, "/data/local/tmp.log");
    ASSERT_EQ(extractFileName, "tmp.log");
}

TEST_F(TestCheckPath, test_path_utils_path_transform_extract_directory)
{
    // 截取不能影响原始字符串
    std::string path = "/data/local/tmp.log";
    const auto &extracDirectory = PathTransform::ExtractDirectory(path);
    ASSERT_EQ(path, "/data/local/tmp.log");
    ASSERT_EQ(extracDirectory, "/data/local");
}

TEST_F(TestCheckPath, test_kv_helper_check_path_valid_for_file_not_ok)
{
    auto realPath = realpath(EXIST_FILE_STR.c_str(), nullptr);
    ASSERT_EQ(realPath != nullptr, true);
    std::string existPath(realPath);
    free(realPath);
    // 不是目录，不可通过CheckPath
    ASSERT_EQ(CheckPathValid(existPath, false, true), false);
}

TEST_F(TestCheckPath, test_kv_helper_check_path_valid_for_directory_ok)
{
    auto realPath = realpath(EXIST_DIR_STR.c_str(), nullptr);
    ASSERT_EQ(realPath != nullptr, true);
    std::string existPath(realPath);
    free(realPath);
    // 是目录，可通过CheckPath
    ASSERT_EQ(CheckPathValid(existPath, false, true), true);
}

TEST_F(TestCheckPath, test_kv_helper_check_path_valid_for_remote_ok)
{
    // HDFS远端路径，即使不存在，也可通过Check
    std::string hdfsPath = "hdfs://namenode:8020/data/input/logs/2023/access.log";
    ASSERT_EQ(CheckPathValid(hdfsPath, true), true);
}

TEST_F(TestCheckPath, test_kv_helper_check_path_valid)
{
    auto realPath = realpath(EXIST_DIR_STR.c_str(), nullptr);
    ASSERT_EQ(realPath != nullptr, true);
    std::string existPath(realPath);
    free(realPath);

    std::string noExistPath = PathTransform::ExtractDirectory(existPath) + "/no_exist_dir";
    // 路径不存在，无法通过Check
    ASSERT_EQ(CheckPathValid(noExistPath), false);
    // 截取后路径存在，可通过Check
    ASSERT_EQ(CheckPathValid(PathTransform::ExtractDirectory(noExistPath)), true);

    // 以“file://”开头并且存在，可通过Check
    std::string existPath2 = "file://" + existPath;
    ASSERT_EQ(CheckPathValid(existPath2), true);

    // 以“file://”开头但不存在，不可通过Check
    std::string noExistPath2 = "file://" + noExistPath;
    ASSERT_EQ(CheckPathValid(noExistPath2), false);
}
}  // namespace test
}  // namespace bss
}  // namespace ock