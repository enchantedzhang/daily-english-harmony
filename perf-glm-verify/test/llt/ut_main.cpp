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
#include <dirent.h>
#include <gtest/gtest.h>

#include <string>

#include "memory/mem_manager.h"

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

std::string GetCurrentWorkingDirectory()
{
    char buffer[1024];
    if (getcwd(buffer, sizeof(buffer)) != nullptr) {
        return std::string(buffer);
    }
    return "";
}

int RemoveFile(const char *fpath, const struct stat *sb, int typeflag, struct FTW *ftwbuf)
{
    return remove(fpath);
}

void CleanupSstFiles()
{
    std::string currentDir = GetCurrentWorkingDirectory();
    DIR *dirp = opendir(currentDir.c_str());
    if (dirp) {
        struct dirent *entry;
        while ((entry = readdir(dirp)) != nullptr) {
            std::string fileName = entry->d_name;
            if (fileName.find("sst") != std::string::npos) {
                std::string filePath = currentDir + "/" + fileName;
                remove(filePath.c_str());
            }
        }
        closedir(dirp);
    }
}

void CleanupBlobFiles()
{
    std::string currentDir = GetCurrentWorkingDirectory();
    std::string currentBlobDir = currentDir + "/blobFile";
    DIR *dirp = opendir(currentBlobDir.c_str());
    if (dirp) {
        struct dirent *entry;
        while ((entry = readdir(dirp)) != nullptr) {
            std::string fileName = entry->d_name;
            if (fileName.find("sst") != std::string::npos) {
                std::string filePath = currentBlobDir + "/" + fileName;
                remove(filePath.c_str());
            }
        }
        closedir(dirp);
    }
}

void CleanupTombstoneFiles()
{
    std::string currentDir = GetCurrentWorkingDirectory();
    std::string currentBlobDir = currentDir + "/blobFile";
    DIR *dirp = opendir(currentBlobDir.c_str());
    if (dirp) {
        struct dirent *entry;
        while ((entry = readdir(dirp)) != nullptr) {
            std::string fileName = entry->d_name;
            if (fileName.find("sst") != std::string::npos) {
                std::string filePath = currentBlobDir + "/" + fileName;
                remove(filePath.c_str());
            }
        }
        closedir(dirp);
    }
}