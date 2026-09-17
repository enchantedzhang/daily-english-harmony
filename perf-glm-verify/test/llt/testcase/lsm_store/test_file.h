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

#ifndef LOG_TEST_H
#define LOG_TEST_H

#include <random>

#include "test_lsm_store.h"

using namespace ock::bss;

class TestFile : public TestLsmStore {
public:
    std::vector<std::pair<SliceKey, Value>> GenerateKeyValue(uint64_t size, int32_t seed = -1);
    std::vector<std::pair<SliceKey, Value>> GenerateKeyMapValue(uint64_t size, std::vector<uint32_t> &priKeyHash,
                                                                KeyVectorValueMap &prefixMap, int32_t seed = -1);
    std::vector<std::pair<SliceKey, Value>> GenerateListKeyValue(uint64_t size, ValueType valueType, int32_t seed = -1);
    Value GenerateDeleteValue();
    std::vector<uint8_t> BuildFilterBlock(std::set<uint32_t> hashVec);
    bool KeyMatch(std::vector<uint8_t> array, uint32_t hash);
};

#endif
