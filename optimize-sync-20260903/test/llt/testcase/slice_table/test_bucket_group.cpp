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

#include "test_bucket_group.h"
#define private public
#include "lsm_store/file/file_cache_factory.h"
#include "lsm_store/file/file_store_impl.h"
#include "slice_table/bucket_group.h"
#include "slice_table/bucket_group_manager.h"
#include "slice_table/bucket_group_range.h"
#undef private

using namespace ock::bss;

const std::string testFilePath = "test_output.txt";
auto fileInput = std::make_shared<FileInputView>();
auto fileOutput = std::make_shared<FileOutputView>();

class TestBucketGroup : public ::testing::Test {
public:
    void SetUp() override
    {
        // 初始化所有依赖对象
        fileStoreId = std::make_shared<FileStoreID>();
        config = std::make_shared<Config>();
        memManager = std::make_shared<MemManager>(AllocatorType::DIRECT);
        stateIdProvider = std::make_shared<StateIdProvider>(1, 1, memManager);
        tableFactory = std::make_shared<FileFactory>(config, memManager);
        fileCache = std::make_shared<FileCacheManager>();
        KeyGroupUtilRef codec;
        ASSERT_EQ(KeyGroupUtil::Create(NO_128, codec), BSS_OK);
        stateFilterManager = std::make_shared<StateFilterManager>(stateIdProvider, config, 1, 1, codec);
        sliceIndex = std::make_shared<SliceBucketIndex>();
        Uri uri(testFilePath);
        PathRef path = std::make_shared<Path>(uri);
        ASSERT_EQ(fileOutput->Init(path, config, FileOutputView::WriteMode::OVERWRITE), BSS_OK);
        ASSERT_EQ(fileInput->Init(FileSystemType::LOCAL, path), BSS_OK);
    }
    void TearDown() override
    {
        fileOutput->Close();
        fileInput->Close();
    }
    std::shared_ptr<FileStoreID> fileStoreId;
    std::shared_ptr<Config> config;
    std::shared_ptr<StateIdProvider> stateIdProvider;
    std::shared_ptr<FileFactory> tableFactory;
    std::shared_ptr<FileCacheManager> fileCache;
    std::shared_ptr<StateFilterManager> stateFilterManager;
    std::shared_ptr<MemManager> memManager;
    SliceBucketIndexRef sliceIndex;
};

/**
 * @tc.name  : Initialize_ShouldReturnError_WhenSliceIndexIsNull
 * @tc.number: 001
 * @tc.desc  : Test scenario where sliceIndex is null.
 */
TEST_F(TestBucketGroup, Initialize_ShouldReturnError_WhenSliceIndexIsNull)
{
    uint32_t bucketGroupId = 1;
    LsmStoreRef lsmStore;
    SliceBucketIndexRef sliceIndex = nullptr;
    uint32_t startBucket = 1;
    uint32_t endBucket = 10;
    BucketGroup bucketGroup;
    BResult result = bucketGroup.Initialize(bucketGroupId, lsmStore, sliceIndex, startBucket, endBucket);
    EXPECT_EQ(result, BSS_ERR);
}

/**
 * @tc.name  : Initialize_ShouldReturnError_WhenFileStoreIsNull
 * @tc.number: 002
 * @tc.desc  : Test scenario where fileStore is null.
 */
TEST_F(TestBucketGroup, Initialize_ShouldReturnError_WhenFileStoreIsNull)

{
    uint32_t bucketGroupId = 1;
    LsmStoreRef lsmStore = nullptr;
    SliceBucketIndexRef sliceIndex = std::make_shared<SliceBucketIndex>();
    uint32_t startBucket = 1;
    uint32_t endBucket = 10;
    BucketGroup bucketGroup;
    BResult result = bucketGroup.Initialize(bucketGroupId, lsmStore, sliceIndex, startBucket, endBucket);
    EXPECT_EQ(result, BSS_ERR);
}

/**
 * @tc.name  : Initialize_ShouldReturnSuccess_WhenAllParamsAreValid
 * @tc.number: 003
 * @tc.desc  : Test scenario where all parameters are valid.
 */
TEST_F(TestBucketGroup, Initialize_ShouldReturnSuccess_WhenAllParamsAreValid)
{
    uint32_t bucketGroupId = 1;
    LsmStoreRef lsmStore = std::make_shared<LsmStore>(fileStoreId, config, tableFactory, fileCache, stateFilterManager,
                                                      memManager);
    SliceBucketIndexRef sliceIndex = std::make_shared<SliceBucketIndex>();
    uint32_t startBucket = 1;
    uint32_t endBucket = 10;
    BucketGroup bucketGroup;
    BResult result = bucketGroup.Initialize(bucketGroupId, lsmStore, sliceIndex, startBucket, endBucket);
    EXPECT_EQ(result, BSS_OK);
}

/**
 * @tc.name  : Restore_ShouldReturnError_WhenReadStartBucketFails
 * @tc.number: BucketGroup_Restore_001
 * @tc.desc  : Test that Restore returns an error when reading startBucket fails
 */
TEST_F(TestBucketGroup, Restore_ShouldReturnError_WhenReadStartBucketFails)
{
    uint32_t totalBucketNum = 10;
    BucketGroupRangeRef bucketGroupRange;
    fileInput->Seek(0);
    BResult result = BucketGroup::Restore(fileInput, totalBucketNum, bucketGroupRange);
    EXPECT_NE(result, BSS_OK);
}

/**
 * @tc.name  : Restore_ShouldReturnError_WhenReadEndBucketFails
 * @tc.number: BucketGroup_Restore_002
 * @tc.desc  : Test that Restore returns an error when reading endBucket fails
 */
TEST_F(TestBucketGroup, Restore_ShouldReturnError_WhenReadEndBucketFails)
{
    uint32_t startBucket = 1;
    uint32_t totalBucketNum = 10;
    ASSERT_EQ(fileOutput->WriteUint32(startBucket), BSS_OK);
    BucketGroupRangeRef bucketGroupRange;
    fileInput->Seek(0);
    BResult result = BucketGroup::Restore(fileInput, totalBucketNum, bucketGroupRange);
    EXPECT_NE(result, BSS_OK);
}

/**
 * @tc.name  : Restore_ShouldReturnError_WhenReadBucketGroupIdFails
 * @tc.number: BucketGroup_Restore_003
 * @tc.desc  : Test that Restore returns an error when reading bucketGroupId fails
 */
TEST_F(TestBucketGroup, Restore_ShouldReturnError_WhenReadBucketGroupIdFails)
{
    uint32_t startBucket = 1;
    uint32_t endBucket = 2;
    uint32_t totalBucketNum = 10;

    ASSERT_EQ(fileOutput->WriteUint32(startBucket), BSS_OK);
    ASSERT_EQ(fileOutput->WriteUint32(endBucket), BSS_OK);
    BucketGroupRangeRef bucketGroupRange;
    fileInput->Seek(0);
    BResult result = BucketGroup::Restore(fileInput, totalBucketNum, bucketGroupRange);
    EXPECT_NE(result, BSS_OK);
}

/**
 * @tc.name  : Restore_ShouldReturnSuccess_WhenAllReadsSucceed
 * @tc.number: BucketGroup_Restore_004
 * @tc.desc  : Test that Restore returns success when all reads succeed
 */
TEST_F(TestBucketGroup, Restore_ShouldReturnSuccess_WhenAllReadsSucceed)
{
    uint32_t startBucket = 1;
    uint32_t endBucket = 2;
    uint32_t bucketGroupId = 3;
    uint32_t totalBucketNum = 10;
    ASSERT_EQ(fileOutput->WriteUint32(startBucket), BSS_OK);
    ASSERT_EQ(fileOutput->WriteUint32(endBucket), BSS_OK);
    ASSERT_EQ(fileOutput->WriteUint32(bucketGroupId), BSS_OK);
    BucketGroupRangeRef bucketGroupRange;
    fileInput->Seek(0);
    BResult result = BucketGroup::Restore(fileInput, totalBucketNum, bucketGroupRange);
    EXPECT_EQ(result, BSS_OK);
    EXPECT_NE(bucketGroupRange, nullptr);
    EXPECT_EQ(bucketGroupRange->GetStartBucket(), startBucket);
    EXPECT_EQ(bucketGroupRange->GetEndBucket(), endBucket);
    EXPECT_EQ(bucketGroupRange->GetTotalBucket(), totalBucketNum);
    EXPECT_EQ(bucketGroupRange->GetBucketGroupId(), bucketGroupId);
}

/**
 * @tc.name  : Initialize_ShouldReturnBSS_ERR_WhenConfigIsNullptr
 * @tc.number: Initialize_Test_001
 * @tc.desc  : Test Initialize function when config is nullptr
 */
TEST_F(TestBucketGroup, Initialize_ShouldReturnBSS_ERR_WhenConfigIsNullptr)
{
    BucketGroupManager manager;
    BResult result = manager.Initialize(nullptr, sliceIndex, fileCache, 1, 1, memManager, nullptr);
    EXPECT_EQ(result, BSS_ERR);
}

/**
 * @tc.name  : Initialize_ShouldReturnBSS_ERR_WhenSliceIndexIsNullptr
 * @tc.number: Initialize_Test_002
 * @tc.desc  : Test Initialize function when sliceIndex is nullptr
 */
TEST_F(TestBucketGroup, Initialize_ShouldReturnBSS_ERR_WhenSliceIndexIsNullptr)
{
    BucketGroupManager manager;
    BResult result = manager.Initialize(config, nullptr, fileCache, 1, 1, memManager, nullptr);
    EXPECT_EQ(result, BSS_ERR);
}

TEST_F(TestBucketGroup, SingleGroupFileStoreUsesTaskHashRange)
{
    config = std::make_shared<Config>(8, 15, 1024);
    config->SetLocalPath(".");
    ASSERT_EQ(memManager->Initialize(config), BSS_OK);
    stateIdProvider = std::make_shared<StateIdProvider>(8, 15, memManager);
    KeyGroupUtilRef codec;
    ASSERT_EQ(KeyGroupUtil::Create(config->GetMaxParallelism(), codec), BSS_OK);
    stateFilterManager = std::make_shared<StateFilterManager>(stateIdProvider, config, 8, 15, codec);
    BoostNativeMetricPtr *metric = nullptr;
    auto fileCacheFactory = std::make_shared<FileCacheFactory>(config, nullptr, metric);
    fileCache = fileCacheFactory->GetFileCache();
    sliceIndex = std::make_shared<SliceBucketIndex>();
    ASSERT_EQ(sliceIndex->Initialize(1024, config), BSS_OK);

    BucketGroupManager manager;
    ASSERT_EQ(manager.Initialize(config, sliceIndex, fileCache, 1, 1024, memManager, stateFilterManager), BSS_OK);
    auto bucketGroups = manager.GetBucketGroupVector();
    ASSERT_EQ(bucketGroups.size(), 1);
    auto actualRange = bucketGroups[0]->GetLsmStore()->GetFileStoreId()->GetHashCodeRange();
    auto expectedRange = sliceIndex->GetBucketMapper()->GetTaskRange();
    ASSERT_NE(actualRange, nullptr);
    ASSERT_NE(expectedRange, nullptr);
    EXPECT_EQ(actualRange->GetStartHashCode(), expectedRange->GetStartHashCode());
    EXPECT_EQ(actualRange->GetEndHashCode(), expectedRange->GetEndHashCode());
    manager.Close();
}

/**
 * @tc.name  : RestoreMeta_ShouldReturnError_WhenReadBucketGroupNumFails
 * @tc.number: 001
 * @tc.desc  : Test that RestoreMeta returns error when reading bucketGroupNum fails.
 */
TEST_F(TestBucketGroup, RestoreMeta_ShouldReturnError_WhenReadBucketGroupNumFails)
{
    FileInputViewRef reader = std::make_shared<FileInputView>();
    std::vector<BucketGroupRangeRef> bucketGroupRanges;
    uint32_t totalBucketNum = 10;
    fileInput->Seek(0);
    BResult result = BucketGroupManager::RestoreMeta(fileInput, totalBucketNum, bucketGroupRanges);
    EXPECT_NE(result, BSS_OK);
}

/**
 * @tc.name  : RestoreMeta_ShouldReturnSuccess_WhenReadBucketGroupNumSucceeds
 * @tc.number: 002
 * @tc.desc  : Test that RestoreMeta returns success when reading bucketGroupNum succeeds.
 */
TEST_F(TestBucketGroup, RestoreMeta_ShouldReturnSuccess_WhenReadBucketGroupNumSucceeds)
{
    std::vector<BucketGroupRangeRef> bucketGroupRanges;
    uint32_t bucketGroupNum = 0;
    uint32_t totalBucketNum = 10;
    ASSERT_EQ(fileOutput->WriteUint32(bucketGroupNum), BSS_OK);
    fileInput->Seek(0);
    BResult result = BucketGroupManager::RestoreMeta(fileInput, totalBucketNum, bucketGroupRanges);
    EXPECT_EQ(result, BSS_OK);
    EXPECT_EQ(bucketGroupRanges.size(), bucketGroupNum);
}

/**
 * @tc.name  : RestoreMeta_ShouldReturnError_WhenBucketGroupRestoreFails
 * @tc.number: 003
 * @tc.desc  : Test that RestoreMeta returns error when BucketGroup::Restore fails.
 */
TEST_F(TestBucketGroup, RestoreMeta_ShouldReturnError_WhenBucketGroupRestoreFails)
{
    std::vector<BucketGroupRangeRef> bucketGroupRanges;
    uint32_t bucketGroupNum = 2;
    uint32_t totalBucketNum = 10;
    ASSERT_EQ(fileOutput->WriteUint32(bucketGroupNum), BSS_OK);
    fileInput->Seek(0);
    BResult result = BucketGroupManager::RestoreMeta(fileInput, totalBucketNum, bucketGroupRanges);
    EXPECT_NE(result, BSS_OK);
}

/**
 * @tc.name  : RestoreMeta_ShouldReturnSuccess_WhenBucketGroupRestoreSucceeds
 * @tc.number: 004
 * @tc.desc  : Test that RestoreMeta returns success when BucketGroup::Restore succeeds.
 */
TEST_F(TestBucketGroup, RestoreMeta_ShouldReturnSuccess_WhenBucketGroupRestoreSucceeds)
{
    std::vector<BucketGroupRangeRef> bucketGroupRanges;
    uint32_t bucketGroupNum = 2;
    uint32_t startBucket = 1;
    uint32_t endBucket = 2;
    uint32_t bucketGroupId = 3;
    uint32_t totalBucketNum = 10;
    ASSERT_EQ(fileOutput->WriteUint32(bucketGroupNum), BSS_OK);
    for (uint32_t i = 0; i < bucketGroupNum; i++) {
        ASSERT_EQ(fileOutput->WriteUint32(startBucket), BSS_OK);
        ASSERT_EQ(fileOutput->WriteUint32(endBucket), BSS_OK);
        ASSERT_EQ(fileOutput->WriteUint32(bucketGroupId), BSS_OK);
    }
    fileInput->Seek(0);
    BResult result = BucketGroupManager::RestoreMeta(fileInput, totalBucketNum, bucketGroupRanges);
    // Assert
    EXPECT_EQ(result, BSS_OK);
    EXPECT_EQ(bucketGroupRanges.size(), bucketGroupNum);
}

/**
 * @tc.name  : RestoreFileStore_ShouldReturnBSS_OK_WhenNoBucketGroups
 * @tc.number: 001
 * @tc.desc  : 测试当没有BucketGroup时，RestoreFileStore函数返回BSS_OK
 */
TEST_F(TestBucketGroup, RestoreFileStore_ShouldReturnBSS_OK_WhenNoBucketGroups)
{
    BucketGroupManager manager;
    manager.mBucketGroups.clear();
    std::vector<SliceTableRestoreMetaRef> sliceTableRestoreMetaList;
    std::unordered_map<std::string, std::string> pathMap;
    std::unordered_map<std::string, uint32_t> pathFileIdMap;
    BResult result = manager.RestoreFileStore(sliceTableRestoreMetaList, pathMap, pathFileIdMap, false);
    EXPECT_EQ(result, BSS_OK);
}
