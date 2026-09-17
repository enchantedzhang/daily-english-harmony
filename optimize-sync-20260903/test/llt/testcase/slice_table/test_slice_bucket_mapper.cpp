/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of the Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#include <algorithm>
#include <array>
#include <climits>
#include <cstdint>
#include <memory>
#include <string>

#include "common/util/key_group_util.h"
#include "gtest/gtest.h"
#define private public
#include "slice_table/index/slice_bucket_mapper.h"
#undef private

namespace ock {
namespace bss {

TEST(SliceBucketMapperTest, RejectsInvalidParameters)
{
    SliceBucketMapper mapper;
    EXPECT_EQ(mapper.Initialize(0, 0, 0, 1024), BSS_INVALID_PARAM);
    EXPECT_EQ(mapper.Initialize(32769, 0, 0, 1024), BSS_INVALID_PARAM);
    EXPECT_EQ(mapper.Initialize(128, 2, 1, 1024), BSS_INVALID_PARAM);
    EXPECT_EQ(mapper.Initialize(128, 0, 128, 1024), BSS_INVALID_PARAM);
    EXPECT_EQ(mapper.Initialize(128, 0, 1, 0), BSS_INVALID_PARAM);
}

TEST(SliceBucketMapperTest, RejectsSecondInitialization)
{
    SliceBucketMapper mapper;
    ASSERT_EQ(mapper.Initialize(128, 0, 1, 1024), BSS_OK);
    EXPECT_EQ(mapper.Initialize(128, 0, 1, 1024), BSS_ALREADY_DONE);
}

TEST(SliceBucketMapperTest, MapsTaskEndpointsToFirstAndLastBucket)
{
    SliceBucketMapper mapper;
    ASSERT_EQ(mapper.Initialize(1024, 8, 15, 1024), BSS_OK);
    auto taskRange = mapper.GetTaskRange();
    ASSERT_NE(taskRange, nullptr);

    uint32_t bucket = 0;
    ASSERT_EQ(mapper.Map(taskRange->GetStartHashCode(), bucket), BSS_OK);
    EXPECT_EQ(bucket, 0);
    ASSERT_EQ(mapper.Map(taskRange->GetEndHashCode(), bucket), BSS_OK);
    EXPECT_EQ(bucket, 1023);
}

TEST(SliceBucketMapperTest, PowerOfTwoBucketSpanSelectsShiftFastPath)
{
    SliceBucketMapper mapper;
    ASSERT_EQ(mapper.Initialize(1024, 8, 15, 1024), BSS_OK);
    EXPECT_TRUE(mapper.mUseShiftFastPath);
    EXPECT_EQ(mapper.mBucketShift, 14U);
}

TEST(SliceBucketMapperTest, UnevenBucketSpanKeepsDivisionFallback)
{
    SliceBucketMapper mapper;
    ASSERT_EQ(mapper.Initialize(3, 0, 0, 1024), BSS_OK);
    EXPECT_FALSE(mapper.mUseShiftFastPath);
}

TEST(SliceBucketMapperTest, UncheckedMappingMapsRepresentativeHashes)
{
    struct MappingConfig {
        uint32_t maxParallelism;
        uint32_t startKeyGroup;
        uint32_t endKeyGroup;
        uint32_t bucketNum;
    };
    const std::array<MappingConfig, 2> configs = { {
        { 1024, 8, 15, 1024 },
        { 3, 0, 0, 1024 },
    } };

    for (const auto &config : configs) {
        SliceBucketMapper mapper;
        ASSERT_EQ(mapper.Initialize(config.maxParallelism, config.startKeyGroup, config.endKeyGroup, config.bucketNum),
                  BSS_OK);
        auto taskRange = mapper.GetTaskRange();
        ASSERT_NE(taskRange, nullptr);
        const std::array<uint32_t, 3> hashes = {
            taskRange->GetStartHashCode(),
            taskRange->GetStartHashCode() + (taskRange->GetEndHashCode() - taskRange->GetStartHashCode()) / 2,
            taskRange->GetEndHashCode(),
        };
        const std::array<uint32_t, 3> expectedBuckets = { 0, 511, 1023 };
        for (uint32_t i = 0; i < hashes.size(); ++i) {
            EXPECT_EQ(mapper.MapUnchecked(hashes[i]), expectedBuckets[i]);
        }
    }
}

TEST(SliceBucketMapperTest, ForwardAndReverseMappingHaveNoGapOrOverlap)
{
    const std::array<uint32_t, 7> parallelisms = { 3, 127, 129, 1024, 30000, 32767, 32768 };
    for (uint32_t parallelism : parallelisms) {
        const uint32_t start = parallelism == 3 ? 0 : parallelism / 3;
        const uint32_t end = std::min(parallelism - 1, start + 7);
        SliceBucketMapper mapper;
        ASSERT_EQ(mapper.Initialize(parallelism, start, end, 1024), BSS_OK);

        uint32_t previousEnd = 0;
        for (uint32_t bucket = 0; bucket < 1024; ++bucket) {
            auto range = mapper.GetBucketRange(bucket);
            ASSERT_NE(range, nullptr);
            if (bucket > 0) {
                EXPECT_EQ(range->GetStartHashCode(), previousEnd + 1);
            }
            uint32_t mapped = 0;
            ASSERT_EQ(mapper.Map(range->GetStartHashCode(), mapped), BSS_OK);
            EXPECT_EQ(mapped, bucket);
            ASSERT_EQ(mapper.Map(range->GetEndHashCode(), mapped), BSS_OK);
            EXPECT_EQ(mapped, bucket);
            previousEnd = range->GetEndHashCode();
        }
        EXPECT_EQ(previousEnd, mapper.GetTaskRange()->GetEndHashCode());
    }
}

TEST(SliceBucketMapperTest, RejectsHashOutsideTaskRange)
{
    SliceBucketMapper mapper;
    ASSERT_EQ(mapper.Initialize(1024, 8, 15, 1024), BSS_OK);
    auto taskRange = mapper.GetTaskRange();
    ASSERT_NE(taskRange, nullptr);
    uint32_t bucket = 0;
    EXPECT_EQ(mapper.Map(taskRange->GetStartHashCode() - 1, bucket), BSS_INVALID_PARAM);
    EXPECT_EQ(mapper.Map(taskRange->GetEndHashCode() + 1, bucket), BSS_INVALID_PARAM);
    EXPECT_EQ(mapper.GetBucketRange(1024), nullptr);
}

TEST(SliceBucketMapperTest, MaxParallelismSupportsSingleAndFullKeyGroupRange)
{
    SliceBucketMapper singleGroup;
    ASSERT_EQ(singleGroup.Initialize(32768, 32767, 32767, 1024), BSS_OK);
    EXPECT_EQ(singleGroup.GetTaskRange()->GetEndHashCode(), INT32_MAX);

    SliceBucketMapper fullRange;
    ASSERT_EQ(fullRange.Initialize(32768, 0, 32767, 1024), BSS_OK);
    EXPECT_EQ(fullRange.GetTaskRange()->GetStartHashCode(), 0);
    EXPECT_EQ(fullRange.GetTaskRange()->GetEndHashCode(), INT32_MAX);
}

TEST(SliceBucketMapperTest, TaskRangeMatchesKeyGroupUtilEncoding)
{
    constexpr uint64_t hashSpace = 1ULL << 31U;
    const std::array<uint32_t, 7> parallelisms = { 3, 127, 129, 1024, 30000, 32767, 32768 };
    for (uint32_t parallelism : parallelisms) {
        KeyGroupUtilRef codec;
        ASSERT_EQ(KeyGroupUtil::Create(parallelism, codec), BSS_OK);
        const std::array<uint32_t, 3> groups = { 0, parallelism / 2, parallelism - 1 };
        for (uint32_t group : groups) {
            SliceBucketMapper mapper;
            ASSERT_EQ(mapper.Initialize(parallelism, group, group, 1024), BSS_OK);
            auto taskRange = mapper.GetTaskRange();
            ASSERT_NE(taskRange, nullptr);

            uint32_t firstRawHash = group;
            uint32_t lastRawHash = static_cast<uint32_t>(((hashSpace - 1 - group) / parallelism) * parallelism + group);
            codec->SetKeyGroup(firstRawHash, group);
            codec->SetKeyGroup(lastRawHash, group);

            EXPECT_EQ(firstRawHash, taskRange->GetStartHashCode());
            EXPECT_EQ(lastRawHash, taskRange->GetEndHashCode());
            EXPECT_EQ(codec->ComputeKeyGroupForKeyHash(firstRawHash), group);
            EXPECT_EQ(codec->ComputeKeyGroupForKeyHash(lastRawHash), group);
        }
    }
}

}  // namespace bss
}  // namespace ock
