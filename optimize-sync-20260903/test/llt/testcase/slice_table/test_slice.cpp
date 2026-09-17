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

#include "test_slice.h"

#include <iostream>
#include <random>
#include <vector>

#include "binary/query_binary.h"
#include "generator.h"
#include "include/bss_err.h"
#include "include/bss_types.h"
#include "slice_table/binary/byte_buffer.h"
#include "slice_table/slice/slice.h"
#include "test_slice_table.h"

using namespace ock::bss;

class TestSlice : public ::testing::Test {
public:
    TestSlice()
    {
    }
    static void SetUpTestCase();
    static void TearDownTestCase();
    void SetUp()
    {
        mGenerator = MakeRef<Generator>(44);
    }
    void TearDown()
    {
    }

public:
    static MemManagerRef mMemManager;
    SliceRef mSlice;
    GeneratorRef mGenerator;
};

MemManagerRef TestSlice::mMemManager = nullptr;

void TestSlice::SetUpTestCase()
{
    auto mConfig = std::make_shared<Config>();
    mMemManager = std::make_shared<MemManager>(AllocatorType::DIRECT);
    mMemManager->Initialize(mConfig);
}

void TestSlice::TearDownTestCase()
{
}

TEST_F(TestSlice, TestByteBufferReadWrite)
{
    uint32_t n = 1024;
    ByteBufferRef buffer = MakeRef<ByteBuffer>(NO_4, MemoryType::SLICE_TABLE, mMemManager);
    buffer->WriteUint32(n, NO_0);
    uint32_t i = 0;
    buffer->ReadUint32(i, 0);
    ASSERT_EQ(n, i);
    int32_t k = -1;
    buffer->WriteAt(reinterpret_cast<uint8_t *>(&k), sizeof(int32_t), NO_0);
    int32_t p = 0;
    buffer->ReadAt(reinterpret_cast<uint8_t *>(&p), sizeof(int32_t), NO_0);
    ASSERT_EQ(k, p);
}

TEST_F(TestSlice, TestByteBufferCopy)
{
    uint32_t n1 = 1024;
    uint32_t n2 = 1024;
    ByteBufferRef buffer1 = MakeRef<ByteBuffer>(NO_4, MemoryType::SLICE_TABLE, mMemManager);
    buffer1->WriteUint32(n1, NO_0);
    ByteBufferRef buffer2 = MakeRef<ByteBuffer>(NO_4, MemoryType::SLICE_TABLE, mMemManager);
    buffer2->WriteUint32(n2, NO_0);
    buffer1->WriteFromBuffer(buffer2, NO_0, NO_4, NO_0);
    uint32_t i = 0;
    buffer1->ReadUint32(i, 0);
    ASSERT_EQ(n2, i);
}

TEST_F(TestSlice, TestSlicePut)
{
    SliceRef slice = std::make_shared<Slice>();
    std::vector<std::pair<SliceKey, Value>> kvPairs;
    uintptr_t addr = 0;
    mMemManager->GetMemory(MemoryType::SLICE_TABLE, 40, addr);
    MemorySegmentRef memorySegment = MakeRef<MemorySegment>(40, reinterpret_cast<uint8_t *>(addr), mMemManager);
    RawDataSlice *rawDataSlice = new RawDataSlice(memorySegment, 0);
    uint32_t keyLen = 14;
    memorySegment->PutUint32_t(0, keyLen);
    uint32_t hash = 123456;
    memorySegment->PutUint32_t(NO_4, hash);
    uint16_t stateId = 12;
    memorySegment->PutUint16_t(NO_8, stateId);
    uint64_t key1 = 789456L;
    memorySegment->PutUint64_t(NO_10, key1);
    FreshKeyNodePtr key = FreshKeyNode::FromBuffer(memorySegment->Data());
    PriKey primaryKey;
    primaryKey.Parse(key);
    BinaryKey binaryKey;
    binaryKey.Parse(primaryKey, true);
    uint32_t valueLen = NO_17;
    memorySegment->PutUint32_t(NO_18, valueLen);
    NodeType nodeType = NodeType::SERIALIZED;
    memorySegment->PutUint8_t(NO_22, static_cast<uint8_t>(nodeType));
    uint64_t seq = 147258L;
    memorySegment->PutUint64_t(NO_23, seq);
    ValueType valueType = PUT;
    memorySegment->PutUint8_t(NO_31, static_cast<uint8_t>(valueType));
    uint64_t v = 852741;
    memorySegment->PutUint64_t(NO_32, v);
    FreshValueNodePtr valueNodePtr = FreshValueNode::FromBuffer(memorySegment->Data() + NO_18);
    rawDataSlice->AddBinaryData({ binaryKey, valueNodePtr });
    rawDataSlice->PutMixHashCode(binaryKey.mMixedHashCode);
    rawDataSlice->PutIndexVec();
    SliceCreateMeta meta = { 0L, 1L, 0L };
    bool force = false;
    auto ret = slice->Initialize(*rawDataSlice, meta, mMemManager, force, nullptr);
    ASSERT_EQ(ret, BSS_OK);
    BinaryData prikey(reinterpret_cast<uint8_t *>(&key1), sizeof(uint64_t));
    QueryKey queryKey(stateId, hash, prikey);
    Value value;
    bool res = slice->Get(queryKey, value);
    ASSERT_EQ(res, true);
    uint64_t key2 = 1234;
    BinaryData prikey1(reinterpret_cast<uint8_t *>(&key2), sizeof(uint64_t));
    QueryKey queryKey1(stateId, 3214, prikey);
    Value value1;
    res = slice->Get(queryKey1, value1);
    ASSERT_EQ(res, false);
    delete rawDataSlice;
}

TEST_F(TestSlice, TestSliceGetLong)
{
    SliceRef slice = std::make_shared<Slice>();
    uintptr_t addr = 0;
    mMemManager->GetMemory(MemoryType::SLICE_TABLE, 4000000, addr);
    MemorySegmentRef memorySegment = MakeRef<MemorySegment>(4000000, reinterpret_cast<uint8_t *>(addr), mMemManager);
    RawDataSlice *rawDataSlice = new RawDataSlice(memorySegment, 0);
    uint32_t cap = NO_18;
    uint16_t stateId = 16;  // 00 010 000 = PUTValue
    uint32_t loopCount = 100000;
    uint64_t keyStart1 = 20000;
    uint32_t seqStart = 40000;
    uint64_t valueStart = 50000;
    std::vector<uint32_t> hashVector;
    std::vector<BinaryKey> keys;
    uint32_t pos = 0;
    for (uint32_t i = 0; i < loopCount; i++) {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<uint32_t> dis(0, UINT32_MAX);
        uint32_t hash = dis(gen);
        memorySegment->PutUint32_t(pos + NO_0, 14);
        memorySegment->PutUint32_t(pos + NO_4, hash);
        memorySegment->PutUint16_t(pos + NO_8, stateId);
        uint64_t key1 = keyStart1 + i;
        memorySegment->PutUint64_t(pos + NO_10, key1);
        FreshKeyNodePtr key = FreshKeyNode::FromBuffer(memorySegment->Data() + pos);
        pos += cap;
        PriKey primaryKey;
        primaryKey.Parse(key);
        BinaryKey binaryKey;
        binaryKey.Parse(primaryKey, true);
        uint32_t valueLen = NO_17;
        memorySegment->PutUint32_t(pos + NO_0, valueLen);
        NodeType nodeType = NodeType::SERIALIZED;
        memorySegment->PutUint8_t(pos + NO_4, static_cast<uint8_t>(nodeType));
        uint64_t seq = seqStart + i;
        memorySegment->PutUint64_t(pos + NO_5, seq);
        ValueType valueType = PUT;
        memorySegment->PutUint8_t(pos + NO_13, static_cast<uint8_t>(valueType));
        uint64_t v = valueStart + i;
        memorySegment->PutUint64_t(pos + NO_14, v);
        FreshValueNodePtr valueNodePtr = FreshValueNode::FromBuffer(memorySegment->Data() + pos);
        pos += NO_22;
        rawDataSlice->AddBinaryData({ binaryKey, valueNodePtr });
        rawDataSlice->PutMixHashCode(binaryKey.mMixedHashCode);
        rawDataSlice->PutIndexVec();
    }
    SliceCreateMeta meta = { 0L, 1L, 0L };
    bool force = false;
    slice->Initialize(*rawDataSlice, meta, mMemManager, force, nullptr);
    for (auto &item : keys) {
        uint32_t hashCode = item.mKeyHashCode;
        BinaryData prikey(item.mPrimaryKey.mKeyData, item.mPrimaryKey.mKeyDataLength);
        stateId = item.mPrimaryKey.mStateId;
        QueryKey queryKey(stateId, hashCode, prikey);
        Value value;
        bool ret = slice->Get(queryKey, value);
        ASSERT_EQ(ret, true);
        ASSERT_EQ(value.ValueLen(), NO_17 - NO_9);
        uint64_t valueNum = *reinterpret_cast<uint64_t *>(const_cast<uint8_t *>(value.ValueData()));
        uint64_t key = *reinterpret_cast<uint64_t *>(item.mPrimaryKey.mKeyData);
        ASSERT_EQ(valueNum - valueStart, key - keyStart1);
    }
    delete rawDataSlice;
}

TEST_F(TestSlice, TestSliceGetShort)
{
    SliceRef slice = std::make_shared<Slice>();
    uintptr_t addr = 0;
    mMemManager->GetMemory(MemoryType::SLICE_TABLE, 4000000, addr);
    MemorySegmentRef memorySegment = MakeRef<MemorySegment>(4000000, reinterpret_cast<uint8_t *>(addr), mMemManager);
    RawDataSlice *rawDataSlice = new RawDataSlice(memorySegment, 0);
    uint32_t cap = 22;
    uint16_t stateId = 16;  // 00 010 000 = PUTValue
    uint32_t loopCount = 65536;
    uint64_t keyStart1 = 20000;
    uint32_t seqStart = 40000;
    uint64_t valueStart = 50000;
    std::vector<uint32_t> hashVector;
    std::vector<BinaryKey> keys;
    uint32_t pos = 0;
    for (uint32_t i = 0; i < loopCount; i++) {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<uint32_t> dis(0, UINT32_MAX);
        uint32_t hash = dis(gen);
        memorySegment->PutUint32_t(pos + NO_0, 14);
        memorySegment->PutUint32_t(pos + NO_4, hash);
        memorySegment->PutUint16_t(pos + NO_8, stateId);
        uint64_t key1 = keyStart1 + i;
        memorySegment->PutUint64_t(pos + NO_10, key1);
        FreshKeyNodePtr key = FreshKeyNode::FromBuffer(memorySegment->Data() + pos);
        pos += cap;
        PriKey primaryKey;
        primaryKey.Parse(key);
        BinaryKey binaryKey;
        binaryKey.Parse(primaryKey, true);
        uint32_t valueLen = NO_17;
        memorySegment->PutUint32_t(pos + NO_0, valueLen);
        NodeType nodeType = NodeType::SERIALIZED;
        memorySegment->PutUint8_t(pos + NO_4, static_cast<uint8_t>(nodeType));
        uint64_t seq = seqStart + i;
        memorySegment->PutUint64_t(pos + NO_5, seq);
        ValueType valueType = PUT;
        memorySegment->PutUint8_t(pos + NO_13, static_cast<uint8_t>(valueType));
        uint64_t v = valueStart + i;
        memorySegment->PutUint64_t(pos + NO_14, v);
        FreshValueNodePtr valueNodePtr = FreshValueNode::FromBuffer(memorySegment->Data() + pos);
        pos += NO_22;
        rawDataSlice->AddBinaryData({ binaryKey, valueNodePtr });
        rawDataSlice->PutMixHashCode(binaryKey.mMixedHashCode);
        rawDataSlice->PutIndexVec();
    }
    SliceCreateMeta meta = { 0L, 1L, 0L };
    bool force = false;
    slice->Initialize(*rawDataSlice, meta, mMemManager, force, nullptr);
    for (auto &item : keys) {
        uint32_t hashCode = item.mKeyHashCode;
        BinaryData prikey(item.mPrimaryKey.mKeyData, item.mPrimaryKey.mKeyDataLength);
        stateId = item.mPrimaryKey.mStateId;
        QueryKey queryKey(stateId, hashCode, prikey);
        Value value;
        bool ret = slice->Get(queryKey, value);
        ASSERT_EQ(ret, true);
        ASSERT_EQ(value.ValueLen(), NO_17 - NO_9);
        uint64_t valueNum = *reinterpret_cast<uint64_t *>(const_cast<uint8_t *>(value.ValueData()));
        uint64_t key = *reinterpret_cast<uint64_t *>(item.mPrimaryKey.mKeyData);
        ASSERT_EQ(valueNum - valueStart, key - keyStart1);
    }
}

TEST_F(TestSlice, TestSizeOfSliceHeader)
{
    ASSERT_EQ(sizeof(SliceHead), NO_42);
}

TEST_F(TestSlice, test_find_started_index_slot_return_right_index)
{
    // add kv to slice.
    mSlice = std::make_shared<Slice>();
    SliceCreateMeta meta = { 1 };

    // create kv pair and add to list.
    uint32_t kvCount = 64;
    uint16_t stateId = MAP << NO_13;
    std::vector<KVPair> kvPairList;
    for (uint64_t i = 0; i < kvCount; ++i) {
        std::string priKey = std::to_string(i + 0xFFFFFFFF);
        std::string secKey = std::to_string(i + 0xFFFF);
        SliceKey key = mGenerator->GenerateDualKey(priKey.data(), priKey.length(), secKey.data(), secKey.length());
        Value value = mGenerator->GenerateValue(10);
        kvPairList.push_back({ key, value });
    }

    // shuffle kv pair list
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, kvPairList.size() - 1);
    auto shuffleKvPairList = kvPairList;
    std::shuffle(shuffleKvPairList.begin(), shuffleKvPairList.end(), gen);

    // initialize slice.
    auto result = mSlice->Initialize(shuffleKvPairList, meta, mMemManager);
    ASSERT_EQ(result, BSS_OK);

    // get sorted key index and check order.
    for (uint32_t i = 0; i < kvCount; ++i) {
        uint32_t index = mSlice->GetSortedIndex(i);
        auto key = mSlice->GetBinaryKey(index);
        auto cmp = key.Compare(kvPairList[i].first);
        ASSERT_EQ(cmp, 0L);
    }

    // get start slot by prefix key
    Key prefix;
    uint64_t prefixId = 32;
    auto prefixKeyData = std::to_string(prefixId + 0xFFFFFFFF);
    mGenerator->InitKey(stateId, (uint8_t *)prefixKeyData.data(), prefixKeyData.length(), prefix);

    auto startSlot = mSlice->FindStartSortedIndexSlot(prefix);
    ASSERT_EQ(startSlot, (uint32_t)prefixId);
    auto endSlot = mSlice->FindEndSortedIndexSlot(prefix);
    ASSERT_EQ(endSlot, (uint32_t)prefixId);

    // prefix key out of range, can't find it.
    prefixId = 65;
    prefixKeyData = std::to_string(prefixId + 0xFFFFFFFF);
    mGenerator->InitKey(stateId, (uint8_t *)prefixKeyData.data(), prefixKeyData.length(), prefix);
    startSlot = mSlice->FindStartSortedIndexSlot(prefix);
    ASSERT_EQ(startSlot, 64U);
    endSlot = mSlice->FindEndSortedIndexSlot(prefix);
    ASSERT_EQ(endSlot, 63U);

    // prefix key out of range, can't find it.
    prefixId = -1;
    prefixKeyData = std::to_string(prefixId + 0xFFFFFFFF);
    mGenerator->InitKey(stateId, (uint8_t *)prefixKeyData.data(), prefixKeyData.length(), prefix);
    startSlot = mSlice->FindStartSortedIndexSlot(prefix);
    ASSERT_EQ(startSlot, 0);
    endSlot = mSlice->FindEndSortedIndexSlot(prefix);
    ASSERT_EQ(endSlot, -1);

    // get sub prefix boost map.
    uint64_t startId = 1;
    auto startKeyData = std::to_string(startId + 0xFFFFFFFF);

    Key prefixKey;
    mGenerator->InitKey(stateId, (uint8_t *)startKeyData.data(), startKeyData.length(), prefixKey);

    SliceKVMap kvMap;
    auto iterator = mSlice->SubIterator(prefixKey, false);
    ASSERT_TRUE(iterator != nullptr);
    std::vector<KeyValueRef> keyValues;
    while (iterator->HasNext()) {
        auto keyValue = iterator->Next();
        keyValues.emplace_back(keyValue);
        // todo: check key and value.
    }
    ASSERT_EQ(keyValues.size(), 1UL);
}

TEST_F(TestSlice, test_slice_space_return_ok)
{
    // prepare key and values.
    // add kv to slice.
    mSlice = std::make_shared<Slice>();
    SliceCreateMeta meta = { 1 };

    // create kv pair and add to list.
    uint32_t kvCount = 1024;
    std::vector<KVPair> kvPairList;
    for (uint32_t i = 0; i < kvCount; ++i) {
        std::string priKey = std::to_string(i + 0xFFFFFFFF);
        std::string secKey = std::to_string(i + 0xFFFF);
        SliceKey key = mGenerator->GenerateDualKey(priKey.data(), priKey.length(), secKey.data(), secKey.length());
        Value value = mGenerator->GenerateValue(10);
        kvPairList.push_back(KVPair(key, value));
    }

    // put key and values to slice.
    auto result = mSlice->Initialize(kvPairList, meta, mMemManager);
    ASSERT_EQ(result, BSS_OK);

    // get value by key from slice.
    auto &sliceSpace = mSlice->GetSliceSpace();
    std::vector<KeyValueRef> keyValue;
    for (uint32_t i = 0; i < kvCount; ++i) {
        auto kv = MakeRef<KeyValue>();
        sliceSpace.GetKeyValue(i, kv);
        keyValue.emplace_back(kv);
    }

    // check result.
    for (uint32_t i = 0; i < kvCount; ++i) {
        auto &key = keyValue[i]->key;
        bool found = false;
        for (const auto &kvPair : kvPairList) {
            if (key.Compare(kvPair.first) == 0) {
                auto &first = keyValue[i]->value;
                auto &second = kvPair.second;
                ASSERT_EQ(first.ValueType(), second.ValueType());
                ASSERT_EQ(first.ValueLen(), second.ValueLen());
                ASSERT_EQ(first.SeqId(), second.SeqId());
                ASSERT_TRUE(memcmp(first.ValueData(), second.ValueData(), first.ValueLen()) == 0);
                found = true;
            }
        }
        ASSERT_TRUE(found);
    }
}

TEST_F(TestSlice, test_slice_hash_conflict_return_ok)
{
    // prepare key and values.
    // add kv to slice.
    mSlice = std::make_shared<Slice>();
    SliceCreateMeta meta = { 1 };

    // create kv pair and add to list.
    uint32_t kvCount = 1020;
    uint16_t stateId = MAP << NO_13;
    std::vector<KVPair> kvPairList;
    std::vector<QueryKey> keys;
    std::vector<Value> values;
    for (uint32_t i = 0; i < kvCount; ++i) {
        std::string priKey = std::to_string(i + 0xFFFFFFFF);
        std::string secKey = std::to_string(i + 0xFFFF);
        SliceKey key = mGenerator->GenerateDualKey(priKey.data(), priKey.length(), secKey.data(), secKey.length());
        Value value = mGenerator->GenerateValue(10);
        kvPairList.push_back(KVPair(key, value));
    }

    // 构造hash冲突数量大于3的key
    for (uint32_t i = 0; i < NO_3; i++) {
        std::string priKey = std::to_string(i + kvCount + 0xFFFFFFFF);
        std::string secKey = std::to_string(i + kvCount + 0xFFFF);
        SliceKey key =
            mGenerator->GenerateHashCollisionDualKey(reinterpret_cast<uint8_t *>(const_cast<char *>(priKey.data())),
                                                     priKey.length(),
                                                     reinterpret_cast<uint8_t *>(const_cast<char *>(secKey.data())),
                                                     secKey.length(), kvPairList[0].first, NO_1024);
        Value value = mGenerator->GenerateValue(10);
        kvPairList.push_back(KVPair(key, value));
        uint8_t *priData = new uint8_t[priKey.length()];
        uint8_t *secData = new uint8_t[secKey.length()];
        memcpy_s(priData, priKey.length(), priKey.data(), priKey.length());
        memcpy_s(secData, secKey.length(), secKey.data(), secKey.length());
        BinaryData priKey1(priData, priKey.length());
        BinaryData secKey1(secData, secKey.length());
        QueryKey queryKey(stateId, key.KeyHashCode(), priKey1, secKey1);
        ASSERT_EQ(memcmp(queryKey.PriKey().KeyData(), key.PriKey().KeyData(), queryKey.PriKey().KeyLen()), 0);
        ASSERT_EQ(memcmp(queryKey.SecKey().KeyData(), key.SecKey().KeyData(), queryKey.SecKey().KeyLen()), 0);
        keys.emplace_back(queryKey);
        values.emplace_back(value);
    }
    kvCount += NO_3;
    // 构造hash冲突数量小于3的key
    std::string priKey = std::to_string(kvCount + 0xFFFFFFFF);
    std::string secKey = std::to_string(kvCount + 0xFFFF);
    SliceKey key =
        mGenerator->GenerateHashCollisionDualKey(reinterpret_cast<uint8_t *>(const_cast<char *>(priKey.data())),
                                                 priKey.length(),
                                                 reinterpret_cast<uint8_t *>(const_cast<char *>(secKey.data())),
                                                 secKey.length(), kvPairList[NO_6].first, NO_1024);
    Value value = mGenerator->GenerateValue(10);
    kvPairList.push_back(KVPair(key, value));
    uint8_t *priData = new uint8_t[priKey.length()];
    uint8_t *secData = new uint8_t[secKey.length()];
    memcpy_s(priData, priKey.length(), priKey.data(), priKey.length());
    memcpy_s(secData, secKey.length(), secKey.data(), secKey.length());
    BinaryData priKey1(priData, priKey.length());
    BinaryData secKey1(secData, secKey.length());
    QueryKey queryKey(stateId, key.KeyHashCode(), priKey1, secKey1);
    ASSERT_EQ(memcmp(queryKey.PriKey().KeyData(), key.PriKey().KeyData(), queryKey.PriKey().KeyLen()), 0);
    ASSERT_EQ(memcmp(queryKey.SecKey().KeyData(), key.SecKey().KeyData(), queryKey.SecKey().KeyLen()), 0);
    keys.emplace_back(queryKey);
    values.emplace_back(value);

    // put key and values to slice.
    auto result = mSlice->Initialize(kvPairList, meta, mMemManager);
    ASSERT_EQ(result, BSS_OK);
    for (uint32_t i = 0; i < keys.size(); ++i) {
        auto key = keys[i];
        Value value;
        result = mSlice->Get(key, value);
        Value oriValue = values[i];
        ASSERT_EQ(result, true);
        ASSERT_EQ(value.ValueType(), oriValue.ValueType());
        ASSERT_EQ(value.ValueLen(), oriValue.ValueLen());
        ASSERT_EQ(value.SeqId(), oriValue.SeqId());
        ASSERT_EQ(memcmp(value.ValueData(), oriValue.ValueData(), value.ValueLen()), 0);
        delete[] key.PriKey().KeyData();
        delete[] key.SecKey().KeyData();
    }
}
