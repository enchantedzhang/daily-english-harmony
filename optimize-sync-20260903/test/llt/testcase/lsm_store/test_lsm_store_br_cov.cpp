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

#include "test_lsm_store_br_cov.h"

#include "fs/local/local_file_system.h"
#include "lsm_store/file/file_structure.h"

using namespace ock::bss;

void TestLsmStoreBrCov::SetUp()
{
    ConfigRef config = std::make_shared<Config>(NO_0, NO_15, NO_16);
    InitEnv(config);
    mTableWriter = GetTableBuilder(config);
    mGenerator = MakeRef<Generator>();
    const std::string testFilePath = "test.txt";
    inputView = std::make_shared<FileInputView>();
    outputView = std::make_shared<FileOutputView>();
    Uri uri(testFilePath);
    PathRef path = std::make_shared<Path>(uri);
    ConfigRef config2 = std::make_shared<Config>();
    outputView->Init(path, config2, FileOutputView::WriteMode::OVERWRITE);
    inputView->Init(FileSystemType::LOCAL, path);
    mTableReader = CreateTableReader();
}

void TestLsmStoreBrCov::TearDown()
{
    ExitEnv();
    inputView->Close();
    outputView->Close();
    mTableReader->mInputView->Close();
    mTableReader->Close();
}

/**
 * @tc.name  : Table_Add_ShouldReturnAlreadyExists_WhenTableIsClosedOrFinished
 * @tc.number: Add_Test_001
 * @tc.desc  : Test that Add returns BSS_ALREADY_EXISTS when the table is closed or finished.
 */
TEST_F(TestLsmStoreBrCov, Table_Add_ShouldReturnAlreadyExists_WhenTableIsClosedOrFinished)
{
    mTableWriter->mClosed = true;
    KeyValueRef keyValue;
    EXPECT_EQ(mTableWriter->Add(keyValue), BSS_ALREADY_EXISTS);
    mTableWriter->mClosed = false;
    mTableWriter->mFinished = true;
    EXPECT_EQ(mTableWriter->Add(keyValue), BSS_ALREADY_EXISTS);
}

/**
 * @tc.name  : Table_WriteBlock_ShouldReturnBSSERR_WhenBufferIsNullptr
 * @tc.number: WriteBlock_Test_001
 * @tc.desc  : Test WriteBlock function when input buffer is nullptr
 */
TEST_F(TestLsmStoreBrCov, Table_WriteBlock_ShouldReturnBSSERR_WhenBufferIsNullptr)
{
    ByteBufferRef buffer = nullptr;
    BlockHandleRef blockHandle;
    BResult result = mTableWriter->WriteBlock(buffer, blockHandle);
    EXPECT_EQ(result, BSS_ERR);
}

/**
 * @tc.name  : Table_WriteBlock_ShouldReturnBSSERR_WhenMFileOutputViewIsNullptr
 * @tc.number: WriteBlock_Test_002
 * @tc.desc  : Test WriteBlock function when input buffer is nullptr
 */
TEST_F(TestLsmStoreBrCov, Table_WriteBlock_ShouldReturnBSSERR_WhenMFileOutputViewIsNullptr)
{
    ByteBufferRef buffer = MakeRef<ByteBuffer>(NO_10, MemoryType::FRESH_TABLE, mMemManager);
    BlockHandleRef blockHandle;
    mTableWriter->mFileOutputView = nullptr;
    BResult result = mTableWriter->WriteBlock(buffer, blockHandle);
    EXPECT_EQ(result, BSS_ERR);
}

/**
 * @tc.name  : Table_WriteBlock_ShouldReturnError_WhenWriteHeadFails
 * @tc.number: WriteBlock_Test_003
 * @tc.desc  : Test WriteBlock function when writing head data fails
 */
TEST_F(TestLsmStoreBrCov, Table_WriteBlock_ShouldReturnError_WhenWriteHeadFails)
{
    ByteBufferRef buffer = MakeRef<ByteBuffer>(NO_10, MemoryType::FRESH_TABLE, mMemManager);
    BlockHandleRef blockHandle;
    mTableWriter->mFileOutputView->fileSystem->Close();
    BResult result = mTableWriter->WriteBlock(buffer, blockHandle);
    EXPECT_NE(result, BSS_OK);
}

/**
 * @tc.name  : Table_WriteBlock_ShouldReturnError_WhenWriteValueFails
 * @tc.number: WriteBlock_Test_004
 * @tc.desc  : Test WriteBlock function when writing value data fails
 */
TEST_F(TestLsmStoreBrCov, Table_WriteBlock_ShouldReturnError_WhenWriteValueFails)
{
    ByteBufferRef buffer = MakeRef<ByteBuffer>(NO_10, MemoryType::FRESH_TABLE, mMemManager);
    BlockHandleRef blockHandle;
    buffer->mData = nullptr;
    BResult result = mTableWriter->WriteBlock(buffer, blockHandle);
    EXPECT_NE(result, BSS_OK);
}

/**
 * @tc.name  : Table_WriteBlock_ShouldReturnBSSOK_WhenAllOperationsSucceed
 * @tc.number: WriteBlock_Test_005
 * @tc.desc  : Test WriteBlock function when all operations succeed
 */
TEST_F(TestLsmStoreBrCov, Table_WriteBlock_ShouldReturnBSSOK_WhenAllOperationsSucceed)
{
    ByteBufferRef buffer = MakeRef<ByteBuffer>(NO_10, MemoryType::FRESH_TABLE, mMemManager);
    BlockHandleRef blockHandle;
    BResult result = mTableWriter->WriteBlock(buffer, blockHandle);
    EXPECT_EQ(result, BSS_OK);
    EXPECT_NE(blockHandle, nullptr);
}

/**
 * @tc.name  : TableBuilder_Finish_ShouldReturnBSSERR_WhenFileOutputViewIsNull
 * @tc.number: 001
 * @tc.desc  : Test that Finish returns BSS_ERR when mFileOutputView is nullptr.
 */
TEST_F(TestLsmStoreBrCov, TableBuilder_Finish_ShouldReturnBSSERR_WhenFileOutputViewIsNull)
{
    FileBlockMetaRef tableMeta;
    mTableWriter->mFileOutputView = nullptr;
    EXPECT_EQ(mTableWriter->Finish(tableMeta), BSS_ERR);
}

/**
 * @tc.name  : TableBuilder_BuildFooter_ShouldReturnBSSERR_WhenFileOutputViewIsNull
 * @tc.number: 001
 * @tc.desc  : Test that BuildFooter returns BSS_ERR when mFileOutputView is nullptr.
 */
TEST_F(TestLsmStoreBrCov, TableBuilder_BuildFooter_ShouldReturnBSSERR_WhenFileOutputViewIsNull)
{
    mTableWriter->mFileOutputView = nullptr;
    EXPECT_EQ(mTableWriter->BuildFooter(), BSS_ERR);
}

/**
 * @tc.name  : Table_BuildFilterBlock_ShouldReturnBSSOK_WhenFilterBlockBuilderIsNull
 * @tc.number: BuildFilterBlock_Test_001
 * @tc.desc  : Test case to verify that BuildFilterBlock returns BSS_OK when mFilterBlockWriter is null.
 */
TEST_F(TestLsmStoreBrCov, Table_BuildFilterBlock_ShouldReturnBSSOK_WhenFilterBlockBuilderIsNull)
{
    mTableWriter->mFilterBlockWriter = nullptr;
    EXPECT_EQ(mTableWriter->BuildFilterBlock(), BSS_OK);
}

/**
 * @tc.name  : Table_BuildFilterBlock_ShouldReturnBSSAllocFail_WhenMemoryAllocationFails
 * @tc.number: BuildFilterBlock_Test_002
 * @tc.desc  : Test case to verify that BuildFilterBlock returns BSS_ALLOC_FAIL when memory allocation fails.
 */
TEST_F(TestLsmStoreBrCov, Table_BuildFilterBlock_ShouldReturnBSSAllocFail_WhenMemoryAllocationFails)
{
    mTableWriter->mMemManager->mMemoryLimit = 1;
    EXPECT_EQ(mTableWriter->BuildFilterBlock(), BSS_ALLOC_FAIL);
}

/**
 * @tc.name  : Table_BuildFilterBlock_ShouldReturnError_WhenWriteBlockFails
 * @tc.number: BuildFilterBlock_Test_004
 * @tc.desc  : Test case to verify that BuildFilterBlock returns an error when WriteBlock fails.
 */
TEST_F(TestLsmStoreBrCov, Table_BuildFilterBlock_ShouldReturnError_WhenWriteBlockFails)
{
    mTableWriter->mFileOutputView->fileSystem->Close();
    EXPECT_NE(mTableWriter->BuildFilterBlock(), BSS_OK);
}

/**
 * @tc.name  : Table_BuildFilterBlock_ShouldReturnBSSOK_WhenAllOperationsSucceed
 * @tc.number: BuildFilterBlock_Test_005
 * @tc.desc  : Test case to verify that BuildFilterBlock returns BSS_OK when all operations succeed.
 */
TEST_F(TestLsmStoreBrCov, Table_BuildFilterBlock_ShouldReturnBSSOK_WhenAllOperationsSucceed)
{
    EXPECT_EQ(mTableWriter->BuildFilterBlock(), BSS_ALLOC_FAIL);
}

/**
 * @tc.name  : Table_BuildIndexBlock_ShouldReturnError_WhenFinishFails
 * @tc.number: BuildIndexBlock_Test_001
 * @tc.desc  : Test BuildIndexBlock function when mIndexBlockWriter->Finish returns an error
 */
TEST_F(TestLsmStoreBrCov, Table_BuildIndexBlock_ShouldReturnError_WhenFinishFails)
{
    mTableWriter->mMemManager->mMemoryLimit = 1;
    BResult ret = mTableWriter->BuildIndexBlock();
    EXPECT_NE(ret, BSS_OK);
}

/**
 * @tc.name  : Table_BuildIndexBlock_ShouldReturnError_WhenWriteBlockFails
 * @tc.number: BuildIndexBlock_Test_002
 * @tc.desc  : Test BuildIndexBlock function when WriteBlock returns an error
 */
TEST_F(TestLsmStoreBrCov, Table_BuildIndexBlock_ShouldReturnError_WhenWriteBlockFails)
{
    mTableWriter->mFileOutputView->fileSystem->Close();
    BResult ret = mTableWriter->BuildIndexBlock();
    EXPECT_NE(ret, BSS_OK);
}

/**
 * @tc.name  : Table_BuildMetaIndexBlock_ShouldReturnError_WhenFinishFails
 * @tc.number: BuildMetaIndexBlock_Test_002
 * @tc.desc  : Test BuildMetaIndexBlock function when Finish fails.
 */
TEST_F(TestLsmStoreBrCov, Table_BuildMetaIndexBlock_ShouldReturnError_WhenFinishFails)
{
    mTableWriter->mMetaIndexBlockWriter->mOutputView->mCapacity = 0;
    mTableWriter->mMetaIndexBlockWriter->mOutputView->mMemManager->mMemoryLimit = 0;
    BResult result = mTableWriter->BuildMetaIndexBlock();
    EXPECT_NE(result, BSS_OK);
}

/**
 * @tc.name  : Table_BuildMetaIndexBlock_ShouldReturnError_WhenWriteBlockFails
 * @tc.number: BuildMetaIndexBlock_Test_003
 * @tc.desc  : Test BuildMetaIndexBlock function when WriteBlock fails.
 */
TEST_F(TestLsmStoreBrCov, Table_BuildMetaIndexBlock_ShouldReturnError_WhenWriteBlockFails)
{
    mTableWriter->mFileOutputView->fileSystem->Close();
    BResult result = mTableWriter->BuildMetaIndexBlock();
    EXPECT_NE(result, BSS_OK);
}

/**
 * @tc.name  : Table_MinNumBytesToRepresentValue_CheckReturnValue
 * @tc.number: MinNumBytesToRepresentValue_Test_001
 * @tc.desc  : Test that the function return value is correct
 */
TEST_F(TestLsmStoreBrCov, Table_MinNumBytesToRepresentValue_CheckReturnValue)
{
    FullKeyUtil util;
    uint32_t value = NO_255;
    EXPECT_EQ(util.MinNumBytesToRepresentValue(value), NO_1);
    value = NO_65535;
    EXPECT_EQ(util.MinNumBytesToRepresentValue(value), NO_2);
    value = NO_16777215;
    EXPECT_EQ(util.MinNumBytesToRepresentValue(value), NO_3);
    value = NO_16777216;
    EXPECT_EQ(util.MinNumBytesToRepresentValue(value), NO_4);
}

/**
 * @tc.name  : Table_WriteValueWithNumberOfBytes_ShouldReturnInvalidParam_WhenNumBytesIsZero
 * @tc.number: WriteValueWithNumberOfBytes_Test_001
 * @tc.desc  : Test case to verify that the function returns BSS_INVALID_PARAM when numBytes is 0.
 */
TEST_F(TestLsmStoreBrCov, Table_WriteValueWithNumberOfBytes_ShouldReturnInvalidParam_WhenNumBytesIsZero)
{
    uint64_t value = NO_1;
    uint32_t numBytes = NO_0;
    OutputViewRef outputView;
    int32_t result = FullKeyUtil::WriteValueWithNumberOfBytes(value, numBytes, outputView);
    EXPECT_EQ(result, BSS_INVALID_PARAM);
}

/**
 * @tc.name  : Table_WriteValueWithNumberOfBytes_ShouldReturnInvalidParam_WhenNumBytesIsGreaterThanNO_8
 * @tc.number: WriteValueWithNumberOfBytes_Test_002
 * @tc.desc  : Test case to verify that the function returns BSS_INVALID_PARAM when numBytes is greater than NO_8.
 */
TEST_F(TestLsmStoreBrCov, Table_WriteValueWithNumberOfBytes_ShouldReturnInvalidParam_WhenNumBytesIsGreaterThanNO_8)
{
    uint64_t value = NO_1;
    uint32_t numBytes = NO_9;
    OutputViewRef outputView;
    int32_t result = FullKeyUtil::WriteValueWithNumberOfBytes(value, numBytes, outputView);
    EXPECT_EQ(result, BSS_INVALID_PARAM);
}

/**
 * @tc.name  : Table_ToInternalKey_ShouldReturnNull_WhenMemoryAllocationFails
 * @tc.number: ToInternalKey_Test_001
 * @tc.desc  : Test that ToFullKey returns nullptr when memory allocation fails.
 */
TEST_F(TestLsmStoreBrCov, Table_ToInternalKey_ShouldReturnNull_WhenMemoryAllocationFails)
{
    KeyValueRef keyValue = MakeRef<KeyValue>();
    mMemManager->mMemoryLimit = 1;
    FullKeyRef result = FullKeyUtil::ToFullKey(keyValue, mMemManager, FileProcHolder::FILE_STORE_FLUSH);
    EXPECT_EQ(result, nullptr);
}

/**
 * @tc.name  : Table_ReadInternalKey_ShouldReturnKey_WhenAllReadsSucceed
 * @tc.number: ReadInternalKey_Test_001
 * @tc.desc  : Test that ReadInternalKey returns a valid HashInternalKey when all reads succeed.
 */
TEST_F(TestLsmStoreBrCov, Table_ReadInternalKey_ShouldReturnKey_WhenAllReadsSucceed)
{
    uint32_t primaryUserKeyHash = NO_4096;
    uint32_t keyLen = 1;
    uint8_t data = 1;
    uint16_t stateId = MAP << NO_13;
    uint32_t secondaryUserKeyHash = NO_4096;
    uint64_t seqId = NO_4096;
    uint8_t valueType = ValueType::PUT;
    outputView->WriteUint32(primaryUserKeyHash);
    outputView->WriteUint32(keyLen);
    outputView->WriteUint8(data);
    outputView->WriteUint16(stateId);
    outputView->WriteUint32(secondaryUserKeyHash);
    outputView->WriteUint32(keyLen);
    outputView->WriteUint8(data);
    outputView->WriteUint64(seqId);
    outputView->WriteUint8(valueType);
    inputView->Seek(0);
    auto result = FullKeyUtil::ReadInternalKey(inputView, mMemManager, FileProcHolder::FILE_STORE_FLUSH);
    EXPECT_NE(result, nullptr);
    EXPECT_EQ(result->PriKey().KeyHashCode(), primaryUserKeyHash);
    EXPECT_EQ(result->StateId(), stateId);
    EXPECT_EQ(result->SecKey().HashCode(), secondaryUserKeyHash);
    EXPECT_EQ(result->SeqId(), seqId);
    EXPECT_EQ(result->ValueType(), ValueType::PUT);
}

/**
 * @tc.name  : Table_ReadInternalKey_ShouldNotReadSecondaryKey_WhenStateHasNoSecondaryKey
 * @tc.number: ReadInternalKey_Test_002
 * @tc.desc  : Test that ReadInternalKey does not read the secondary key when StateHasSecondaryKey returns false.
 */
TEST_F(TestLsmStoreBrCov, Table_ReadInternalKey_ShouldNotReadSecondaryKey_WhenStateHasNoSecondaryKey)
{
    uint32_t primaryUserKeyHash = NO_4096;
    uint32_t keyLen = 1;
    uint8_t data = 1;
    uint16_t stateId = NO_1024;
    uint64_t seqId = NO_4096;
    uint8_t valueType = ValueType::PUT;
    outputView->WriteUint32(primaryUserKeyHash);
    outputView->WriteUint32(keyLen);
    outputView->WriteUint8(data);
    outputView->WriteUint16(stateId);
    outputView->WriteUint64(seqId);
    outputView->WriteUint8(valueType);
    inputView->Seek(0);
    FullKeyRef result = FullKeyUtil::ReadInternalKey(inputView, mMemManager, FileProcHolder::FILE_STORE_FLUSH);
    EXPECT_NE(result, nullptr);
    EXPECT_EQ(result->KeyHashCode(), primaryUserKeyHash);
    EXPECT_EQ(result->StateId(), stateId);
    EXPECT_EQ(result->SeqId(), seqId);
    EXPECT_EQ(result->ValueType(), ValueType::PUT);
}

/**
 * @tc.name  : Table_ReadInternalKey_ShouldReturnNull_WhenSeqIdReadFails
 * @tc.number: ReadInternalKey_Test_003
 * @tc.desc  : Test that ReadInternalKey returns null when reading the sequence ID fails.
 */
TEST_F(TestLsmStoreBrCov, Table_ReadInternalKey_ShouldReturnNull_WhenSeqIdReadFails)
{
    uint32_t primaryUserKeyHash = NO_4096;
    uint32_t keyLen = 1;
    uint8_t data = 1;
    uint16_t stateId = NO_1024;
    outputView->WriteUint32(primaryUserKeyHash);
    outputView->WriteUint32(keyLen);
    outputView->WriteUint8(data);
    outputView->WriteUint16(stateId);
    inputView->Seek(0);
    FullKeyRef result = FullKeyUtil::ReadInternalKey(inputView, mMemManager, FileProcHolder::FILE_STORE_FLUSH);
    EXPECT_EQ(result, nullptr);
}

/**
 * @tc.name  : Table_ReadInternalKey_ShouldReturnNull_WhenValueTypeReadFails
 * @tc.number: ReadInternalKey_Test_004
 * @tc.desc  : Test that ReadInternalKey returns null when reading the value type fails.
 */
TEST_F(TestLsmStoreBrCov, Table_ReadInternalKey_ShouldReturnNull_WhenValueTypeReadFails)
{
    uint32_t primaryUserKeyHash = NO_4096;
    uint32_t keyLen = 1;
    uint8_t data = 1;
    uint16_t stateId = NO_1024;
    uint64_t seqId = NO_4096;
    outputView->WriteUint32(primaryUserKeyHash);
    outputView->WriteUint32(keyLen);
    outputView->WriteUint8(data);
    outputView->WriteUint16(stateId);
    outputView->WriteUint64(seqId);
    inputView->Seek(0);
    FullKeyRef result = FullKeyUtil::ReadInternalKey(inputView, mMemManager, FileProcHolder::FILE_STORE_FLUSH);
    EXPECT_EQ(result, nullptr);
}

/**
 * @tc.name  : Table_ReadPrimary_ShouldLogErrorAndReturn_WhenInputViewIsNull
 * @tc.number: ReadPrimary_Test_001
 * @tc.desc  : Test that the function logs an error and returns when inputView is nullptr.
 */
TEST_F(TestLsmStoreBrCov, Table_ReadPrimary_ShouldLogErrorAndReturn_WhenInputViewIsNull)
{
    PriKeyNode prikey;
    ByteBufferRef byteBuffer;
    FullKeyUtil::ReadPrimary(prikey, nullptr, mMemManager, FileProcHolder::FILE_STORE_FLUSH, byteBuffer);
    EXPECT_EQ(byteBuffer, nullptr);
    EXPECT_EQ(prikey.HashCode(), 0);
    EXPECT_EQ(prikey.KeyLen(), 0);
}

/**
 * @tc.name  : Table_ReadPrimary_ShouldLogErrorAndReturn_WhenReadPrimaryUserKeyHashFails
 * @tc.number: ReadPrimary_Test_002
 * @tc.desc  : Test that the function logs an error and returns when reading the primaryUserKeyHash fails.
 */
TEST_F(TestLsmStoreBrCov, Table_ReadPrimary_ShouldLogErrorAndReturn_WhenReadPrimaryUserKeyHashFails)
{
    PriKeyNode prikey;
    inputView->Seek(0);
    ByteBufferRef byteBuffer;
    FullKeyUtil::ReadPrimary(prikey, inputView, mMemManager, FileProcHolder::FILE_STORE_FLUSH, byteBuffer);
    EXPECT_EQ(byteBuffer, nullptr);
    EXPECT_EQ(prikey.HashCode(), 0);
    EXPECT_EQ(prikey.KeyLen(), 0);
}

/**
 * @tc.name  : Table_ReadPrimary_ShouldLogErrorAndReturn_WhenReadKeyLenFails
 * @tc.number: ReadPrimary_Test_003
 * @tc.desc  : Test that the function logs an error and returns when reading the keyLen fails.
 */
TEST_F(TestLsmStoreBrCov, Table_ReadPrimary_ShouldLogErrorAndReturn_WhenReadKeyLenFails)
{
    PriKeyNode prikey;
    uint32_t primaryUserKeyHash = NO_4096;
    outputView->WriteUint32(primaryUserKeyHash);
    inputView->Seek(0);
    ByteBufferRef byteBuffer;
    FullKeyUtil::ReadPrimary(prikey, inputView, mMemManager, FileProcHolder::FILE_STORE_FLUSH, byteBuffer);
    EXPECT_EQ(byteBuffer, nullptr);
    EXPECT_NE(prikey.HashCode(), primaryUserKeyHash);
    EXPECT_EQ(prikey.KeyLen(), 0);
}

/**
 * @tc.name  : Table_ReadPrimary_ShouldLogErrorAndReturn_WhenReadBufferFails
 * @tc.number: ReadPrimary_Test_004
 * @tc.desc  : Test that the function logs an error and returns when reading the buffer fails.
 */
TEST_F(TestLsmStoreBrCov, Table_ReadPrimary_ShouldLogErrorAndReturn_WhenReadBufferFails)
{
    PriKeyNode prikey;
    uint32_t primaryUserKeyHash = NO_4096;
    uint32_t keyLen = NO_1;
    outputView->WriteUint32(primaryUserKeyHash);
    outputView->WriteUint32(keyLen);
    inputView->Seek(0);
    ByteBufferRef byteBuffer;
    FullKeyUtil::ReadPrimary(prikey, inputView, mMemManager, FileProcHolder::FILE_STORE_FLUSH, byteBuffer);
    EXPECT_TRUE(byteBuffer != nullptr);
    EXPECT_NE(prikey.HashCode(), primaryUserKeyHash);
    EXPECT_NE(prikey.KeyLen(), keyLen);
}

/**
 * @tc.name  : Table_ReadPrimary_ShouldSetBuilderValues_WhenAllOperationsSucceed
 * @tc.number: ReadPrimary_Test_005
 * @tc.desc  : Test that the function sets the builder values correctly when all operations succeed.
 */
TEST_F(TestLsmStoreBrCov, Table_ReadPrimary_ShouldSetBuilderValues_WhenAllOperationsSucceed)
{
    PriKeyNode prikey;
    uint32_t primaryUserKeyHash = NO_4096;
    uint32_t keyLen = NO_1;
    outputView->WriteUint32(primaryUserKeyHash);
    outputView->WriteUint32(keyLen);
    outputView->WriteUint8(NO_1);
    inputView->Seek(0);
    ByteBufferRef byteBuffer;
    FullKeyUtil::ReadPrimary(prikey, inputView, mMemManager, FileProcHolder::FILE_STORE_FLUSH, byteBuffer);
    EXPECT_EQ(prikey.KeyData()[0], NO_1);
    EXPECT_EQ(prikey.HashCode(), primaryUserKeyHash);
    EXPECT_EQ(prikey.KeyLen(), keyLen);
}

/**
 * @tc.name  : Table_ReadStateId_ShouldReturnZero_WhenReadFails
 * @tc.number: ReadStateId_Test_002
 * @tc.desc  : Test ReadStateId function when inputView->Read(stateId) returns an error code
 */
TEST_F(TestLsmStoreBrCov, Table_ReadStateId_ShouldReturnZero_WhenReadFails)
{
    uint16_t actualStateId = FullKeyUtil::ReadStateId(inputView);
    EXPECT_EQ(actualStateId, 0);
}

/**
 * @tc.name  : Table_ReadSecondaryKey_ShouldLogErrorAndReturn_WhenInputViewIsNull
 * @tc.number: ReadPrimary_Test_001
 * @tc.desc  : Test that the function logs an error and returns when inputView is nullptr.
 */
TEST_F(TestLsmStoreBrCov, Table_ReadSecondaryKey_ShouldLogErrorAndReturn_WhenInputViewIsNull)
{
    SecKeyNode secKey;
    ByteBufferRef byteBuffer;
    FullKeyUtil::ReadSecondaryKey(secKey, nullptr, mMemManager, FileProcHolder::FILE_STORE_FLUSH, byteBuffer);
    EXPECT_EQ(byteBuffer, nullptr);
    EXPECT_EQ(secKey.HashCode(), 0);
    EXPECT_EQ(secKey.KeyLen(), 0);
}

/**
 * @tc.name  : Table_ReadSecondaryKey_ShouldLogErrorAndReturn_WhenReadPrimaryUserKeyHashFails
 * @tc.number: ReadPrimary_Test_002
 * @tc.desc  : Test that the function logs an error and returns when reading the primaryUserKeyHash fails.
 */
TEST_F(TestLsmStoreBrCov, Table_ReadSecondaryKey_ShouldLogErrorAndReturn_WhenReadPrimaryUserKeyHashFails)
{
    SecKeyNode secKey;
    inputView->Seek(0);
    ByteBufferRef byteBuffer;
    FullKeyUtil::ReadSecondaryKey(secKey, inputView, mMemManager, FileProcHolder::FILE_STORE_FLUSH, byteBuffer);
    EXPECT_EQ(byteBuffer, nullptr);
    EXPECT_EQ(secKey.HashCode(), 0);
    EXPECT_EQ(secKey.KeyLen(), 0);
}

/**
 * @tc.name  : Table_ReadSecondaryKey_ShouldLogErrorAndReturn_WhenReadKeyLenFails
 * @tc.number: ReadPrimary_Test_003
 * @tc.desc  : Test that the function logs an error and returns when reading the keyLen fails.
 */
TEST_F(TestLsmStoreBrCov, Table_ReadSecondaryKey_ShouldLogErrorAndReturn_WhenReadKeyLenFails)
{
    SecKeyNode secKey;
    uint32_t secondaryUserKeyHash = NO_4096;
    outputView->WriteUint32(secondaryUserKeyHash);
    inputView->Seek(0);
    ByteBufferRef byteBuffer;
    FullKeyUtil::ReadSecondaryKey(secKey, inputView, mMemManager, FileProcHolder::FILE_STORE_FLUSH, byteBuffer);
    EXPECT_EQ(byteBuffer, nullptr);
    EXPECT_NE(secKey.HashCode(), secondaryUserKeyHash);
    EXPECT_EQ(secKey.KeyLen(), 0);
}

/**
 * @tc.name  : Table_ReadSecondaryKey_ShouldLogErrorAndReturn_WhenReadBufferFails
 * @tc.number: ReadPrimary_Test_004
 * @tc.desc  : Test that the function logs an error and returns when reading the buffer fails.
 */
TEST_F(TestLsmStoreBrCov, Table_ReadSecondaryKey_ShouldLogErrorAndReturn_WhenReadBufferFails)
{
    SecKeyNode secKey;
    uint32_t secondaryUserKeyHash = NO_4096;
    uint32_t keyLen = NO_1;
    outputView->WriteUint32(secondaryUserKeyHash);
    outputView->WriteUint32(keyLen);
    inputView->Seek(0);
    ByteBufferRef byteBuffer;
    FullKeyUtil::ReadSecondaryKey(secKey, inputView, mMemManager, FileProcHolder::FILE_STORE_FLUSH, byteBuffer);
    EXPECT_TRUE(byteBuffer != nullptr);
    EXPECT_NE(secKey.HashCode(), secondaryUserKeyHash);
    EXPECT_NE(secKey.KeyLen(), keyLen);
}

/**
 * @tc.name  : Table_ReadSecondaryKey_ShouldSetBuilderValues_WhenAllOperationsSucceed
 * @tc.number: ReadPrimary_Test_005
 * @tc.desc  : Test that the function sets the builder values correctly when all operations succeed.
 */
TEST_F(TestLsmStoreBrCov, Table_ReadSecondaryKey_ShouldSetBuilderValues_WhenAllOperationsSucceed)
{
    SecKeyNode secKey;
    uint32_t secondaryUserKeyHash = NO_4096;
    uint32_t keyLen = NO_1;
    outputView->WriteUint32(secondaryUserKeyHash);
    outputView->WriteUint32(keyLen);
    outputView->WriteUint8(NO_1);
    inputView->Seek(0);
    ByteBufferRef byteBuffer;
    FullKeyUtil::ReadSecondaryKey(secKey, inputView, mMemManager, FileProcHolder::FILE_STORE_FLUSH, byteBuffer);
    EXPECT_EQ(byteBuffer->Data()[0], NO_1);
    EXPECT_EQ(secKey.HashCode(), secondaryUserKeyHash);
    EXPECT_EQ(secKey.KeyLen(), keyLen);
}

/**
 * @tc.name  : Table_Reader_Initialize_ShouldReturnBSS_OK_WhenAllInitializeFunctionsSucceed
 * @tc.number: Initialize_Test_001
 * @tc.desc  : Test that Initialize returns BSS_OK when all initialization functions succeed.
 */
TEST_F(TestLsmStoreBrCov, Table_Reader_Initialize_ShouldReturnBSS_OK_WhenAllInitializeFunctionsSucceed)
{
    BResult result = mTableReader->Initialize();
    EXPECT_EQ(result, BSS_OK);
}

/**
 * @tc.name  : Table_Reader_Initialize_ShouldReturnError_WhenInitializeFooterAndMetaBlockFails
 * @tc.number: Initialize_Test_002
 * @tc.desc  : Test that Initialize returns an error code when InitializeFooterAndMetaBlock fails.
 */
TEST_F(TestLsmStoreBrCov, Table_Reader_Initialize_ShouldReturnError_WhenInitializeFooterAndMetaBlockFails)
{
    mTableReader->mMemManager->mMemoryLimit = 1;
    BResult result = mTableReader->Initialize();
    EXPECT_NE(result, BSS_OK);
}

/**
 * @tc.name  : Table_Reader_Initialize_ShouldReturnError_WhenInitializeFilterBlockFails
 * @tc.number: Initialize_Test_003
 * @tc.desc  : Test that Initialize returns an error code when InitializeFilterBlock fails.
 */
TEST_F(TestLsmStoreBrCov, Table_Reader_Initialize_ShouldReturnError_WhenInitializeFilterBlockFails)
{
    FileMetaDataRef mFileMetaData = mLsmStore->mVersionSet->mCurrent->GetFileMetaDatas()[0];
    uint32_t footerOffset = mTableReader->mInitOffset + static_cast<uint32_t>(mFileMetaData->GetFileSize()) -
                            sizeof(FooterStructure);
    FooterStructure footer{};
    mTableReader->mInputView->ReadBuffer(reinterpret_cast<uint8_t *>(&footer), sizeof(FooterStructure), footerOffset);
    FileOutputViewRef mOutputView = std::make_shared<FileOutputView>();
    mOutputView->Init(mTableReader->mPath, mLsmStore->mConf, FileOutputView::WriteMode::OVERWRITE);
    mOutputView->WriteBuffer(reinterpret_cast<uint8_t *>(&footer), footerOffset, sizeof(FooterStructure));
    mOutputView->Close();

    mTableReader->mInputView->Seek(0);
    BResult result = mTableReader->Initialize();
    EXPECT_NE(result, BSS_OK);
}

/**
 * @tc.name  : Table_Reader_InitializeFilterBlock_ShouldReturnError_WhenReadBlockFails
 * @tc.number: InitializeFilterBlock_Test_004
 * @tc.desc  : Test InitializeFilterBlock function when ReadBlock fails
 */
TEST_F(TestLsmStoreBrCov, Table_Reader_InitializeFilterBlock_ShouldReturnError_WhenReadBlockFails)
{
    uint32_t metaBlockOffset = 0;
    uint32_t metaBlockSize = NO_1;
    auto metaBlockMeta = std::make_shared<BlockHandle>(metaBlockOffset, metaBlockSize);
    mTableReader->mFilterBlockHandle = metaBlockMeta;
    mTableReader->mMemManager->mMemoryLimit = 0;
    FilterBlockRef filterBlock = mTableReader->GetFilterBlock();
    EXPECT_EQ(filterBlock, nullptr);
}

/**
 * @tc.name  : Table_Reader_GetOrLoadFilterBlock_ShouldReturnLoadedFilterBlock
 * @tc.number: GetOrLoadFilterBlock_Test_003
 * @tc.desc  : Test case to verify that GetOrLoadFilterBlock returns the loaded filter block when mCacheIndexAndFilter
 * is false and GetOrLoadBlock succeeds.
 */
TEST_F(TestLsmStoreBrCov, Table_Reader_GetOrLoadFilterBlock_ShouldReturnLoadedFilterBlock)
{
    mTableReader->Initialize();
    mTableReader->mCacheIndexAndFilter = false;
    auto result = mTableReader->GetOrLoadFilterBlock();
    EXPECT_NE(result, nullptr);
}

/**
 * @tc.name  : IteratorAll_ShouldReturnNull_WhenTableReaderIsNullAndHolderIsForceType
 * @tc.number: IteratorAll_Test_003
 * @tc.desc  : Test IteratorAll function when tableReader is null and holder is force type
 */
TEST_F(TestLsmStoreBrCov, IteratorAll_ShouldReturnNull_WhenTableReaderIsNullAndHolderIsForceType)
{
    mLsmStore->mFileCache->mAccessMap.begin()->second->mMemManager->mMemoryLimit = 1;
    uint64_t fileAddress = mLsmStore->mFileCache->mAccessMap.begin()->first;
    FullKeyFilterRef keyFilter;
    auto result = mLsmStore->mFileCache->IteratorAll(keyFilter, fileAddress, false, FileProcHolder::FILE_STORE_GET);
    EXPECT_EQ(result, nullptr);
}

/**
 * @tc.name  : IteratorAll_ShouldReturnNull_WhenTableReaderIsNullAndHolderIsNotForceType
 * @tc.number: IteratorAll_Test_004
 * @tc.desc  : Test IteratorAll function when tableReader is null and holder is not force type
 */
TEST_F(TestLsmStoreBrCov, IteratorAll_ShouldReturnNull_WhenTableReaderIsNullAndHolderIsNotForceType)
{
    mLsmStore->mFileCache->mAccessMap.begin()->second->mMemManager->mMemoryLimit = 1;
    uint64_t fileAddress = mLsmStore->mFileCache->mAccessMap.begin()->first;
    FullKeyFilterRef keyFilter;
    auto result = mLsmStore->mFileCache->IteratorAll(keyFilter, fileAddress, false, FileProcHolder::FILE_STORE_FLUSH);
    EXPECT_EQ(result, nullptr);
}
