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

#include "test_lsm_store.h"

using namespace ock::bss;

class TestFileManager : public TestLsmStore {
public:
    GeneratorRef mGenerator;
    void SetUp() override
    {
        auto config = std::make_shared<Config>();
        config->Init(NO_0, NO_15, NO_16);
        InitEnv(config);
        mGenerator = MakeRef<Generator>(44);
    }
};

#endif
