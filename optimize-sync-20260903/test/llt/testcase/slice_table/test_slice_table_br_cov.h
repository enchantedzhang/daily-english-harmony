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

#ifndef BOOST_SS_TEST_SLICE_TABLE_BR_COV_H
#define BOOST_SS_TEST_SLICE_TABLE_BR_COV_H

#include "gtest/gtest.h"
#define private public
#define protected public
#include "boost_state_db_impl.h"
#include "generator.h"
#undef private
#undef protected

using namespace ock::bss;

class TestSliceTableBrCov : public ::testing::Test {
public:
    void SetUp() override;
    void TearDown() override;
    static BoostStateDBImpl *mBoostStateDb;
    static GeneratorRef mGenerator;
    static SliceTableManagerRef mSliceTable;
    static LogicalSliceChainRef mLogicalSliceChain;
    static SliceIndexContextRef mSliceIndexContext;
    static FileInputViewRef mInputView;
    static FileOutputViewRef mOutputView;

    LogicalSliceChainRef mockAddSlice()
    {
        auto kvPair = getOneKVPair();
        uint32_t priHashCode = kvPair.first.KeyHashCode();
        auto context = mBoostStateDb->GetSliceTable()->GetSliceBucketIndex()->GetSliceIndexContext(priHashCode, true);
        std::vector<std::pair<SliceKey, Value>> kvList;
        kvList.push_back(kvPair);
        mBoostStateDb->GetSliceTable()->AddSlice(context, kvList, 0);
        return context->GetLogicalSliceChain();
    }
    std::pair<SliceKey, Value> getOneKVPair()
    {
        SliceKey dualKey = mGenerator->GenerateDualKey(NO_1, NO_2);
        Value value = mGenerator->GenerateValue(NO_10);
        return std::pair<SliceKey, Value>(dualKey, value);
    }
    SliceRef getSlice()
    {
        SliceRef slice = std::make_shared<Slice>();
        SliceCreateMeta meta = { 0L, 1L, 0L };
        std::vector<std::pair<SliceKey, Value>> kvPairs;
        std::pair<SliceKey, Value> kvPair = getOneKVPair();
        kvPairs.emplace_back(kvPair);
        slice->Initialize(kvPairs, meta, mSliceTable->mMemManager);
        return slice;
    }
    static void GenerateData(uint8_t *&data, uint32_t &length, uint32_t seed)
    {
        std::random_device rd;
        std::mt19937 gen(seed);
        int32_t lenBegin = NO_1;
        int32_t lenEnd = NO_10;
        int32_t byteBegin = NO_1;
        int32_t byteEnd = NO_255;
        std::uniform_int_distribution<> LenDist(lenBegin, lenEnd);
        std::uniform_int_distribution<> byteDist(byteBegin, byteEnd);
        length = LenDist(gen);
        data = new (std::nothrow) uint8_t[length];
        ASSERT_NE(data, nullptr);
        for (uint32_t i = 0; i < length; ++i) {
            data[i] = static_cast<uint8_t>(byteDist(gen));
        }
    }
    static void GenerateMapKeyStateData(uint8_t *&data, uint32_t &length)
    {
        std::random_device rd;
        std::mt19937 gen(rd());
        int32_t byteBegin = NO_1;
        int32_t byteEnd = NO_255;
        std::uniform_int_distribution<> byteDist(byteBegin, byteEnd);
        data = new (std::nothrow) uint8_t[length];
        ASSERT_NE(data, nullptr);
        for (uint32_t i = 0; i < length; ++i) {
            data[i] = static_cast<uint8_t>(byteDist(gen));
        }
        data[NO_4] = 0;
        data[NO_5] = NO_16;
    }
};

#endif  // BOOST_SS_TEST_SLICE_TABLE_BR_COV_H