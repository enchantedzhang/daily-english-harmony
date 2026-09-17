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

#ifndef BOOST_SS_KEY_VALUE_GENERATOR_H
#define BOOST_SS_KEY_VALUE_GENERATOR_H

#include <random>

#include "binary/query_binary.h"
#include "binary/slice_binary.h"
#include "binary/value/value.h"
#include "include/ref.h"
#include "test_utils.h"

namespace ock {
namespace bss {
class Generator : public Referable {
public:
    Generator(uint32_t seed = 0) : mSeed(seed)
    {
        auto mConfig = std::make_shared<Config>();
        mMemManager = std::make_shared<MemManager>(AllocatorType::DIRECT);
        mMemManager->Initialize(mConfig);
    }

    void GenerateRandomData(uint8_t *data, uint32_t len, uint32_t seed)
    {
        std::srand(seed);
        for (uint32_t i = 0; i < len; ++i) {
            data[i] = std::rand();
        }
    }

    SliceKey GenerateSglKey(uint32_t keyLen, uint16_t stateId)
    {
        uint8_t *keyData = new uint8_t[keyLen];
        GenerateRandomData(keyData, keyLen, mSeed++);
        SliceKey sliceKey = GenerateSglKey(keyData, keyLen, stateId);
        delete keyData;
        return sliceKey;
    }

    SliceKey GenerateSglKey(uint8_t *keyData, uint32_t keyLen, uint16_t stateId)
    {
        // single key buffer.
        ByteBufferRef priKeyBuffer = MakeRef<ByteBuffer>(keyLen, MemoryType::SLICE_TABLE, mMemManager);
        priKeyBuffer->Write(keyData, keyLen);

        // key hash code.
        uint32_t keyHashCode = test::HashForTest(keyData, keyLen);

        // prepare single key.
        BinaryData priKey(priKeyBuffer->Data(), keyLen);
        BufferRef buffer = StaticPointerCast<Buffer>(priKeyBuffer);
        QueryKey queryKey(stateId, keyHashCode, priKey, buffer);
        return SliceKey::Of(queryKey);
    }

    std::vector<SliceKey> GenerateDualKey(uint32_t priKeyLen, uint32_t secKeyLen, uint32_t secKeyNum)
    {
        std::vector<SliceKey> keyList;
        // primary key.
        uint8_t *priKeyData = new uint8_t[priKeyLen];
        GenerateRandomData(priKeyData, priKeyLen, mSeed++);

        // secondary key.
        uint8_t *secKeyData = new uint8_t[secKeyLen];
        for (uint32_t i = 0; i < secKeyNum; ++i) {
            GenerateRandomData(secKeyData, secKeyLen, mSeed++);
            auto dualKey = GenerateDualKey(priKeyData, priKeyLen, secKeyData, secKeyLen);
            keyList.push_back(dualKey);
        }

        delete[] priKeyData;
        delete[] secKeyData;

        return keyList;
    }

    std::vector<QueryKey> GenerateKey(uint32_t priKeyLen, uint32_t secKeyLen, uint32_t secKeyNum)
    {
        std::vector<QueryKey> keyList;
        // primary key.
        uint8_t *priKeyData = new uint8_t[priKeyLen];
        GenerateRandomData(priKeyData, priKeyLen, mSeed++);
        ByteBufferRef priKeyBuffer = MakeRef<ByteBuffer>(priKeyLen, MemoryType::SLICE_TABLE, mMemManager);
        priKeyBuffer->Write(priKeyData, priKeyLen);
        BinaryData prikey(priKeyBuffer->Data(), priKeyLen);
        uint32_t KeyHashCode = test::HashForTest(priKeyData, priKeyLen);
        uint16_t stateId = MAP << NO_13;
        // secondary key.
        uint8_t *secKeyData = new uint8_t[secKeyLen];
        for (uint32_t i = 0; i < secKeyNum; ++i) {
            GenerateRandomData(secKeyData, secKeyLen, mSeed++);
            ByteBufferRef secKeyBuffer = MakeRef<ByteBuffer>(secKeyLen, MemoryType::SLICE_TABLE, mMemManager);
            secKeyBuffer->Write(secKeyData, secKeyLen);
            BinaryData seckey(secKeyBuffer->Data(), priKeyLen);
            BufferRef buffer = MakeRef<CompositeBuffer>(priKeyBuffer, secKeyBuffer);
            QueryKey queryKey(stateId, KeyHashCode, prikey, seckey, buffer);
            keyList.push_back(queryKey);
        }

        return keyList;
    }

    SliceKey GenerateDualKey(uint32_t priKeyLen, uint32_t secKeyLen)
    {
        return GenerateDualKey(priKeyLen, secKeyLen, 1)[0];
    }

    QueryKey GenerateKey(uint32_t priKeyLen, uint32_t secKeyLen)
    {
        return GenerateKey(priKeyLen, secKeyLen, 1)[0];
    }

    SliceKey GenerateDualKey(const char *priKeyData, uint32_t priKeyLen, const char *secKeyData, uint32_t secKeyLen)
    {
        return GenerateDualKey(reinterpret_cast<uint8_t *>(const_cast<char *>(priKeyData)), priKeyLen,
                               reinterpret_cast<uint8_t *>(const_cast<char *>(secKeyData)), secKeyLen);
    }

    SliceKey GenerateDualKey(uint8_t *priKeyData, uint32_t priKeyLen, uint8_t *secKeyData, uint32_t secKeyLen)
    {
        // primary key.
        ByteBufferRef priKeyBuffer = MakeRef<ByteBuffer>(priKeyLen, MemoryType::SLICE_TABLE, mMemManager);
        priKeyBuffer->Write(priKeyData, priKeyLen);

        // secondary key.
        ByteBufferRef secKeyBuffer = MakeRef<ByteBuffer>(secKeyLen, MemoryType::SLICE_TABLE, mMemManager);
        secKeyBuffer->Write(secKeyData, secKeyLen);

        // hash code.
        uint32_t priKeyHashCode = test::HashForTest(priKeyData, priKeyLen);

        // state id;
        uint16_t stateId = MAP << NO_13;

        // prepare dual key.
        BinaryData priKey(priKeyBuffer->Data(), priKeyLen);
        BinaryData secKey(secKeyBuffer->Data(), secKeyLen);
        BufferRef buffer = MakeRef<CompositeBuffer>(priKeyBuffer, secKeyBuffer);
        QueryKey queryKey(stateId, priKeyHashCode, priKey, secKey, buffer);
        return SliceKey::Of(queryKey);
    }

    SliceKey GenerateHashCollisionDualKey(uint8_t *priKeyData, uint32_t priKeyLen, uint8_t *secKeyData,
                                          uint32_t secKeyLen, const SliceKey &key, uint32_t count)
    {
        auto index = key.MixedHashCode() & (count - 1);
        // primary key.
        ByteBufferRef priKeyBuffer = MakeRef<ByteBuffer>(priKeyLen, MemoryType::SLICE_TABLE, mMemManager);
        priKeyBuffer->Write(priKeyData, priKeyLen);

        // secondary key.
        ByteBufferRef secKeyBuffer = MakeRef<ByteBuffer>(secKeyLen, MemoryType::SLICE_TABLE, mMemManager);
        secKeyBuffer->Write(secKeyData, secKeyLen);
        // state id;
        uint16_t stateId = MAP << NO_13;
        uint32_t c = index & 0x3FF;
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<uint32_t> dist;
        // hash code.
        uint32_t secKeyHashCode = test::HashForTest(secKeyData, secKeyLen);
        uint32_t priKeyHashCode = secKeyHashCode ^ stateId ^ (c | (dist(gen) << 10));
        // prepare dual key.
        BinaryData priKey(priKeyBuffer->Data(), priKeyLen);
        BinaryData secKey(secKeyBuffer->Data(), secKeyLen);
        BufferRef buffer = MakeRef<CompositeBuffer>(priKeyBuffer, secKeyBuffer);
        QueryKey queryKey(stateId, priKeyHashCode, priKey, secKey, buffer);
        return SliceKey::Of(queryKey);
    }

    Value GenerateValue(uint32_t valueLen, uint64_t seqId = 0)
    {
        // value
        uint8_t *valueData = new uint8_t[valueLen];
        GenerateRandomData(valueData, valueLen, mSeed++);
        ByteBufferRef valueBuffer = MakeRef<ByteBuffer>(valueLen, MemoryType::SLICE_TABLE, mMemManager);
        valueBuffer->Write(valueData, valueLen);
        delete[] valueData;
        Value value;
        value.Init(PUT, valueLen, valueBuffer->Data(), seqId, valueBuffer);
        return value;
    }

    void InitKey(uint16_t stateId, const uint8_t *keyData, uint32_t keyLen, Key &key)
    {
        uint32_t hashCode = test::HashForTest(keyData, keyLen);
        PriKeyNode priKeyNode(stateId, hashCode, keyData, keyLen);
        key.Init(priKeyNode, nullptr);
    }

private:
    uint32_t mSeed;
    MemManagerRef mMemManager;
};
using GeneratorRef = Ref<Generator>;
}  // namespace bss
}  // namespace ock

#endif  // BOOST_SS_KEY_VALUE_GENERATOR_H
