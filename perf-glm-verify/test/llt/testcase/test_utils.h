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

#include <cstdint>
#include <fstream>
#include <iostream>
#include <memory>
#include <random>

#include "gtest/gtest.h"

#define private public
#define protected public

#include "../ut_main.h"
#include "include/bss_types.h"

namespace ock {
namespace bss {
namespace test {
inline std::vector<uint8_t> GetRandomData(bool useRandomLen = true, uint32_t length = 200)
{
    std::random_device rd;
    std::mt19937 gen(g_testSeed++);
    uint32_t lenBegin = 1;
    uint32_t lenEnd = 300;
    uint32_t byteBegin = 0;
    uint32_t byteEnd = 255;

    std::uniform_int_distribution<> LenDist(lenBegin, lenEnd);
    std::uniform_int_distribution<> byteDist(byteBegin, byteEnd);

    if (useRandomLen) {
        length = LenDist(gen);
    }
    std::vector<uint8_t> data(length);

    for (uint32_t i = 0; i < length; ++i) {
        data[i] = static_cast<uint8_t>(byteDist(gen));
    }

    return data;
}

inline std::vector<uint8_t> GetRandom2MData()
{
    std::random_device rd;
    std::mt19937 gen(g_testSeed++);
    uint32_t byteBegin = 0;
    uint32_t byteEnd = 255;

    std::uniform_int_distribution<> byteDist(byteBegin, byteEnd);
    std::vector<uint8_t> data(IO_SIZE_2M);

    for (uint32_t i = 0; i < IO_SIZE_2M; ++i) {
        data[i] = static_cast<uint8_t>(byteDist(gen));
    }

    return data;
}

inline uint32_t GetRandomLen()
{
    std::random_device rd;
    std::mt19937 gen(g_testSeed++);
    uint32_t lenBegin = 0;
    uint32_t lenEnd = IO_SIZE_1M;
    std::uniform_int_distribution<> LenDist(lenBegin, lenEnd);

    return LenDist(gen);
}

inline std::vector<uint8_t> GetRandomData(const std::vector<std::vector<uint8_t>> &sourceKey)
{
    std::random_device rd;
    std::mt19937 gen(g_testSeed++);

    uint32_t mapSize = sourceKey.size();
    std::uniform_int_distribution<> dist(0, mapSize - 1);

    return sourceKey[dist(gen)];
}

inline static uint32_t HashForTest(const uint8_t *data, uint32_t dataLen)
{
    uint32_t hash = NO_1;
    for (uint32_t i = 0; i < dataLen; i++) {
        hash = NO_31 * hash + data[i];
    }
    return hash & 0x7FFFFFFF;
}

inline void PrintData(std::vector<uint8_t> data)
{
    for (uint32_t i = 0; i < data.size(); ++i) {
        std::cout << static_cast<int>(data[i]) << ",";
    }
    std::cout << std::endl;
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