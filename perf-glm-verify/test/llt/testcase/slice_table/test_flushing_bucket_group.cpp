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
#include "test_flushing_bucket_group.h"
#define private public
#include "memory/full_sort_evictor.h"
#include "slice_table/flushing_bucket_group.h"
#undef private

using namespace ock::bss;

class TestFlushingBucketGroup : public ::testing::Test {
public:
    void SetUp() override
    {
    }
    void TearDown() override
    {
    }
};

/**
 * @tc.name  : Initialize_ShouldReturnBSSERR_WhenEvictorIsNullptr
 * @tc.number: Initialize_Test_001
 * @tc.desc  : Test Initialize function when evictor is nullptr
 */
TEST_F(TestFlushingBucketGroup, Initialize_ShouldReturnBSSERR_WhenEvictorIsNullptr)
{
    FlushingBucketGroup fbg;
    std::vector<SliceScore> list;
    FullSortEvictorRef evictor = nullptr;
    FlushQueueForBucketGroupRef flushQueueForBucketGroup =
        std::make_shared<FullSortEvictor::FlushQueueForBucketGroup>();

    BResult result = fbg.Initialize(list, 1, evictor, flushQueueForBucketGroup);
    EXPECT_EQ(result, BSS_ERR);
}

/**
 * @tc.name  : Initialize_ShouldReturnBSSERR_WhenFlushQueueForBucketGroupIsNullptr
 * @tc.number: Initialize_Test_002
 * @tc.desc  : Test Initialize function when flushQueueForBucketGroup is nullptr
 */
TEST_F(TestFlushingBucketGroup, Initialize_ShouldReturnBSSERR_WhenFlushQueueForBucketGroupIsNullptr)
{
    FlushingBucketGroup fbg;
    std::vector<SliceScore> list;
    FullSortEvictorRef evictor = std::make_shared<FullSortEvictor>();
    FlushQueueForBucketGroupRef flushQueueForBucketGroup = nullptr;

    BResult result = fbg.Initialize(list, 1, evictor, flushQueueForBucketGroup);
    EXPECT_EQ(result, BSS_ERR);
}

/**
 * @tc.name  : Initialize_ShouldReturnBSSOK_WhenListIsEmpty
 * @tc.number: Initialize_Test_003
 * @tc.desc  : Test Initialize function when list is empty
 */
TEST_F(TestFlushingBucketGroup, Initialize_ShouldReturnBSSOK_WhenListIsEmpty)
{
    FlushingBucketGroup fbg;
    std::vector<SliceScore> list;
    FullSortEvictorRef evictor = std::make_shared<FullSortEvictor>();
    FlushQueueForBucketGroupRef flushQueueForBucketGroup =
        std::make_shared<FullSortEvictor::FlushQueueForBucketGroup>();

    BResult result = fbg.Initialize(list, 1, evictor, flushQueueForBucketGroup);
    EXPECT_EQ(result, BSS_OK);
    EXPECT_TRUE(fbg.mDataSlices.empty());
}
