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

#ifndef BOOST_SS_SLICE_FACTORY_H
#define BOOST_SS_SLICE_FACTORY_H

#include "common/bss_metric.h"
#include "common/util/seq_generator.h"
#include "flushing_bucket_group.h"
#include "generator.h"
#include "gtest/gtest.h"
#include "lsm_store/file/file_cache_factory.h"
#include "slice/slice.h"

using namespace ock::bss;
using KeyVectorValueMap = std::unordered_map<SliceKey, std::vector<Value>, SliceKeyHash, SliceKeyEqual>;

class TestLsmStore : public ::testing::Test {
public:
    GeneratorRef mGenerator;
    void SetUp() override
    {
        mGenerator = MakeRef<Generator>(44);
    }

    DataSliceRef CreateDataSlice(uint32_t kvCount, std::vector<std::pair<SliceKey, Value>> &kvPairList,
                                 bool withNameSpace, bool withMultiSecKeys, uint32_t seed)
    {
        Generator generator(seed);
        if (withMultiSecKeys) {
            uint32_t secKeyNum = 10;
            for (uint32_t i = 0; i < kvCount / secKeyNum; ++i) {
                std::vector<SliceKey> keys = generator.GenerateDualKey(NO_32, NO_64, secKeyNum);
                for (const auto &key : keys) {
                    Value value = generator.GenerateValue(128);
                    kvPairList.emplace_back(key, value);
                }
            }
            uint32_t leftNum = kvCount % secKeyNum;
            for (uint32_t i = 0; i < leftNum; ++i) {
                std::vector<SliceKey> keys = generator.GenerateDualKey(NO_32, NO_64, leftNum);
                for (const auto &key : keys) {
                    Value value = generator.GenerateValue(128);
                    kvPairList.emplace_back(key, value);
                }
            }
        } else {
            for (uint32_t i = 0; i < kvCount; ++i) {
                SliceKey key = generator.GenerateDualKey(32, 64);
                Value value = generator.GenerateValue(128);
                kvPairList.emplace_back(key, value);
            }
        }

        // put key and values to slice.
        auto slice = std::make_shared<Slice>();
        SliceCreateMeta meta = { 1 };
        auto result = slice->Initialize(kvPairList, meta, mMemManager);
        if (result != BSS_OK) {
            return nullptr;
        }

        // create data slice.
        auto dataSlice = std::make_shared<DataSlice>();
        dataSlice->Init(slice);

        return dataSlice;
    }

    DataSliceRef CreateDataSlice(uint32_t kvCount, std::vector<std::pair<SliceKey, Value>> &kvPairList, uint32_t seed)
    {
        return CreateDataSlice(kvCount, kvPairList, false, false, seed);
    }

    void InitEnv(ConfigRef config, uint32_t blockCacheCapacity = NO_10000)
    {
        mMemManager = std::make_shared<MemManager>(AllocatorType::DIRECT);
        mMemManager->Initialize(config);

        auto groupRange = std::make_shared<GroupRange>(0, NO_65535);
        auto hashCodeRange = std::make_shared<HashCodeRange>(0, INT32_MAX);
        auto fileStoreId = std::make_shared<FileStoreID>(groupRange, 0, hashCodeRange);

        auto stateIdProvider = std::make_shared<StateIdProvider>(0, NO_65535, mMemManager);
        auto cacheFactory = CreateFileCacheFactory();
        auto fileCache = cacheFactory->GetFileCache();
        auto tableFactory = std::make_shared<FileFactory>(config, std::make_shared<BlockCache>(blockCacheCapacity));
        tableFactory->Initialize(mMemManager);
        KeyGroupUtilRef codec;
        uint32_t maxParallelism = config->GetMaxParallelism() == 0 ? NO_128 : config->GetMaxParallelism();
        ASSERT_EQ(KeyGroupUtil::Create(maxParallelism, codec), BSS_OK);
        auto stateFilterManager =
            std::make_shared<StateFilterManager>(std::make_shared<StateIdProvider>(0, NO_65535, mMemManager), config, 0,
                                                 NO_65535, codec);
        mLsmStore = std::make_shared<LsmStore>(fileStoreId, config, tableFactory, fileCache, stateFilterManager,
                                               mMemManager);
        mLsmStore->Initialize();

        mSeqGenerator = std::make_shared<SeqGenerator>();
        mMetric = new (std::nothrow) BoostNativeMetric(NO_31);
        mMetric->Init();
        mLsmStore->RegisterMetric(mMetric);
    }

    void TearDown() override
    {
        ExitEnv();
    }

    void ExitEnv()
    {
        if (mMetric != nullptr) {
            mMetric->Close();
            delete mMetric;
            mMetric = nullptr;
        }
        // clean up files.
        if (mLsmStore == nullptr) {
            return;
        }
        auto current = mLsmStore->GetVersionSet()->GetCurrent();
        if (current != nullptr) {
            for (auto level : current->GetLevels()) {
                auto fileMetaDataGroups = level.GetFileMetaDataGroups();
                for (const auto &fileMetaDataGroup : fileMetaDataGroups) {
                    for (const auto &fileMetaData : fileMetaDataGroup->GetFiles()) {
                        unlink(fileMetaData->GetIdentifier().c_str());
                    }
                }
            }
        }
    }

    Ref<FlushingBucketGroupIterator> CreateDataSliceIterator(std::vector<std::pair<SliceKey, Value>> &kvPairs)
    {
        SliceRef slice = std::make_shared<Slice>();
        SliceCreateMeta meta = {};
        slice->Initialize(kvPairs, meta, mMemManager);

        DataSliceRef dataSlice = std::make_shared<DataSlice>();
        dataSlice->Init(slice);

        std::vector<DataSliceRef> dataSlices = { dataSlice };

        Ref<FlushingBucketGroupIterator> iterator = MakeRef<FlushingBucketGroupIterator>();
        std::vector<std::vector<DataSliceRef>> slices = std::vector<std::vector<DataSliceRef>>{ dataSlices };
        iterator->Initialize(slices);

        return iterator;
    }

    void FlushLevel0Table(std::vector<std::pair<SliceKey, Value>> &entries)
    {
        auto iterator = CreateDataSliceIterator(entries);
        auto ret = mLsmStore->Put(iterator);
        ASSERT_EQ(ret, BSS_OK);
    }

    void GetAndCheckValue(const SliceKey &checkKey, const Value &checkValue,
                          const std::vector<SliceKey> &deleteKeys = {})
    {
        Value value;
        auto existed = mLsmStore->Get(checkKey, value);
        // deleted key already been compaction, then it doesn't exist.
        if (!existed) {
            for (const auto &deletedKey : deleteKeys) {
                if (IsTheSameKey(deletedKey, checkKey)) {
                    return;
                }
            }
        }
        ASSERT_TRUE(existed);
        bool found = true;
        for (const auto &deletedKey : deleteKeys) {
            if (IsTheSameKey(deletedKey, checkKey)) {
                ASSERT_EQ(value.ValueType(), DELETE);
                found = true;
                break;
            }
        }
        if (found) {
            return;
        }
        ASSERT_TRUE(IsTheSameValue(checkValue, value));
    }

    void GetAndCheckPrefixIterator(const SliceKey &prefixKey, const Value &checkValue,
                                   const std::vector<SliceKey> &deleteKeys = {}, bool reverseOrder = false)
    {
        bool hasDeletedKey = false;
        for (const auto &deletedKey : deleteKeys) {
            if (IsTheSamePriKey(deletedKey, prefixKey)) {
                hasDeletedKey = true;
            }
        }
        auto iterator = mLsmStore->PrefixIterator(prefixKey, reverseOrder);
        // deleted key already been compaction, then it doesn't exist.
        if (iterator == nullptr || !iterator->HasNext()) {
            ASSERT_TRUE(hasDeletedKey);
            return;
        }
        ASSERT_NE(iterator, nullptr);
        ASSERT_TRUE(iterator->HasNext());

        bool found = false;
        while (iterator->HasNext()) {
            auto keyValue = iterator->Next();
            if (hasDeletedKey) {
                // before compaction, deleted key exists, but ValueType is DELETE.
                for (const auto &deletedKey : deleteKeys) {
                    if (IsTheSameKey(deletedKey, keyValue->key)) {
                        ASSERT_EQ(keyValue->value.ValueType(), DELETE);
                        found = true;
                        break;
                    }
                }
                if (found) {
                    continue;
                }
            }

            ASSERT_NE(keyValue, nullptr);
            ASSERT_TRUE(IsTheSameKey(prefixKey, keyValue->key));
            ASSERT_TRUE(IsTheSameValue(checkValue, keyValue->value));
            found = true;
        }
        ASSERT_TRUE(found);
        iterator->Close();
    }

    void GetAndCheckPrefixIterator(const SliceKey &prefixKey, const std::vector<Value> &checkValues,
                                   const std::vector<SliceKey> &deleteKeys = {}, bool reverseOrder = false)
    {
        bool hasDeletedKey = false;
        for (const auto &deletedKey : deleteKeys) {
            if (IsTheSamePriKey(deletedKey, prefixKey)) {
                hasDeletedKey = true;
            }
        }
        auto iterator = mLsmStore->PrefixIterator(prefixKey, reverseOrder);
        // deleted key already been compaction, then it doesn't exist.
        if (iterator == nullptr) {
            ASSERT_TRUE(hasDeletedKey);
            return;
        }
        ASSERT_NE(iterator, nullptr);
        ASSERT_TRUE(iterator->HasNext());

        uint32_t checkCount = 0;
        while (iterator->HasNext()) {
            checkCount++;
            auto keyValue = iterator->Next();
            ASSERT_NE(keyValue, nullptr);

            if (hasDeletedKey) {
                // before compaction, deleted key exists, but ValueType is DELETE.
                bool isDeleted = false;
                for (const auto &deletedEntry : deleteKeys) {
                    if (IsTheSameKey(deletedEntry, keyValue->key)) {
                        ASSERT_EQ(keyValue->value.ValueType(), DELETE);
                        isDeleted = true;
                        break;
                    }
                }
                if (isDeleted) {
                    continue;
                }
            }

            bool found = false;
            for (const auto &checkValue : checkValues) {
                if (IsTheSameValue(checkValue, keyValue->value)) {
                    found = true;
                    break;
                }
            }
            ASSERT_TRUE(found);
        }
        // some key have been deleted, we check these keys is in deletedKeys list.
        if (checkCount < checkValues.size()) {
            ASSERT_TRUE(hasDeletedKey);
            ASSERT_EQ(checkCount + 1, checkValues.size());
            return;
        }
        ASSERT_EQ(checkCount, checkValues.size());
    }

    void GetAndCheckPrefixIterator(const SliceKey &prefixKey, const SliceKVMap &checkEntries,
                                   const std::vector<SliceKey> &deleteKeys = {}, bool reverseOrder = false)
    {
        bool hasDeletedKey = false;
        for (const auto &deletedKey : deleteKeys) {
            if (IsTheSamePriKey(deletedKey, prefixKey)) {
                hasDeletedKey = true;
            }
        }
        auto iterator = mLsmStore->PrefixIterator(prefixKey, reverseOrder);
        // deleted key already been compaction, then it doesn't exist.
        if (iterator == nullptr) {
            ASSERT_TRUE(hasDeletedKey);
            return;
        }
        ASSERT_NE(iterator, nullptr);
        ASSERT_TRUE(iterator->HasNext());

        uint32_t checkCount = 0;
        while (iterator->HasNext()) {
            checkCount++;
            auto keyValue = iterator->Next();
            ASSERT_NE(keyValue, nullptr);
            if (hasDeletedKey) {
                // before compaction, deleted key exists, but ValueType is DELETE.
                bool isDeleted = false;
                for (const auto &deletedEntry : deleteKeys) {
                    if (IsTheSameKey(deletedEntry, keyValue->key)) {
                        ASSERT_EQ(keyValue->value.ValueType(), DELETE);
                        isDeleted = true;
                        break;
                    }
                }
                if (isDeleted) {
                    continue;
                }
            }

            bool found = false;
            for (const auto &checkEntry : checkEntries) {
                if (IsTheSameKey(checkEntry.first, keyValue->key)) {
                    ASSERT_TRUE(IsTheSameValue(checkEntry.second, keyValue->value));
                    found = true;
                    break;
                }
            }
            ASSERT_TRUE(found);
        }
        // some key have been deleted, we check these keys is in deletedKeys list.
        if (checkCount < checkEntries.size()) {
            ASSERT_TRUE(hasDeletedKey);
            ASSERT_EQ(checkCount + 1, checkEntries.size());
            return;
        }
        ASSERT_EQ(checkCount, checkEntries.size());
    }

    bool IsTheSameKey(const SliceKey &rawKey, const Key &key)
    {
        return rawKey.Compare(key) == 0;
    }

    bool IsTheSamePriKey(const SliceKey &first, const SliceKey &second)
    {
        return first.Compare(second) == 0;
    }

    bool IsTheSameValue(const Value &binaryValue, const Value &value)
    {
        if (binaryValue.ValueType() != value.ValueType()) {
            return false;
        }
        if (binaryValue.SeqId() != value.SeqId()) {
            return false;
        }
        if (binaryValue.ValueLen() != value.ValueLen()) {
            return false;
        }
        return memcmp(binaryValue.ValueData(), value.ValueData(), binaryValue.ValueLen()) == 0;
    }

protected:
    MemManagerRef mMemManager;
    LsmStoreRef mLsmStore;
    SeqGeneratorRef mSeqGenerator;
    BoostNativeMetric *mMetric = nullptr;

private:
    static FileCacheFactoryRef CreateFileCacheFactory()
    {
        std::string localBasePath = ".";
        char buffer[PATH_MAX];
        if (getcwd(buffer, sizeof(buffer)) != nullptr) {
            localBasePath = std::string(buffer);
        }
        std::string fileName = "test";
        PathRef localPath = std::make_shared<Path>(Uri(localBasePath));
        ConfigRef config = std::make_shared<Config>();
        config->SetLocalPath(localPath->Name());
        BoostNativeMetricPtr *metric = nullptr;
        return std::make_shared<FileCacheFactory>(config, nullptr, metric);
    }
};

#endif  // BOOST_SS_SLICE_FACTORY_H
