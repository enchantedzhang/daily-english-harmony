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

#include "test_slice_table_br_cov.h"

#include <condition_variable>
#include <cstdio>
#include <mutex>
#include <thread>

#define private public
#include "snapshot/slice_table_restore_operation.h"
#undef private

using namespace ock::bss;

BoostStateDBImpl *TestSliceTableBrCov::mBoostStateDb = nullptr;
GeneratorRef TestSliceTableBrCov::mGenerator = nullptr;
SliceTableManagerRef TestSliceTableBrCov::mSliceTable = nullptr;
LogicalSliceChainRef TestSliceTableBrCov::mLogicalSliceChain = nullptr;
SliceIndexContextRef TestSliceTableBrCov::mSliceIndexContext = nullptr;
FileInputViewRef TestSliceTableBrCov::mInputView = nullptr;
FileOutputViewRef TestSliceTableBrCov::mOutputView = nullptr;

void TestSliceTableBrCov::SetUp()
{
    ConfigRef config = std::make_shared<Config>();
    config->Init(NO_0, NO_15, NO_16);
    mBoostStateDb = (BoostStateDBImpl *)BoostStateDBFactory::Create();
    mBoostStateDb->Open(config);
    mGenerator = MakeRef<Generator>();
    mSliceTable = mBoostStateDb->GetSliceTable();
    mLogicalSliceChain = mSliceTable->mSliceBucketIndex->GetLogicChainedSlice(0);
    mSliceIndexContext = mSliceTable->mSliceBucketIndex->GetSliceIndexContext(0);

    const std::string testFilePath = "test.txt";
    mInputView = std::make_shared<FileInputView>();
    mOutputView = std::make_shared<FileOutputView>();
    Uri uri(testFilePath);
    PathRef path = std::make_shared<Path>(uri);
    ConfigRef config2 = std::make_shared<Config>();
    ASSERT_EQ(mOutputView->Init(path, config2, FileOutputView::WriteMode::OVERWRITE), BSS_OK);
    ASSERT_EQ(mInputView->Init(FileSystemType::LOCAL, path), BSS_OK);
}

void TestSliceTableBrCov::TearDown()
{
    mInputView->Close();
    mInputView = nullptr;
    mOutputView->Close();
    mOutputView = nullptr;
    mBoostStateDb->Close();
    mBoostStateDb = nullptr;
}

TEST_F(TestSliceTableBrCov, Composite_ForceCompactionFlagIsSticky)
{
    auto composite = std::make_shared<CompositeLogicalSliceChain>();
    auto chain1 = std::make_shared<LogicalSliceChainImpl>();
    auto chain2 = std::make_shared<LogicalSliceChainImpl>();
    ASSERT_EQ(composite->AddLogicalSliceChain(chain1, false), BSS_OK);
    EXPECT_FALSE(composite->RequireForceCompaction());
    ASSERT_EQ(composite->AddLogicalSliceChain(chain2, true), BSS_OK);
    EXPECT_TRUE(composite->RequireForceCompaction());
    ASSERT_EQ(composite->AddLogicalSliceChain(chain1, false), BSS_OK);
    EXPECT_TRUE(composite->RequireForceCompaction());
    EXPECT_EQ(composite->AddLogicalSliceChain(nullptr, false), BSS_INVALID_PARAM);
}

TEST_F(TestSliceTableBrCov, Restore_ForceCompactionDoesNotDirectReplaceSingleChild)
{
    auto child = mSliceTable->mSliceBucketIndex->CreateLogicalChainedSlice();
    auto slice = getSlice();
    auto dataSlice = std::make_shared<DataSlice>();
    dataSlice->Init(slice);
    ASSERT_NE(child->CreateSlice(dataSlice, 0), nullptr);

    auto composite = std::make_shared<CompositeLogicalSliceChain>();
    ASSERT_EQ(composite->AddLogicalSliceChain(child, true), BSS_OK);
    mSliceTable->mSliceBucketIndex->mMappingTable[0] = composite;

    SliceTableRestoreOperation restoreOperation(mSliceTable->mConfig, mSliceTable);
    ASSERT_EQ(restoreOperation.CompactCompositeLogicalSliceChain(0), BSS_OK);
    EXPECT_NE(mSliceTable->mSliceBucketIndex->mMappingTable[0], child);
}

TEST_F(TestSliceTableBrCov, Restore_ForceCompactionCreatesValidEmptyChainWhenAllKeysAreFiltered)
{
    auto kvPair = getOneKVPair();
    uint32_t sourceBucket = 0;
    auto mapper = mSliceTable->mSliceBucketIndex->GetBucketMapper();
    ASSERT_NE(mapper, nullptr);
    ASSERT_EQ(mapper->Map(kvPair.first.KeyHashCode(), sourceBucket), BSS_OK);
    const uint32_t targetBucket = (sourceBucket + 1) % mSliceTable->mSliceBucketIndex->GetIndexCapacity();

    std::vector<std::pair<SliceKey, Value>> kvPairs{ kvPair };
    auto slice = std::make_shared<Slice>();
    SliceCreateMeta meta = { 0L, 1L, 0L };
    ASSERT_EQ(slice->Initialize(kvPairs, meta, mSliceTable->mMemManager), BSS_OK);
    auto dataSlice = std::make_shared<DataSlice>();
    dataSlice->Init(slice);
    auto child = mSliceTable->mSliceBucketIndex->CreateLogicalChainedSlice();
    ASSERT_NE(child->CreateSlice(dataSlice, 0), nullptr);
    auto lsmStore = mSliceTable->GetBucketGroupManager()->GetLsmStoreByBucketIndex(targetBucket);
    ASSERT_NE(lsmStore, nullptr);
    child->InsertFilePage(std::make_shared<FilePage>(lsmStore));

    auto composite = std::make_shared<CompositeLogicalSliceChain>();
    ASSERT_EQ(composite->AddLogicalSliceChain(child, true), BSS_OK);
    mSliceTable->mSliceBucketIndex->mMappingTable[targetBucket] = composite;

    auto usedMemory = mSliceTable->mEvictManager->mSliceEvictManager;
    ASSERT_NE(usedMemory, nullptr);
    const int64_t baseline = usedMemory->GetUsedMemorySize();
    mSliceTable->AddSliceUsedMemory(dataSlice->GetSize());

    SliceTableRestoreOperation restoreOperation(mSliceTable->mConfig, mSliceTable);
    ASSERT_EQ(restoreOperation.CompactCompositeLogicalSliceChain(targetBucket), BSS_OK);
    auto restoredChain = mSliceTable->mSliceBucketIndex->mMappingTable[targetBucket];
    ASSERT_NE(restoredChain, nullptr);
    EXPECT_EQ(std::dynamic_pointer_cast<CompositeLogicalSliceChain>(restoredChain), nullptr);
    EXPECT_TRUE(restoredChain->IsEmpty());
    EXPECT_TRUE(restoredChain->HasFilePage());
    EXPECT_EQ(usedMemory->GetUsedMemorySize(), baseline);
}

TEST_F(TestSliceTableBrCov, Restore_FilteredCompactionReconcilesUsedMemory)
{
    auto firstKv = getOneKVPair();
    auto secondKv = getOneKVPair();
    std::vector<std::pair<SliceKey, Value>> kvPairs{ firstKv, secondKv };
    auto inputSlice = std::make_shared<Slice>();
    SliceCreateMeta meta = { 0L, 1L, 0L };
    ASSERT_EQ(inputSlice->Initialize(kvPairs, meta, mSliceTable->mMemManager), BSS_OK);
    auto inputDataSlice = std::make_shared<DataSlice>();
    inputDataSlice->Init(inputSlice);
    const uint32_t inputSize = inputDataSlice->GetSize();

    auto child = mSliceTable->mSliceBucketIndex->CreateLogicalChainedSlice();
    ASSERT_NE(child, nullptr);
    auto inputAddress = child->CreateSlice(inputDataSlice, 0);
    ASSERT_NE(inputAddress, nullptr);
    auto composite = std::make_shared<CompositeLogicalSliceChain>();
    ASSERT_EQ(composite->AddLogicalSliceChain(child, true), BSS_OK);
    const uint32_t targetBucket = 0;
    mSliceTable->mSliceBucketIndex->mMappingTable[targetBucket] = composite;

    std::vector<std::pair<SliceKey, Value>> filteredKvPairs{ firstKv };
    auto outputSlice = std::make_shared<Slice>();
    ASSERT_EQ(outputSlice->Initialize(filteredKvPairs, meta, mSliceTable->mMemManager), BSS_OK);
    auto outputDataSlice = std::make_shared<DataSlice>();
    outputDataSlice->Init(outputSlice);
    const uint32_t outputSize = outputDataSlice->GetSize();
    ASSERT_LT(outputSize, inputSize);

    auto usedMemory = mSliceTable->mEvictManager->mSliceEvictManager;
    ASSERT_NE(usedMemory, nullptr);
    const int64_t baseline = usedMemory->GetUsedMemorySize();
    mSliceTable->AddSliceUsedMemory(inputSize);

    SliceTableRestoreOperation operation(mSliceTable->mConfig, mSliceTable);
    LogicalSliceChainRef logicalSliceChain = composite;
    std::vector<SliceAddressRef> invalidSliceAddressList{ inputAddress };
    ASSERT_EQ(operation.ReplaceCompositeLogicalSlice(logicalSliceChain, targetBucket, outputDataSlice,
                                                     invalidSliceAddressList),
              BSS_OK);

    auto outputChain = mSliceTable->mSliceBucketIndex->mMappingTable[targetBucket];
    ASSERT_NE(outputChain, nullptr);
    auto outputAddress = outputChain->GetSliceAddress(0);
    ASSERT_NE(outputAddress, nullptr);
    EXPECT_EQ(outputAddress->GetDataLen(), outputSize);
    EXPECT_EQ(usedMemory->GetUsedMemorySize(), baseline + static_cast<int64_t>(outputAddress->GetDataLen()));
}

TEST_F(TestSliceTableBrCov, RestoreCacheKeyIncludesAllDecodeMetadata)
{
    SliceTableRestoreOperation::SliceRestoreCacheKey base{ "restore-cache.slice", 17,   1024, 512,
                                                           CompressAlgo::LZ4,     12568 };

    auto expectDifferent = [&base](SliceTableRestoreOperation::SliceRestoreCacheKey changed) {
        EXPECT_FALSE(base == changed);
    };
    expectDifferent({ "other.slice", 17, 1024, 512, CompressAlgo::LZ4, 12568 });
    expectDifferent({ "restore-cache.slice", 18, 1024, 512, CompressAlgo::LZ4, 12568 });
    expectDifferent({ "restore-cache.slice", 17, 1025, 512, CompressAlgo::LZ4, 12568 });
    expectDifferent({ "restore-cache.slice", 17, 1024, 513, CompressAlgo::LZ4, 12568 });
    expectDifferent({ "restore-cache.slice", 17, 1024, 512, CompressAlgo::NONE, 12568 });
    expectDifferent({ "restore-cache.slice", 17, 1024, 512, CompressAlgo::LZ4, 12569 });
}

TEST_F(TestSliceTableBrCov, RestoreReusesLoadedSliceAfterPreviousFanoutSlotCompaction)
{
    const std::string path = "test.txt";
    auto sourceSlice = getSlice();
    ASSERT_NE(sourceSlice, nullptr);
    auto sourceBuffer = sourceSlice->GetByteBuffer();
    ASSERT_NE(sourceBuffer, nullptr);
    uint32_t rawLength = 0;
    ASSERT_EQ(sourceSlice->BytesSize(rawLength), BSS_OK);
    ASSERT_GT(rawLength, 0U);
    ASSERT_EQ(mOutputView->WriteByteBuffer(sourceBuffer, 0, rawLength), BSS_OK);
    ASSERT_EQ(mOutputView->Flush(), BSS_OK);
    mOutputView->Close();

    struct FanoutCopy {
        CompositeLogicalSliceChainRef composite;
        LogicalSliceChainRef child;
        SliceAddressRef address;
    };
    auto makeFanoutCopy = [&]() {
        FanoutCopy copy;
        copy.child = mSliceTable->mSliceBucketIndex->CreateLogicalChainedSlice();
        EXPECT_NE(copy.child, nullptr);
        copy.address = std::make_shared<SliceAddress>(rawLength, 12568, 0, 0);
        copy.address->SetLocalAddress(path);
        copy.address->SetSnapshotCompressionMeta(rawLength, CompressAlgo::NONE);
        EXPECT_NE(copy.child->InsertSlice(copy.address), INVALID_U32);
        copy.composite = std::make_shared<CompositeLogicalSliceChain>();
        EXPECT_EQ(copy.composite->AddLogicalSliceChain(copy.child, true), BSS_OK);
        return copy;
    };
    auto first = makeFanoutCopy();
    auto second = makeFanoutCopy();

    SliceTableRestoreOperation operation(mSliceTable->mConfig, mSliceTable);
    SliceTableRestoreOperation::DataSliceRestoreCache cache;
    mSliceTable->mSliceBucketIndex->mMappingTable[0] = first.composite;
    ASSERT_EQ(operation.TryLoadCompositeLogicalSliceChain(first.composite, 0, cache), BSS_OK);
    cache.ClearPreviousSlot();
    ASSERT_EQ(operation.CompactCompositeLogicalSliceChain(0), BSS_OK);
    cache.CompleteSlot();

    std::weak_ptr<Slice> restoredSlice = first.address->GetDataSlice()->GetSlice();
    first = {};
    ASSERT_FALSE(restoredSlice.expired());
    ASSERT_EQ(std::remove(path.c_str()), 0);
    ASSERT_EQ(operation.TryLoadCompositeLogicalSliceChain(second.composite, 0, cache), BSS_OK);
    cache.ClearPreviousSlot();

    auto secondDataSlice = second.address->GetDataSlice();
    ASSERT_NE(secondDataSlice, nullptr);
    EXPECT_EQ(secondDataSlice->GetSlice(), restoredSlice.lock());
}

TEST_F(TestSliceTableBrCov, RestoreProductionFlowReusesLoadedSliceAcrossFanoutSlots)
{
    const std::string path = "test.txt";
    auto sourceSlice = getSlice();
    ASSERT_NE(sourceSlice, nullptr);
    auto sourceBuffer = sourceSlice->GetByteBuffer();
    ASSERT_NE(sourceBuffer, nullptr);
    uint32_t rawLength = 0;
    ASSERT_EQ(sourceSlice->BytesSize(rawLength), BSS_OK);
    ASSERT_GT(rawLength, 0U);
    ASSERT_EQ(mOutputView->WriteByteBuffer(sourceBuffer, 0, rawLength), BSS_OK);
    ASSERT_EQ(mOutputView->Flush(), BSS_OK);
    mOutputView->Close();

    auto makeFanoutCopy = [&]() {
        auto child = mSliceTable->mSliceBucketIndex->CreateLogicalChainedSlice();
        EXPECT_NE(child, nullptr);
        auto address = std::make_shared<SliceAddress>(rawLength, 12568, 0, 0);
        address->SetLocalAddress(path);
        address->SetSnapshotCompressionMeta(rawLength, CompressAlgo::NONE);
        EXPECT_NE(child->InsertSlice(address), INVALID_U32);
        auto composite = std::make_shared<CompositeLogicalSliceChain>();
        EXPECT_EQ(composite->AddLogicalSliceChain(child, true), BSS_OK);
        return std::make_pair(composite, address);
    };
    auto first = makeFanoutCopy();
    auto second = makeFanoutCopy();
    ASSERT_GE(mSliceTable->mSliceBucketIndex->GetIndexCapacity(), NO_2);
    mSliceTable->mSliceBucketIndex->mMappingTable[0] = first.first;
    mSliceTable->mSliceBucketIndex->mMappingTable[1] = second.first;

    SliceTableRestoreOperation operation(mSliceTable->mConfig, mSliceTable);
    ASSERT_EQ(operation.LoadSlicesIntoSliceTable(0, true), BSS_OK);

    auto firstDataSlice = first.second->GetDataSlice();
    auto secondDataSlice = second.second->GetDataSlice();
    ASSERT_NE(firstDataSlice, nullptr);
    ASSERT_NE(secondDataSlice, nullptr);
    EXPECT_EQ(firstDataSlice->GetSlice(), secondDataSlice->GetSlice());
}

TEST_F(TestSliceTableBrCov, RestoreReleasesUnusedPreviousPayloadBeforeCompaction)
{
    auto sliceA = getSlice();
    auto sliceB = getSlice();
    ASSERT_NE(sliceA, nullptr);
    ASSERT_NE(sliceB, nullptr);
    std::weak_ptr<Slice> weakA = sliceA;
    std::weak_ptr<Slice> weakB = sliceB;

    SliceTableRestoreOperation::SliceRestoreCacheKey keyA{ "payload-a.slice", 0, 16, 16, CompressAlgo::NONE, 100 };
    SliceTableRestoreOperation::SliceRestoreCacheKey keyB{ "payload-b.slice", 0, 16, 16, CompressAlgo::NONE, 200 };
    SliceTableRestoreOperation::DataSliceRestoreCache cache;
    cache.Put(keyA, sliceA);
    cache.Put(keyB, sliceB);
    cache.CompleteSlot();
    sliceA.reset();
    sliceB.reset();

    auto currentA = cache.Find(keyA);
    cache.ClearPreviousSlot();

    EXPECT_FALSE(weakA.expired());
    EXPECT_TRUE(weakB.expired());
    EXPECT_EQ(currentA, weakA.lock());
}

TEST_F(TestSliceTableBrCov, RestoreRejectsDecodeWorkingSetLargerThanSliceTableCapacity)
{
    SliceTableRestoreOperation operation(mSliceTable->mConfig, mSliceTable);
    auto memManager = mSliceTable->mMemManager;
    ASSERT_NE(memManager, nullptr);
    const auto index = static_cast<size_t>(MemoryType::SLICE_TABLE);
    const uint64_t oldCapacity = memManager->mTypeMaxSize[index];
    memManager->mTypeMaxSize[index] = memManager->CalcSize(2);

    auto chain = mSliceTable->mSliceBucketIndex->CreateLogicalChainedSlice();
    ASSERT_NE(chain, nullptr);
    auto address = std::make_shared<SliceAddress>(2, 12568, 0, 0);
    address->SetLocalAddress("test.txt");
    address->SetSnapshotCompressionMeta(1, CompressAlgo::LZ4);
    ASSERT_NE(chain->InsertSlice(address), INVALID_U32);
    EXPECT_EQ(operation.LoadSlicesIntoLogicalSliceChain(chain, 0), BSS_ALLOC_FAIL);

    memManager->mTypeMaxSize[index] = oldCapacity;
}

/**
 * @tc.name  : TryCompact_ShouldReturnInvalidParam_WhenSliceIndexContextIsNull
 * @tc.number: TryCompact_Test_001
 * @tc.desc  : Test TryCompact function when sliceIndexContext is nullptr
 */
TEST_F(TestSliceTableBrCov, TryCompact_ShouldReturnInvalidParam_WhenSliceIndexContextIsNull)
{
    EXPECT_EQ(mSliceTable->TryCompact(nullptr, nullptr), BSS_INVALID_PARAM);
}

/**
 * @tc.name  : TryCompact_ShouldReturnInnerErr_WhenCurrentLogicalSliceChainIsNull
 * @tc.number: TryCompact_Test_002
 * @tc.desc  : Test TryCompact function when currentLogicalSliceChain is nullptr
 */
TEST_F(TestSliceTableBrCov, TryCompact_ShouldReturnInnerErr_WhenCurrentLogicalSliceChainIsNull)
{
    mSliceIndexContext->mLogicalSliceChain = nullptr;
    EXPECT_EQ(mSliceTable->TryCompact(mSliceIndexContext, nullptr), BSS_INNER_ERR);
}

/**
 * @tc.name  : TryCompact_ShouldReturnInnerErr_WhenLogicSliceChainNotMatched
 * @tc.number: TryCompact_Test_003
 * @tc.desc  : Test TryCompact function when currentLogicSliceChain does not match
 */
TEST_F(TestSliceTableBrCov, TryCompact_ShouldReturnInnerErr_WhenLogicSliceChainNotMatched)
{
    mSliceIndexContext->mSliceIndexSlot = 0;
    mSliceIndexContext->mLogicalSliceChain = mSliceTable->mSliceBucketIndex->CreateLogicalChainedSlice();
    EXPECT_EQ(mSliceTable->TryCompact(mSliceIndexContext, nullptr), BSS_INNER_ERR);
}

/**
 * @tc.name  : AddSlice_ShouldReturnMinusOne_WhenLogicalSliceChainIsNull
 * @tc.number: AddSlice_Test_001
 * @tc.desc  : Test AddSlice function when the logical slice chain is null
 */
TEST_F(TestSliceTableBrCov, AddSlice_ShouldReturnMinusOne_WhenLogicalSliceChainIsNull)
{
    std::vector<std::pair<SliceKey, Value>> dataList;
    mSliceIndexContext->mLogicalSliceChain = nullptr;
    int32_t result = mSliceTable->AddSlice(mSliceIndexContext, dataList, 1);
    EXPECT_EQ(result, -1);
}

/**
 * @tc.name  : AddSlice_ShouldReturnMinusOne_WhenLogicalSliceChainIsNone
 * @tc.number: AddSlice_Test_002
 * @tc.desc  : Test AddSlice function when the logical slice chain is none
 */
TEST_F(TestSliceTableBrCov, AddSlice_ShouldReturnMinusOne_WhenLogicalSliceChainIsNone)
{
    std::vector<std::pair<SliceKey, Value>> dataList;
    mSliceIndexContext->mLogicalSliceChain = std::make_shared<LogicalSliceChainImpl>();
    mSliceIndexContext->mLogicalSliceChain->NotifyNoneFlag(true);
    int32_t result = mSliceTable->AddSlice(mSliceIndexContext, dataList, 1);
    EXPECT_EQ(result, -1);
}

/**
 * @tc.name  : AddSlice_ShouldReturnZero_WhenDataListIsEmpty
 * @tc.number: AddSlice_Test_003
 * @tc.desc  : Test AddSlice function when the data list is empty
 */
TEST_F(TestSliceTableBrCov, AddSlice_ShouldReturnZero_WhenDataListIsEmpty)
{
    std::vector<std::pair<SliceKey, Value>> dataList;
    mSliceIndexContext->mLogicalSliceChain = std::make_shared<LogicalSliceChainImpl>();
    int32_t result = mSliceTable->AddSlice(mSliceIndexContext, dataList, 1);
    EXPECT_EQ(result, 0);
}

/**
 * @tc.name  : AddSlice_ShouldReturnMinusOne_WhenSliceInitializationFails
 * @tc.number: AddSlice_Test_004
 * @tc.desc  : Test AddSlice function when the slice initialization fails
 */
TEST_F(TestSliceTableBrCov, AddSlice_ShouldReturnMinusOne_WhenSliceInitializationFails)
{
    std::vector<std::pair<SliceKey, Value>> kvPairs;
    kvPairs.emplace_back(getOneKVPair());
    mSliceIndexContext->mLogicalSliceChain = std::make_shared<LogicalSliceChainImpl>();
    mBoostStateDb->mMemManager->mMemoryLimit = NO_50;
    int32_t result = mSliceTable->AddSlice(mSliceIndexContext, kvPairs, 1);
    EXPECT_EQ(result, -1);
}

/**
 * @tc.name  : Initialize_ShouldReturnBSSERR_WhenConfigIsNullptr
 * @tc.number: Initialize_Test_001
 * @tc.desc  : Test Initialize function when config is nullptr
 */
TEST_F(TestSliceTableBrCov, Initialize_ShouldReturnBSSERR_WhenConfigIsNullptr)
{
    SliceTable sliceTable;
    BResult ret = sliceTable.Initialize(nullptr, nullptr, nullptr, nullptr);
    EXPECT_EQ(ret, BSS_ERR);
}

/**
 * @tc.name  : ComputeIndexBucketNum_ShouldReturnLower_WhenBucketNumIsLessThanThreshold
 * @tc.number: ComputeIndexBucketNum_Test_001
 * @tc.desc  : Test that the function returns the lower power of two when bucketNum is less than the threshold.
 */
TEST_F(TestSliceTableBrCov, ComputeIndexBucketNum_ShouldReturnLower_WhenBucketNumIsLessThanThreshold)
{
    std::shared_ptr<Config> config = std::make_shared<Config>();
    uint64_t totalMem = NO_1024;                    // Example total memory
    config->SetSliceStandardSizePerBucket(NO_256);  // Example slice size per bucket
    uint32_t result = mSliceTable->ComputeIndexBucketNum(totalMem, config);
    EXPECT_EQ(result, NO_4);
}

/**
 * @tc.name  : ComputeIndexBucketNum_ShouldLogWarning_WhenBucketNumIsLessThanNO10
 * @tc.number: ComputeIndexBucketNum_Test_003
 * @tc.desc  : Test that the function logs a warning when bucketNum is less than NO_10.
 */
TEST_F(TestSliceTableBrCov, ComputeIndexBucketNum_ShouldLogWarning_WhenBucketNumIsLessThanNO10)
{
    std::shared_ptr<Config> config = std::make_shared<Config>();
    uint64_t totalMem = NO_128;                     // Example total memory
    config->SetSliceStandardSizePerBucket(NO_128);  // Example slice size per bucket
    uint32_t result = mSliceTable->ComputeIndexBucketNum(totalMem, config);
    EXPECT_EQ(result, NO_1);
}

/**
 * @tc.name  : ComputeIndexBucketNum_ShouldReturn1_WhenTotalMemIsLessThanSliceSize
 * @tc.number: ComputeIndexBucketNum_Test_004
 * @tc.desc  : Test that the function returns 1 when totalMem is less than sliceSizePerBucket.
 */
TEST_F(TestSliceTableBrCov, ComputeIndexBucketNum_ShouldReturn1_WhenTotalMemIsLessThanSliceSize)
{
    std::shared_ptr<Config> config = std::make_shared<Config>();
    uint64_t totalMem = NO_64;                      // Example total memory
    config->SetSliceStandardSizePerBucket(NO_128);  // Example slice size per bucket
    uint32_t result = mSliceTable->ComputeIndexBucketNum(totalMem, config);
    EXPECT_EQ(result, NO_1);
}

/**
 * @tc.name  : Init_ShouldReturnBSSERR_WhenLogicalSliceChainIsNull
 * @tc.number: Init_Test_001
 * @tc.desc  : Test that Init returns BSS_ERR when mLogicalSliceChain is nullptr
 */
TEST_F(TestSliceTableBrCov, Init_ShouldReturnBSSERR_WhenLogicalSliceChainIsNull)
{
    SliceTablePrefixIterator iterator;
    Key prefixKey;
    BResult result = iterator.Init(nullptr, prefixKey);
    EXPECT_EQ(result, BSS_ERR);
    iterator.Close();
}

/**
 * @tc.name  : Init_ShouldReturnBSSERR_WhenSliceAddressIsNull
 * @tc.number: Init_Test_002
 * @tc.desc  : Test that Init returns BSS_ERR when SliceAddressRef is nullptr
 */
TEST_F(TestSliceTableBrCov, Init_ShouldReturnBSSERR_WhenSliceAddressIsNull)
{
    SliceTablePrefixIterator iterator;
    Key prefixKey;
    LogicalSliceChainRef logicalSliceChain = mockAddSlice();
    logicalSliceChain->SetSliceAddress(0, nullptr);
    logicalSliceChain->SetSliceChainTailIndex(0);
    BResult result = iterator.Init(logicalSliceChain, prefixKey);
    EXPECT_EQ(result, BSS_ERR);
    iterator.Close();
}

/**
 * @tc.name  : Init_ShouldReturnBSSERR_WhenDataSliceIsNull
 * @tc.number: Init_Test_004
 * @tc.desc  : Test that Init returns BSS_ERR when DataSlice is nullptr
 */
TEST_F(TestSliceTableBrCov, Init_ShouldReturnBSSERR_WhenDataSliceIsNull)
{
    SliceTablePrefixIterator iterator;
    Key prefixKey;
    LogicalSliceChainRef logicalSliceChain = mockAddSlice();
    auto sliceAddresses = logicalSliceChain->GetSliceAddresses();
    sliceAddresses[0]->mDataSlice = nullptr;
    logicalSliceChain->SetSliceChainTailIndex(0);
    BResult result = iterator.Init(logicalSliceChain, prefixKey);
    EXPECT_EQ(result, BSS_ERR);
    iterator.Close();
}

/**
 * @tc.name  : InternalGetList_ShouldReturnBSSERR_WhenLogicalSliceChainIsNull
 * @tc.number: InternalGetList_Test_001
 * @tc.desc  : Test case to verify that InternalGetList returns BSS_ERR when logical slice chain is null.
 */
TEST_F(TestSliceTableBrCov, InternalGetList_ShouldReturnBSSERR_WhenLogicalSliceChainIsNull)
{
    std::deque<Value> result;
    QueryKey key = mGenerator->GenerateKey(NO_1, NO_2);
    mSliceTable->mSliceBucketIndex->mMappingTable[0] = nullptr;
    std::vector<SectionsReadContextRef> readMetas;
    EXPECT_EQ(mSliceTable->InternalGetList(key, result, readMetas), BSS_ERR);
}

/**
 * @tc.name  : InternalGetList_ShouldReturnBSSNOTEXISTS_WhenLogicalSliceChainIsEmptyAndNoFilePage
 * @tc.number: InternalGetList_Test_002
 * @tc.desc  : Test case to verify that InternalGetList returns BSS_NOT_EXISTS when logical slice chain is empty and has
 * no file page.
 */
TEST_F(TestSliceTableBrCov, InternalGetList_ShouldReturnBSSNOTEXISTS_WhenLogicalSliceChainIsEmptyAndNoFilePage)
{
    std::deque<Value> result;
    QueryKey key = mGenerator->GenerateKey(NO_1, NO_2);
    mSliceTable->mSliceBucketIndex->mMappingTable[0]->NotifyNoneFlag(true);
    std::vector<SectionsReadContextRef> readMetas;
    EXPECT_EQ(mSliceTable->InternalGetList(key, result, readMetas), BSS_NOT_EXISTS);
}

/**
 * @tc.name  : AddSlice_ShouldReturnBSS_OK_WhenRawDataSliceIsEmpty
 * @tc.number: AddSlice_Test_001
 * @tc.desc  : Test that AddSlice returns BSS_OK when rawDataSlice is empty.
 */
TEST_F(TestSliceTableBrCov, AddSlice_ShouldReturnBSS_OK_WhenRawDataSliceIsEmpty)
{
    SliceIndexContextRef curSliceIndexContext;
    std::shared_ptr<RawDataSlice> rawDataSlice;
    uint32_t addSize;
    uint32_t capacity = NO_1024;
    uintptr_t addr = 0;
    mSliceTable->mMemManager->GetMemory(MemoryType::FRESH_TABLE, capacity, addr);
    MemorySegmentRef memorySegment = MakeRef<MemorySegment>(capacity, reinterpret_cast<uint8_t *>(addr),
                                                            mSliceTable->mMemManager);
    BoostSegmentRef segment = std::make_shared<BoostSegment>();
    segment->Init(0, memorySegment, 0);
    BoostHashMapRef binaryData = segment->GetBinaryData();
    rawDataSlice = std::make_shared<RawDataSlice>(binaryData->GetMemorySegment(), segment->GetVersion());
    rawDataSlice->mSliceData = std::vector<std::pair<BinaryKey, FreshValueNodePtr>>();
    addSize = 0;
    bool forceEvict = false;
    BResult result = mSliceTable->AddSlice(curSliceIndexContext, *rawDataSlice, addSize, forceEvict);
    EXPECT_EQ(result, BSS_OK);
}

/**
 * @tc.name  : AddSlice_ShouldReturnError_WhenSliceInitializeFails
 * @tc.number: AddSlice_Test_002
 * @tc.desc  : Test that AddSlice returns an error when slice initialization fails.
 */
TEST_F(TestSliceTableBrCov, AddSlice_ShouldReturnError_WhenSliceInitializeFails)
{
    SliceIndexContextRef curSliceIndexContext;
    std::shared_ptr<RawDataSlice> rawDataSlice;
    uint32_t addSize;
    uint32_t capacity = NO_1024;
    uintptr_t addr = 0;
    mSliceTable->mMemManager->GetMemory(MemoryType::FRESH_TABLE, capacity, addr);
    MemorySegmentRef memorySegment = MakeRef<MemorySegment>(capacity, reinterpret_cast<uint8_t *>(addr),
                                                            mSliceTable->mMemManager);
    BoostSegmentRef segment = std::make_shared<BoostSegment>();
    segment->Init(0, memorySegment, 0);
    BoostHashMapRef binaryData = segment->GetBinaryData();
    rawDataSlice = std::make_shared<RawDataSlice>(binaryData->GetMemorySegment(), segment->GetVersion());

    PriKey primaryKey;
    auto buffer = binaryData->GetMemorySegment()->GetSegment();
    auto bufferLen = binaryData->GetMemorySegment()->mCapacity;
    primaryKey.mHashCode = *reinterpret_cast<uint32_t *>(buffer);
    primaryKey.mStateId = *reinterpret_cast<uint16_t *>(buffer + NO_4);
    primaryKey.mKeyData = buffer;
    primaryKey.mKeyDataLength = bufferLen;
    SecKey secondKey;
    secondKey.mHashCode = *reinterpret_cast<uint32_t *>(buffer);
    secondKey.mKeyData = buffer;
    secondKey.mKeyDataLength = bufferLen;
    BinaryKey binaryKey;
    binaryKey.Parse(primaryKey, secondKey);

    auto bufSegment = reinterpret_cast<uint8_t *>(addr);
    uint32_t valLen = sizeof(FreshValueNode);
    FreshValueNode v1{ NodeType::SERIALIZED, valLen, ValueType::APPEND, NO_1 };
    auto nodeDistance = valLen + NO_5 + NO_4;
    memcpy_s(bufSegment + nodeDistance, capacity - nodeDistance, &v1, sizeof(FreshValueNode));
    auto node = FreshValueNode::FromBuffer(bufSegment + nodeDistance);
    rawDataSlice->mSliceData.emplace_back(std::move(binaryKey), node);
    rawDataSlice->PutMixHashCode(binaryKey.mMixedHashCode);
    rawDataSlice->PutIndexVec(0);
    addSize = 0;
    mBoostStateDb->mMemManager->mMemoryLimit = NO_100;
    bool forceEvict = false;
    BResult result = mSliceTable->AddSlice(curSliceIndexContext, *rawDataSlice, addSize, forceEvict);
    EXPECT_NE(result, BSS_OK);
}

/**
 * @tc.name  : PrefixIterator_ShouldReturnNull_WhenLogicalSliceChainIsNull
 * @tc.number: PrefixIterator_Test_001
 * @tc.desc  : Test that PrefixIterator returns nullptr when logical slice chain is nullptr
 */
TEST_F(TestSliceTableBrCov, PrefixIterator_ShouldReturnNull_WhenLogicalSliceChainIsNull)
{
    Key prefixKey;
    mSliceTable->mSliceBucketIndex->mMappingTable[0] = nullptr;
    KeyValueIteratorRef result = mSliceTable->PrefixIterator(prefixKey);
    EXPECT_EQ(result, nullptr);
}

/**
 * @tc.name  : PrefixIterator_ShouldReturnNull_WhenLogicalSliceChainIsEmpty
 * @tc.number: PrefixIterator_Test_002
 * @tc.desc  : Test that PrefixIterator returns nullptr when logical slice chain is empty
 */
TEST_F(TestSliceTableBrCov, PrefixIterator_ShouldReturnNull_WhenLogicalSliceChainIsEmpty)
{
    Key prefixKey;
    mSliceTable->mSliceBucketIndex->mMappingTable[0]->NotifyNoneFlag(true);
    KeyValueIteratorRef result = mSliceTable->PrefixIterator(prefixKey);
    EXPECT_EQ(result, nullptr);
}

/**
 * @tc.name  : PrefixIterator_ShouldReturnNull_WhenIteratorInitFails
 * @tc.number: PrefixIterator_Test_004
 * @tc.desc  : Test that PrefixIterator returns nullptr when iterator initialization fails
 */
TEST_F(TestSliceTableBrCov, PrefixIterator_ShouldReturnNull_WhenIteratorInitFails)
{
    Key prefixKey;
    LogicalSliceChainRef logicalSliceChain = mockAddSlice();
    logicalSliceChain->SetSliceAddress(0, nullptr);
    logicalSliceChain->SetSliceChainTailIndex(0);
    KeyValueIteratorRef result = mSliceTable->PrefixIterator(prefixKey);
    EXPECT_EQ(result, nullptr);
}

/**
 * @tc.name  : Index_GetLogicChainedSlice_ShouldReturnNullptr_WhenBucketIndexOutOfRange
 * @tc.number: GetLogicChainedSlice_Test_001
 * @tc.desc  : 测试当 bucketIndex 超出范围时，函数应返回 nullptr
 */
TEST_F(TestSliceTableBrCov, Index_GetLogicChainedSlice_ShouldReturnNullptr_WhenBucketIndexOutOfRange)
{
    auto mMappingTableSize = mSliceTable->mSliceBucketIndex->mMappingTable.size();
    LogicalSliceChainRef result = mSliceTable->mSliceBucketIndex->GetLogicChainedSlice(mMappingTableSize + 1);
    EXPECT_EQ(result, nullptr);
}

/**
 * @tc.name  : Index_UpdateLogicChainedSlice_ShouldLogError_WhenOldLogicalSliceChainDoesNotMatch
 * @tc.number: UpdateLogicChainedSlice_Test_002
 * @tc.desc  : Test that an error is logged when the oldLogicalSliceChain does not match the current value.
 */
TEST_F(TestSliceTableBrCov, Index_UpdateLogicChainedSlice_ShouldLogError_WhenOldLogicalSliceChainDoesNotMatch)
{
    LogicalSliceChainRef newLogicalSliceChain = mSliceTable->mSliceBucketIndex->CreateLogicalChainedSlice();
    mSliceTable->mSliceBucketIndex->mMappingTable[0] = newLogicalSliceChain;
    mSliceTable->mSliceBucketIndex->UpdateLogicalSliceChain(0, mLogicalSliceChain, newLogicalSliceChain);
}

/**
 * @tc.name  : Index_ComputeHashCodeRange_ShouldReturnNullptr_WhenStartBucketIsInvalid
 * @tc.number: 001
 * @tc.desc  : Test ComputeHashCodeRange function when startBucket is invalid
 */
TEST_F(TestSliceTableBrCov, Index_ComputeHashCodeRange_ShouldReturnNullptr_WhenStartBucketIsInvalid)
{
    int32_t startBucket = -1;
    int32_t endBucket = NO_5;
    auto result = mSliceTable->mSliceBucketIndex->ComputeHashCodeRange(startBucket, endBucket);
    EXPECT_EQ(result, nullptr);
    startBucket = NO_4096 + NO_1;
    auto result2 = mSliceTable->mSliceBucketIndex->ComputeHashCodeRange(startBucket, endBucket);
    EXPECT_EQ(result2, nullptr);
}

/**
 * @tc.name  : Index_ComputeHashCodeRange_ShouldReturnNullptr_WhenEndBucketIsInvalid
 * @tc.number: 002
 * @tc.desc  : Test ComputeHashCodeRange function when endBucket is invalid
 */
TEST_F(TestSliceTableBrCov, Index_ComputeHashCodeRange_ShouldReturnNullptr_WhenEndBucketIsInvalid)
{
    int32_t startBucket = NO_5;
    int32_t endBucket = -1;
    auto result = mSliceTable->mSliceBucketIndex->ComputeHashCodeRange(startBucket, endBucket);
    EXPECT_EQ(result, nullptr);
    endBucket = NO_4096 + NO_1;
    auto result2 = mSliceTable->mSliceBucketIndex->ComputeHashCodeRange(startBucket, endBucket);
    EXPECT_EQ(result2, nullptr);
}

/**
 * @tc.name  : Index_Initialize_ShouldReturnBSSERR_WhenTotalBucketNumIsZero
 * @tc.number: Initialize_Test_001
 * @tc.desc  : Test Initialize function when totalBucketNum is zero
 */
TEST_F(TestSliceTableBrCov, Index_Initialize_ShouldReturnBSSERR_WhenTotalBucketNumIsZero)
{
    BResult result = mSliceTable->mSliceBucketIndex->Initialize(0, nullptr);
    EXPECT_EQ(result, BSS_ERR);
}

/**
 * @tc.name  : Index_Initialize_ShouldReturnBSSERR_WhenConfigIsNullptr
 * @tc.number: Initialize_Test_002
 * @tc.desc  : Test Initialize function when config is nullptr
 */
TEST_F(TestSliceTableBrCov, Index_Initialize_ShouldReturnBSSERR_WhenConfigIsNullptr)
{
    BResult result = mSliceTable->mSliceBucketIndex->Initialize(10, nullptr);
    EXPECT_EQ(result, BSS_ERR);
}

TEST_F(TestSliceTableBrCov, Index_TaskLocalMapperUsesAllBuckets)
{
    auto config = std::make_shared<Config>(8, 15, 1024);
    auto index = std::make_shared<SliceBucketIndex>();
    ASSERT_EQ(index->Initialize(1024, config), BSS_OK);
    auto mapper = index->GetBucketMapper();
    ASSERT_NE(mapper, nullptr);

    for (uint32_t bucket = 0; bucket < 1024; ++bucket) {
        auto range = mapper->GetBucketRange(bucket);
        ASSERT_NE(range, nullptr);
        auto context = index->GetSliceIndexContext(range->GetStartHashCode(), true);
        ASSERT_NE(context, nullptr);
        EXPECT_EQ(context->GetSliceIndexSlot(), bucket);
    }
}

/**
 * @tc.name  : DataSlice_Init_ShouldReturnBSSERR_WhenSliceIsNullptr
 * @tc.number: DataSlice_Init_001
 * @tc.desc  : Test that Init function returns BSS_ERR when the input slice is nullptr
 */
TEST_F(TestSliceTableBrCov, DataSlice_Init_ShouldReturnBSSERR_WhenSliceIsNullptr)
{
    DataSlice dataSlice;
    SliceRef nullptrSlice = nullptr;
    dataSlice.Init(nullptrSlice);
}

/**
 * @tc.name  : LogicalSliceChain_CreateSlice_ShouldReturnNull_WhenDataSliceIsNull
 * @tc.number: LogicalSliceChainTest_001
 * @tc.desc  : Test that CreateSlice returns nullptr when dataSlice is nullptr
 */
TEST_F(TestSliceTableBrCov, LogicalSliceChain_CreateSlice_ShouldReturnNull_WhenDataSliceIsNull)
{
    DataSliceRef dataSlice = nullptr;
    uint64_t readAccessNumber = 1;
    SliceAddressRef result = mLogicalSliceChain->CreateSlice(dataSlice, readAccessNumber);
    EXPECT_EQ(result, nullptr);
}

/**
 * @tc.name  : LogicalSliceChain_CreateSlice_ShouldReturnNull_WhenChainEndIndexIsGreaterThanSliceAddressesSize
 * @tc.number: LogicalSliceChainTest_002
 * @tc.desc  : Test that CreateSlice returns nullptr when mChainEndIndex is greater than or equal to
 * mSliceAddresses.size()
 */
TEST_F(TestSliceTableBrCov,
       LogicalSliceChain_CreateSlice_ShouldReturnNull_WhenChainEndIndexIsGreaterThanSliceAddressesSize)
{
    DataSliceRef dataSlice = std::make_shared<DataSlice>();
    uint64_t readAccessNumber = 1;
    mLogicalSliceChain->SetSliceChainTailIndex(NO_1);
    mLogicalSliceChain->ClearSliceAddresses();
    SliceAddressRef result = mLogicalSliceChain->CreateSlice(dataSlice, readAccessNumber);
    EXPECT_EQ(result, nullptr);
}

/**
 * @tc.name  : LogicalSliceChain_InsertSlice_ShouldReturnMinusOne_WhenSliceAddressIsNullptr
 * @tc.number: InsertSlice_Test_001
 * @tc.desc  : Test the InsertSlice function when the sliceAddress is nullptr.
 */
TEST_F(TestSliceTableBrCov, LogicalSliceChain_InsertSlice_ShouldReturnMinusOne_WhenSliceAddressIsNullptr)
{
    SliceAddressRef sliceAddress = nullptr;
    EXPECT_EQ(mLogicalSliceChain->InsertSlice(sliceAddress), INVALID_U32);
}

/**
 * @tc.name  : LogicalSliceChain_InsertSlice_ShouldReturnMinusOne_WhenDataSliceIsNullptr
 * @tc.number: InsertSlice_Test_002
 * @tc.desc  : Test the InsertSlice function when the GetDataSlice of sliceAddress is nullptr.
 */
TEST_F(TestSliceTableBrCov, LogicalSliceChain_InsertSlice_ShouldReturnMinusOne_WhenDataSliceIsNullptr)
{
    SliceAddressRef sliceAddress = std::make_shared<SliceAddress>();
    sliceAddress->SetDataSlice(nullptr);
    EXPECT_EQ(mLogicalSliceChain->InsertSlice(sliceAddress), 0U);
}

/**
 * @tc.name  : LogicalSliceChain_SetBaseSliceIndex_ShouldReturnBSSERR_WhenIndexTooLarge
 * @tc.number: SetBaseSliceIndex_Test_001
 * @tc.desc  : 测试当传入的index大于mChainEndIndex + 1时，SetBaseSliceIndex应返回BSS_ERR
 */
TEST_F(TestSliceTableBrCov, LogicalSliceChain_SetBaseSliceIndex_ShouldReturnBSSERR_WhenIndexTooLarge)
{
    mLogicalSliceChain->SetSliceChainTailIndex(NO_10);
    BResult result = mLogicalSliceChain->SetBaseSliceIndex(NO_12);
    EXPECT_EQ(result, BSS_ERR);
}

/**
 * @tc.name  : LogicalSliceChain_SliceIterator_ShouldReturnEmptySliceAddressIterator_WhenSliceAddressesIsEmpty
 * @tc.number: SliceIterator_Test_001
 * @tc.desc  : Test the SliceIterator function when mSliceAddresses is empty.
 */
TEST_F(TestSliceTableBrCov,
       LogicalSliceChain_SliceIterator_ShouldReturnEmptySliceAddressIterator_WhenSliceAddressesIsEmpty)
{
    mLogicalSliceChain->ClearSliceAddresses();
    IteratorRef<SliceAddressRef> iterator = mLogicalSliceChain->SliceIterator();
    EXPECT_NE(iterator, nullptr);
    EXPECT_TRUE(dynamic_cast<EmptySliceAddressIterator *>(iterator.Get()) != nullptr);
}

/**
 * @tc.name  : LogicalSliceChain_Init_ShouldReturnBSS_ERR_WhenDefaultChainLenIsTooSmall
 * @tc.number: Init_Test_001
 * @tc.desc  : Test the Init function when defaultChainLen is smaller than NO_3
 */
TEST_F(TestSliceTableBrCov, LogicalSliceChain_Init_ShouldReturnBSS_ERR_WhenDefaultChainLenIsTooSmall)
{
    uint32_t defaultChainLen = NO_3 - 1;
    BResult result = mLogicalSliceChain->Init(SliceStatus::NORMAL, defaultChainLen);
    EXPECT_EQ(result, BSS_ERR);
}

/**
 * @tc.name  : LogicalSliceChain_Initialize_ShouldReturnInvalidParam_WhenLogicalSliceChainIsNull
 * @tc.number: Initialize_Test_001
 * @tc.desc  : Test Initialize function when logicalSliceChain is null
 */
TEST_F(TestSliceTableBrCov, LogicalSliceChain_Initialize_ShouldReturnInvalidParam_WhenLogicalSliceChainIsNull)
{
    std::unordered_map<SliceAddressRef, DataSliceRef, SliceAddressHash, SliceAddressEqual> copiedDataSlice;
    EXPECT_EQ(mLogicalSliceChain->Initialize(nullptr, NO_0, NO_10, copiedDataSlice, false, false), BSS_INVALID_PARAM);
}

/**
 * @tc.name  : LogicalSliceChain_Initialize_ShouldReturnErr_WhenStartIndexGreaterThanChainEndIndex
 * @tc.number: Initialize_Test_003
 * @tc.desc  : Test Initialize function when startIndex is greater than chainEndIndex
 */
TEST_F(TestSliceTableBrCov, LogicalSliceChain_Initialize_ShouldReturnErr_WhenStartIndexGreaterThanChainEndIndex)
{
    std::unordered_map<SliceAddressRef, DataSliceRef, SliceAddressHash, SliceAddressEqual> copiedDataSlice;
    mLogicalSliceChain->SetBaseSliceIndex(NO_5);
    mLogicalSliceChain->SetSliceChainTailIndex(NO_0);
    EXPECT_EQ(mLogicalSliceChain->Initialize(mLogicalSliceChain, NO_5, NO_5, copiedDataSlice, false, false), BSS_ERR);
}

/**
 * @tc.name  : LogicalSliceChain_Initialize_ShouldSuccess_WhenSliceAddressesIsEmpty
 * @tc.number: Initialize_Test_004
 * @tc.desc  : Test Initialize function when mSliceAddresses is empty
 */
TEST_F(TestSliceTableBrCov, LogicalSliceChain_Initialize_ShouldSuccess_WhenSliceAddressesIsEmpty)
{
    std::unordered_map<SliceAddressRef, DataSliceRef, SliceAddressHash, SliceAddressEqual> copiedDataSlice;
    LogicalSliceChainRef logicalSliceChain = mockAddSlice();
    logicalSliceChain->SetBaseSliceIndex(NO_1);
    logicalSliceChain->ClearSliceAddresses();
    EXPECT_EQ(logicalSliceChain->Initialize(logicalSliceChain, NO_0, NO_1, copiedDataSlice, false, false), BSS_OK);
}

/**
 * @tc.name  : LogicalSliceChain_Initialize_ShouldSuccess_WhenSliceAddressesIsNotEmpty
 * @tc.number: Initialize_Test_005
 * @tc.desc  : Test Initialize function when mSliceAddresses is not empty
 */
TEST_F(TestSliceTableBrCov, LogicalSliceChain_Initialize_ShouldSuccess_WhenSliceAddressesIsNotEmpty)
{
    std::unordered_map<SliceAddressRef, DataSliceRef, SliceAddressHash, SliceAddressEqual> copiedDataSlice;
    LogicalSliceChainRef logicalSliceChain = mockAddSlice();
    logicalSliceChain->SetBaseSliceIndex(NO_1);
    EXPECT_EQ(logicalSliceChain->Initialize(logicalSliceChain, NO_0, NO_1, copiedDataSlice, false, false), BSS_OK);
}

/**
 * @tc.name  : LogicalSliceChain_Initialize_ShouldResizeFilePage_WhenHasFilePageIsTrue
 * @tc.number: Initialize_Test_006
 * @tc.desc  : Test Initialize function when hasFilePage is true
 */
TEST_F(TestSliceTableBrCov, LogicalSliceChain_Initialize_ShouldResizeFilePage_WhenHasFilePageIsTrue)
{
    std::unordered_map<SliceAddressRef, DataSliceRef, SliceAddressHash, SliceAddressEqual> copiedDataSlice;
    LogicalSliceChainRef logicalSliceChain = mockAddSlice();
    logicalSliceChain->SetBaseSliceIndex(NO_1);
    logicalSliceChain->ClearSliceAddresses();
    EXPECT_EQ(logicalSliceChain->Initialize(logicalSliceChain, NO_0, NO_1, copiedDataSlice, false, true), BSS_OK);
    std::vector<FilePageRef> filePages;
    logicalSliceChain->GetFilePages(filePages);
    EXPECT_EQ(filePages.size(), NO_0);
}

/**
 * @tc.name  : LogicalSliceChain_CaptureSnapshotView_ShouldKeepSliceAliveAfterRelease
 * @tc.number: CaptureSnapshotView_Test_001
 * @tc.desc  : Test captured SliceAddressRef keeps data alive until snapshot deep copy
 * completes
 */
TEST_F(TestSliceTableBrCov, LogicalSliceChain_CaptureSnapshotView_ShouldKeepSliceAliveAfterRelease)
{
    auto logicalSliceChain = std::dynamic_pointer_cast<LogicalSliceChainImpl>(mockAddSlice());
    ASSERT_NE(logicalSliceChain, nullptr);
    SliceAddressRef sourceSliceAddress = logicalSliceChain->GetSliceAddress(NO_0);
    ASSERT_NE(sourceSliceAddress, nullptr);
    DataSliceRef sourceDataSlice = sourceSliceAddress->GetDataSlice();
    ASSERT_NE(sourceDataSlice, nullptr);
    SliceRef sourceSlice = sourceDataSlice->GetSlice();
    ASSERT_NE(sourceSlice, nullptr);
    uint32_t expectedDataLen = sourceSliceAddress->GetDataLen();
    uint32_t expectedCheckSum = sourceSliceAddress->GetCheckSum();
    std::weak_ptr<SliceAddress> weakSliceAddress = sourceSliceAddress;
    std::weak_ptr<DataSlice> weakDataSlice = sourceDataSlice;

    LogicalSliceChainSnapshotView snapshotView;
    ASSERT_EQ(logicalSliceChain->CaptureSnapshotView(snapshotView), BSS_OK);
    ASSERT_EQ(snapshotView.mSliceAddresses.size(), NO_1);
    ASSERT_EQ(snapshotView.mSliceStatuses.size(), NO_1);
    EXPECT_EQ(snapshotView.mSliceStatuses[NO_0], SliceStatus::NORMAL);

    ASSERT_TRUE(sourceSliceAddress->SetStatus(SliceEvent::FLUSH));
    ASSERT_TRUE(sourceSliceAddress->SetStatus(SliceEvent::EVICT));
    logicalSliceChain->ReleaseSliceAddress(sourceSliceAddress);
    EXPECT_EQ(logicalSliceChain->GetSliceAddress(NO_0), SliceAddress::mInvalidAddress);
    sourceDataSlice.reset();
    sourceSliceAddress.reset();
    EXPECT_FALSE(weakSliceAddress.expired());
    EXPECT_FALSE(weakDataSlice.expired());

    auto snapshotChain = std::make_shared<LogicalSliceChainImpl>();
    std::unordered_map<SliceAddressRef, DataSliceRef, SliceAddressHash, SliceAddressEqual> copiedDataSlice;
    ASSERT_EQ(snapshotChain->InitializeSnapshot(snapshotView, copiedDataSlice, false), BSS_OK);
    SliceAddressRef copiedSliceAddress = snapshotChain->GetSliceAddress(NO_0);
    ASSERT_NE(copiedSliceAddress, nullptr);
    ASSERT_NE(copiedSliceAddress->GetDataSlice(), nullptr);
    EXPECT_NE(copiedSliceAddress->GetDataSlice(), weakDataSlice.lock());
    EXPECT_EQ(copiedSliceAddress->GetSlice(), sourceSlice);
    EXPECT_EQ(copiedSliceAddress->GetDataLen(), expectedDataLen);
    EXPECT_EQ(copiedSliceAddress->GetCheckSum(), expectedCheckSum);
    EXPECT_EQ(copiedSliceAddress->GetSliceStatus(), SliceStatus::NORMAL);
}

/**
 * @tc.name  : LogicalSliceChain_InitializeSnapshot_ShouldSucceedWhenSourceIsReleasedAfterCapture
 * @tc.number: InitializeSnapshot_Test_001
 * @tc.desc  : Test release waits for capture and cannot invalidate the subsequent
 * lock-free deep copy
 */
TEST_F(TestSliceTableBrCov, LogicalSliceChain_InitializeSnapshot_ShouldSucceedWhenSourceIsReleasedAfterCapture)
{
    auto logicalSliceChain = std::dynamic_pointer_cast<LogicalSliceChainImpl>(mockAddSlice());
    ASSERT_NE(logicalSliceChain, nullptr);
    SliceAddressRef sourceSliceAddress = logicalSliceChain->GetSliceAddress(NO_0);
    ASSERT_NE(sourceSliceAddress, nullptr);

    std::mutex mutex;
    std::condition_variable condition;
    bool captured = false;
    bool released = false;
    bool flushStatusUpdated = false;
    bool evictStatusUpdated = false;
    BResult captureResult = BSS_ERR;
    BResult initializeResult = BSS_ERR;
    LogicalSliceChainSnapshotView snapshotView;
    auto snapshotChain = std::make_shared<LogicalSliceChainImpl>();
    std::unordered_map<SliceAddressRef, DataSliceRef, SliceAddressHash, SliceAddressEqual> copiedDataSlice;

    std::thread snapshotThread([&]() {
        captureResult = logicalSliceChain->CaptureSnapshotView(snapshotView);
        {
            std::lock_guard<std::mutex> lock(mutex);
            captured = true;
        }
        condition.notify_all();
        {
            std::unique_lock<std::mutex> lock(mutex);
            condition.wait(lock, [&]() { return released; });
        }
        if (captureResult == BSS_OK) {
            initializeResult = snapshotChain->InitializeSnapshot(snapshotView, copiedDataSlice, false);
        }
    });
    std::thread releaseThread([&]() {
        {
            std::unique_lock<std::mutex> lock(mutex);
            condition.wait(lock, [&]() { return captured; });
        }
        flushStatusUpdated = sourceSliceAddress->SetStatus(SliceEvent::FLUSH);
        evictStatusUpdated = sourceSliceAddress->SetStatus(SliceEvent::EVICT);
        logicalSliceChain->ReleaseSliceAddress(sourceSliceAddress);
        sourceSliceAddress.reset();
        {
            std::lock_guard<std::mutex> lock(mutex);
            released = true;
        }
        condition.notify_all();
    });

    snapshotThread.join();
    releaseThread.join();
    EXPECT_TRUE(flushStatusUpdated);
    EXPECT_TRUE(evictStatusUpdated);
    EXPECT_EQ(captureResult, BSS_OK);
    EXPECT_EQ(initializeResult, BSS_OK);
    EXPECT_EQ(logicalSliceChain->GetSliceAddress(NO_0), SliceAddress::mInvalidAddress);
    SliceAddressRef copiedSliceAddress = snapshotChain->GetSliceAddress(NO_0);
    ASSERT_NE(copiedSliceAddress, nullptr);
    EXPECT_NE(copiedSliceAddress->GetDataSlice(), nullptr);
    EXPECT_EQ(copiedSliceAddress->GetSliceStatus(), SliceStatus::NORMAL);
}

/**
 * @tc.name  : LogicalSliceChain_SnapshotMeta_ShouldLogError_WhenSliceAddressesSizeIsInvalid
 * @tc.number: SnapshotMeta_Test_002
 * @tc.desc  : 测试当mSliceAddresses的大小小于mChainEndIndex + 1时，SnapshotMeta函数记录错误日志并返回
 */
TEST_F(TestSliceTableBrCov, LogicalSliceChain_SnapshotMeta_ShouldLogError_WhenSliceAddressesSizeIsInvalid)
{
    mLogicalSliceChain->SetSliceChainTailIndex(0);
    mLogicalSliceChain->ClearSliceAddresses();
    SnapshotMetaRef snapshotMeta = nullptr;
    mLogicalSliceChain->SnapshotMeta(1, mOutputView, snapshotMeta);
}

/**
 * @tc.name  : LogicalSliceChain_RestoreFilePage_ShouldDoNothing_WhenFilePageIsEmpty
 * @tc.number: 001
 * @tc.desc  : Test the RestoreFilePage function when mFilePage is empty.
 */
TEST_F(TestSliceTableBrCov, LogicalSliceChain_RestoreFilePage_ShouldDoNothing_WhenFilePageIsEmpty)
{
    LsmStoreRef lsmStore = nullptr;
    std::vector<FilePageRef> emptyFilePages;
    mLogicalSliceChain->SetFilePages(emptyFilePages);
    mLogicalSliceChain->RestoreFilePage(lsmStore);
    std::vector<FilePageRef> filePages;
    mLogicalSliceChain->GetFilePages(filePages);
    EXPECT_TRUE(filePages.empty());
}

/**
 * @tc.name  : LogicalSliceChain_RestoreFilePage_ShouldRestoreFilePage_WhenFilePageIsNotEmpty
 * @tc.number: 002
 * @tc.desc  : Test the RestoreFilePage function when mFilePage is not empty.
 */
TEST_F(TestSliceTableBrCov, LogicalSliceChain_RestoreFilePage_ShouldRestoreFilePage_WhenFilePageIsNotEmpty)
{
    std::shared_ptr<FileStoreID> fileStoreId = std::make_shared<FileStoreID>();
    std::shared_ptr<MemManager> memManager = std::make_shared<MemManager>(AllocatorType::DIRECT);
    std::shared_ptr<Config> config = std::make_shared<Config>();
    std::shared_ptr<StateIdProvider> stateIdProvider = std::make_shared<StateIdProvider>(1, 1, memManager);
    std::shared_ptr<FileFactory> tableFactory = std::make_shared<FileFactory>(config, memManager);
    std::shared_ptr<FileCacheManager> fileCache = std::make_shared<FileCacheManager>();
    KeyGroupUtilRef codec;
    ASSERT_EQ(KeyGroupUtil::Create(NO_128, codec), BSS_OK);
    std::shared_ptr<StateFilterManager> stateFilterManager = std::make_shared<StateFilterManager>(stateIdProvider,
                                                                                                  config, 1, 1, codec);
    LsmStoreRef lsmStore = std::make_shared<LsmStore>(fileStoreId, config, tableFactory, fileCache, stateFilterManager,
                                                      memManager);

    mLogicalSliceChain->InsertFilePage(std::make_shared<FilePage>(lsmStore));
    mLogicalSliceChain->RestoreFilePage(lsmStore);
    std::vector<FilePageRef> filePages;
    mLogicalSliceChain->GetFilePages(filePages);
    EXPECT_FALSE(filePages.empty());
    EXPECT_EQ(filePages.size(), NO_1);
    EXPECT_NE(filePages[0], nullptr);
}

/**
 * @tc.name  : SliceAddress_Init_ShouldReturnBSS_ERR_WhenDataSliceIsNullptr
 * @tc.number: Init_Test_001
 * @tc.desc  : Test the Init function when dataSlice is nullptr
 */
TEST_F(TestSliceTableBrCov, SliceAddress_Init_ShouldReturnBSS_ERR_WhenDataSliceIsNullptr)
{
    SliceAddress sliceAddress;
    DataSliceRef dataSlice = nullptr;
    uint64_t accessNumber = NO_100;
    sliceAddress.Init(dataSlice, accessNumber);
}

/**
 * @tc.name  : Slice_Get_ShouldSetKeyStartOffsetToZero_WhenIndexIsZero
 * @tc.number: Get_Test_001
 * @tc.desc  : 测试当index为0时，键的起始偏移量是否正确设置为0
 */
TEST_F(TestSliceTableBrCov, Slice_Get_ShouldSetKeyStartOffsetToZero_WhenIndexIsZero)
{
    ByteBufferRef keyBuffer = MakeRef<ByteBuffer>(NO_1, MemoryType::SLICE_TABLE, mSliceTable->mMemManager);
    KeySpaceRef keySpace = std::make_shared<KeySpace>(keyBuffer, NO_0, NO_1);
    uint32_t index = 0;
    uint32_t mixHashCode = 12345;
    SliceKey sliceKey;
    keySpace->mKeyOffsets[0] = NO_0;
    keySpace->mKeyDataBaseOffset = 0;
    keySpace->Get(index, mixHashCode, sliceKey);
    EXPECT_EQ(keySpace->mKeyOffsets[0], 0);
    keyBuffer.Release();
}

/**
 * @tc.name  : Slice_Get_ShouldSetKeyStartOffsetToPreviousOffset_WhenIndexIsNotZero
 * @tc.number: Get_Test_002
 * @tc.desc  : 测试当index不为0时，键的起始偏移量是否正确设置为mKeyOffsets[index - 1]
 */
TEST_F(TestSliceTableBrCov, Slice_Get_ShouldSetKeyStartOffsetToPreviousOffset_WhenIndexIsNotZero)
{
    ByteBufferRef keyBuffer = MakeRef<ByteBuffer>(NO_1, MemoryType::SLICE_TABLE, mSliceTable->mMemManager);
    KeySpaceRef keySpace = std::make_shared<KeySpace>(keyBuffer, NO_0, NO_1);
    uint32_t index = 1;
    uint32_t mixHashCode = 12345;
    SliceKey sliceKey;
    keySpace->mKeyOffsets[0] = NO_10;
    keySpace->mKeyOffsets[1] = NO_20;
    keySpace->mKeyDataBaseOffset = 0;
    keySpace->Get(index, mixHashCode, sliceKey);
    EXPECT_EQ(keySpace->mKeyOffsets[index - 1], NO_10);
    keyBuffer.Release();
}

TEST_F(TestSliceTableBrCov, SliceAddress_SnapshotMeta_ShouldPersistCompressionMeta)
{
    uint32_t rawLen = IO_SIZE_1K;
    uint32_t storedLen = NO_512;
    uint32_t checkSum = 7;
    uint64_t startOffset = NO_4096;
    uint64_t sliceId = 42;
    std::string localAddress = "compressed.slice";
    SliceAddressRef sliceAddress = std::make_shared<SliceAddress>(rawLen, checkSum, 0, startOffset, sliceId);
    sliceAddress->SetLocalAddress(localAddress);
    sliceAddress->SetSnapshotCompressionMeta(storedLen, CompressAlgo::LZ4);
    SnapshotMetaRef snapshotMeta = std::make_shared<SnapshotMeta>();

    sliceAddress->SnapshotMeta(mOutputView, snapshotMeta);
    mInputView->Seek(0);

    SliceAddressRef restored = nullptr;
    ASSERT_EQ(SliceAddress::Restore(mInputView, restored), BSS_OK);
    ASSERT_NE(restored, nullptr);
    ASSERT_EQ(restored->GetDataLen(), rawLen);
    ASSERT_EQ(restored->GetStoredDataLen(), storedLen);
    ASSERT_EQ(restored->GetCompressAlgo(), CompressAlgo::LZ4);
    ASSERT_EQ(restored->GetCheckSum(), checkSum);
    ASSERT_EQ(restored->GetStartOffset(), startOffset);
    ASSERT_EQ(restored->GetSliceId(), sliceId);
    ASSERT_EQ(restored->GetLocalAddress(), localAddress);
}

/**
 * @tc.name  : SliceAddress_Restore_ShouldReturnError_WhenReadSliceLenFails
 * @tc.number: Restore_Test_002
 * @tc.desc  : Test the Restore function when reading sliceLen fails
 */
TEST_F(TestSliceTableBrCov, SliceAddress_Restore_ShouldReturnError_WhenReadSliceLenFails)
{
    SliceAddressRef sliceAddress = std::make_shared<SliceAddress>(NO_0);
    BResult result = sliceAddress->Restore(mInputView, sliceAddress);
    EXPECT_NE(result, BSS_OK);
    sliceAddress.reset();
}

/**
 * @tc.name  : SliceAddress_Restore_ShouldReturnError_WhenReadCheckSumFails
 * @tc.number: Restore_Test_003
 * @tc.desc  : Test the Restore function when reading checkSum fails
 */
TEST_F(TestSliceTableBrCov, SliceAddress_Restore_ShouldReturnError_WhenReadCheckSumFails)
{
    uint32_t sliceLen = 1;
    uint32_t storedLen = 1;
    CompressAlgo compressAlgo = CompressAlgo::NONE;
    mOutputView->WriteUint32(sliceLen);
    mOutputView->WriteUint32(storedLen);
    mOutputView->WriteUint8(compressAlgo);
    SliceAddressRef sliceAddress = std::make_shared<SliceAddress>(NO_0);
    mInputView->Seek(0);
    BResult result = sliceAddress->Restore(mInputView, sliceAddress);
    EXPECT_NE(result, BSS_OK);
    sliceAddress.reset();
}

/**
 * @tc.name  : SliceAddress_Restore_ShouldReturnError_WhenReadLocalAddressFails
 * @tc.number: Restore_Test_004
 * @tc.desc  : Test the Restore function when reading localAddress fails
 */
TEST_F(TestSliceTableBrCov, SliceAddress_Restore_ShouldReturnError_WhenReadLocalAddressFails)
{
    uint32_t sliceLen = 1;
    uint32_t storedLen = 1;
    CompressAlgo compressAlgo = CompressAlgo::NONE;
    uint32_t checkSum = 1;
    mOutputView->WriteUint32(sliceLen);
    mOutputView->WriteUint32(storedLen);
    mOutputView->WriteUint8(compressAlgo);
    mOutputView->WriteUint32(checkSum);
    SliceAddressRef sliceAddress = std::make_shared<SliceAddress>(NO_0);
    mInputView->Seek(0);
    BResult result = sliceAddress->Restore(mInputView, sliceAddress);
    EXPECT_NE(result, BSS_OK);
    sliceAddress.reset();
}

/**
 * @tc.name  : SliceAddress_Restore_ShouldReturnError_WhenReadStartOffsetFails
 * @tc.number: Restore_Test_005
 * @tc.desc  : Test the Restore function when reading startOffset fails
 */
TEST_F(TestSliceTableBrCov, SliceAddress_Restore_ShouldReturnError_WhenReadStartOffsetFails)
{
    uint32_t sliceLen = 1;
    uint32_t storedLen = 1;
    CompressAlgo compressAlgo = CompressAlgo::NONE;
    uint32_t checkSum = 1;
    std::string localAddress = "12345";
    mOutputView->WriteUint32(sliceLen);
    mOutputView->WriteUint32(storedLen);
    mOutputView->WriteUint8(compressAlgo);
    mOutputView->WriteUint32(checkSum);
    mOutputView->WriteUTF(localAddress);
    SliceAddressRef sliceAddress = std::make_shared<SliceAddress>(NO_0);
    mInputView->Seek(0);
    BResult result = sliceAddress->Restore(mInputView, sliceAddress);
    EXPECT_NE(result, BSS_OK);
    sliceAddress.reset();
}

/**
 * @tc.name  : SliceAddress_Restore_ShouldReturnError_WhenReadSliceIdFails
 * @tc.number: Restore_Test_006
 * @tc.desc  : Test the Restore function when reading sliceId fails
 */
TEST_F(TestSliceTableBrCov, SliceAddress_Restore_ShouldReturnError_WhenReadSliceIdFails)
{
    uint32_t sliceLen = 1;
    uint32_t storedLen = 1;
    CompressAlgo compressAlgo = CompressAlgo::NONE;
    uint32_t checkSum = 1;
    std::string localAddress = "12345";
    uint64_t startOffset = 1;
    mOutputView->WriteUint32(sliceLen);
    mOutputView->WriteUint32(storedLen);
    mOutputView->WriteUint8(compressAlgo);
    mOutputView->WriteUint32(checkSum);
    mOutputView->WriteUTF(localAddress);
    mOutputView->WriteUint64(startOffset);
    SliceAddressRef sliceAddress = std::make_shared<SliceAddress>(NO_0);
    mInputView->Seek(0);
    BResult result = sliceAddress->Restore(mInputView, sliceAddress);
    EXPECT_NE(result, BSS_OK);
    sliceAddress.reset();
}

/**
 * @tc.name  : LogicalSliceChain_Restore_ShouldReturnError_WhenReadChainEndIndexFails
 * @tc.number: LogicalSliceChainTest_002
 * @tc.desc  : 测试当读取链的结束索引失败时，Restore函数应返回错误。
 */
TEST_F(TestSliceTableBrCov, LogicalSliceChain_Restore_ShouldReturnError_WhenReadChainEndIndexFails)
{
    BResult result = mLogicalSliceChain->Restore(mInputView, 0);
    EXPECT_NE(result, BSS_OK);
}

/**
 * @tc.name  : LogicalSliceChain_Restore_ShouldReturnError_WhenReadBaseSliceIndexFails
 * @tc.number: LogicalSliceChainTest_003
 * @tc.desc  : 测试当读取基础切片索引失败时，Restore函数应返回错误。
 */
TEST_F(TestSliceTableBrCov, LogicalSliceChain_Restore_ShouldReturnError_WhenReadBaseSliceIndexFails)
{
    mOutputView->WriteInt16(NO_1);
    mInputView->Seek(0);
    BResult result = mLogicalSliceChain->Restore(mInputView, 0);
    EXPECT_NE(result, BSS_OK);
}

/**
 * @tc.name  : LogicalSliceChain_Restore_ShouldReturnError_WhenReadEvictedSliceCntAfterBaseFails
 * @tc.number: LogicalSliceChainTest_004
 * @tc.desc  : 测试当读取淘汰的切片数量失败时，Restore函数应返回错误。
 */
TEST_F(TestSliceTableBrCov, LogicalSliceChain_Restore_ShouldReturnError_WhenReadEvictedSliceCntAfterBaseFails)
{
    mOutputView->WriteInt32(NO_1);
    mOutputView->WriteUint32(NO_1);
    mInputView->Seek(0);
    BResult result = mLogicalSliceChain->Restore(mInputView, 0);
    EXPECT_NE(result, BSS_OK);
}

/**
 * @tc.name  : LogicalSliceChain_Restore_ShouldReturnError_WhenReadIsFilePageNotEmptyFails
 * @tc.number: LogicalSliceChainTest_005
 * @tc.desc  : 测试当读取文件页面是否为空失败时，Restore函数应返回错误。
 */
TEST_F(TestSliceTableBrCov, LogicalSliceChain_Restore_ShouldReturnError_WhenReadIsFilePageNotEmptyFails)
{
    mOutputView->WriteInt32(NO_1);
    mOutputView->WriteUint32(NO_1);
    mOutputView->WriteUint16(NO_1);
    mInputView->Seek(0);
    BResult result = mLogicalSliceChain->Restore(mInputView, 0);
    EXPECT_NE(result, BSS_OK);
}

/**
 * @tc.name  : LogicalSliceChain_Restore_ShouldResizeFilePage_WhenIsFilePageNotEmptyReadFilePageLenFails
 * @tc.number: LogicalSliceChainTest_006
 * @tc.desc  : 测试当文件页面不为空时，Restore函数应调整mFilePage的大小。
 */
TEST_F(TestSliceTableBrCov, LogicalSliceChain_Restore_ShouldResizeFilePage_WhenIsFilePageNotEmptyReadFilePageLenFails)
{
    mOutputView->WriteInt32(NO_1);
    mOutputView->WriteUint32(NO_1);
    mOutputView->WriteUint16(NO_1);
    mOutputView->WriteUint8(NO_1);
    mInputView->Seek(0);
    BResult result = mLogicalSliceChain->Restore(mInputView, 0);
    EXPECT_NE(result, BSS_OK);
}

/**
 * @tc.name  : Compaction_TryCompaction_ShouldLogErrorAndReturn_WhenLogicalSliceChainIsNull
 * @tc.number: TryCompaction_Test_001
 * @tc.desc  : Test that the function logs an error and returns when logicalSliceChain is null.
 */
TEST_F(TestSliceTableBrCov, Compaction_TryCompaction_ShouldLogErrorAndReturn_WhenLogicalSliceChainIsNull)
{
    CompactCompletedNotify notify = nullptr;
    mSliceTable->mSliceBucketIndex->mMappingTable[0] = nullptr;
    mSliceTable->mCompactManager->mSliceCompactor->TryCompaction(0, notify, nullptr);
}

/**
 * @tc.name  : Compaction_TryCompaction_ShouldLogErrorAndSetStatusToNormal_WhenLogicalSliceChainIsNone
 * @tc.number: TryCompaction_Test_002
 * @tc.desc  : Test that the function logs an error and sets the status to NORMAL when logicalSliceChain is none.
 */
TEST_F(TestSliceTableBrCov, Compaction_TryCompaction_ShouldLogErrorAndSetStatusToNormal_WhenLogicalSliceChainIsNone)
{
    CompactCompletedNotify notify = nullptr;
    mLogicalSliceChain->NotifyNoneFlag(true);
    mLogicalSliceChain->SetSliceStatus(SliceStatus::COMPACTING);
    mSliceTable->mCompactManager->mSliceCompactor->TryCompaction(0, notify, nullptr);
    EXPECT_EQ(mLogicalSliceChain->GetSliceStatus(), SliceStatus::NORMAL);
}

/**
 * @tc.name  : Compaction_TryCompaction_ShouldLogInfoAndSetStatusToNormal_WhenNoSliceIsSelected
 * @tc.number: TryCompaction_Test_003
 * @tc.desc  : Test that the function logs an info message and sets the status to NORMAL when no slice is selected.
 */
TEST_F(TestSliceTableBrCov, Compaction_TryCompaction_ShouldLogInfoAndSetStatusToNormal_WhenNoSliceIsSelected)
{
    CompactCompletedNotify notify = nullptr;
    auto logicalSliceChain = mockAddSlice();
    logicalSliceChain->SetSliceStatus(SliceStatus::COMPACTING);
    mSliceTable->mCompactManager->mSliceCompactor->TryCompaction(0, notify, nullptr);
    EXPECT_EQ(logicalSliceChain->GetSliceStatus(), SliceStatus::NORMAL);
}

/**
 * @tc.name  : Compaction_DoCompaction_ShouldReturnInnerErr_WhenLogicSliceChainNotMatched
 * @tc.number: DoCompaction_Test_001
 * @tc.desc  : Test that DoCompaction returns BSS_INNER_ERR when the logical slice chain does not match.
 */
TEST_F(TestSliceTableBrCov, Compaction_DoCompaction_ShouldReturnInnerErr_WhenLogicSliceChainNotMatched)
{
    mockAddSlice();
    mSliceIndexContext->mSliceIndexSlot = 0;
    BResult result = mSliceTable->mCompactManager->mSliceCompactor->DoCompaction(mSliceIndexContext, nullptr, nullptr,
                                                                                 nullptr);
    EXPECT_EQ(result, BSS_INNER_ERR);
}

/**
 * @tc.name  : Compaction_DoCompaction_ShouldCallCompactCompletedNotify_WhenDoCompactSliceSucceeds
 * @tc.number: DoCompaction_Test_002
 * @tc.desc  : Test that DoCompaction calls compactCompletedNotify when DoCompactSlice succeeds.
 */
TEST_F(TestSliceTableBrCov, Compaction_DoCompaction_ShouldCallCompactCompletedNotify_WhenDoCompactSliceSucceeds)
{
    LogicalSliceChainRef logicalSliceChain = mockAddSlice();
    mSliceIndexContext = mSliceTable->mSliceBucketIndex->GetSliceIndexContext(0);
    std::shared_ptr<SelectedSliceContextBuild> choiceBuild = std::make_shared<SelectedSliceContextBuild>();
    SliceAddressRef sliceAddress = logicalSliceChain->GetSliceAddress(0);
    DataSliceRef dataSlice = sliceAddress->GetDataSlice();
    choiceBuild->Add(sliceAddress, dataSlice);
    choiceBuild->SetChainIndex(0);
    auto selectedSliceContext = choiceBuild->Build();
    bool notifyCalled = false;
    BResult result = mSliceTable->mCompactManager->mSliceCompactor->DoCompaction(
        mSliceIndexContext, selectedSliceContext,
        [&notifyCalled](LogicalSliceChainRef logicalSliceChain, uint32_t bucketIndex,
                        uint32_t compactionStartChainIndex, uint32_t compactionEndChainIndex,
                        DataSliceRef compactedDataSlice, std::vector<SliceAddressRef> invalidSliceAddressList,
                        uint32_t finalOldSliceSize, bool fromRestore) { notifyCalled = true; },
        nullptr);
    EXPECT_EQ(result, BSS_OK);
    EXPECT_TRUE(notifyCalled);
}

/**
 * @tc.name  : Compaction_DoCompaction_ShouldReturnNonOk_WhenDoCompactSliceFails
 * @tc.number: DoCompaction_Test_003
 * @tc.desc  : Test that DoCompaction returns a non-BSS_OK result when DoCompactSlice fails.
 */
TEST_F(TestSliceTableBrCov, Compaction_DoCompaction_ShouldReturnNonOk_WhenDoCompactSliceFails)
{
    LogicalSliceChainRef logicalSliceChain = mockAddSlice();
    mSliceIndexContext = mSliceTable->mSliceBucketIndex->GetSliceIndexContext(0);
    std::shared_ptr<SelectedSliceContextBuild> choiceBuild = std::make_shared<SelectedSliceContextBuild>();
    SliceAddressRef sliceAddress = logicalSliceChain->GetSliceAddress(0);
    DataSliceRef dataSlice = sliceAddress->GetDataSlice();
    choiceBuild->Add(sliceAddress, dataSlice);
    choiceBuild->SetChainIndex(0);
    auto selectedSliceContext = choiceBuild->Build();
    selectedSliceContext->mCanCompactSliceListReversed.clear();
    bool notifyCalled = false;
    BResult result = mSliceTable->mCompactManager->mSliceCompactor->DoCompaction(
        mSliceIndexContext, selectedSliceContext,
        [&notifyCalled](LogicalSliceChainRef logicalSliceChain, uint32_t bucketIndex,
                        uint32_t compactionStartChainIndex, uint32_t compactionEndChainIndex,
                        DataSliceRef compactedDataSlice, std::vector<SliceAddressRef> invalidSliceAddressList,
                        uint32_t finalOldSliceSize, bool fromRestore) { notifyCalled = true; },
        nullptr);
    EXPECT_EQ(result, BSS_OK);
}

/**
 * @tc.name  : DoCompactSlice_ShouldReturnError_WhenInitializeSliceFails
 * @tc.number: DoCompactSlice_Test_003
 * @tc.desc  : Test that DoCompactSlice returns an error code when slice->Initialize fails.
 */
TEST_F(TestSliceTableBrCov, Compaction_DoCompactSlice_ShouldReturnError_WhenInitializeSliceFails)
{
    LogicalSliceChainRef logicalSliceChain = mockAddSlice();
    mSliceTable->mMemManager->mMemoryLimit = NO_5;
    std::vector<DataSliceRef> canCompactSliceListReversed = { logicalSliceChain->GetSliceAddress(0)->mDataSlice };
    DataSliceRef compactedDataSlice;
    BResult result = mSliceTable->mCompactManager->mSliceCompactor->DoCompactSlice(canCompactSliceListReversed,
                                                                                   compactedDataSlice, false, 0, false);
    EXPECT_NE(result, BSS_OK);
}

/**
 * @tc.name  : Compaction_DoSyncReplaceLogicalSlice_ShouldReturnInvalidParam_WhenLogicalSliceChainNotMatched
 * @tc.number: DoSyncReplaceLogicalSlice_Test_001
 * @tc.desc  : Test case to verify that the function returns BSS_INVALID_PARAM when the logical slice chain does not
 * match.
 */
TEST_F(TestSliceTableBrCov,
       Compaction_DoSyncReplaceLogicalSlice_ShouldReturnInvalidParam_WhenLogicalSliceChainNotMatched)
{
    uint32_t sliceIndexSlot = 0;
    uint32_t compactionStartChainIndex = 0;
    uint32_t compactionEndChainIndex = 1;
    std::vector<SliceAddressRef> invalidSliceAddressList;
    ReplaceLogicalSliceRef replaceLogicalSlice = std::make_shared<ReplaceLogicalSlice>(mSliceTable,
                                                                                       mSliceTable->mConfig);
    DataSliceRef compactedDataSlice = nullptr;
    mLogicalSliceChain = mSliceTable->mSliceBucketIndex->CreateLogicalChainedSlice();
    BResult result = replaceLogicalSlice->SyncReplaceChainAndSlice(mLogicalSliceChain, sliceIndexSlot,
                                                                   compactionStartChainIndex, compactionEndChainIndex,
                                                                   compactedDataSlice, invalidSliceAddressList);
    EXPECT_EQ(result, BSS_INVALID_PARAM);
}

/**
 * @tc.name  : Compaction_DoSyncReplaceLogicalSlice_ShouldReturnOk_WhenBaseSliceIndexGreaterThanCompactionStart
 * @tc.number: DoSyncReplaceLogicalSlice_Test_002
 * @tc.desc  : Test case to verify that the function returns BSS_OK when the base slice index is greater than
 * the compaction start index.
 */
TEST_F(TestSliceTableBrCov,
       Compaction_DoSyncReplaceLogicalSlice_ShouldReturnOk_WhenBaseSliceIndexGreaterThanCompactionStart)
{
    uint32_t sliceIndexSlot = 0;
    uint32_t compactionStartChainIndex = 0;
    uint32_t compactionEndChainIndex = 1;
    std::vector<SliceAddressRef> invalidSliceAddressList;
    ReplaceLogicalSliceRef replaceLogicalSlice = std::make_shared<ReplaceLogicalSlice>(mSliceTable,
                                                                                       mSliceTable->mConfig);
    DataSliceRef compactedDataSlice = nullptr;
    LogicalSliceChainRef logicalSliceChain = mockAddSlice();
    logicalSliceChain->SetBaseSliceIndex(NO_1);
    BResult result = replaceLogicalSlice->SyncReplaceChainAndSlice(logicalSliceChain, sliceIndexSlot,
                                                                   compactionStartChainIndex, compactionEndChainIndex,
                                                                   compactedDataSlice, invalidSliceAddressList);
    EXPECT_EQ(result, BSS_OK);
}

/**
 * @tc.name  : Compaction_DoSyncReplaceLogicalSlice_ShouldReturnInnerErr_WhenBaseSliceAddressIsNull
 * @tc.number: DoSyncReplaceLogicalSlice_Test_003
 * @tc.desc  : Test case to verify that the function returns BSS_INNER_ERR when the base slice address is null.
 */
TEST_F(TestSliceTableBrCov, Compaction_DoSyncReplaceLogicalSlice_ShouldReturnInnerErr_WhenBaseSliceAddressIsNull)
{
    uint32_t sliceIndexSlot = 0;
    uint32_t compactionStartChainIndex = 2;
    uint32_t compactionEndChainIndex = 3;
    std::vector<SliceAddressRef> invalidSliceAddressList;
    ReplaceLogicalSliceRef replaceLogicalSlice = std::make_shared<ReplaceLogicalSlice>(mSliceTable,
                                                                                       mSliceTable->mConfig);
    DataSliceRef compactedDataSlice = nullptr;
    LogicalSliceChainRef logicalSliceChain = mockAddSlice();
    logicalSliceChain->SetSliceAddress(0, nullptr);
    BResult result = replaceLogicalSlice->SyncReplaceChainAndSlice(logicalSliceChain, sliceIndexSlot,
                                                                   compactionStartChainIndex, compactionEndChainIndex,
                                                                   compactedDataSlice, invalidSliceAddressList);

    EXPECT_EQ(result, BSS_INNER_ERR);
}

/**
 * @tc.name  : Compaction_DoSyncReplaceLogicalSlice_ShouldReturnInvalidParam_WhenSliceAddressIsNull
 * @tc.number: DoSyncReplaceLogicalSlice_Test_005
 * @tc.desc  : Test case to verify that the function returns BSS_INVALID_PARAM when a slice address is null.
 */
TEST_F(TestSliceTableBrCov, Compaction_DoSyncReplaceLogicalSlice_ShouldReturnInvalidParam_WhenSliceAddressIsNull)
{
    uint32_t sliceIndexSlot = 0;
    uint32_t compactionStartChainIndex = 2;
    uint32_t compactionEndChainIndex = 3;
    std::vector<SliceAddressRef> invalidSliceAddressList;
    ReplaceLogicalSliceRef replaceLogicalSlice = std::make_shared<ReplaceLogicalSlice>(mSliceTable,
                                                                                       mSliceTable->mConfig);
    DataSliceRef compactedDataSlice = nullptr;
    LogicalSliceChainRef logicalSliceChain = mockAddSlice();
    logicalSliceChain->EmplaceSlice(nullptr);
    BResult result = replaceLogicalSlice->SyncReplaceChainAndSlice(logicalSliceChain, sliceIndexSlot,
                                                                   compactionStartChainIndex, compactionEndChainIndex,
                                                                   compactedDataSlice, invalidSliceAddressList);
    EXPECT_EQ(result, BSS_INVALID_PARAM);
}

/**
 * @tc.name  : Compaction_DoSyncReplaceLogicalSlice_ShouldReturnInnerErr_WhenBaseSliceAddressIsNull
 * @tc.number: DoSyncReplaceLogicalSlice_Test_006
 * @tc.desc  : Test case to verify that the function returns BSS_INNER_ERR when the compactedLogicalSliceChain create
 * slice address is null.
 */
TEST_F(TestSliceTableBrCov, Compaction_DoSyncReplaceLogicalSlice_ShouldReturnInnerErr_WhenSliceAddressIsNull)
{
    uint32_t sliceIndexSlot = 0;
    uint32_t compactionStartChainIndex = 0;
    uint32_t compactionEndChainIndex = 1;
    std::vector<SliceAddressRef> invalidSliceAddressList;
    ReplaceLogicalSliceRef replaceLogicalSlice = std::make_shared<ReplaceLogicalSlice>(mSliceTable,
                                                                                       mSliceTable->mConfig);
    DataSliceRef compactedDataSlice = nullptr;
    LogicalSliceChainRef logicalSliceChain = mockAddSlice();
    BResult result = replaceLogicalSlice->SyncReplaceChainAndSlice(logicalSliceChain, sliceIndexSlot,
                                                                   compactionStartChainIndex, compactionEndChainIndex,
                                                                   compactedDataSlice, invalidSliceAddressList);
    EXPECT_EQ(result, BSS_INNER_ERR);
}

/**
 * @tc.name  : Compaction_Run_ShouldLogError_WhenDoSyncReplaceLogicalSliceFails
 * @tc.number: Run_Test_001
 * @tc.desc  : 测试当DoSyncReplaceLogicalSlice返回非BSS_OK时，Run函数应记录错误日志
 */
TEST_F(TestSliceTableBrCov, Compaction_Run_ShouldLogError_WhenDoSyncReplaceLogicalSliceFails)
{
    ReplaceLogicalSliceRef replaceLogicalSlice = std::make_shared<ReplaceLogicalSlice>(mSliceTable,
                                                                                       mSliceTable->mConfig);
    DataSliceRef compactedDataSlice = nullptr;
    std::vector<SliceAddressRef> invalidSliceAddressList;
    mLogicalSliceChain->SetSliceStatus(SliceStatus::COMPACTING);
    RunnablePtr processor = std::make_shared<ReplaceLogicalSliceTask>(replaceLogicalSlice, mLogicalSliceChain, 0, 0, 1,
                                                                      compactedDataSlice, invalidSliceAddressList);
    mockAddSlice();
    processor->Run();
    EXPECT_EQ(mLogicalSliceChain->GetSliceStatus(), SliceStatus::NORMAL);
}

/**
 * @tc.name  : Compaction_SelectCompactionSlice_ShouldReturnNull_WhenLogicalSliceChainIsNull
 * @tc.number: SelectCompactionSlice_Test_001
 * @tc.desc  : 测试当logicalSliceChain为空时，SelectCompactionSlice应返回nullptr
 */
TEST_F(TestSliceTableBrCov, Compaction_SelectCompactionSlice_ShouldReturnNull_WhenLogicalSliceChainIsNull)
{
    mSliceIndexContext->mLogicalSliceChain = nullptr;
    auto result =
        mSliceTable->mCompactManager->mSliceCompactor->mSliceCompactionPolicy->SelectCompactionSlice(mSliceIndexContext,
                                                                                                     0, 1);
    EXPECT_EQ(result, nullptr);
}

/**
 * @tc.name  : Compaction_SelectCompactionSlice_ShouldReturnNull_WhenLogicalSliceChainNotMatched
 * @tc.number: SelectCompactionSlice_Test_002
 * @tc.desc  : 测试当logicalSliceChain与mBuketIndex中的逻辑链不匹配时，SelectCompactionSlice应返回nullptr
 */
TEST_F(TestSliceTableBrCov, Compaction_SelectCompactionSlice_ShouldReturnNull_WhenLogicalSliceChainNotMatched)
{
    mockAddSlice();
    auto result =
        mSliceTable->mCompactManager->mSliceCompactor->mSliceCompactionPolicy->SelectCompactionSlice(mSliceIndexContext,
                                                                                                     0, 1);
    EXPECT_EQ(result, nullptr);
}

/**
 * @tc.name  : SelectCompactionSlice_ShouldReturnNull_WhenSliceAddressIsNull
 * @tc.number: SelectCompactionSlice_Test_003
 * @tc.desc  : 测试当sliceAddress为空时，SelectCompactionSlice应返回nullptr
 */
TEST_F(TestSliceTableBrCov, Compaction_SelectCompactionSlice_ShouldReturnNull_WhenSliceAddressIsNull)
{
    LogicalSliceChainRef logicalSliceChain = mockAddSlice();
    logicalSliceChain->SetSliceAddress(0, nullptr);
    mSliceIndexContext = mSliceTable->mSliceBucketIndex->GetSliceIndexContext(0);
    auto result =
        mSliceTable->mCompactManager->mSliceCompactor->mSliceCompactionPolicy->SelectCompactionSlice(mSliceIndexContext,
                                                                                                     0, 1);
    EXPECT_EQ(result, nullptr);
}

/**
 * @tc.name  : Compaction_SelectCompactionSlice_ShouldReturnNull_WhenDataSliceIsNullOrTriggerFlush
 * @tc.number: SelectCompactionSlice_Test_004
 * @tc.desc  : 测试当dataSlice为空或sliceAddress处于触发Flush状态时，SelectCompactionSlice应返回nullptr
 */
TEST_F(TestSliceTableBrCov, Compaction_SelectCompactionSlice_ShouldReturnNull_WhenDataSliceIsNullOrTriggerFlush)
{
    LogicalSliceChainRef logicalSliceChain = mockAddSlice();
    auto sliceAddress = logicalSliceChain->GetSliceAddress(0);
    sliceAddress->mDataSlice = nullptr;
    mSliceIndexContext = mSliceTable->mSliceBucketIndex->GetSliceIndexContext(0);
    auto result =
        mSliceTable->mCompactManager->mSliceCompactor->mSliceCompactionPolicy->SelectCompactionSlice(mSliceIndexContext,
                                                                                                     0, 1);
    EXPECT_EQ(result, nullptr);
}

/**
 * @tc.name  : Compaction_AsyncCompactSlice_ShouldReturnInvalidParam_WhenSliceIndexContextIsNull
 * @tc.number: AsyncCompactSlice_Test_001
 * @tc.desc  : Test that AsyncCompactSlice returns BSS_INVALID_PARAM when sliceIndexContext is nullptr.
 */
TEST_F(TestSliceTableBrCov, Compaction_AsyncCompactSlice_ShouldReturnInvalidParam_WhenSliceIndexContextIsNull)
{
    BResult result = mSliceTable->mCompactManager->AsyncCompactSlice(nullptr, nullptr);
    EXPECT_EQ(result, BSS_INVALID_PARAM);
}

/**
 * @tc.name  : Compaction_AsyncCompactSlice_ShouldReturnOk_WhenStatusSetFails
 * @tc.number: AsyncCompactSlice_Test_002
 * @tc.desc  : Test that AsyncCompactSlice returns BSS_OK when status set fails.
 */
TEST_F(TestSliceTableBrCov, Compaction_AsyncCompactSlice_ShouldReturnOk_WhenStatusSetFails)
{
    mSliceIndexContext->mLogicalSliceChain->SetSliceStatus(SliceStatus::NORMAL);
    BResult result = mSliceTable->mCompactManager->AsyncCompactSlice(mSliceIndexContext, nullptr);
    EXPECT_EQ(result, BSS_INVALID_PARAM);
}

/**
 * @tc.name  : Compaction_AsyncCompactSlice_ShouldReturnErr_WhenTaskExecutionFails
 * @tc.number: AsyncCompactSlice_Test_003
 * @tc.desc  : Test that AsyncCompactSlice returns BSS_ERR when task execution fails.
 */
TEST_F(TestSliceTableBrCov, Compaction_AsyncCompactSlice_ShouldReturnErr_WhenTaskExecutionFails)
{
    mSliceIndexContext->mLogicalSliceChain->SetSliceStatus(SliceStatus::NORMAL);
    mSliceTable->mCompactManager->mCompactionEventExecutor->mStarted = false;
    BResult result = mSliceTable->mCompactManager->AsyncCompactSlice(mSliceIndexContext, nullptr);
    EXPECT_EQ(result, BSS_INVALID_PARAM);
}

/**
 * @tc.name  : Binary_Valid_ShouldReturnFalse_WhenDataIsNullptrAndCapacityIsZero
 * @tc.number: Valid_Test_001
 * @tc.desc  : 测试当 mData 为 nullptr 且 mCapacity 为 0 时，Valid 函数应返回 false
 */
TEST_F(TestSliceTableBrCov, Binary_Valid_ShouldReturnFalse_WhenDataIsNullptrAndCapacityIsZero)
{
    ByteBuffer byteBuffer = ByteBuffer();
    byteBuffer.mData = nullptr;
    byteBuffer.mCapacity = NO_0;
    EXPECT_FALSE(byteBuffer.Valid());
}

/**
 * @tc.name  : Binary_Valid_ShouldReturnFalse_WhenDataIsNullptrAndCapacityIsNotZero
 * @tc.number: Valid_Test_002
 * @tc.desc  : 测试当 mData 为 nullptr 且 mCapacity 不为 0 时，Valid 函数应返回 false
 */
TEST_F(TestSliceTableBrCov, Binary_Valid_ShouldReturnFalse_WhenDataIsNullptrAndCapacityIsNotZero)
{
    ByteBuffer byteBuffer = ByteBuffer();
    byteBuffer.mData = nullptr;
    byteBuffer.mCapacity = NO_1;
    EXPECT_FALSE(byteBuffer.Valid());
}

/**
 * @tc.name  : Binary_Valid_ShouldReturnFalse_WhenDataIsNotNullptrAndCapacityIsZero
 * @tc.number: Valid_Test_003
 * @tc.desc  : 测试当 mData 不为 nullptr 且 mCapacity 为 0 时，Valid 函数应返回 false
 */
TEST_F(TestSliceTableBrCov, Binary_Valid_ShouldReturnFalse_WhenDataIsNotNullptrAndCapacityIsZero)
{
    ByteBuffer byteBuffer = ByteBuffer();
    byteBuffer.mData = (uint8_t *)malloc(NO_10);
    byteBuffer.mCapacity = NO_0;
    EXPECT_FALSE(byteBuffer.Valid());
}

/**
 * @tc.name  : Binary_Valid_ShouldReturnTrue_WhenDataIsNotNullptrAndCapacityIsNotZero
 * @tc.number: Valid_Test_004
 * @tc.desc  : 测试当 mData 不为 nullptr 且 mCapacity 不为 0 时，Valid 函数应返回 true
 */
TEST_F(TestSliceTableBrCov, Binary_Valid_ShouldReturnTrue_WhenDataIsNotNullptrAndCapacityIsNotZero)
{
    ByteBuffer byteBuffer = ByteBuffer();
    byteBuffer.mData = (uint8_t *)malloc(NO_10);
    byteBuffer.mCapacity = NO_1;
    EXPECT_TRUE(byteBuffer.Valid());
}

/**
 * @tc.name  : Binary_WriteAt_ShouldReturnInvalidParam_WhenBufIsNull
 * @tc.number: WriteAt_Test_001
 * @tc.desc  : Test that WriteAt returns BSS_INVALID_PARAM when buf is nullptr.
 */
TEST_F(TestSliceTableBrCov, Binary_WriteAt_ShouldReturnInvalidParam_WhenBufIsNull)
{
    ByteBuffer byteBuffer = ByteBuffer();
    byteBuffer.mData = (uint8_t *)malloc(NO_10);
    byteBuffer.mCapacity = NO_1;
    BResult result = byteBuffer.WriteAt(nullptr, 1, 0);
    EXPECT_EQ(result, BSS_INVALID_PARAM);
}

/**
 * @tc.name  : Binary_WriteAt_ShouldReturnInvalidParam_WhenLenIsZero
 * @tc.number: WriteAt_Test_002
 * @tc.desc  : Test that WriteAt returns BSS_INVALID_PARAM when len is 0.
 */
TEST_F(TestSliceTableBrCov, Binary_WriteAt_ShouldReturnInvalidParam_WhenLenIsZero)
{
    ByteBuffer byteBuffer = ByteBuffer();
    byteBuffer.mData = (uint8_t *)malloc(NO_10);
    byteBuffer.mCapacity = NO_1;
    uint8_t buf[10] = { 0 };
    BResult result = byteBuffer.WriteAt(buf, 0, 0);
    EXPECT_EQ(result, BSS_INVALID_PARAM);
}

/**
 * @tc.name  : Binary_WriteAt_ShouldReturnInvalidParam_WhenPosPlusLenExceedsCapacity
 * @tc.number: WriteAt_Test_003
 * @tc.desc  : Test that WriteAt returns BSS_INVALID_PARAM when pos + len exceeds mCapacity.
 */
TEST_F(TestSliceTableBrCov, Binary_WriteAt_ShouldReturnInvalidParam_WhenPosPlusLenExceedsCapacity)
{
    ByteBuffer byteBuffer = ByteBuffer();
    byteBuffer.mData = (uint8_t *)malloc(NO_10);
    byteBuffer.mCapacity = NO_1;
    uint8_t buf[10] = { 0 };
    BResult result = byteBuffer.WriteAt(buf, NO_10, 0);
    EXPECT_EQ(result, BSS_INVALID_PARAM);
}

/**
 * @tc.name  : Binary_Compare_ShouldReturnNegative_WhenBufIsNull
 * @tc.number: Compare_Test_001
 * @tc.desc  : Test Compare function when buf is null
 */
TEST_F(TestSliceTableBrCov, Binary_Compare_ShouldReturnNegative_WhenBufIsNull)
{
    ByteBufferRef buf = nullptr;
    ByteBufferRef buffer = MakeRef<ByteBuffer>(NO_1, MemoryType::FRESH_TABLE, mSliceTable->mMemManager);
    int32_t result = buffer->Compare(buf, 0, 0, 1, 1);
    EXPECT_EQ(result, -1);
}

/**
 * @tc.name  : Binary_Compare_ShouldReturnNegative_WhenStart1PlusLen1ExceedsCapacity
 * @tc.number: Compare_Test_002
 * @tc.desc  : Test Compare function when start1 + len1 exceeds capacity
 */
TEST_F(TestSliceTableBrCov, Binary_Compare_ShouldReturnNegative_WhenStart1PlusLen1ExceedsCapacity)
{
    ByteBufferRef buffer = MakeRef<ByteBuffer>(NO_1, MemoryType::FRESH_TABLE, mSliceTable->mMemManager);
    ByteBufferRef buffer2 = MakeRef<ByteBuffer>(NO_1, MemoryType::FRESH_TABLE, mSliceTable->mMemManager);
    int32_t result = buffer->Compare(buffer2, 0, 0, NO_10, 1);
    EXPECT_EQ(result, -1);
}

/**
 * @tc.name  : Binary_Compare_ShouldReturnPositive_WhenStart2PlusLen2ExceedsCapacity
 * @tc.number: Compare_Test_003
 * @tc.desc  : Test Compare function when start2 + len2 exceeds buf capacity
 */
TEST_F(TestSliceTableBrCov, Binary_Compare_ShouldReturnPositive_WhenStart2PlusLen2ExceedsCapacity)
{
    ByteBufferRef buffer = MakeRef<ByteBuffer>(NO_10, MemoryType::FRESH_TABLE, mSliceTable->mMemManager);
    ByteBufferRef buffer2 = MakeRef<ByteBuffer>(NO_1, MemoryType::FRESH_TABLE, mSliceTable->mMemManager);
    int32_t result = buffer->Compare(buffer2, 0, 0, NO_10, NO_10);
    EXPECT_EQ(result, 1);
}
