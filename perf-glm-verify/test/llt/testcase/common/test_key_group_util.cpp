/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#include <algorithm>
#include <array>
#include <cstdint>
#include <set>
#include <tuple>
#include <utility>
#include <vector>

#include "common/util/key_group_util.h"
#include "gtest/gtest.h"

namespace ock {
namespace bss {

class KeyGroupUtilTest : public testing::Test {
    // Empty by design.
};

static KeyGroupUtilRef CreateCodec(uint32_t maxParallelism)
{
    KeyGroupUtilRef codec;
    EXPECT_EQ(KeyGroupUtil::Create(maxParallelism, codec), BSS_OK);
    EXPECT_NE(codec, nullptr);
    return codec;
}

static uint32_t EncodeKV(const KeyGroupUtilRef &codec, uint32_t rawHash, uint32_t maxParallelism)
{
    if (maxParallelism == 0) {
        return rawHash;
    }
    uint32_t keyGroup = rawHash % maxParallelism;
    codec->SetKeyGroup(rawHash, keyGroup);
    return rawHash;
}

TEST_F(KeyGroupUtilTest, KvEncodingRoundTripsRepresentativeParallelism)
{
    const std::vector<uint32_t> maxParallelisms = { 1, 2, 3, 127, 128, 129, 30000, 32767, 32768 };
    const std::vector<uint32_t> hashes = { 0, 1, 2, 127, 128, 129, 0x12345678U, static_cast<uint32_t>(INT32_MAX) };
    for (uint32_t maxParallelism : maxParallelisms) {
        auto codec = CreateCodec(maxParallelism);
        for (uint32_t rawHash : hashes) {
            uint32_t orderHash = EncodeKV(codec, rawHash, maxParallelism);
            ASSERT_LE(orderHash, static_cast<uint32_t>(INT32_MAX));
            ASSERT_EQ(codec->ComputeKeyGroupForKeyHash(orderHash), rawHash % maxParallelism);
        }
    }
}

TEST_F(KeyGroupUtilTest, KvOrderIsLexicographicByGroupThenRawHash)
{
    for (uint32_t maxParallelism : { 3U, 128U, 129U, 32768U }) {
        auto codec = CreateCodec(maxParallelism);
        std::vector<std::tuple<uint32_t, uint32_t, uint32_t>> encoded;
        for (uint32_t rawHash = 0; rawHash < 200000; rawHash += 37) {
            encoded.emplace_back(EncodeKV(codec, rawHash, maxParallelism), rawHash % maxParallelism, rawHash);
        }
        std::sort(encoded.begin(), encoded.end());
        for (size_t i = 1; i < encoded.size(); ++i) {
            auto previous = std::make_pair(std::get<1>(encoded[i - 1]), std::get<2>(encoded[i - 1]));
            auto current = std::make_pair(std::get<1>(encoded[i]), std::get<2>(encoded[i]));
            ASSERT_TRUE(previous <= current);
        }
    }
}

TEST_F(KeyGroupUtilTest, PowerOfTwoLayoutMatchesExpectedBits)
{
    auto codec128 = CreateCodec(128);
    uint32_t raw128 = (0x123456U << 7U) | 0x55U;
    ASSERT_EQ(EncodeKV(codec128, raw128, 128), (0x55U << 24U) | 0x123456U);

    auto codec32768 = CreateCodec(32768);
    uint32_t raw32768 = (0xABCDU << 15U) | 0x2345U;
    ASSERT_EQ(EncodeKV(codec32768, raw32768, 32768), (0x2345U << 16U) | 0xABCDU);
}

TEST_F(KeyGroupUtilTest, IndependentInstancesKeepTheirLayouts)
{
    auto codec128 = CreateCodec(128);
    uint32_t orderHash = EncodeKV(codec128, 120U, 128U);
    auto codec256 = CreateCodec(256);
    EXPECT_EQ(codec256->GetMaxParallelism(), 256U);
    EXPECT_EQ(codec128->ComputeKeyGroupForKeyHash(orderHash), 120U);
}

TEST_F(KeyGroupUtilTest, FixedGroupUsesAllAvailableFreshBuckets)
{
    constexpr uint32_t maxParallelism = 32768;
    constexpr uint32_t keyGroup = 7;
    auto codec = CreateCodec(maxParallelism);
    for (uint32_t bucketCount : { 8U, 64U, 1024U, 32768U, 65536U }) {
        std::set<uint32_t> buckets;
        for (uint32_t quotient = 0; quotient < 65536; ++quotient) {
            uint32_t rawHash = quotient * maxParallelism + keyGroup;
            buckets.insert(EncodeKV(codec, rawHash, maxParallelism) & (bucketCount - 1));
        }
        ASSERT_EQ(buckets.size(), bucketCount);
    }
}

TEST_F(KeyGroupUtilTest, PqEncodingRemainsByteCompatible)
{
    auto codec128 = CreateCodec(128);
    uint32_t oneByteHash = 0x12345678U;
    codec128->SetPQKeyGroup(oneByteHash, 0x5AU);
    ASSERT_EQ(oneByteHash, 0x5A345678U);
    ASSERT_EQ(codec128->ComputePQKeyGroupForKeyHash(oneByteHash), 0x5AU);

    auto codec32768 = CreateCodec(32768);
    uint32_t twoByteHash = 0x12345678U;
    codec32768->SetPQKeyGroup(twoByteHash, 0x2345U);
    ASSERT_EQ(twoByteHash, 0x23455678U);
    ASSERT_EQ(codec32768->ComputePQKeyGroupForKeyHash(twoByteHash), 0x2345U);
    ASSERT_EQ(codec128->ComputePQKeyGroupForKeyHash(oneByteHash), 0x5AU);
}

TEST_F(KeyGroupUtilTest, CreateRejectsInvalidParallelismWithoutChangingValidInstance)
{
    auto codec = CreateCodec(128);
    KeyGroupUtilRef invalidCodec;
    EXPECT_EQ(KeyGroupUtil::Create(0, invalidCodec), BSS_INVALID_PARAM);
    EXPECT_EQ(KeyGroupUtil::Create(32769, invalidCodec), BSS_INVALID_PARAM);

    uint32_t rawHash = 0x12345678U;
    uint32_t keyGroup = rawHash % 128;
    uint32_t orderHash = EncodeKV(codec, rawHash, 128);
    EXPECT_EQ(codec->ComputeKeyGroupForKeyHash(orderHash), keyGroup);
}

TEST_F(KeyGroupUtilTest, GeneralLayoutMatchesGoldenVectors)
{
    struct Vector {
        uint32_t maxParallelism;
        uint32_t rawHash;
        uint32_t encodedHash;
    };
    const std::array<Vector, 4> vectors = { { { 3, 1, 0x2aaaaaab },
                                              { 129, 0x12345678, 0x0c0c5014 },
                                              { 30000, 0x12345678, 0x54e3f50c },
                                              { 32767, 0x7fffffff, 0x00020005 } } };
    for (const auto &vector : vectors) {
        auto codec = CreateCodec(vector.maxParallelism);
        EXPECT_EQ(EncodeKV(codec, vector.rawHash, vector.maxParallelism), vector.encodedHash);
    }
}

}  // namespace bss
}  // namespace ock
