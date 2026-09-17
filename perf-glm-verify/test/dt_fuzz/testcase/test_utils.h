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

#ifndef BOOST_STATE_STORE_KV_TEST_UTILS_H
#define BOOST_STATE_STORE_KV_TEST_UTILS_H

#include <glob.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <atomic>
#include <climits>
#include <csignal>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <deque>
#include <fstream>
#include <iostream>
#include <memory>
#include <mutex>
#include <queue>
#include <random>
#include <sstream>
#include <string>
#include <thread>
#include <unordered_map>

#include "gtest/gtest.h"
#include "securec.h"

#define private public
#define protected public

#include "../fuzz_main.h"
#include "include/bss_types.h"

namespace ock {
namespace bss {
namespace test {
inline std::vector<uint8_t> GetRandomData(uint32_t length, uint8_t byte)
{
    std::vector<uint8_t> data(length);

    for (uint32_t i = 0; i < length; ++i) {
        data[i] = byte;
    }

    return data;
}

inline std::vector<uint8_t> GetRandomData(const std::vector<std::vector<uint8_t>> &sourceKey)
{
    std::random_device rd;
    std::mt19937 gen(g_testSeed++);

    uint32_t mapSize = sourceKey.size();
    std::uniform_int_distribution<> dist(0, mapSize - 1);

    return sourceKey[dist(gen)];
}

inline std::string GetCurrentWorkingDirectory()
{
    char buffer[1024];
    if (getcwd(buffer, sizeof(buffer)) != nullptr) {
        return std::string(buffer);
    }
    return "";
}

bool RemoveDirectoryRecursive(const std::string &path)
{
    DIR *dir = opendir(path.c_str());
    if (!dir) {
        return unlink(path.c_str()) == 0;
    }

    dirent *entry;
    while ((entry = readdir(dir)) != nullptr) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        std::string fullPath = path + "/" + entry->d_name;
        if (entry->d_type == DT_DIR) {
            // 递归删除子目录
            if (!RemoveDirectoryRecursive(fullPath)) {
                closedir(dir);
                return false;
            }
        } else {
            // 删除文件
            if (unlink(fullPath.c_str())) {
                closedir(dir);
                return false;
            }
        }
    }
    closedir(dir);

    // 删除空目录
    return rmdir(path.c_str()) == 0;
}

inline bool DeleteCpFile()
{
    RemoveDirectoryRecursive("/home/workspace/w30061906/self/boost_state_store/tmp/");
    return true;
}

struct NsKey {
    std::vector<uint8_t> mKey;
    std::vector<uint8_t> mNS;

    // vector是深拷贝
    NsKey(const std::vector<uint8_t> &key, std::vector<uint8_t> ns) : mKey(key), mNS(ns)
    {
    }

    bool operator<(const NsKey &other) const
    {
        return std::tie(mKey, mNS) < std::tie(other.mKey, other.mNS);
    }
};

}  // namespace test
}  // namespace bss
}  // namespace ock

#endif