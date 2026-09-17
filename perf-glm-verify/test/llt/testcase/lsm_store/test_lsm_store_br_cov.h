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

#ifndef BOOST_SS_TEST_LSM_STORE_BR_COV_H
#define BOOST_SS_TEST_LSM_STORE_BR_COV_H

#include <utility>

#include "gtest/gtest.h"

#define private public
#define protected public

#include "generator.h"
#include "test_lsm_store.h"

#undef private
#undef protected
using namespace ock::bss;

class TestLsmStoreBrCov : public TestLsmStore {
public:
    void SetUp() override;

    void TearDown() override;

    FileWriterRef GetTableBuilder(ConfigRef config)
    {
        FileInfoRef fileInfo = mLsmStore->mFileCacheManager->AllocateFile(mLsmStore->mFileDirectory,
                                                                          FileName::CreateFileName);

        FileProcHolder holder = FileProcHolder::FILE_STORE_FLUSH;
        return mLsmStore->mFileCache->CreateBuilder(fileInfo->GetFilePath(), 0, holder);
    }

    void WriteKeyBuffer(ByteBufferRef buffer, QueryKey key)
    {
        // | priKeyHash | priKeyLen | priKeyData | stateId | secHashCode | secKeyLen | secKeyData
        uint32_t pos = 0;
        buffer->WriteUint32(key.mPriKey.mHashCode, pos);
        pos += sizeof(uint32_t);
        buffer->WriteUint32(key.mPriKey.mKeyLen, pos);
        pos += sizeof(uint32_t);
        buffer->WriteAt(key.mPriKey.mKeyData, sizeof(uint8_t), pos);
        pos += sizeof(uint8_t);
        buffer->WriteUint16(key.StateId(), pos);
        pos += sizeof(uint16_t);
        buffer->WriteUint32(key.mSecKey.mHashCode, pos);
        pos += sizeof(uint32_t);
        buffer->WriteUint32(key.mSecKey.mKeyLen, pos);
        pos += sizeof(uint32_t);
        buffer->WriteAt(key.mSecKey.mKeyData, sizeof(uint8_t), pos);
    }

    std::shared_ptr<FileReader> CreateTableReader()
    {
        uint32_t kvCount = 1;
        std::vector<std::pair<SliceKey, Value>> kvPairList;
        auto dataSlice = CreateDataSlice(kvCount, kvPairList, NO_11);
        Ref<FlushingBucketGroupIterator> iterator = MakeRef<FlushingBucketGroupIterator>();
        std::vector<std::vector<DataSliceRef>> dataSlices = std::vector<std::vector<DataSliceRef>>{ { dataSlice } };
        iterator->Initialize(dataSlices);
        mLsmStore->Put(iterator);
        FileMetaDataRef mFileMetaData = mLsmStore->mVersionSet->mCurrent->GetFileMetaDatas()[0];
        FileInfoRef fileInfo = mLsmStore->mFileCacheManager->GetPrimaryFileInfo(mFileMetaData->GetFileAddress());
        uint64_t fileMemLimit = mMemManager->GetMemoryTypeMaxSize(MemoryType::FILE_STORE);
        BlockCacheRef mBlockCache = BlockCacheManager::Instance()->CreateBlockCache(mLsmStore->mConf->GetTaskSlotFlag(),
                                                                                    fileMemLimit, 0.0f);
        std::shared_ptr<FileReader> tableReader =
            std::make_shared<FileReader>(mFileMetaData, mLsmStore->mConf, fileInfo->GetFilePath(), mBlockCache,
                                         mMemManager, FileProcHolder::FILE_STORE_FLUSH);
        tableReader->mInputView = std::make_shared<FileInputView>();
        tableReader->mInputView->Init(FileSystemType::LOCAL, tableReader->mPath);
        tableReader->mInputView->Seek(0);
        return tableReader;
    }

    GeneratorRef mGenerator;
    FileWriterRef mTableWriter;
    std::shared_ptr<FileReader> mTableReader;
    FileInputViewRef inputView;
    FileOutputViewRef outputView;
};

#endif  // BOOST_SS_TEST_LSM_STORE_BR_COV_H
