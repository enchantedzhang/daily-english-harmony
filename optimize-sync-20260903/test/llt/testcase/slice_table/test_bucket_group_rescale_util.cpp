/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 */

#include "gtest/gtest.h"
#define private public
#include "slice_table/bucket_group_rescale_util.h"
#undef private

using namespace ock::bss;

namespace {
struct RescaleFixture {
    uint32_t bucketNum;
    SliceBucketMapperRef oldMapper;
    SliceBucketMapperRef targetMapper;
    SliceBucketIndexRef oldIndex;
    SliceBucketIndexRef newIndex;
    SliceBucketGroupRangeGroupRef oldGroup;
    SliceBucketGroupRangeGroupRef newGroup;
    BucketGroupManagerRef manager = std::make_shared<BucketGroupManager>();

    RescaleFixture(uint32_t maxParallelism, uint32_t oldStart, uint32_t oldEnd, uint32_t targetStart,
                   uint32_t targetEnd, uint32_t buckets)
        : bucketNum(buckets)
    {
        oldMapper = std::make_shared<SliceBucketMapper>();
        EXPECT_EQ(oldMapper->Initialize(maxParallelism, oldStart, oldEnd, buckets), BSS_OK);
        targetMapper = std::make_shared<SliceBucketMapper>();
        EXPECT_EQ(targetMapper->Initialize(maxParallelism, targetStart, targetEnd, buckets), BSS_OK);

        auto oldConfig = std::make_shared<Config>(oldStart, oldEnd, maxParallelism);
        auto targetConfig = std::make_shared<Config>(targetStart, targetEnd, maxParallelism);
        oldIndex = std::make_shared<SliceBucketIndex>();
        EXPECT_EQ(oldIndex->Initialize(buckets, oldConfig), BSS_OK);
        newIndex = std::make_shared<SliceBucketIndex>();
        EXPECT_EQ(newIndex->Initialize(buckets, targetConfig), BSS_OK);

        std::vector<BucketGroupRangeRef> oldRanges{ std::make_shared<BucketGroupRange>(0, buckets - 1, buckets, 0) };
        std::vector<BucketGroupRangeRef> newRanges{ std::make_shared<BucketGroupRange>(0, buckets - 1, buckets, 0) };
        oldGroup = std::make_shared<SliceBucketGroupRangeGroup>(buckets, oldRanges, oldIndex);
        newGroup = std::make_shared<SliceBucketGroupRangeGroup>(buckets, newRanges, newIndex);
    }

    LogicalSliceChainRef PutChain(uint32_t oldBucket)
    {
        auto chain = oldIndex->CreateLogicalChainedSlice();
        EXPECT_NE(chain, nullptr);
        oldIndex->SetLogicChainedSlice(oldBucket, chain);
        return chain;
    }

    BResult Rescale()
    {
        std::unordered_map<uint32_t, std::vector<uint32_t>> relation;
        return BucketGroupRescaleUtil::Rescale(oldGroup, newGroup, manager, oldMapper, targetMapper, relation);
    }
};

CompositeLogicalSliceChainRef GetComposite(const SliceBucketIndexRef &index, uint32_t bucket)
{
    return std::dynamic_pointer_cast<CompositeLogicalSliceChain>(index->GetLogicChainedSlice(bucket));
}
}  // namespace

TEST(BucketGroupRescaleUtilTest, SameRangeMapsOneToOneWithoutDeepCopy)
{
    RescaleFixture fixture(16, 0, 15, 0, 15, 16);
    auto oldChain = fixture.PutChain(3);
    ASSERT_EQ(fixture.Rescale(), BSS_OK);
    auto composite = GetComposite(fixture.newIndex, 3);
    ASSERT_NE(composite, nullptr);
    ASSERT_EQ(composite->GetSliceChains().size(), 1);
    EXPECT_EQ(composite->GetSliceChains()[0], oldChain);
    EXPECT_FALSE(composite->RequireForceCompaction());
}

TEST(BucketGroupRescaleUtilTest, SplitOldBucketMapsToEveryOverlappingTargetBucket)
{
    RescaleFixture fixture(16, 0, 15, 8, 15, 16);
    auto oldChain = fixture.PutChain(8);
    ASSERT_EQ(fixture.Rescale(), BSS_OK);
    for (uint32_t bucket = 0; bucket < 2; ++bucket) {
        auto composite = GetComposite(fixture.newIndex, bucket);
        ASSERT_NE(composite, nullptr);
        EXPECT_TRUE(composite->RequireForceCompaction());
        EXPECT_FALSE(composite->HasFilePage());
        ASSERT_EQ(composite->GetSliceChains().size(), 1);
        EXPECT_NE(composite->GetSliceChains()[0], oldChain);
    }
    EXPECT_TRUE(fixture.newIndex->GetLogicChainedSlice(2)->IsNone());
}

TEST(BucketGroupRescaleUtilTest, ClippedSingleTargetBucketStillRequiresForceCompaction)
{
    RescaleFixture fixture(3, 0, 2, 1, 2, 1024);
    auto oldRange = fixture.oldMapper->GetBucketRange(341);
    ASSERT_NE(oldRange, nullptr);
    EXPECT_EQ(oldRange->GetStartHashCode(), 715128832U);
    EXPECT_EQ(oldRange->GetEndHashCode(), 717225983U);
    EXPECT_EQ(fixture.targetMapper->GetTaskRange()->GetStartHashCode(), 715827883U);
    auto oldChain = fixture.PutChain(341);
    auto oldStore = std::make_shared<LsmStore>(std::make_shared<FileStoreID>(), nullptr, nullptr, nullptr, nullptr,
                                               nullptr);
    auto targetStore = std::make_shared<LsmStore>(std::make_shared<FileStoreID>(), nullptr, nullptr, nullptr, nullptr,
                                                  nullptr);
    auto bucketGroup = std::make_shared<BucketGroup>();
    ASSERT_EQ(bucketGroup->Initialize(0, targetStore, fixture.newIndex, 0, 1023), BSS_OK);
    fixture.manager->mBucketNum = 1024;
    fixture.manager->mBucketGroups.push_back(bucketGroup);
    oldChain->InsertFilePage(std::make_shared<FilePage>(oldStore));

    ASSERT_EQ(fixture.Rescale(), BSS_OK);
    auto composite = GetComposite(fixture.newIndex, 0);
    ASSERT_NE(composite, nullptr);
    ASSERT_EQ(composite->GetSliceChains().size(), 1);
    EXPECT_EQ(composite->GetSliceChains()[0], oldChain);
    EXPECT_TRUE(composite->RequireForceCompaction());
    EXPECT_TRUE(composite->HasFilePage());
    EXPECT_EQ(composite->GetFilePageSize(), 1);
}

TEST(BucketGroupRescaleUtilTest, ExtremeScaleOutMapsToEveryTargetBucket)
{
    RescaleFixture fixture(1024, 0, 1023, 8, 8, 1024);
    fixture.PutChain(8);
    ASSERT_EQ(fixture.Rescale(), BSS_OK);
    for (uint32_t bucket = 0; bucket < fixture.bucketNum; ++bucket) {
        auto composite = GetComposite(fixture.newIndex, bucket);
        ASSERT_NE(composite, nullptr);
        EXPECT_TRUE(composite->RequireForceCompaction());
    }
}

TEST(BucketGroupRescaleUtilTest, PartialTaskOverlapSkipsOutOfRangeBuckets)
{
    RescaleFixture fixture(16, 0, 15, 8, 15, 16);
    fixture.PutChain(0);
    ASSERT_EQ(fixture.Rescale(), BSS_OK);
    for (uint32_t bucket = 0; bucket < fixture.bucketNum; ++bucket) {
        EXPECT_TRUE(fixture.newIndex->GetLogicChainedSlice(bucket)->IsNone());
    }
}

TEST(BucketGroupRescaleUtilTest, RejectsDifferentBucketCounts)
{
    RescaleFixture fixture(16, 0, 15, 0, 15, 16);
    fixture.newGroup->mTotalBucket = 8;
    EXPECT_EQ(fixture.Rescale(), BSS_NOT_SUPPORTED);
}

TEST(BucketGroupRescaleUtilTest, RejectsNonZeroOrMultipleBucketGroups)
{
    RescaleFixture fixture(16, 0, 15, 0, 15, 16);
    fixture.oldGroup->mSliceSegments[0] = std::make_shared<BucketGroupRange>(0, 15, 16, 1);
    EXPECT_EQ(fixture.Rescale(), BSS_NOT_SUPPORTED);

    fixture.oldGroup->mSliceSegments.push_back(std::make_shared<BucketGroupRange>(0, 15, 16, 0));
    EXPECT_EQ(fixture.Rescale(), BSS_NOT_SUPPORTED);
}

TEST(BucketGroupRescaleUtilTest, FilePageOnlyPropagatesToOverlappingTargets)
{
    RescaleFixture fixture(16, 0, 15, 8, 15, 16);
    auto oldStore = std::make_shared<LsmStore>(std::make_shared<FileStoreID>(), nullptr, nullptr, nullptr, nullptr,
                                               nullptr);
    auto targetStore = std::make_shared<LsmStore>(std::make_shared<FileStoreID>(), nullptr, nullptr, nullptr, nullptr,
                                                  nullptr);
    auto bucketGroup = std::make_shared<BucketGroup>();
    ASSERT_EQ(bucketGroup->Initialize(0, targetStore, fixture.newIndex, 0, 15), BSS_OK);
    fixture.manager->mBucketNum = 16;
    fixture.manager->mBucketGroups.push_back(bucketGroup);

    auto oldChain = fixture.PutChain(8);
    oldChain->InsertFilePage(std::make_shared<FilePage>(oldStore));
    ASSERT_EQ(fixture.Rescale(), BSS_OK);
    for (uint32_t bucket = 0; bucket < 2; ++bucket) {
        auto composite = GetComposite(fixture.newIndex, bucket);
        ASSERT_NE(composite, nullptr);
        std::vector<FilePageRef> pages;
        composite->GetFilePages(pages);
        ASSERT_EQ(pages.size(), 1);
        EXPECT_EQ(pages[0]->mLsmStore, targetStore);
    }
    EXPECT_TRUE(fixture.newIndex->GetLogicChainedSlice(2)->IsNone());
}

TEST(BucketGroupRescaleUtilTest, MetadataOnlySliceAddressIsPreservedAcrossDeepCopy)
{
    RescaleFixture fixture(16, 0, 15, 8, 15, 16);
    auto oldChain = fixture.PutChain(8);
    auto metadataOnly = std::make_shared<SliceAddress>(123, 456, 789, 1011, 1213);
    ASSERT_NE(oldChain->InsertSlice(metadataOnly), INVALID_U32);

    ASSERT_EQ(fixture.Rescale(), BSS_OK);
    for (uint32_t bucket = 0; bucket < 2; ++bucket) {
        auto composite = GetComposite(fixture.newIndex, bucket);
        ASSERT_NE(composite, nullptr);
        auto copied = composite->GetSliceChains()[0]->GetSliceAddress(0);
        ASSERT_NE(copied, nullptr);
        EXPECT_EQ(copied->GetDataSlice(), nullptr);
        EXPECT_EQ(copied->GetDataLen(), 123);
        EXPECT_EQ(copied->GetStoredDataLen(), 123);
        EXPECT_EQ(copied->GetCheckSum(), 456);
        EXPECT_EQ(copied->GetStartOffset(), 1011);
        EXPECT_EQ(copied->GetSliceId(), 1213);
    }
}

TEST(BucketGroupRescaleUtilTest, NormalDeepCopyRejectsMetadataOnlySliceAddress)
{
    auto chain = std::make_shared<LogicalSliceChainImpl>();
    auto metadataOnly = std::make_shared<SliceAddress>(123, 456, 789, 1011, 1213);
    ASSERT_NE(chain->InsertSlice(metadataOnly), INVALID_U32);

    EXPECT_EQ(chain->DeepCopy(), nullptr);
}

TEST(BucketGroupRescaleUtilTest, CompositeInputBytesDoNotWrapAboveUint32)
{
    RescaleFixture fixture(16, 0, 7, 0, 15, 16);
    constexpr uint32_t childBytes = 0x90000000U;
    constexpr uint64_t expectedBytes = static_cast<uint64_t>(childBytes) * 2;
    for (uint32_t bucket = 0; bucket < 2; ++bucket) {
        auto child = fixture.PutChain(bucket);
        auto metadataOnly = std::make_shared<SliceAddress>(childBytes, bucket, bucket, bucket, bucket);
        ASSERT_NE(child->InsertSlice(metadataOnly), INVALID_U32);
    }

    ASSERT_EQ(fixture.Rescale(), BSS_OK);
    auto composite = GetComposite(fixture.newIndex, 0);
    ASSERT_NE(composite, nullptr);
    ASSERT_EQ(composite->GetSliceChains().size(), 2);
    EXPECT_EQ(composite->GetCompositeSliceSize(), expectedBytes);
}
