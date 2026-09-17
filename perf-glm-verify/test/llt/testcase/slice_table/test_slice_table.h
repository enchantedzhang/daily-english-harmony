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

#ifndef BOOST_SS_TEST_SLICE_TABLE_H
#define BOOST_SS_TEST_SLICE_TABLE_H

#include <iostream>
#include <memory>
#include <random>

#include "boost_state_db_impl.h"
#include "generator.h"
#include "gtest/gtest.h"
#include "include/boost_state_db.h"
#include "slice_table.h"

using namespace ock::bss;
using KVPair = std::pair<SliceKey, Value>;
class TestSliceTable : public testing::Test {
protected:
    void SetUp() override;
    void TearDown() override;
    static BoostStateDBImpl *mBoostStateDb;
    static ConfigRef mConfig;
    static BoostNativeMetric *mMetric;

public:
    KVPair PrepareKvPair(uint32_t seed)
    {
        // dual key.
        SliceKey dualKey = mGenerator->GenerateDualKey(NO_10, NO_20);

        // prepare value.
        Value value = mGenerator->GenerateValue(NO_1024);

        return KVPair(dualKey, value);
    }

    void AddSlice(uint32_t kvCount, std::vector<KVPair> &kvList, bool fixedSeed = true)
    {
        uint32_t seed = 10;
        if (!fixedSeed) {
            seed = std::rand();
        }

        std::map<SliceIndexContextRef, std::vector<KVPair>> contextMap;
        for (uint32_t i = 0; i < kvCount; ++i) {
            // prepare data for key and value.
            auto kvPair = PrepareKvPair(seed);

            // add to context map.
            auto sliceBucketIndex = mBoostStateDb->GetSliceTable()->GetSliceBucketIndex();
            uint32_t priHashCode = kvPair.first.KeyHashCode();
            auto context = sliceBucketIndex->GetSliceIndexContext(priHashCode, true);
            contextMap[context].push_back(kvPair);

            // add to kv list
            kvList.push_back(kvPair);
        }

        // add to slice.
        for (const auto &contextIt : contextMap) {
            auto context = contextIt.first;

            auto list = contextIt.second;
            mBoostStateDb->GetSliceTable()->AddSlice(context, list, 0);
        }
    }

    void AddSliceTheSameKey(uint32_t kvCount, std::vector<KVPair> &kvList, bool fixedSeed = true)
    {
        uint32_t seed = 10;
        if (!fixedSeed) {
            seed = std::rand();
        }

        // prepare data for key and value.
        auto kvPair = PrepareKvPair(seed);
        // add to context map.
        auto sliceBucketIndex = mBoostStateDb->GetSliceTable()->GetSliceBucketIndex();
        uint32_t priHashCode = kvPair.first.KeyHashCode();
        auto context = sliceBucketIndex->GetSliceIndexContext(priHashCode, true);

        for (uint32_t i = 0; i < kvCount; ++i) {
            // add to kv list
            kvList.push_back(kvPair);
        }

        // add to slice.
        mBoostStateDb->GetSliceTable()->AddSlice(context, kvList, 0);
    }

    void ValidateResult(const KVPair &kv, bool exist = true)
    {
        // prepare dual key.
        auto key = kv.first;
        auto value = kv.second;

        auto priKey = key.PriKey();
        auto secKey = key.SecKey();

        BinaryData tempPriKey(priKey.KeyData(), priKey.KeyLen());
        BinaryData tempSecKey(secKey.KeyData(), secKey.KeyLen());
        QueryKey queryKey(key.StateId(), key.KeyHashCode(), tempPriKey, tempSecKey);

        // get value by key from slice table.
        Value searchValue = {};
        bool fount = mBoostStateDb->GetSliceTable()->Get(queryKey, searchValue);

        // validate result.
        // fount key.
        ASSERT_TRUE(fount == exist);
        // if exist, validate value.
        if (exist) {
            // value is not null.
            ASSERT_TRUE(!searchValue.IsNull());
            // validate value length.
            ASSERT_TRUE(searchValue.ValueLen() == value.ValueLen());
            // validate value data.
            ASSERT_TRUE(memcmp(searchValue.ValueData(), value.ValueData(), value.ValueLen()) == 0);
        }
    }

    KVPair PrepareKvPairForKv(uint32_t seed)
    {
        // state id;
        uint16_t stateId = VALUE << NO_13;

        SliceKey sglKey = mGenerator->GenerateSglKey(NO_10, stateId);
        Value value = mGenerator->GenerateValue(NO_1024);

        return KVPair(sglKey, value);
    }

    void AddSliceForKv(uint32_t kvCount, std::vector<KVPair> &kvList, bool fixedSeed = true)
    {
        uint32_t seed = 10;
        if (!fixedSeed) {
            seed = std::rand();
        }

        std::map<SliceIndexContextRef, std::vector<KVPair>> contextMap;
        for (uint32_t i = 0; i < kvCount; ++i) {
            // prepare data for key and value.
            auto kvPair = PrepareKvPairForKv(seed);

            // add to context map.
            auto sliceBucketIndex = mBoostStateDb->GetSliceTable()->GetSliceBucketIndex();
            uint32_t priHashCode = kvPair.first.KeyHashCode();
            auto context = sliceBucketIndex->GetSliceIndexContext(priHashCode, true);
            contextMap[context].push_back(kvPair);

            // add to kv list
            kvList.push_back(kvPair);
        }

        // add to slice.
        for (const auto &contextIt : contextMap) {
            auto context = contextIt.first;
            auto list = contextIt.second;
            mBoostStateDb->GetSliceTable()->AddSlice(context, list, 0);
        }
    }

    void ValidateResultForKv(const KVPair &kv, bool exist = true)
    {
        // prepare dual key.
        auto key = kv.first;
        auto value = kv.second;

        auto priKey = key.PriKey();

        BinaryData tempPriKey(priKey.KeyData(), priKey.KeyLen());
        QueryKey queryKey(key.StateId(), priKey.KeyHashCode(), tempPriKey);

        // get value by key from slice table.
        Value searchValue = {};
        bool fount = mBoostStateDb->GetSliceTable()->Get(queryKey, searchValue);

        // validate result.
        // fount key.
        ASSERT_TRUE(fount == exist);
        // if exist, validate value.
        if (exist) {
            // value is not null.
            ASSERT_TRUE(!searchValue.IsNull());
            // validate value length.
            ASSERT_TRUE(searchValue.ValueLen() == value.ValueLen());
            // validate value data.
            ASSERT_TRUE(memcmp(searchValue.ValueData(), value.ValueData(), value.ValueLen()) == 0);
        }
    }

    void AddSliceForDeleteKv(std::vector<KVPair> &kvList)
    {
        std::map<SliceIndexContextRef, std::vector<KVPair>> contextMap;
        for (auto iter : kvList) {
            Value value;
            value.Init(DELETE, 0, nullptr, value.SeqId(), nullptr);
            auto kvPair = KVPair(iter.first, value);

            // add to context map.
            auto sliceBucketIndex = mBoostStateDb->GetSliceTable()->GetSliceBucketIndex();
            uint32_t priHashCode = kvPair.first.KeyHashCode();
            auto context = sliceBucketIndex->GetSliceIndexContext(priHashCode, true);
            contextMap[context].push_back(kvPair);
        }

        // add to slice.
        for (const auto &contextIt : contextMap) {
            auto context = contextIt.first;
            auto list = contextIt.second;
            mBoostStateDb->GetSliceTable()->AddSlice(context, list, 0);
        }
    }

public:
    GeneratorRef mGenerator;
};

#endif  // BOOST_SS_TEST_SLICE_TABLE_H
