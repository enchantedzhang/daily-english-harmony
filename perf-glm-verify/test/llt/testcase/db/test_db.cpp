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
#include <dirent.h>
#include <ftw.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "common/bss_log.h"
#include "gtest/gtest.h"
#include "include/boost_state_db.h"
#include "include/boost_state_table.h"
#include "kv_table/kv_table_iterator.h"
#include "kv_table/pq_table.h"
#include "slice_table/test_slice_table_manager.h"
#include "test_utils.h"

namespace ock {
namespace bss {
namespace test {
namespace test_kv_table {

struct DataComparator {
    bool operator()(const std::vector<uint8_t> &a, const std::vector<uint8_t> &b) const
    {
        uint32_t minLen = std::min(a.size(), b.size());
        for (uint32_t i = 0; i < minLen; ++i) {
            uint8_t aByte = a[i];
            uint8_t bByte = b[i];
            if (aByte != bByte) {
                return aByte < bByte;  // 直接比较
            }
        }
        return a.size() < b.size();
    }
    static int Compare(const std::vector<uint8_t> &a, const BinaryData &b)
    {
        if (a.size() != b.Length()) {
            return 1;
        }
        for (uint32_t i = 0; i < b.Length(); ++i) {
            uint8_t aByte = a[i];
            uint8_t bByte = b.Data()[i];
            if (aByte != bByte) {
                return 1;
            }
        }
        return 0;
    }
};

BoostStateDB *mDB;
BoostStateDB *mDB1;
KVTableRef kVTable;
NsKVTableRef nsKVTable;
KMapTableRef kMapTable;
NsKMapTableRef nsKMapTable;
KListTableRef kListTable;
NsKListTableRef nsKListTable;
KVTableRef kVTable1;
NsKVTableRef nsKVTable1;
KMapTableRef kMapTable1;
NsKMapTableRef nsKMapTable1;
KListTableRef kListTable1;
NsKListTableRef nsKListTable1;
BoostNativeMetric *metric = nullptr;
PQTableRef pqTable;
PQTableRef pqTable1;
std::vector<std::string> compressionLevelPolicy = { "lz4", "lz4", "lz4" };
std::string lsmStoreCompressionPolicy = "lz4";

std::map<std::vector<uint8_t>, std::vector<uint8_t>> mKV;
std::map<std::vector<uint8_t>, std::map<std::vector<uint8_t>, std::vector<uint8_t>>> mNsKV;
std::map<std::vector<uint8_t>, std::map<std::vector<uint8_t>, std::vector<uint8_t>>> mKMap;
std::map<NsKey, std::map<std::vector<uint8_t>, std::vector<uint8_t>>> mNsKMap;
std::map<std::vector<uint8_t>, std::vector<std::vector<uint8_t>>> mKList;
std::map<NsKey, std::vector<std::vector<uint8_t>>> mNsKList;
std::map<uint32_t, std::set<std::vector<uint8_t>, DataComparator>> mPQ;

std::vector<std::vector<uint8_t>> sourceKey;
std::vector<std::vector<uint8_t>> sourceSubKey;
std::vector<std::vector<uint8_t>> sourceNS;
std::vector<uint8_t> fixed2MSizeValue;
std::string ToString(const std::vector<uint8_t> &data)
{
    uint32_t printLen = std::min(NO_20, static_cast<uint32_t>(data.size()));
    std::ostringstream oss;
    oss << "[";
    for (uint32_t i = 0; i < printLen; i++) {
        oss << std::hex << std::uppercase << std::setw(NO_2) << std::setfill('0') << static_cast<int>(data[i]);
    }
    oss << "]" << std::endl;
    return oss.str();
}
void Print(const std::vector<uint8_t> &key, const std::vector<uint8_t> &value)
{
    std::ostringstream oss;
    oss << "key:" << ToString(key) << std::endl;
    oss << "value:" << ToString(value) << std::endl;
    std::cout << oss.str();
}
void Print(const std::string desc, const std::vector<uint8_t> &key)
{
    std::ostringstream oss;
    oss << desc << ", key:" << ToString(key) << std::endl;
    std::cout << oss.str();
}
void AppendUint32ToVector(std::vector<uint8_t> &vec, uint32_t value)
{
    uint32_t currentSize = vec.size();
    vec.resize(currentSize + sizeof(value));
    memcpy_s(vec.data() + currentSize, vec.size(), &value, sizeof(value));
}

// 拼接数据
std::vector<uint8_t> ConcatenateData(const std::vector<uint8_t> &key, const std::vector<uint8_t> &ns)
{
    std::vector<uint8_t> result;

    // 1. 插入 ns 的哈希值（4 字节）
    uint32_t nsHashCode = HashForTest(ns.data(), ns.size());
    AppendUint32ToVector(result, nsHashCode);

    // 2. 插入 key 的内容
    result.insert(result.end(), key.begin(), key.end());

    // 3. 插入 ns 的内容
    result.insert(result.end(), ns.begin(), ns.end());

    // 4. 插入 key 的长度（4 字节）
    auto keyLength = static_cast<uint32_t>(key.size());
    AppendUint32ToVector(result, keyLength);
    return result;
}

void KVPut(uint32_t groupStart = 0, uint32_t groupEnd = UINT32_MAX, bool useMDb = true)
{
    std::vector<uint8_t> key = GetRandomData(sourceKey);
    std::vector<uint8_t> value = GetRandomData();
    uint32_t keyHashCode = HashForTest(key.data(), key.size());
    auto group = keyHashCode % NO_128;
    if (group > groupEnd || group < groupStart) {
        return;
    }
    BinaryData priKey(key.data(), key.size());
    BinaryData val(value.data(), value.size());
    ASSERT_FALSE(priKey.IsNull());
    ASSERT_FALSE(val.IsNull());
    if (useMDb) {
        kVTable->Put(keyHashCode, priKey, val);
    } else {
        kVTable1->Put(keyHashCode, priKey, val);
    }
    mKV[key] = value;
}

void KVRemove(uint32_t groupStart = 0, uint32_t groupEnd = UINT32_MAX, bool useMDb = true)
{
    std::vector<uint8_t> key = GetRandomData(sourceKey);
    uint32_t keyHashCode = HashForTest(key.data(), key.size());
    auto group = keyHashCode % NO_128;
    if (group > groupEnd || group < groupStart) {
        return;
    }
    auto it = mKV.find(key);
    if (it != mKV.end()) {
        mKV.erase(it);
    }

    BinaryData priKey(key.data(), key.size());
    if (useMDb) {
        kVTable->Remove(keyHashCode, priKey);
    } else {
        kVTable1->Remove(keyHashCode, priKey);
    }
}

void NsKVPut(uint32_t groupStart = 0, uint32_t groupEnd = UINT32_MAX, bool useMDb = true)
{
    std::vector<uint8_t> key = GetRandomData(sourceKey);
    std::vector<uint8_t> ns = GetRandomData(sourceNS);
    std::vector<uint8_t> value = GetRandomData();
    uint32_t keyHashCode = HashForTest(key.data(), key.size());
    auto group = keyHashCode % NO_128;
    if (group > groupEnd || group < groupStart) {
        return;
    }

    mNsKV[key][ns] = value;
    BinaryData priKey(key.data(), key.size());
    BinaryData secKey(ns.data(), ns.size());
    BinaryData val(value.data(), value.size());
    if (useMDb) {
        nsKVTable->Put(keyHashCode, priKey, secKey, val);
    } else {
        nsKVTable1->Put(keyHashCode, priKey, secKey, val);
    }
}

void NsKVRemove(uint32_t groupStart = 0, uint32_t groupEnd = UINT32_MAX, bool useMDb = true)
{
    std::vector<uint8_t> key = GetRandomData(sourceKey);
    std::vector<uint8_t> ns = GetRandomData(sourceNS);
    uint32_t keyHashCode = HashForTest(key.data(), key.size());
    auto group = keyHashCode % NO_128;
    if (group > groupEnd || group < groupStart) {
        return;
    }

    auto it = mNsKV.find(key);
    if (it != mNsKV.end()) {
        auto &innerMap = it->second;
        auto i = innerMap.find(ns);
        if (i != innerMap.end()) {
            innerMap.erase(i);
            if (innerMap.empty()) {
                mNsKV.erase(it);
            }
        }
    }

    BinaryData priKey(key.data(), key.size());
    BinaryData secKey(ns.data(), ns.size());
    if (useMDb) {
        nsKVTable->Remove(keyHashCode, priKey, secKey);
    } else {
        nsKVTable1->Remove(keyHashCode, priKey, secKey);
    }
}

void KMapPut(uint32_t groupStart = 0, uint32_t groupEnd = UINT32_MAX, bool useMDb = true)
{
    std::vector<uint8_t> key = GetRandomData(sourceKey);
    std::vector<uint8_t> subKey = GetRandomData(sourceSubKey);
    std::vector<uint8_t> value = GetRandomData();
    uint32_t keyHashCode = HashForTest(key.data(), key.size());
    auto group = keyHashCode % NO_128;
    if (group > groupEnd || group < groupStart) {
        return;
    }

    mKMap[key][subKey] = value;
    BinaryData priKey(key.data(), key.size());
    BinaryData secKey(subKey.data(), subKey.size());
    BinaryData val(value.data(), value.size());
    if (useMDb) {
        kMapTable->Put(keyHashCode, priKey, secKey, val);
    } else {
        kMapTable1->Put(keyHashCode, priKey, secKey, val);
    }
}

void KMapRemove(uint32_t groupStart = 0, uint32_t groupEnd = UINT32_MAX, bool useMDb = true)
{
    std::vector<uint8_t> key = GetRandomData(sourceKey);
    std::vector<uint8_t> subKey = GetRandomData(sourceSubKey);
    uint32_t keyHashCode = HashForTest(key.data(), key.size());
    auto group = keyHashCode % NO_128;
    if (group > groupEnd || group < groupStart) {
        return;
    }

    auto it = mKMap.find(key);
    if (it != mKMap.end()) {
        auto &innerMap = it->second;
        auto i = innerMap.find(subKey);
        if (i != innerMap.end()) {
            innerMap.erase(i);
            if (innerMap.empty()) {
                mKMap.erase(it);
            }
        }
    }

    BinaryData priKey(key.data(), key.size());
    BinaryData secKey(subKey.data(), subKey.size());

    if (useMDb) {
        kMapTable->Remove(keyHashCode, priKey, secKey);
    } else {
        kMapTable1->Remove(keyHashCode, priKey, secKey);
    }
}

void KMapRemovePriKey(uint32_t groupStart = 0, uint32_t groupEnd = UINT32_MAX, bool useMDb = true)
{
    std::vector<uint8_t> key = GetRandomData(sourceKey);
    uint32_t keyHashCode = HashForTest(key.data(), key.size());
    auto group = keyHashCode % NO_128;
    if (group > groupEnd || group < groupStart) {
        return;
    }

    auto it = mKMap.find(key);
    if (it != mKMap.end()) {
        mKMap.erase(it);
        BinaryData priKey(key.data(), key.size());
        if (useMDb) {
            kMapTable->Remove(keyHashCode, priKey);
        } else {
            kMapTable1->Remove(keyHashCode, priKey);
        }
    }
}

void NsKMapPut(uint32_t groupStart = 0, uint32_t groupEnd = UINT32_MAX, bool useMDb = true)
{
    std::vector<uint8_t> key = GetRandomData(sourceKey);
    std::vector<uint8_t> ns = GetRandomData(sourceNS);
    std::vector<uint8_t> keyNs = ConcatenateData(key, ns);
    uint32_t keyHashCode = HashForTest(key.data(), key.size());
    auto group = keyHashCode % NO_128;
    if (group > groupEnd || group < groupStart) {
        return;
    }

    std::vector<uint8_t> subKey = GetRandomData(sourceSubKey);
    std::vector<uint8_t> value = GetRandomData();

    NsKey mapKey{ key, ns };
    mNsKMap[mapKey][subKey] = value;
    BinaryData priKey(keyNs.data(), keyNs.size());
    BinaryData secKey(subKey.data(), subKey.size());
    BinaryData val(value.data(), value.size());

    if (useMDb) {
        nsKMapTable->Put(keyHashCode, priKey, secKey, val);
    } else {
        nsKMapTable1->Put(keyHashCode, priKey, secKey, val);
    }
}

void NsKMapRemove(uint32_t groupStart = 0, uint32_t groupEnd = UINT32_MAX, bool useMDb = true)
{
    std::vector<uint8_t> key = GetRandomData(sourceKey);
    std::vector<uint8_t> ns = GetRandomData(sourceNS);
    std::vector<uint8_t> keyNs = ConcatenateData(key, ns);
    uint32_t keyHashCode = HashForTest(key.data(), key.size());
    auto group = keyHashCode % NO_128;
    if (group > groupEnd || group < groupStart) {
        return;
    }

    std::vector<uint8_t> subKey = GetRandomData(sourceSubKey);

    NsKey mapKey{ key, ns };
    auto it = mNsKMap.find(mapKey);
    if (it != mNsKMap.end()) {
        auto &innerMap = it->second;
        auto i = innerMap.find(subKey);
        if (i != innerMap.end()) {
            innerMap.erase(i);
            if (innerMap.empty()) {
                mNsKMap.erase(it);
            }
        }
    }

    BinaryData priKey(keyNs.data(), keyNs.size());
    BinaryData secKey(subKey.data(), subKey.size());
    if (useMDb) {
        nsKMapTable->Remove(keyHashCode, priKey, secKey);
    } else {
        nsKMapTable1->Remove(keyHashCode, priKey, secKey);
    }
}

void NsKMapRemovePriKey(uint32_t groupStart = 0, uint32_t groupEnd = UINT32_MAX, bool useMDb = true)
{
    std::vector<uint8_t> key = GetRandomData(sourceKey);
    std::vector<uint8_t> ns = GetRandomData(sourceNS);
    std::vector<uint8_t> keyNs = ConcatenateData(key, ns);
    uint32_t keyHashCode = HashForTest(key.data(), key.size());
    auto group = keyHashCode % NO_128;
    if (group > groupEnd || group < groupStart) {
        return;
    }

    NsKey mapKey{ key, ns };
    auto it = mNsKMap.find(mapKey);
    if (it != mNsKMap.end()) {
        mNsKMap.erase(it);
        BinaryData priKey(keyNs.data(), keyNs.size());
        if (useMDb) {
            nsKMapTable->Remove(keyHashCode, priKey);
        } else {
            nsKMapTable1->Remove(keyHashCode, priKey);
        }
    }
}

void KListPut(uint32_t groupStart = 0, uint32_t groupEnd = UINT32_MAX, bool useMDb = true)
{
    std::vector<uint8_t> key = GetRandomData(sourceKey);
    std::vector<uint8_t> value = GetRandomData();
    uint32_t keyHashCode = HashForTest(key.data(), key.size());
    auto group = keyHashCode % NO_128;
    if (group > groupEnd || group < groupStart) {
        return;
    }

    mKList[key] = { value };
    BinaryData tempKey(key.data(), key.size());
    BinaryData tempValue(value.data(), value.size());

    if (useMDb) {
        kListTable->Put(keyHashCode, tempKey, tempValue);
    } else {
        kListTable1->Put(keyHashCode, tempKey, tempValue);
    }
}

void KListAdd(uint32_t groupStart = 0, uint32_t groupEnd = UINT32_MAX, bool useMDb = true)
{
    std::vector<uint8_t> key = GetRandomData(sourceKey);
    std::vector<uint8_t> value = GetRandomData();
    uint32_t keyHashCode = HashForTest(key.data(), key.size());
    auto group = keyHashCode % NO_128;
    if (group > groupEnd || group < groupStart) {
        return;
    }

    mKList[key].push_back(value);
    BinaryData tempKey(key.data(), key.size());
    BinaryData tempValue(value.data(), value.size());

    if (useMDb) {
        kListTable->Add(keyHashCode, tempKey, tempValue);
    } else {
        kListTable1->Add(keyHashCode, tempKey, tempValue);
    }
}

void KListAddBigValue(uint32_t groupStart = 0, uint32_t groupEnd = UINT32_MAX, bool useMDb = true)
{
    std::vector<uint8_t> key = GetRandomData(sourceKey);
    uint32_t begin = GetRandomLen();
    std::vector<uint8_t> value(fixed2MSizeValue.begin() + begin, fixed2MSizeValue.begin() + begin + IO_SIZE_1M);

    uint32_t keyHashCode = HashForTest(key.data(), key.size());
    auto group = keyHashCode % NO_128;
    if (group > groupEnd || group < groupStart) {
        return;
    }

    mKList[key].emplace_back(value);
    BinaryData tempKey(key.data(), key.size());
    BinaryData tempValue(value.data(), value.size());

    if (useMDb) {
        kListTable->Add(keyHashCode, tempKey, tempValue);
    } else {
        kListTable1->Add(keyHashCode, tempKey, tempValue);
    }
}

void KListRemove(uint32_t groupStart = 0, uint32_t groupEnd = UINT32_MAX, bool useMDb = true)
{
    std::vector<uint8_t> key = GetRandomData(sourceKey);
    uint32_t keyHashCode = HashForTest(key.data(), key.size());
    auto group = keyHashCode % NO_128;
    if (group > groupEnd || group < groupStart) {
        return;
    }

    auto it = mKList.find(key);
    if (it != mKList.end()) {
        mKList.erase(it);
    }

    BinaryData tempKey(key.data(), key.size());
    if (useMDb) {
        kListTable->Remove(keyHashCode, tempKey);
    } else {
        kListTable1->Remove(keyHashCode, tempKey);
    }
}

void NsKListPut(uint32_t groupStart = 0, uint32_t groupEnd = UINT32_MAX, bool useMDb = true)
{
    std::vector<uint8_t> key = GetRandomData(sourceKey);
    std::vector<uint8_t> ns = GetRandomData(sourceNS);
    std::vector<uint8_t> keyNs = ConcatenateData(key, ns);
    uint32_t keyHashCode = HashForTest(key.data(), key.size());
    auto group = keyHashCode % NO_128;
    if (group > groupEnd || group < groupStart) {
        return;
    }

    std::vector<uint8_t> value = GetRandomData();

    NsKey mapKey{ key, ns };
    mNsKList[mapKey] = { value };

    BinaryData tempKeyNs(keyNs.data(), keyNs.size());
    BinaryData tempValue(value.data(), value.size());

    if (useMDb) {
        nsKListTable->Put(keyHashCode, tempKeyNs, tempValue);
    } else {
        nsKListTable1->Put(keyHashCode, tempKeyNs, tempValue);
    }
}

void NsKListAdd(uint32_t groupStart = 0, uint32_t groupEnd = UINT32_MAX, bool useMDb = true)
{
    std::vector<uint8_t> key = GetRandomData(sourceKey);
    std::vector<uint8_t> ns = GetRandomData(sourceNS);
    std::vector<uint8_t> keyNs = ConcatenateData(key, ns);
    uint32_t keyHashCode = HashForTest(key.data(), key.size());
    auto group = keyHashCode % NO_128;
    if (group > groupEnd || group < groupStart) {
        return;
    }

    std::vector<uint8_t> value = GetRandomData();

    NsKey mapKey{ key, ns };
    mNsKList[mapKey].push_back(value);

    BinaryData tempKeyNs(keyNs.data(), keyNs.size());
    BinaryData tempValue(value.data(), value.size());

    if (useMDb) {
        nsKListTable->Add(keyHashCode, tempKeyNs, tempValue);
    } else {
        nsKListTable1->Add(keyHashCode, tempKeyNs, tempValue);
    }
}

void NsKListRemove(uint32_t groupStart = 0, uint32_t groupEnd = UINT32_MAX, bool useMDb = true)
{
    std::vector<uint8_t> key = GetRandomData(sourceKey);
    std::vector<uint8_t> ns = GetRandomData(sourceNS);
    std::vector<uint8_t> keyNs = ConcatenateData(key, ns);
    uint32_t keyHashCode = HashForTest(key.data(), key.size());
    auto group = keyHashCode % NO_128;
    if (group > groupEnd || group < groupStart) {
        return;
    }

    NsKey mapKey{ key, ns };
    auto it = mNsKList.find(mapKey);
    if (it != mNsKList.end()) {
        mNsKList.erase(it);
    }

    BinaryData tempKeyNs(keyNs.data(), keyNs.size());
    if (useMDb) {
        nsKListTable->Remove(keyHashCode, tempKeyNs);
    } else {
        nsKListTable1->Remove(keyHashCode, tempKeyNs);
    }
}

void ValidateMetric()
{
    if (metric != nullptr) {
        uint32_t sliceMissCount = metric->GetMetrics(MetricType::SLICE_MISS_COUNT);
        uint32_t filterHitCount = metric->GetMetrics(MetricType::FILTER_HIT_COUNT);
        uint32_t filterMissCount = metric->GetMetrics(MetricType::FILTER_MISS_COUNT);
        uint32_t dataHitCount = metric->GetMetrics(MetricType::DATA_BLOCK_HIT_COUNT);
        uint32_t dataMissCount = metric->GetMetrics(MetricType::DATA_BLOCK_MISS_COUNT);
        uint32_t filterSuccess = metric->GetMetrics(MetricType::FILTER_SUCCESS_COUNT);
        uint32_t filterExistSuccess = metric->GetMetrics(MetricType::FILTER_EXIST_SUCCESS_COUNT);
        uint32_t filterExistFail = metric->GetMetrics(MetricType::FILTER_EXIST_FAIL_COUNT);
        ASSERT_LE(sliceMissCount, filterHitCount + filterMissCount);
        ASSERT_EQ(filterHitCount + filterMissCount, filterSuccess + filterExistSuccess + filterExistFail);
        ASSERT_LE(sliceMissCount, dataHitCount + dataMissCount);
    }
}

std::vector<uint8_t> GetPrefix(uint32_t groupId)
{
    std::vector<uint8_t> prefix;
    for (int i = 4; --i >= 0;) {
        prefix.emplace_back((groupId >> (i << 3)));
    }
    return prefix;
}
void PQTableAdd(uint32_t groupStart = 0, uint32_t groupEnd = UINT32_MAX, bool useMDb = true)
{
    std::vector<uint8_t> randomData = GetRandomData(false, 200);
    uint32_t hashcode = HashForTest(randomData.data(), randomData.size());
    uint32_t groupId = hashcode % 128;
    std::vector<uint8_t> prefix = GetPrefix(groupId);
    randomData.insert(randomData.begin(), prefix.begin(), prefix.end());
    BinaryData key(randomData.data(), randomData.size());
    std::set<std::vector<uint8_t>, DataComparator> &set = mPQ[groupId];
    set.emplace(randomData);
    pqTable->AddKey(key, hashcode);
    LOG_DEBUG("add " << key.ToString() << " groupId: " << groupId);
}

void PQTableRemove(uint32_t groupStart = 0, uint32_t groupEnd = UINT32_MAX, bool useMDb = true)
{
    std::vector<uint8_t> randomData = GetRandomData(false, 200);
    uint32_t hashcode = HashForTest(randomData.data(), randomData.size());
    uint32_t groupId = hashcode % 128;
    std::vector<uint8_t> prefix = GetPrefix(groupId);
    randomData.insert(randomData.begin(), prefix.begin(), prefix.end());
    BinaryData key(randomData.data(), randomData.size());
    mPQ[groupId].erase(randomData);
    pqTable->RemoveKey(key, hashcode);
    LOG_DEBUG("remove " << key.ToString() << " groupId: " << groupId);
}

void ValidateKV()
{
    for (auto &it : mKV) {
        std::vector<uint8_t> key = it.first;
        std::vector<uint8_t> value = it.second;

        BinaryData testKey(key.data(), key.size());
        BinaryData result = {};
        uint32_t hashcode = HashForTest(key.data(), key.size());
        auto retVal = kVTable->Get(hashcode, testKey, result);
        ASSERT_EQ(retVal, BSS_OK);
        ASSERT_EQ(result.Length(), value.size());
        auto cmp = memcmp(result.Data(), value.data(), value.size());
        ASSERT_EQ(cmp, 0);
    }

    ValidateMetric();

    std::set<std::vector<uint8_t>> expectedKeysSet;
    std::set<std::pair<std::vector<uint8_t>, std::vector<uint8_t>>> expectedEntrySet;
    for (auto &it : mKV) {
        std::vector<uint8_t> key = it.first;
        std::vector<uint8_t> value = it.second;
        expectedKeysSet.insert(key);
        expectedEntrySet.insert(std::make_pair(key, value));
    }

    std::set<std::pair<std::vector<uint8_t>, std::vector<uint8_t>>> tempEntrySet;
    auto entryIterator = kVTable->FullEntryIterator();
    while (entryIterator->HasNext()) {
        auto pair = entryIterator->Next();
        auto priKeykey = pair->key.PriKey();
        auto val = pair->value;

        std::vector<uint8_t> key(priKeykey.KeyData(), priKeykey.KeyData() + priKeykey.KeyLen());
        std::vector<uint8_t> value(val.ValueData(), val.ValueData() + val.ValueLen());

        tempEntrySet.insert(std::make_pair(key, value));
    }
    ASSERT_EQ(tempEntrySet, expectedEntrySet);
    delete entryIterator;
    tempEntrySet.clear();

    std::set<std::vector<uint8_t>> tempKeysSet;
    auto keysIterator = kVTable->KeysIterator({});
    while (keysIterator->HasNext()) {
        auto result = keysIterator->Next();
        std::vector<uint8_t> key(result->key.PriKey().KeyData(),
                                 result->key.PriKey().KeyData() + result->key.PriKey().KeyLen());
        tempKeysSet.insert(key);
    }
    ASSERT_EQ(tempKeysSet, expectedKeysSet);
    delete keysIterator;
    tempKeysSet.clear();
}

void ValidateKVTwoDB()
{
    for (auto &it : mKV) {
        std::vector<uint8_t> key = it.first;
        std::vector<uint8_t> value = it.second;

        BinaryData testKey(key.data(), key.size());
        BinaryData result = {};
        uint32_t hashCode = HashForTest(key.data(), key.size());
        auto group = hashCode % NO_128;
        BResult retVal = BSS_OK;
        if (group < NO_64) {
            retVal = kVTable->Get(hashCode, testKey, result);
        } else {
            retVal = kVTable1->Get(hashCode, testKey, result);
        }
        ASSERT_EQ(retVal, BSS_OK);
        ASSERT_EQ(result.Length(), value.size());
        auto cmp = memcmp(result.Data(), value.data(), value.size());
        ASSERT_EQ(cmp, 0);
    }
}

void ValidateNsKV()
{
    for (auto &it : mNsKV) {
        std::vector<uint8_t> key = it.first;
        BinaryData tempKey(key.data(), key.size());
        uint32_t keyHashCode = HashForTest(key.data(), key.size());
        std::map<std::vector<uint8_t>, std::vector<uint8_t>> innerMap = it.second;

        for (auto &i : innerMap) {
            std::vector<uint8_t> ns = i.first;
            std::vector<uint8_t> value = i.second;

            BinaryData tempNS(ns.data(), ns.size());
            BinaryData tempValue(value.data(), value.size(), nullptr);

            BinaryData readValue;
            BResult result = nsKVTable->Get(keyHashCode, tempKey, tempNS, readValue);
            ASSERT_EQ(result, BSS_OK);
            ASSERT_TRUE(readValue == tempValue);
        }
        auto entryIterator = nsKVTable->EntryIterator(keyHashCode, tempKey);
        while (entryIterator->HasNext()) {
            auto pair = entryIterator->Next();
            SecKeyNode secKeyNode = pair->key.SecKey();
            std::vector<uint8_t> mapKey(secKeyNode.KeyData(), secKeyNode.KeyData() + secKeyNode.KeyLen());
            std::vector<uint8_t> mapValue = mNsKV[key][mapKey];
            BinaryData tempValue(mapValue.data(), mapValue.size());

            ASSERT_EQ(pair->value.ValueLen(), tempValue.Length());
            ASSERT_EQ(*(pair->value.ValueData()), *(tempValue.Data()));
        }
        delete entryIterator;
    }
    std::map<std::vector<uint8_t>, std::set<std::vector<uint8_t>>> tempKv;
    for (const auto &entry : mNsKV) {
        const auto &key = entry.first;
        const auto &innerMap = entry.second;

        for (const auto &innerEntry : innerMap) {
            const auto &subkey = innerEntry.first;

            tempKv[subkey].insert(key);
        }
    }

    for (auto &it : tempKv) {
        std::vector<uint8_t> ns = it.first;
        BinaryData tempNS(ns.data(), ns.size());

        std::set<std::vector<uint8_t>> tempKeysSet;
        auto keysIterator = nsKVTable->KeysIterator(tempNS);
        while (keysIterator->HasNext()) {
            auto result = keysIterator->Next();
            std::vector<uint8_t> key(result->key.PriKey().KeyData(),
                                     result->key.PriKey().KeyData() + result->key.PriKey().KeyLen());
            tempKeysSet.insert(key);
            ASSERT_TRUE(it.second.count(key) > 0);
        }
        delete keysIterator;

        if (tempKeysSet != it.second) {
            SecKeyNode secKey(HashForTest(ns.data(), ns.size()), ns.data(), ns.size());
            std::cout << secKey.ToString() << std::endl;
        }
        ASSERT_EQ(tempKeysSet, it.second);
    }
}

void ValidateKMap()
{
    for (auto &it : mKMap) {
        std::vector<uint8_t> key = it.first;
        BinaryData tempKey(key.data(), key.size());
        uint32_t keyHashCode = HashForTest(key.data(), key.size());
        std::map<std::vector<uint8_t>, std::vector<uint8_t>> innerMap = it.second;
        for (auto &i : innerMap) {
            std::vector<uint8_t> subKey = i.first;
            std::vector<uint8_t> value = i.second;

            BinaryData tempSubKey(subKey.data(), subKey.size());
            BinaryData tempValue(value.data(), value.size(), nullptr);

            BinaryData readValue;
            BResult result = kMapTable->Get(keyHashCode, tempKey, tempSubKey, readValue);
            ASSERT_EQ(result, BSS_OK);
            ASSERT_TRUE(readValue == tempValue);
        }

        std::set<std::pair<std::vector<uint8_t>, std::vector<uint8_t>>> expectedEntrySet;
        for (auto &j : innerMap) {
            std::vector<uint8_t> innerKey = j.first;
            std::vector<uint8_t> innerValue = j.second;
            expectedEntrySet.insert(std::make_pair(innerKey, innerValue));
        }
        std::set<std::pair<std::vector<uint8_t>, std::vector<uint8_t>>> tempEntrySet;
        auto entryIterator = kMapTable->EntryIterator(keyHashCode, tempKey);
        while (entryIterator->HasNext()) {
            auto pair = entryIterator->Next();
            auto priKeykey = pair->key.SecKey();
            auto val = pair->value;

            std::vector<uint8_t> innerKey(priKeykey.KeyData(), priKeykey.KeyData() + priKeykey.KeyLen());
            std::vector<uint8_t> innerValue(val.ValueData(), val.ValueData() + val.ValueLen());
            tempEntrySet.insert(std::make_pair(innerKey, innerValue));
        }
        delete entryIterator;

        ASSERT_EQ(tempEntrySet, expectedEntrySet);
    }

    std::set<std::vector<uint8_t>> expectedKeysSet;
    for (auto &it : mKMap) {
        std::vector<uint8_t> key = it.first;
        expectedKeysSet.insert(key);
    }

    std::set<std::vector<uint8_t>> tempKeysSet;
    auto keysIterator = kMapTable->KeysIterator({});
    while (keysIterator->HasNext()) {
        auto next = keysIterator->Next();
        std::vector<uint8_t> key(next->key.PriKey().KeyData(),
                                 next->key.PriKey().KeyData() + next->key.PriKey().KeyLen());
        tempKeysSet.insert(key);
    }
    ASSERT_EQ(tempKeysSet, expectedKeysSet);

    auto iter = kMapTable->FullEntryIterator();
    uint32_t count = 0;
    ASSERT_TRUE(iter != nullptr);
    while (iter->HasNext()) {
        auto entry = iter->Next();
        ASSERT_TRUE(entry != nullptr);
        ASSERT_TRUE(entry->key.HasSecKey() == true);
        std::vector<uint8_t> priKey(entry->key.PriKey().KeyData(),
                                    entry->key.PriKey().KeyData() + entry->key.PriKey().KeyLen());
        auto it = mKMap.find(priKey);
        ASSERT_TRUE(it != mKMap.end());
        std::vector<uint8_t> secKey(entry->key.SecKey().KeyData(),
                                    entry->key.SecKey().KeyData() + entry->key.SecKey().KeyLen());
        auto item = it->second.find(secKey);
        ASSERT_TRUE(item != it->second.end());
        std::vector<uint8_t> value(entry->value.ValueData(), entry->value.ValueData() + entry->value.ValueLen());
        ASSERT_EQ(value, item->second);
    }
    delete keysIterator;
}

void ValidateNewKMap()
{
    for (auto &it : mKMap) {
        std::vector<uint8_t> key = it.first;
        BinaryData tempKey(key.data(), key.size());
        uint32_t keyHashCode = HashForTest(key.data(), key.size());
        std::map<std::vector<uint8_t>, std::vector<uint8_t>> innerMap = it.second;
        for (auto &i : innerMap) {
            std::vector<uint8_t> subKey = i.first;
            std::vector<uint8_t> value = i.second;
            BinaryData tempSubKey(subKey.data(), subKey.size());
            BinaryData tempValue(value.data(), value.size(), nullptr);

            BinaryData readValue;
            BResult result = kMapTable->Get(keyHashCode, tempKey, tempSubKey, readValue);
            ASSERT_EQ(result, BSS_OK);
            ASSERT_TRUE(readValue == tempValue);
        }
    }
}

void ValidateNewNsKMap()
{
    for (auto &it : mKMap) {
        std::vector<uint8_t> key = it.first;
        BinaryData tempKey(key.data(), key.size());

        std::map<std::vector<uint8_t>, std::vector<uint8_t>> innerMap = it.second;
        for (auto &i : innerMap) {
            std::vector<uint8_t> subKey = i.first;
            std::vector<uint8_t> value = i.second;
            BinaryData tempSubKey(subKey.data(), subKey.size());
            BinaryData tempValue(value.data(), value.size(), nullptr);

            uint32_t keyHashCode = HashForTest(key.data(), key.size());

            BinaryData readValue;
            BResult result = nsKMapTable->Get(keyHashCode, tempKey, tempSubKey, readValue);
            ASSERT_EQ(result, BSS_OK);
            ASSERT_TRUE(readValue == tempValue);
        }
    }
}

void ValidateNsKMap()
{
    for (auto &it : mNsKMap) {
        std::vector<uint8_t> ns = it.first.mNS;
        std::vector<uint8_t> key = it.first.mKey;
        uint32_t keyHashCode = HashForTest(key.data(), key.size());

        std::vector<uint8_t> keyNs = ConcatenateData(key, ns);
        BinaryData tempKeyNs(keyNs.data(), keyNs.size());

        std::map<std::vector<uint8_t>, std::vector<uint8_t>> innerMap = it.second;
        for (auto &i : innerMap) {
            std::vector<uint8_t> subKey = i.first;
            std::vector<uint8_t> value = i.second;

            BinaryData tempSubKey(subKey.data(), subKey.size());
            BinaryData tempValue(value.data(), value.size(), nullptr);

            BinaryData readValue = {};
            BResult result = nsKMapTable->Get(keyHashCode, tempKeyNs, tempSubKey, readValue);
            ASSERT_EQ(result, BSS_OK);
            ASSERT_TRUE(readValue == tempValue);
        }

        std::set<std::pair<std::vector<uint8_t>, std::vector<uint8_t>>> expectedEntrySet;
        for (auto &j : innerMap) {
            std::vector<uint8_t> innerKey = j.first;
            std::vector<uint8_t> innerValue = j.second;
            expectedEntrySet.insert(std::make_pair(innerKey, innerValue));
        }
        std::set<std::pair<std::vector<uint8_t>, std::vector<uint8_t>>> tempEntrySet;
        auto entryIterator = nsKMapTable->EntryIterator(HashForTest(key.data(), key.size()), tempKeyNs);
        while (entryIterator->HasNext()) {
            auto pair = entryIterator->Next();
            auto priKeykey = pair->key.SecKey();
            auto val = pair->value;

            std::vector<uint8_t> innerKey(priKeykey.KeyData(), priKeykey.KeyData() + priKeykey.KeyLen());
            std::vector<uint8_t> innerValue(val.ValueData(), val.ValueData() + val.ValueLen());
            tempEntrySet.insert(std::make_pair(innerKey, innerValue));
        }
        EXPECT_EQ(tempEntrySet, expectedEntrySet);
        delete entryIterator;
        tempEntrySet.clear();
    }

    std::map<std::vector<uint8_t>, std::set<std::vector<uint8_t>>> tempKv;
    for (const auto &entry : mNsKMap) {
        const auto &nsKey = entry.first;
        std::vector<uint8_t> ns = nsKey.mNS;
        std::vector<uint8_t> key = nsKey.mKey;

        tempKv[ns].insert(key);
    }

    for (auto &it : tempKv) {
        std::vector<uint8_t> ns = it.first;
        BinaryData tempNS(ns.data(), ns.size());

        std::set<std::vector<uint8_t>> tempKeysSet;
        auto keysIterator = nsKMapTable->KeysIterator(tempNS);
        while (keysIterator->HasNext()) {
            auto next = keysIterator->Next();
            std::vector<uint8_t> key(next->key.PriKey().RealKeyData(),
                                     next->key.PriKey().RealKeyData() + next->key.PriKey().RealKeyLen());
            tempKeysSet.insert(key);
        }
        delete keysIterator;
        ASSERT_EQ(tempKeysSet, it.second);
        tempKeysSet.clear();
    }
}

void ValidateKList(bool flag = false)
{
    for (auto &it : mKList) {
        std::vector<uint8_t> key = it.first;
        std::vector<std::vector<uint8_t>> value = it.second;

        BinaryData tempKey(reinterpret_cast<uint8_t *>(key.data()), static_cast<uint32_t>(key.size()));
        std::vector<uint32_t> resIds;
        uint32_t hashcode = HashForTest(key.data(), key.size());
        ListResult listCount = kListTable->Get(hashcode, tempKey);
        resIds.emplace_back(listCount.resId);
        int readSectionId = listCount.sectionReadId;
        while (readSectionId > 0) {
            ListResult temp = kListTable->SectionRead(readSectionId);
            readSectionId = temp.sectionReadId;
            if (temp.size > 0) {
                listCount.size += NO_1;
                listCount.lengths.insert(listCount.lengths.begin(), temp.lengths[0]);
                listCount.addresses.insert(listCount.addresses.begin(), temp.addresses[0]);
                resIds.emplace_back(temp.resId);
            }
        }

        uint32_t totalReadValueLen = 0;
        for (uint32_t i = 0; i < listCount.size; i++) {
            totalReadValueLen += listCount.lengths[i];
        }
        uint32_t totalExpectedValueLen = 0;
        for (uint32_t i = 0; i < value.size(); i++) {
            totalExpectedValueLen += value[i].size();
        }
        ASSERT_EQ(totalReadValueLen, totalExpectedValueLen);
        uint32_t mCompareSize = 0;
        uint32_t readValueIndex = 0;
        int32_t subReadValuePos = 0;
        for (uint32_t expectedValueIndex = 0; expectedValueIndex < value.size(); expectedValueIndex++) {
            uint32_t index = flag ? value.size() - expectedValueIndex - 1 : expectedValueIndex;
            BinaryData tempValue(value[index].data(), value[index].size());
            auto ret = memcmp(reinterpret_cast<uint8_t *>(listCount.addresses[readValueIndex]) + subReadValuePos,
                              tempValue.Data(), tempValue.Length());
            mCompareSize += tempValue.Length();
            subReadValuePos += tempValue.Length();
            if (subReadValuePos >= listCount.lengths[readValueIndex]) {
                subReadValuePos = 0;
                readValueIndex++;
            }
        }
        ASSERT_EQ(mCompareSize, totalReadValueLen);
        for (const auto &item : resIds) {
            kListTable->CleanResource(item);
        }
    }

    std::set<std::vector<uint8_t>> expectedKeysSet;
    for (auto &it : mKList) {
        std::vector<uint8_t> key = it.first;
        expectedKeysSet.insert(key);
    }

    std::set<std::vector<uint8_t>> tempKeysSet;
    auto keysIterator = kListTable->KeysIterator({});
    while (keysIterator->HasNext()) {
        auto next = keysIterator->Next();
        std::vector<uint8_t> key(next->key.PriKey().KeyData(),
                                 next->key.PriKey().KeyData() + next->key.PriKey().KeyLen());
        tempKeysSet.insert(key);
    }
    delete keysIterator;
    ASSERT_EQ(tempKeysSet, expectedKeysSet);
}

void ValidateNsKList(bool flag = false)
{
    for (auto &it : mNsKList) {
        std::vector<uint8_t> ns = it.first.mNS;
        std::vector<uint8_t> key = it.first.mKey;
        std::vector<uint8_t> keyNs = ConcatenateData(key, ns);
        std::vector<std::vector<uint8_t>> value = it.second;

        BinaryData tempKeyNs(reinterpret_cast<uint8_t *>(keyNs.data()), static_cast<uint32_t>(keyNs.size()));
        ListResult listCount = nsKListTable->Get(HashForTest(key.data(), key.size()), tempKeyNs);
        ASSERT_TRUE(listCount.size != 0);

        uint32_t totalReadValueLen = 0;
        for (uint32_t i = 0; i < listCount.size; i++) {
            totalReadValueLen += listCount.lengths[i];
        }
        uint32_t totalExpectedValueLen = 0;
        for (uint32_t i = 0; i < value.size(); i++) {
            totalExpectedValueLen += value[i].size();
        }
        ASSERT_EQ(totalReadValueLen, totalExpectedValueLen);
        uint32_t mCompareSize = 0;
        uint32_t readValueIndex = 0;
        int32_t subReadValuePos = 0;
        for (uint32_t expectedValueIndex = 0; expectedValueIndex < value.size(); expectedValueIndex++) {
            uint32_t index = flag ? value.size() - expectedValueIndex - 1 : expectedValueIndex;
            BinaryData tempValue(value[index].data(), value[index].size());
            ASSERT_EQ(memcmp(reinterpret_cast<uint8_t *>(listCount.addresses[readValueIndex]) + subReadValuePos,
                             tempValue.Data(), tempValue.Length()),
                      0);
            mCompareSize += tempValue.Length();
            subReadValuePos += tempValue.Length();
            if (subReadValuePos >= listCount.lengths[readValueIndex]) {
                subReadValuePos = 0;
                readValueIndex++;
            }
        }
        ASSERT_EQ(mCompareSize, totalReadValueLen);
        kListTable->CleanResource(listCount.resId);
    }

    std::map<std::vector<uint8_t>, std::set<std::vector<uint8_t>>> tempKv;
    for (const auto &entry : mNsKList) {
        const auto &nsKey = entry.first;
        std::vector<uint8_t> ns = nsKey.mNS;
        std::vector<uint8_t> key = nsKey.mKey;

        tempKv[ns].insert(key);
    }

    for (auto &it : tempKv) {
        std::vector<uint8_t> ns = it.first;
        BinaryData tempNS(ns.data(), ns.size());

        std::set<std::vector<uint8_t>> tempKeysSet;
        auto keysIterator = nsKListTable->KeysIterator(tempNS);
        while (keysIterator->HasNext()) {
            auto next = keysIterator->Next();
            std::vector<uint8_t> key(next->key.PriKey().RealKeyData(),
                                     next->key.PriKey().RealKeyData() + next->key.PriKey().RealKeyLen());
            tempKeysSet.insert(key);
        }
        delete keysIterator;
        ASSERT_EQ(tempKeysSet, it.second);
        tempKeysSet.clear();
    }
}

void ValidatePQ()
{
    uint32_t total = 0;
    uint32_t realTotal = 0;
    for (const auto &item : mPQ) {
        std::vector<uint8_t> prefix = GetPrefix(item.first);
        BinaryData data1(prefix.data(), prefix.size());
        auto iter = pqTable->KeyIterator(data1);
        auto iter1 = pqTable1->KeyIterator(data1);
        ASSERT_FALSE(iter1->HasNext());
        auto keys = item.second;
        auto key1 = keys.begin();
        total += keys.size();
        while (iter->HasNext()) {
            BinaryData key2 = iter->Next();
            int cmp = DataComparator::Compare(*key1, key2);
            if (cmp != 0) {
                LOG_INFO("EXPECT: " << ToString(*key1) << ", fact: " << key2.ToString() << "groupId: " << item.first);
            }
            ASSERT_EQ(cmp, 0);
            ++key1;
            realTotal++;
        }
        if (total != realTotal) {
            LOG_INFO("Total: " << total);
        }
        ASSERT_EQ(total, realTotal);
        iter->Close();
        iter1->Close();
        delete iter;
        delete iter1;
    }
}

// 定义操作配置
struct OperationConfig {
    double probability;                                  // 操作的概率（0~1）
    std::function<void(uint32_t, uint32_t, bool)> func;  // 对应的操作函数
};

// 根据概率选择并执行操作
void executeOperation(const std::vector<OperationConfig> &operationConfig, uint32_t groupStart = NO_0,
                      uint32_t groupEnd = UINT32_MAX, bool useMDb = true)
{
    // 随机数生成器
    double totalProbability = 0.0;
    for (const auto &config : operationConfig) {
        totalProbability += config.probability;
    }
    std::random_device rd;
    std::mt19937 gen(g_testSeed++);
    std::uniform_real_distribution<> dis(0.0, totalProbability);

    double randomValue = dis(gen);  // 生成 [0, 1) 之间的随机数
    double cumulativeProbability = 0.0;

    for (const auto &config : operationConfig) {
        cumulativeProbability += config.probability;
        if (randomValue < cumulativeProbability) {
            config.func(groupStart, groupEnd, useMDb);  // 执行对应的操作
            break;
        }
    }
}

void ValidateFiles()
{
    std::string currentDir = GetCurrentWorkingDirectory();
    DIR *dirp = opendir(currentDir.c_str());
    if (dirp) {
        struct dirent *entry;
        while ((entry = readdir(dirp)) != nullptr) {
            std::string fileName = entry->d_name;
            ASSERT_FALSE(fileName.find("sst") != std::string::npos);
        }
        closedir(dirp);
    }
}

TableRef CreateFromDB(StateType keyedStateType, std::string tableName, BoostStateDB *db)
{
    auto tblDesc = std::make_shared<TableDescription>(keyedStateType, tableName, -1, TableSerializer{},
                                                      db->GetConfig());
    return db->GetTableOrCreate(tblDesc);
}

BResult UpdateFromDBWithTTL(StateType keyedStateType, std::string tableName, BoostStateDB *db, int64_t ttl)
{
    auto tblDesc = std::make_shared<TableDescription>(keyedStateType, tableName, ttl, TableSerializer{},
                                                      db->GetConfig());
    return db->UpdateTtlConfig(tblDesc);
}

void WaitForTTLExpiration(int64_t ttl)
{
    uint64_t expirationTime = TimeStampUtil::GetCurrentTime() + static_cast<uint64_t>(ttl);
    uint64_t currentTime = TimeStampUtil::GetCurrentTime();
    while (currentTime < expirationTime) {
        usleep(NO_10000);
        currentTime = TimeStampUtil::GetCurrentTime();
    }
}

void ForceFlushToSlice(bool allDb = false)
{
    mDB->ForceTriggerTransform();
    if (allDb) {
        mDB1->ForceTriggerTransform();
    }
}

void ForceEvictToLsm(bool allDb = false)
{
    ForceFlushToSlice(allDb);
    mDB->ForceTriggerEvict();
    if (allDb) {
        mDB1->ForceTriggerEvict();
    }
}

void CleanCurrentVersion()
{
    mDB->ForceCleanCurrentVersion();
}

class TestDB : public ::testing::Test {
public:
    TestDB() = default;
    void SetUp() override;
    void TearDown() override;
    void PrepareForScaleIn();
    void PrepareDbWithBlob();
    void PrepareForTTL(int64_t ttlTime);
    void RestoreDb(std::string basePth, uint64_t cpId, std::string &restorePath);

    void CloseAllDb()
    {
        kVTable.reset();
        nsKVTable.reset();
        kMapTable.reset();
        nsKMapTable.reset();
        kListTable.reset();
        nsKListTable.reset();
        kVTable1.reset();
        nsKVTable1.reset();
        kMapTable1.reset();
        nsKMapTable1.reset();
        kListTable1.reset();
        nsKListTable1.reset();
        pqTable.reset();
        pqTable1.reset();
        if (mDB != nullptr) {
            mDB->Close();
            delete mDB;
            mDB = nullptr;
            metric = nullptr;
        }
        if (mDB1 != nullptr) {
            mDB1->Close();
            delete mDB1;
            mDB1 = nullptr;
        }
    }

    bool DeleteCpFile()
    {
        std::string currentDir = GetCurrentWorkingDirectory();
        std::string cpPath = currentDir + "/cp/";
        RemoveDirectoryRecursive(cpPath.c_str());
        return true;
    }

    bool RemoveDirectoryRecursive(const std::string &path)
    {
        DIR *dir = opendir(path.c_str());
        if (!dir) {
            return unlink(path.c_str()) == 0;
        }

        dirent *entry;
        while ((entry = readdir(dir)) != nullptr) {
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
                continue;
            }
            std::string fullPath = path + "/" + entry->d_name;
            if (entry->d_type == DT_DIR) {
                // 递归删除子目录
                if (!RemoveDirectoryRecursive(fullPath)) {
                    closedir(dir);
                    return false;
                }
            } else {
                // 删除文件
                if (unlink(fullPath.c_str())) {
                    closedir(dir);
                    return false;
                }
            }
        }
        closedir(dir);

        // 删除空目录
        return rmdir(path.c_str()) == 0;
    }
};

void TestDB::PrepareForScaleIn()
{
    mDB->Close();
    delete mDB;
    mDB = nullptr;
    mDB1->Close();
    delete mDB1;
    mDB1 = nullptr;
    ConfigRef config = std::make_shared<Config>();
    config->mMemorySegmentSize = IO_SIZE_4M;
    config->SetEvictMinSize(IO_SIZE_4M);
    config->Init(NO_0, NO_63, NO_128);
    config->SetFileStoreL0NumTrigger(NO_2);
    config->SetLsmStoreCompactionSwitch(true);
    config->SetLsmStoreCompressionPolicy(lsmStoreCompressionPolicy);
    config->SetCompressionLevelPolicy(compressionLevelPolicy);
    config->SetBackendUID("db1-test");
    config->SetTaskSlotFlag(NO_0);
    mDB = BoostStateDBFactory::Create();
    mDB->Open(config);

    ConfigRef config1 = std::make_shared<Config>();
    config1->mMemorySegmentSize = IO_SIZE_4M;
    config1->SetEvictMinSize(IO_SIZE_4M);
    config1->Init(NO_63, NO_127, NO_128);
    config1->SetFileStoreL0NumTrigger(NO_2);
    config1->SetLsmStoreCompactionSwitch(true);
    config1->SetLsmStoreCompressionPolicy(lsmStoreCompressionPolicy);
    config1->SetCompressionLevelPolicy(compressionLevelPolicy);
    config1->SetBackendUID("db2-test");
    config1->SetTaskSlotFlag(NO_1);
    mDB1 = BoostStateDBFactory::Create();
    mDB1->Open(config1);

    kVTable = std::dynamic_pointer_cast<KVTable>(CreateFromDB(StateType::VALUE, "kVTable", mDB));
    nsKVTable = std::dynamic_pointer_cast<NsKVTable>(CreateFromDB(StateType::SUB_VALUE, "nsKVTable", mDB));
    kMapTable = std::dynamic_pointer_cast<KMapTable>(CreateFromDB(StateType::MAP, "kMapTable", mDB));
    nsKMapTable = std::dynamic_pointer_cast<NsKMapTable>(CreateFromDB(StateType::SUB_MAP, "nsKMapTable", mDB));
    kListTable = std::dynamic_pointer_cast<KListTable>(CreateFromDB(StateType::LIST, "kListTable", mDB));
    nsKListTable = std::dynamic_pointer_cast<NsKListTable>(CreateFromDB(StateType::SUB_LIST, "nsKListTable", mDB));

    kVTable1 = std::dynamic_pointer_cast<KVTable>(CreateFromDB(StateType::VALUE, "kVTable", mDB1));
    nsKVTable1 = std::dynamic_pointer_cast<NsKVTable>(CreateFromDB(StateType::SUB_VALUE, "nsKVTable", mDB1));
    kMapTable1 = std::dynamic_pointer_cast<KMapTable>(CreateFromDB(StateType::MAP, "kMapTable", mDB1));
    nsKMapTable1 = std::dynamic_pointer_cast<NsKMapTable>(CreateFromDB(StateType::SUB_MAP, "nsKMapTable", mDB1));
    kListTable1 = std::dynamic_pointer_cast<KListTable>(CreateFromDB(StateType::LIST, "kListTable", mDB1));
    nsKListTable1 = std::dynamic_pointer_cast<NsKListTable>(CreateFromDB(StateType::SUB_LIST, "nsKListTable", mDB1));
}

void TestDB::PrepareDbWithBlob()
{
    CloseAllDb();
    std::string backupPath = GetCurrentWorkingDirectory() + "/backup";
    ConfigRef config = std::make_shared<Config>();
    config->mMemorySegmentSize = IO_SIZE_32M;
    config->SetEvictMinSize(IO_SIZE_32M);
    config->Init(NO_0, NO_127, NO_128);
    config->SetFileStoreL0NumTrigger(NO_2);
    config->SetLsmStoreCompactionSwitch(true);
    config->SetHeapAvailableSize(IO_SIZE_2G * NO_3);
    config->SetLsmStoreCompressionPolicy(lsmStoreCompressionPolicy);
    config->SetCompressionLevelPolicy(compressionLevelPolicy);
    config->SetBackupPath(backupPath);
    config->SetEnableKVSeparate(true);
    config->SetBlobDefaultBlockSize(IO_SIZE_16K);
    config->SetBlobFileSize(IO_SIZE_64K);
    config->mEnableTombstone = true;
    config->mMaxBlobNumInMemCache = 1000;
    config->mTombstoneDataBlockSize = IO_SIZE_64K;
    config->mTombstoneFileSize = IO_SIZE_256K;
    config->mTombstoneLevel0CompactionFileNum = 4;
    config->mBlobMinCompactionThreshold = 0;
    config->mBlobMaxSpaceAmplificationRatio = 0;
    mkdir(backupPath.c_str(), mode_t(NO_777));

    mDB = BoostStateDBFactory::Create();
    mDB->Open(config);
    metric = new BoostNativeMetric(NO_31);
    metric->Init();
    mDB->RegisterMetric(metric);

    kVTable = std::dynamic_pointer_cast<KVTable>(CreateFromDB(StateType::VALUE, "kVTable", mDB));
    nsKVTable = std::dynamic_pointer_cast<NsKVTable>(CreateFromDB(StateType::SUB_VALUE, "nsKVTable", mDB));
    kMapTable = std::dynamic_pointer_cast<KMapTable>(CreateFromDB(StateType::MAP, "kMapTable", mDB));
    nsKMapTable = std::dynamic_pointer_cast<NsKMapTable>(CreateFromDB(StateType::SUB_MAP, "nsKMapTable", mDB));
    kListTable = std::dynamic_pointer_cast<KListTable>(CreateFromDB(StateType::LIST, "kListTable", mDB));
    nsKListTable = std::dynamic_pointer_cast<NsKListTable>(CreateFromDB(StateType::SUB_LIST, "nsKListTable", mDB));
}

void TestDB::PrepareForTTL(int64_t ttlTime)
{
    auto boostStateDbImpl = reinterpret_cast<BoostStateDBImpl *>(mDB);
    boostStateDbImpl->GetConfig().SetTtlFilterSwitch(true);

    BResult result1 = UpdateFromDBWithTTL(StateType::VALUE, "kVTable", mDB, ttlTime);
    ASSERT_EQ(result1, BSS_OK);

    BResult result2 = UpdateFromDBWithTTL(StateType::MAP, "kMapTable", mDB, ttlTime);
    ASSERT_EQ(result2, BSS_OK);

    BResult result3 = UpdateFromDBWithTTL(StateType::LIST, "kListTable", mDB, ttlTime);
    ASSERT_EQ(result3, BSS_OK);
}

void TestDB::RestoreDb(std::string basePth, uint64_t cpId, std::string &restorePath)
{
    ConfigRef config = std::make_shared<Config>();
    config->Init(NO_0, NO_63, NO_128);
    config->mMemorySegmentSize = IO_SIZE_64M;
    config->SetLocalPath(basePth + std::to_string(cpId) + "/sst");
    config->SetBackendUID("cp-test-0");
    mDB = BoostStateDBFactory::Create();
    mDB->Open(config);
    restorePath = basePth + std::to_string(cpId);
    kVTable = nullptr;
    kVTable = std::dynamic_pointer_cast<KVTable>(CreateFromDB(StateType::VALUE, "kVTable", mDB));
    nsKVTable = std::dynamic_pointer_cast<NsKVTable>(CreateFromDB(StateType::SUB_VALUE, "nsKVTable", mDB));
    kMapTable = std::dynamic_pointer_cast<KMapTable>(CreateFromDB(StateType::MAP, "kMapTable", mDB));
    nsKMapTable = std::dynamic_pointer_cast<NsKMapTable>(CreateFromDB(StateType::SUB_MAP, "nsKMapTable", mDB));
    kListTable = std::dynamic_pointer_cast<KListTable>(CreateFromDB(StateType::LIST, "kListTable", mDB));
    nsKListTable = std::dynamic_pointer_cast<NsKListTable>(CreateFromDB(StateType::SUB_LIST, "nsKListTable", mDB));
    ConfigRef config1 = std::make_shared<Config>();
    config1->Init(NO_64, NO_127, NO_128);
    config1->mMemorySegmentSize = IO_SIZE_64M;
    config1->SetLocalPath(basePth + std::to_string(cpId) + "/sst");
    config1->SetBackendUID("cp-test-1");
    mDB1 = BoostStateDBFactory::Create();
    mDB1->Open(config1);

    kVTable1 = std::dynamic_pointer_cast<KVTable>(CreateFromDB(StateType::VALUE, "kVTable", mDB1));
    nsKVTable1 = std::dynamic_pointer_cast<NsKVTable>(CreateFromDB(StateType::SUB_VALUE, "nsKVTable", mDB1));
    kMapTable1 = std::dynamic_pointer_cast<KMapTable>(CreateFromDB(StateType::MAP, "kMapTable", mDB1));
    nsKMapTable1 = std::dynamic_pointer_cast<NsKMapTable>(CreateFromDB(StateType::SUB_MAP, "nsKMapTable", mDB1));
    kListTable1 = std::dynamic_pointer_cast<KListTable>(CreateFromDB(StateType::LIST, "kListTable", mDB1));
    nsKListTable1 = std::dynamic_pointer_cast<NsKListTable>(CreateFromDB(StateType::SUB_LIST, "nsKListTable", mDB1));
}

void TestDB::SetUp()
{
    fixed2MSizeValue = GetRandom2MData();
    std::string backupPath = GetCurrentWorkingDirectory() + "/backup";
    ConfigRef config = std::make_shared<Config>();
    config->mMemorySegmentSize = IO_SIZE_32M;
    config->SetEvictMinSize(IO_SIZE_32M);
    config->Init(NO_0, NO_127, NO_128);
    config->SetFileStoreL0NumTrigger(NO_2);
    config->SetLsmStoreCompactionSwitch(true);
    config->SetHeapAvailableSize(IO_SIZE_2G * NO_3);
    config->SetLsmStoreCompressionPolicy(lsmStoreCompressionPolicy);
    config->SetCompressionLevelPolicy(compressionLevelPolicy);
    config->SetBackupPath(backupPath);
    mkdir(backupPath.c_str(), mode_t(NO_777));

    mDB = BoostStateDBFactory::Create();
    mDB->Open(config);
    metric = new BoostNativeMetric(NO_31);
    metric->Init();
    mDB->RegisterMetric(metric);

    mDB1 = BoostStateDBFactory::Create();
    mDB1->Open(config);

    kVTable = std::dynamic_pointer_cast<KVTable>(CreateFromDB(StateType::VALUE, "kVTable", mDB));
    nsKVTable = std::dynamic_pointer_cast<NsKVTable>(CreateFromDB(StateType::SUB_VALUE, "nsKVTable", mDB));
    kMapTable = std::dynamic_pointer_cast<KMapTable>(CreateFromDB(StateType::MAP, "kMapTable", mDB));
    nsKMapTable = std::dynamic_pointer_cast<NsKMapTable>(CreateFromDB(StateType::SUB_MAP, "nsKMapTable", mDB));
    kListTable = std::dynamic_pointer_cast<KListTable>(CreateFromDB(StateType::LIST, "kListTable", mDB));
    nsKListTable = std::dynamic_pointer_cast<NsKListTable>(CreateFromDB(StateType::SUB_LIST, "nsKListTable", mDB));
    pqTable1 = mDB->CreatePQTable("stateName1");
    pqTable = mDB->CreatePQTable("stateName");

    // 从一个有限大小的随机数据源获取key和subKey和namespace, 增大相同的概率，方便测试
    const uint32_t KEY_NUM = 30;
    for (uint32_t idx = 0; idx < KEY_NUM; ++idx) {
        std::vector<uint8_t> tempKey = GetRandomData();
        sourceKey.push_back(tempKey);
    }

    const uint32_t SUBKEY_NUM = 30;
    for (uint32_t idx = 0; idx < SUBKEY_NUM; ++idx) {
        std::vector<uint8_t> tempSubKey = GetRandomData();
        sourceSubKey.push_back(tempSubKey);
    }

    const uint32_t NS_NUM = 30;
    for (uint32_t idx = 0; idx < NS_NUM; ++idx) {
        std::vector<uint8_t> tempNS = GetRandomData();
        sourceNS.push_back(tempNS);
    }
    CleanupSstFiles();
    CleanupBlobFiles();
    CleanupTombstoneFiles();
}

void TestDB::TearDown()
{
    kVTable.reset();
    nsKVTable.reset();
    kMapTable.reset();
    nsKMapTable.reset();
    kListTable.reset();
    nsKListTable.reset();
    pqTable.reset();
    kVTable1.reset();
    nsKVTable1.reset();
    kMapTable1.reset();
    nsKMapTable1.reset();
    kListTable1.reset();
    nsKListTable1.reset();
    pqTable1.reset();

    mKV.clear();
    mNsKV.clear();
    mKMap.clear();
    mNsKMap.clear();
    mKList.clear();
    mNsKList.clear();
    mPQ.clear();

    sourceKey.clear();
    sourceSubKey.clear();
    sourceNS.clear();
    DeleteCpFile();

    mDB->Close();
    delete mDB;
    mDB = nullptr;
    if (mDB1 != nullptr) {
        mDB1->Close();
        delete mDB1;
        mDB1 = nullptr;
    }

    CleanupSstFiles();
    CleanupBlobFiles();
    CleanupTombstoneFiles();
    DeleteCpFile();

    LOG_INFO("TestDB::TearDownTestCase finish.");
}

TEST_F(TestDB, SameTaskSlotDifferentMaxParallelismKeepsDbACodec)
{
    CloseAllDb();
    ConfigRef configA = std::make_shared<Config>(NO_0, NO_127, NO_128);
    configA->mMemorySegmentSize = IO_SIZE_32M;
    configA->SetEvictMinSize(IO_SIZE_1K);
    configA->SetBackendUID("codec-root-a");
    configA->SetTaskSlotFlag(NO_0);
    mDB = BoostStateDBFactory::Create();
    ASSERT_NE(mDB, nullptr);
    ASSERT_EQ(mDB->Open(configA), BSS_OK);
    kVTable = std::dynamic_pointer_cast<KVTable>(CreateFromDB(StateType::VALUE, "codec-root-kv", mDB));
    ASSERT_NE(kVTable, nullptr);

    std::array<uint8_t, 4> key = { 'r', 'o', 'o', 't' };
    std::array<uint8_t, 4> value = { 1, 2, 3, 4 };
    BinaryData keyData(key.data(), key.size());
    BinaryData valueData(value.data(), value.size());
    ASSERT_EQ(kVTable->Put(120U, keyData, valueData), BSS_OK);
    auto pqTableA = mDB->CreatePQTable("codec-root-pq");
    ASSERT_NE(pqTableA, nullptr);
    ASSERT_EQ(pqTableA->AddKey(keyData, 120U), BSS_OK);

    ConfigRef configB = std::make_shared<Config>(NO_0, NO_255, NO_256);
    configB->mMemorySegmentSize = IO_SIZE_32M;
    configB->SetEvictMinSize(IO_SIZE_1K);
    configB->SetBackendUID("codec-root-b");
    configB->SetTaskSlotFlag(NO_0);
    mDB1 = BoostStateDBFactory::Create();
    ASSERT_NE(mDB1, nullptr);
    ASSERT_EQ(mDB1->Open(configB), BSS_OK);
    kVTable1 = std::dynamic_pointer_cast<KVTable>(CreateFromDB(StateType::VALUE, "codec-root-kv", mDB1));
    ASSERT_NE(kVTable1, nullptr);

    auto expectValue = [&] {
        BinaryData actual;
        ASSERT_EQ(kVTable->Get(120U, keyData, actual), BSS_OK);
        ASSERT_EQ(actual.Length(), value.size());
        EXPECT_EQ(memcmp(actual.Data(), value.data(), value.size()), 0);
    };
    expectValue();
    mDB->ForceTriggerTransform();
    expectValue();
    mDB->ForceTriggerEvict();
    expectValue();
    ASSERT_EQ(pqTableA->TriggerSegmentFlush(true), BSS_OK);

    std::string checkpointRoot = GetCurrentWorkingDirectory() + "/codec-root-checkpoint";
    std::string checkpointPath = checkpointRoot + "/1";
    mkdir(checkpointRoot.c_str(), mode_t(NO_777));
    mkdir(checkpointPath.c_str(), mode_t(NO_777));
    ASSERT_NE(mDB->CreateSyncCheckpoint(checkpointPath, 1), nullptr);
    ASSERT_EQ(mDB->CreateAsyncCheckpoint(1, false), BSS_OK);

    auto savepoint = mDB->TriggerSavepoint();
    ASSERT_NE(savepoint, nullptr);
    auto iterator = savepoint->SavepointIterator();
    ASSERT_NE(iterator, nullptr);
    bool sawKv = false;
    bool sawPq = false;
    while (iterator->HasNext()) {
        auto item = iterator->Next();
        ASSERT_NE(item, nullptr);
        sawKv = sawKv || (item->mStateType == VALUE && item->mKeyGroup == 120U);
        sawPq = sawPq || (item->mStateType == PQ && item->mKeyGroup == 120U);
    }
    EXPECT_TRUE(sawKv);
    EXPECT_TRUE(sawPq);
    iterator->Close();
    savepoint->Close();
    delete savepoint;
    pqTableA.reset();
}

TEST_F(TestDB, test_sp_list_and_get_return_ok)
{
    auto skipRemove = [](uint32_t, uint32_t, bool) { (void)GetRandomData(sourceKey); };
    std::vector<OperationConfig> operationsConfig = { { 0.9, KListAddBigValue }, { 0.1, skipRemove } };
    for (uint32_t i = 0; i < NO_1; i++) {
        for (uint32_t j = 0; j < NO_1000; j++) {
            executeOperation(operationsConfig);
        }
    }

    SavepointDataView *dataView = mDB->TriggerSavepoint();
    auto iterator = dataView->SavepointIterator();
    ASSERT_TRUE(dataView != nullptr);
    uint32_t count = 0;
    std::map<std::vector<uint8_t>, uint32_t> map;
    std::vector<uint8_t> lastKey;
    while (iterator->HasNext()) {
        BinaryKeyValueItemRef item = iterator->Next();
        std::vector<uint8_t> key(item->mKey, item->mKey + item->mKeyLength);
        std::vector<uint8_t> value(item->mValue, item->mValue + item->mValueLength);
        auto iter = map.find(key);
        if (iter != map.end()) {
            map[key] = iter->second + value.size();
        } else {
            count++;
            map[key] = value.size();
        }
        if (!lastKey.empty() && lastKey != key) {
            uint32_t totalExpectedValueLen = 0;
            for (uint32_t i = 0; i < mKList[lastKey].size(); i++) {
                totalExpectedValueLen += mKList[lastKey][i].size();
            }
            ASSERT_EQ(map.find(lastKey)->second, totalExpectedValueLen);
        }
        lastKey = key;
    }
    uint32_t totalExpectedValueLen = 0;
    for (uint32_t i = 0; i < mKList[lastKey].size(); i++) {
        totalExpectedValueLen += mKList[lastKey][i].size();
    }
    ASSERT_EQ(map.find(lastKey)->second, totalExpectedValueLen);
    uint32_t expectCount = 0;
    ASSERT_EQ(mKList.size(), count);
}

TEST_F(TestDB, test_put_add_remove_bigklist_to_lsm_store_and_get_return_ok)
{
    std::vector<OperationConfig> operationsConfig = {
        { 0.05, KListPut },
        { 0.9, KListAddBigValue },
        { 0.05, KListRemove },
    };
    for (uint32_t i = 0; i < NO_3; i++) {
        for (uint32_t j = 0; j < NO_1024; j++) {
            executeOperation(operationsConfig);
        }
        ValidateKList();
    }
    CleanCurrentVersion();
    ValidateFiles();
}

TEST_F(TestDB, test_KV_to_lsm_store_and_get_return_ok_with_ttl)
{
    PrepareForTTL(NO_5 * NO_1000);
    std::vector<OperationConfig> operationsConfig = { { 0.9, KVPut }, { 0.1, KVRemove } };
    for (uint32_t i = 0; i < NO_1; i++) {
        for (uint32_t j = 0; j < NO_10000; j++) {
            executeOperation(operationsConfig);
        }
        ForceEvictToLsm();
        ValidateKV();
    }
}

TEST_F(TestDB, test_KV_to_lsm_store_and_get_return_false_with_ttl)
{
    PrepareForTTL(NO_5 * NO_1000);
    std::vector<OperationConfig> operationsConfig = { { 0.9, KVPut }, { 0.1, KVRemove } };
    for (uint32_t i = 0; i < NO_1; i++) {
        for (uint32_t j = 0; j < NO_10000; j++) {
            executeOperation(operationsConfig);
        }
        WaitForTTLExpiration(NO_5 * NO_1000);
        ForceEvictToLsm();

        for (auto &it : mKV) {
            std::vector<uint8_t> key = it.first;
            std::vector<uint8_t> value = it.second;
            BinaryData testKey(key.data(), key.size());
            BinaryData result = {};
            auto retVal = kVTable->Get(HashForTest(key.data(), key.size()), testKey, result);
            ASSERT_EQ(retVal, BSS_NOT_EXISTS);
        }
    }
}

TEST_F(TestDB, test_KV_to_lsm_store_and_get_return_ok_with_update_ttl)
{
    PrepareForTTL(NO_5 * NO_1000);
    std::vector<OperationConfig> operationsConfig = { { 0.9, KVPut }, { 0.1, KVRemove } };
    for (uint32_t i = 0; i < NO_1; i++) {
        for (uint32_t j = 0; j < NO_10000; j++) {
            executeOperation(operationsConfig);
        }
        BResult result = UpdateFromDBWithTTL(StateType::VALUE, "kVTable", mDB, NO_10 * NO_1000);
        ASSERT_EQ(result, BSS_OK);
        sleep(NO_6);
        ForceEvictToLsm();
        ValidateKV();
    }
}

TEST_F(TestDB, test_KV_to_lsm_store_and_get_return_ok_with_no_ttl)
{
    PrepareForTTL(-1);
    std::vector<OperationConfig> operationsConfig = { { 0.9, KVPut }, { 0.1, KVRemove } };
    for (uint32_t i = 0; i < NO_1; i++) {
        for (uint32_t j = 0; j < NO_10000; j++) {
            executeOperation(operationsConfig);
        }
        sleep(NO_6);
        ForceEvictToLsm();

        ValidateKV();
    }
}

TEST_F(TestDB, test_KMap_to_lsm_store_and_get_return_ok_with_ttl)
{
    PrepareForTTL(NO_5 * NO_1000);
    std::vector<OperationConfig> operationsConfig = { { 0.9, KMapPut }, { 0.1, KMapRemove } };
    for (uint32_t i = 0; i < NO_1; i++) {
        for (uint32_t j = 0; j < NO_10000; j++) {
            executeOperation(operationsConfig);
        }
        ForceEvictToLsm();

        ValidateKMap();
    }
}

TEST_F(TestDB, test_KMap_to_lsm_store_and_get_return_false_with_ttl)
{
    PrepareForTTL(NO_5 * NO_1000);
    std::vector<OperationConfig> operationsConfig = { { 0.9, KMapPut }, { 0.1, KMapRemove } };
    for (uint32_t i = 0; i < NO_1; i++) {
        for (uint32_t j = 0; j < NO_10000; j++) {
            executeOperation(operationsConfig);
        }
        WaitForTTLExpiration(NO_5 * NO_1000);
        ForceEvictToLsm();

        for (auto &it : mKMap) {
            std::vector<uint8_t> key = it.first;
            BinaryData tempKey(key.data(), key.size());
            uint32_t keyHashCode = HashForTest(key.data(), key.size());
            std::map<std::vector<uint8_t>, std::vector<uint8_t>> innerMap = it.second;
            for (auto &pair : innerMap) {
                std::vector<uint8_t> subKey = pair.first;
                std::vector<uint8_t> value = pair.second;

                BinaryData tempSubKey(subKey.data(), subKey.size());
                BinaryData tempValue(value.data(), value.size(), nullptr);

                BinaryData readValue;
                BResult result = kMapTable->Get(keyHashCode, tempKey, tempSubKey, readValue);
                ASSERT_EQ(result, BSS_NOT_EXISTS);
            }
        }
    }
}

TEST_F(TestDB, test_KMap_to_lsm_store_and_get_return_ok_with_no_ttl)
{
    PrepareForTTL(-1);
    std::vector<OperationConfig> operationsConfig = { { 0.9, KMapPut }, { 0.1, KMapRemove } };
    for (uint32_t i = 0; i < NO_1; i++) {
        for (uint32_t j = 0; j < NO_10000; j++) {
            executeOperation(operationsConfig);
        }
        sleep(NO_6);
        ForceEvictToLsm();

        ValidateKMap();
    }
}

TEST_F(TestDB, test_KList_to_lsm_store_and_get_return_ok_with_ttl)
{
    PrepareForTTL(NO_5 * NO_1000);
    std::vector<OperationConfig> operationsConfig = { { 0.1, KListPut }, { 0.8, KListAdd }, { 0.1, KListRemove } };
    for (uint32_t i = 0; i < NO_1; i++) {
        for (uint32_t j = 0; j < NO_10000; j++) {
            executeOperation(operationsConfig);
        }
        ForceEvictToLsm();

        ValidateKList();
    }
}

TEST_F(TestDB, test_KList_to_lsm_store_and_get_return_false_with_ttl)
{
    PrepareForTTL(NO_5 * NO_1000);
    std::vector<OperationConfig> operationsConfig = { { 0.1, KListPut }, { 0.8, KListAdd }, { 0.1, KListRemove } };
    for (uint32_t i = 0; i < NO_1; i++) {
        for (uint32_t j = 0; j < NO_10000; j++) {
            executeOperation(operationsConfig);
        }
        WaitForTTLExpiration(NO_5 * NO_1000);
        ForceEvictToLsm();

        for (auto &it : mKList) {
            std::vector<uint8_t> key = it.first;
            std::vector<std::vector<uint8_t>> value = it.second;

            BinaryData tempKey(reinterpret_cast<uint8_t *>(key.data()), static_cast<uint32_t>(key.size()));
            ListResult listCount = kListTable->Get(HashForTest(key.data(), key.size()), tempKey);
            ASSERT_TRUE(listCount.size == 0);
        }
    }
}

TEST_F(TestDB, test_KList_to_lsm_store_and_get_return_ok_with_no_ttl)
{
    PrepareForTTL(-1);
    std::vector<OperationConfig> operationsConfig = { { 0.1, KListPut }, { 0.8, KListAdd }, { 0.1, KListRemove } };
    for (uint32_t i = 0; i < NO_1; i++) {
        for (uint32_t j = 0; j < NO_10000; j++) {
            executeOperation(operationsConfig);
        }
        sleep(NO_6);
        ForceEvictToLsm();

        ValidateKList();
    }
}

TEST_F(TestDB, test_all_states_to_lsm_store_and_get_return_ok_with_ttl)
{
    PrepareForTTL(NO_10 * NO_1000);
    std::vector<OperationConfig> operationsConfig = { { 0.9, KVPut },      { 0.1, KVRemove }, { 0.9, KMapPut },
                                                      { 0.1, KMapRemove }, { 0.1, KListPut }, { 0.8, KListAdd },
                                                      { 0.1, KListRemove } };
    for (uint32_t i = 0; i < NO_1; i++) {
        for (uint32_t j = 0; j < NO_10000; j++) {
            executeOperation(operationsConfig);
        }
        BResult ret1 = UpdateFromDBWithTTL(StateType::VALUE, "kVTable", mDB, NO_5 * NO_1000);
        ASSERT_EQ(ret1, BSS_OK);
        BResult ret2 = UpdateFromDBWithTTL(StateType::LIST, "kListTable", mDB, NO_20 * NO_1000);
        ASSERT_EQ(ret2, BSS_OK);
        WaitForTTLExpiration(NO_5 * NO_1000);
        ForceEvictToLsm();

        for (auto &it : mKV) {
            std::vector<uint8_t> key = it.first;
            std::vector<uint8_t> value = it.second;
            BinaryData testKey(key.data(), key.size());
            BinaryData result = {};
            auto retVal = kVTable->Get(HashForTest(key.data(), key.size()), testKey, result);
            ASSERT_EQ(retVal, BSS_NOT_EXISTS);
        }
        ValidateKList();
        ValidateKMap();
    }
}

TEST_F(TestDB, test_put_remove_kvalue_to_fresh_table_and_get_return_ok)
{
    // Q12 Q17 Q18 Q19 Q20
    std::vector<OperationConfig> operationsConfig = {
        { 0.9, KVPut },
        { 0.1, KVRemove },
    };
    for (uint32_t i = 0; i < NO_1; i++) {
        for (uint32_t j = 0; j < NO_10000; j++) {
            executeOperation(operationsConfig);
        }
        ValidateKV();
    }
}

TEST_F(TestDB, test_put_remove_kvalue_to_slice_table_and_get_return_ok)
{
    // Q12 Q17 Q18 Q19 Q20
    std::vector<OperationConfig> operationsConfig = {
        { 0.9, KVPut },
        { 0.1, KVRemove },
    };
    for (uint32_t i = 0; i < NO_1; i++) {
        for (uint32_t j = 0; j < NO_10000; j++) {
            executeOperation(operationsConfig);
        }
        ForceFlushToSlice();
        ValidateKV();
    }
}

TEST_F(TestDB, test_put_remove_kvalue_to_lsm_store_and_get_return_ok)
{
    // Q12 Q17 Q18 Q19 Q20
    std::vector<OperationConfig> operationsConfig = {
        { 0.9, KVPut },
        { 0.1, KVRemove },
    };
    for (uint32_t i = 0; i < NO_1; i++) {
        for (uint32_t j = 0; j < NO_10; j++) {
            executeOperation(operationsConfig);
        }
        ForceEvictToLsm();
        ValidateKV();
    }
    CleanCurrentVersion();
    ValidateFiles();
}

TEST_F(TestDB, test_put_remove_ns_kv_to_fresh_table_and_get_return_ok)
{
    std::vector<OperationConfig> operationsConfig = {
        { 0.9, NsKVPut },
        { 0.1, NsKVRemove },
    };
    for (uint32_t i = 0; i < NO_1; i++) {
        for (uint32_t j = 0; j < NO_10000; j++) {
            executeOperation(operationsConfig);
        }
        ValidateNsKV();
    }
}

TEST_F(TestDB, test_put_remove_ns_kv_to_slice_table_and_get_return_ok)
{
    std::vector<OperationConfig> operationsConfig = {
        { 0.9, NsKVPut },
        { 0.1, NsKVRemove },
    };
    for (uint32_t i = 0; i < NO_1; i++) {
        for (uint32_t j = 0; j < NO_10000; j++) {
            executeOperation(operationsConfig);
        }
        ForceFlushToSlice();
        ValidateNsKV();
    }
}

TEST_F(TestDB, test_put_remove_ns_kv_to_lsm_store_and_get_return_ok)
{
    std::vector<OperationConfig> operationsConfig = {
        { 0.9, NsKVPut },
        { 0.1, NsKVRemove },
    };
    for (uint32_t i = 0; i < NO_3; i++) {
        for (uint32_t j = 0; j < NO_10000; j++) {
            executeOperation(operationsConfig);
        }
        ForceEvictToLsm();
        ValidateNsKV();
    }
    CleanCurrentVersion();
    ValidateFiles();
}

TEST_F(TestDB, test_put_kmap_to_fresh_table_and_get_return_ok)
{
    // Q3
    std::vector<OperationConfig> operationsConfig = {
        { 0.85, KMapPut },
        { 0.1, KMapRemove },
        { 0.05, KMapRemovePriKey },
    };
    for (uint32_t i = 0; i < NO_1; i++) {
        for (uint32_t j = 0; j < NO_1000; j++) {
            executeOperation(operationsConfig);
        }
        ValidateKMap();
    }
}

TEST_F(TestDB, test_put_kmap_to_slice_table_and_get_return_ok)
{
    // Q3
    std::vector<OperationConfig> operationsConfig = {
        { 0.85, KMapPut },
        { 0.1, KMapRemove },
        { 0.05, KMapRemovePriKey },
    };
    for (uint32_t i = 0; i < NO_1; i++) {
        for (uint32_t j = 0; j < NO_10000; j++) {
            executeOperation(operationsConfig);
        }
        ForceFlushToSlice();
        ValidateKMap();
    }
}

TEST_F(TestDB, test_put_kmap_to_lsm_store_and_get_return_ok)
{
    // Q3
    std::vector<OperationConfig> operationsConfig = {
        { 0.85, KMapPut },
        { 0.1, KMapRemove },
        { 0.05, KMapRemovePriKey },
    };
    for (uint32_t i = 0; i < NO_3; i++) {
        for (uint32_t j = 0; j < NO_1000; j++) {
            executeOperation(operationsConfig);
        }
        ForceEvictToLsm();
        ValidateKMap();
    }
    CleanCurrentVersion();
    ValidateFiles();
}

TEST_F(TestDB, test_put_1_key_and_get_kmap_return_ok)
{
    std::vector<OperationConfig> operationsConfig = {
        { 1, KMapPut },
    };
    for (uint32_t i = 0; i < NO_1; i++) {
        for (uint32_t j = 0; j < NO_1; j++) {
            executeOperation(operationsConfig);
        }
        ValidateNewKMap();
    }
}

TEST_F(TestDB, test_put_1K_key_and_get_kmap_return_ok)
{
    std::vector<OperationConfig> operationsConfig = {
        { 1, KMapPut },
    };
    for (uint32_t i = 0; i < NO_1; i++) {
        for (uint32_t j = 0; j < NO_1024; j++) {
            executeOperation(operationsConfig);
        }
        ValidateNewKMap();
    }
}

TEST_F(TestDB, test_put_remove_1K_key_and_get_kmap_return_ok)
{
    std::vector<OperationConfig> operationsConfig = {
        { 0.85, KMapPut },
        { 0.1, KMapRemove },
        { 0.05, KMapRemovePriKey },
    };
    for (uint32_t i = 0; i < NO_1; i++) {
        for (uint32_t j = 0; j < NO_1024; j++) {
            executeOperation(operationsConfig);
        }
        ValidateNewKMap();
    }
}

TEST_F(TestDB, test_put_remove_1K_key_and_get_nskmap_return_ok)
{
    std::vector<OperationConfig> operationsConfig = {
        { 0.85, NsKMapPut },
        { 0.1, NsKMapRemove },
        { 0.05, NsKMapRemovePriKey },
    };
    for (uint32_t i = 0; i < NO_1; i++) {
        for (uint32_t j = 0; j < NO_1024; j++) {
            executeOperation(operationsConfig);
        }
        ValidateNewNsKMap();
    }
}

TEST_F(TestDB, test_put_remove_ns_kmap_to_fresh_table_and_get_return_ok)
{
    std::vector<OperationConfig> operationsConfig = {
        { 0.85, NsKMapPut },
        { 0.1, NsKMapRemove },
        { 0.05, NsKMapRemovePriKey },
    };
    for (uint32_t i = 0; i < NO_1; i++) {
        for (uint32_t j = 0; j < NO_1000; j++) {
            executeOperation(operationsConfig);
        }
        ValidateNsKMap();
    }
}

TEST_F(TestDB, test_put_remove_ns_kmap_to_slice_table_and_get_return_ok)
{
    std::vector<OperationConfig> operationsConfig = {
        { 0.85, NsKMapPut },
        { 0.1, NsKMapRemove },
        { 0.05, NsKMapRemovePriKey },
    };
    for (uint32_t i = 0; i < NO_1; i++) {
        for (uint32_t j = 0; j < NO_1000; j++) {
            executeOperation(operationsConfig);
        }
        ForceFlushToSlice();
        ValidateNsKMap();
    }
}

TEST_F(TestDB, test_put_remove_ns_kmap_to_lsm_store_and_get_return_ok)
{
    std::vector<OperationConfig> operationsConfig = {
        { 0.85, NsKMapPut },
        { 0.1, NsKMapRemove },
        { 0.05, NsKMapRemovePriKey },
    };
    for (uint32_t i = 0; i < NO_3; i++) {
        for (uint32_t j = 0; j < NO_1000; j++) {
            executeOperation(operationsConfig);
        }
        ForceEvictToLsm();
        ValidateNsKMap();
    }
    CleanCurrentVersion();
    ValidateFiles();
}

TEST_F(TestDB, test_put_add_remove_klist_to_fresh_table_and_get_return_ok)
{
    std::vector<OperationConfig> operationsConfig = {
        { 0.1, KListPut },
        { 0.8, KListAdd },
        { 0.1, KListRemove },
    };
    for (uint32_t i = 0; i < NO_1; i++) {
        for (uint32_t j = 0; j < NO_10000; j++) {
            executeOperation(operationsConfig);
        }
        ValidateKList();
    }
}

TEST_F(TestDB, test_put_add_remove_klist_to_slice_table_and_get_return_ok)
{
    std::vector<OperationConfig> operationsConfig = {
        { 0.1, KListPut },
        { 0.8, KListAdd },
        { 0.1, KListRemove },
    };
    for (uint32_t i = 0; i < NO_1; i++) {
        for (uint32_t j = 0; j < NO_10000; j++) {
            executeOperation(operationsConfig);
        }
        ForceFlushToSlice();
        ValidateKList();
    }
}

TEST_F(TestDB, test_put_add_remove_klist_to_lsm_store_and_get_return_ok)
{
    std::vector<OperationConfig> operationsConfig = {
        { 0.1, KListPut },
        { 0.8, KListAdd },
        { 0.1, KListRemove },
    };
    for (uint32_t i = 0; i < NO_3; i++) {
        for (uint32_t j = 0; j < NO_10000; j++) {
            executeOperation(operationsConfig);
        }
        ForceEvictToLsm();
        ValidateKList();
    }
    CleanCurrentVersion();
    ValidateFiles();
}

TEST_F(TestDB, test_put_add_remove_ns_klist_to_fresh_table_and_get_return_ok)
{
    std::vector<OperationConfig> operationsConfig = { { 0.1, NsKListPut },
                                                      { 0.8, NsKListAdd },
                                                      { 0.1, NsKListRemove } };
    for (uint32_t i = 0; i < NO_1; i++) {
        for (uint32_t j = 0; j < NO_10000; j++) {
            executeOperation(operationsConfig);
        }
        ValidateNsKList();
    }
}

TEST_F(TestDB, test_put_add_remove_ns_klist_to_slice_table_and_get_return_ok)
{
    std::vector<OperationConfig> operationsConfig = { { 0.1, NsKListPut },
                                                      { 0.8, NsKListAdd },
                                                      { 0.1, NsKListRemove } };
    for (uint32_t i = 0; i < NO_1; i++) {
        for (uint32_t j = 0; j < NO_10000; j++) {
            executeOperation(operationsConfig);
        }
        ForceFlushToSlice();
        ValidateNsKList();
    }
}

TEST_F(TestDB, test_put_add_remove_ns_klist_to_lsm_store_and_get_return_ok)
{
    std::vector<OperationConfig> operationsConfig = { { 0.1, NsKListPut },
                                                      { 0.8, NsKListAdd },
                                                      { 0.1, NsKListRemove } };
    for (uint32_t i = 0; i < NO_1; i++) {
        for (uint32_t j = 0; j < NO_10000; j++) {
            executeOperation(operationsConfig);
        }
        ForceEvictToLsm();
        ValidateNsKList();
    }
    CleanCurrentVersion();
    ValidateFiles();
}

TEST_F(TestDB, test_put_kv_kmap_to_fresh_table_and_get_return_ok)
{
    // Q4 Q7 Q9 Q11 Q15 Q16
    std::vector<OperationConfig> operationsConfig = {
        { 0.9, KVPut },   { 0.1, KVRemove },   { 0.9, NsKVPut },   { 0.1, NsKVRemove },
        { 0.9, KMapPut }, { 0.1, KMapRemove }, { 0.9, NsKMapPut }, { 0.1, NsKMapRemove },
    };
    for (uint32_t i = 0; i < NO_1; i++) {
        for (uint32_t j = 0; j < NO_10000; j++) {
            executeOperation(operationsConfig);
        }
        ValidateKV();
        ValidateNsKV();
        ValidateKMap();
        ValidateNsKMap();
    }
}

TEST_F(TestDB, test_put_kv_kmap_to_slice_table_and_get_return_ok)
{
    // Q4 Q7 Q9 Q11 Q15 Q16
    std::vector<OperationConfig> operationsConfig = {
        { 0.9, KVPut },   { 0.1, KVRemove },   { 0.9, NsKVPut },   { 0.1, NsKVRemove },
        { 0.9, KMapPut }, { 0.1, KMapRemove }, { 0.9, NsKMapPut }, { 0.1, NsKMapRemove },
    };
    for (uint32_t i = 0; i < NO_1; i++) {
        for (uint32_t j = 0; j < NO_10000; j++) {
            executeOperation(operationsConfig);
        }
        ForceFlushToSlice();
        ValidateKV();
        ValidateNsKV();
        ValidateKMap();
        ValidateNsKMap();
    }
}

TEST_F(TestDB, test_put_kv_kmap_to_lsm_store_and_get_return_ok)
{
    // Q4 Q7 Q9 Q11 Q15 Q16
    std::vector<OperationConfig> operationsConfig = {
        { 0.9, KVPut },   { 0.1, KVRemove },   { 0.9, NsKVPut },   { 0.1, NsKVRemove },
        { 0.9, KMapPut }, { 0.1, KMapRemove }, { 0.9, NsKMapPut }, { 0.1, NsKMapRemove },
    };
    for (uint32_t i = 0; i < NO_3; i++) {
        for (uint32_t j = 0; j < NO_5000; j++) {
            executeOperation(operationsConfig);
        }
        ForceEvictToLsm();
        ValidateKV();
        ValidateNsKV();
        ValidateKMap();
        ValidateNsKMap();
    }
    CleanCurrentVersion();
    ValidateFiles();
}

TEST_F(TestDB, test_put_kv_kmap_nskv_to_all_table_and_get_return_ok)
{
    // Q7
    std::vector<OperationConfig> operationsConfig = {
        { 0.9, KVPut }, { 0.1, KVRemove }, { 0.9, NsKVPut }, { 0.1, NsKVRemove }, { 0.9, KMapPut }, { 0.1, KMapRemove },
    };
    for (uint32_t i = 0; i < NO_5; i++) {
        for (uint32_t j = 0; j < NO_5000; j++) {
            executeOperation(operationsConfig);
        }
        if (i < NO_2) {
            ForceEvictToLsm();
        } else if (i < NO_3) {
            ForceFlushToSlice();
        }
        ValidateKV();
        ValidateNsKV();
        ValidateKMap();
    }
    CleanCurrentVersion();
    ValidateFiles();
}

TEST_F(TestDB, test_put_kv_add_klist_ns_klist_to_fresh_table_and_get_return_ok)
{
    // Q5 Q8
    std::vector<OperationConfig> operationsConfig = { { 0.9, KVPut },      { 0.1, KVRemove },     { 0.1, KListPut },
                                                      { 0.8, KListAdd },   { 0.1, KListRemove },  { 0.1, NsKListPut },
                                                      { 0.8, NsKListAdd }, { 0.1, NsKListRemove } };
    for (uint32_t i = 0; i < NO_1; i++) {
        for (uint32_t j = 0; j < NO_10000; j++) {
            executeOperation(operationsConfig);
        }
        ValidateKV();
        ValidateNsKV();
        ValidateKList();
        ValidateNsKList();
    }
}

TEST_F(TestDB, test_put_kv_add_klist_ns_klist_to_slice_table_and_get_return_ok)
{
    // Q5 Q8
    std::vector<OperationConfig> operationsConfig = { { 0.9, KVPut },      { 0.1, KVRemove },     { 0.1, KListPut },
                                                      { 0.8, KListAdd },   { 0.1, KListRemove },  { 0.1, NsKListPut },
                                                      { 0.8, NsKListAdd }, { 0.1, NsKListRemove } };
    for (uint32_t i = 0; i < NO_1; i++) {
        for (uint32_t j = 0; j < NO_10000; j++) {
            executeOperation(operationsConfig);
        }
        ForceFlushToSlice();
        ValidateKV();
        ValidateNsKV();
        ValidateKList();
        ValidateNsKList();
    }
}

TEST_F(TestDB, test_put_kv_add_klist_ns_klist_to_lsm_store_and_get_return_ok)
{
    // Q5
    std::vector<OperationConfig> operationsConfig = { { 0.9, KVPut },      { 0.1, KVRemove },     { 0.1, KListPut },
                                                      { 0.8, KListAdd },   { 0.1, KListRemove },  { 0.1, NsKListPut },
                                                      { 0.8, NsKListAdd }, { 0.1, NsKListRemove } };
    for (uint32_t i = 0; i < NO_1; i++) {
        for (uint32_t j = 0; j < NO_10000; j++) {
            executeOperation(operationsConfig);
        }
        ForceEvictToLsm();
        ValidateKV();
        ValidateNsKV();
        ValidateKList();
        ValidateNsKList();
    }
    CleanCurrentVersion();
    ValidateFiles();
}

TEST_F(TestDB, test_put_subkv_add_ns_klist_to_all_and_get_return_ok)
{
    // Q8
    std::vector<OperationConfig> operationsConfig = {
        { 0.9, NsKVPut }, { 0.1, NsKVRemove }, { 0.1, NsKListPut }, { 0.8, NsKListAdd }, { 0.1, NsKListRemove }
    };
    for (uint32_t i = 0; i < NO_5; i++) {
        for (uint32_t j = 0; j < NO_1000; j++) {
            executeOperation(operationsConfig);
        }
        if (i < NO_2) {
            ForceEvictToLsm();
        } else if (i < NO_4) {
            ForceFlushToSlice();
        }
        ValidateNsKV();
        ValidateNsKList();
    }
    CleanCurrentVersion();
    ValidateFiles();
}

TEST_F(TestDB, test_put_subkv_add_ns_klist_to_lsm_store_and_get_return_ok)
{
    // Q8
    std::vector<OperationConfig> operationsConfig = {
        { 0.9, NsKVPut }, { 0.1, NsKVRemove }, { 0.1, NsKListPut }, { 0.8, NsKListAdd }, { 0.1, NsKListRemove }
    };
    for (uint32_t i = 0; i < NO_3; i++) {
        for (uint32_t j = 0; j < NO_10000; j++) {
            executeOperation(operationsConfig);
        }
        ForceEvictToLsm();
        ValidateNsKV();
        ValidateNsKList();
    }
    CleanCurrentVersion();
    ValidateFiles();
}

TEST_F(TestDB, test_all_states_to_fresh_table_and_get_return_ok)
{
    std::vector<OperationConfig> operationsConfig = { { 0.9, KVPut },      { 0.1, KVRemove },     { 0.9, NsKVPut },
                                                      { 0.1, NsKVRemove }, { 0.9, KMapPut },      { 0.1, KMapRemove },
                                                      { 0.9, NsKMapPut },  { 0.1, NsKMapRemove }, { 0.1, KListPut },
                                                      { 0.8, KListAdd },   { 0.1, KListRemove },  { 0.1, NsKListPut },
                                                      { 0.8, NsKListAdd }, { 0.1, NsKListRemove } };
    for (uint32_t i = 0; i < NO_1; i++) {
        for (uint32_t j = 0; j < NO_10000; j++) {
            executeOperation(operationsConfig);
        }
        ValidateKV();
        ValidateNsKV();
        ValidateKMap();
        ValidateNsKMap();
        ValidateKList();
        ValidateNsKList();
    }
}

TEST_F(TestDB, test_all_states_to_slice_table_and_get_return_ok)
{
    std::vector<OperationConfig> operationsConfig = { { 0.9, KVPut },      { 0.1, KVRemove },     { 0.9, NsKVPut },
                                                      { 0.1, NsKVRemove }, { 0.9, KMapPut },      { 0.1, KMapRemove },
                                                      { 0.9, NsKMapPut },  { 0.1, NsKMapRemove }, { 0.1, KListPut },
                                                      { 0.8, KListAdd },   { 0.1, KListRemove },  { 0.1, NsKListPut },
                                                      { 0.8, NsKListAdd }, { 0.1, NsKListRemove } };
    for (uint32_t i = 0; i < NO_1; i++) {
        for (uint32_t j = 0; j < NO_10000; j++) {
            executeOperation(operationsConfig);
        }
        ForceFlushToSlice();
        ValidateKV();
        ValidateNsKV();
        ValidateKMap();
        ValidateNsKMap();
        ValidateKList();
        ValidateNsKList();
    }
}

TEST_F(TestDB, test_all_states_to_lsm_store_and_get_return_ok)
{
    std::vector<OperationConfig> operationsConfig = { { 0.9, KVPut },      { 0.1, KVRemove },     { 0.9, NsKVPut },
                                                      { 0.1, NsKVRemove }, { 0.9, KMapPut },      { 0.1, KMapRemove },
                                                      { 0.9, NsKMapPut },  { 0.1, NsKMapRemove }, { 0.1, KListPut },
                                                      { 0.8, KListAdd },   { 0.1, KListRemove },  { 0.1, NsKListPut },
                                                      { 0.8, NsKListAdd }, { 0.1, NsKListRemove } };
    for (uint32_t i = 0; i < NO_1; i++) {
        for (uint32_t j = 0; j < NO_10000; j++) {
            executeOperation(operationsConfig);
        }
        ForceEvictToLsm();
        ValidateKV();
        ValidateNsKV();
        ValidateKMap();
        ValidateNsKMap();
        ValidateKList();
        ValidateNsKList();
    }
    CleanCurrentVersion();
    ValidateFiles();
}

TEST_F(TestDB, test_cp_and_get_return_ok)
{
    mDB->GetConfig().SetEnableLocalRecovery(false);
    std::vector<OperationConfig> operationsConfig = {
        { 0.9, KVPut }, { 0.1, KVRemove }, { 0.9, PQTableAdd }, { 0.1, PQTableRemove }
    };
    uint64_t cpId = 0;
    std::string currentDir = GetCurrentWorkingDirectory();
    std::string basePth = currentDir + "/cp/";
    mkdir(basePth.c_str(), mode_t(NO_777));
    for (int k = 0; k < NO_3; ++k) {
        cpId++;
        for (uint32_t i = 0; i < NO_8; i++) {
            for (uint32_t j = 0; j < NO_10000; j++) {
                executeOperation(operationsConfig);
            }
            if (i < NO_3) {
                ForceEvictToLsm();
            } else if (i < NO_5) {
                ForceFlushToSlice();
            }
        }
        // 测试同步snapshot流程
        std::string cpPth = basePth + std::to_string(cpId);
        mkdir(cpPth.c_str(), mode_t(NO_777));
        ASSERT_NE(mDB->CreateSyncCheckpoint(cpPth, cpId), nullptr);
        ASSERT_EQ(mDB->CreateAsyncCheckpoint(cpId, false), BSS_OK);
        if (k < NO_2) {
            continue;
        }
        CloseAllDb();
        ConfigRef config = std::make_shared<Config>();
        config->Init(NO_0, NO_127, NO_128);
        config->mMemorySegmentSize = IO_SIZE_64M;
        config->SetEvictMinSize(IO_SIZE_1K);
        config->SetLocalPath(basePth + std::to_string(cpId) + "/sst");
        mDB = BoostStateDBFactory::Create();
        mDB->Open(config);
        pqTable1 = mDB->CreatePQTable("stateName1");
        pqTable = mDB->CreatePQTable("stateName");
        kVTable = std::dynamic_pointer_cast<KVTable>(CreateFromDB(StateType::VALUE, "kVTable", mDB));
        nsKVTable = std::dynamic_pointer_cast<NsKVTable>(CreateFromDB(StateType::SUB_VALUE, "nsKVTable", mDB));
        kMapTable = std::dynamic_pointer_cast<KMapTable>(CreateFromDB(StateType::MAP, "kMapTable", mDB));
        nsKMapTable = std::dynamic_pointer_cast<NsKMapTable>(CreateFromDB(StateType::SUB_MAP, "nsKMapTable", mDB));
        kListTable = std::dynamic_pointer_cast<KListTable>(CreateFromDB(StateType::LIST, "kListTable", mDB));
        nsKListTable = std::dynamic_pointer_cast<NsKListTable>(CreateFromDB(StateType::SUB_LIST, "nsKListTable", mDB));
        pqTable = mDB->CreatePQTable("stateName");
        pqTable1 = mDB->CreatePQTable("stateName1");
        std::string restorePath = basePth + std::to_string(cpId);
        std::unordered_map<std::string, std::string> pathMap;
        std::vector<std::string> restorePaths;
        restorePaths.emplace_back(restorePath);
        ASSERT_EQ(mDB->Restore(restorePaths, pathMap), BSS_OK);
        ValidateKV();
        ValidatePQ();
        DeleteCpFile();
    }
}

TEST_F(TestDB, test_cp_local_recovery_ok)
{
    mDB->GetConfig().SetEnableLocalRecovery(true);
    std::vector<OperationConfig> operationsConfig = { { 0.9, KVPut }, { 0.1, KVRemove } };
    uint64_t cpId = 0;
    std::string currentDir = GetCurrentWorkingDirectory();
    std::string basePth = currentDir + "/cp/";
    std::string backupPath = GetCurrentWorkingDirectory() + "/backup";
    mkdir(basePth.c_str(), mode_t(NO_777));

    for (int k = 0; k < NO_3; ++k) {
        cpId++;
        for (uint32_t i = 0; i < NO_8; i++) {
            for (uint32_t j = 0; j < NO_10000; j++) {
                executeOperation(operationsConfig);
            }
            if (i < NO_3) {
                ForceEvictToLsm();
            } else if (i < NO_5) {
                ForceFlushToSlice();
            }
        }
        // 测试同步snapshot流程
        std::string cpPth = basePth + std::to_string(cpId);
        mkdir(cpPth.c_str(), mode_t(NO_777));
        ASSERT_NE(mDB->CreateSyncCheckpoint(cpPth, cpId), nullptr);
        ASSERT_EQ(mDB->CreateAsyncCheckpoint(cpId, false), BSS_OK);
        if (k < NO_3) {
            continue;
        }
        CloseAllDb();
        ConfigRef config = std::make_shared<Config>();
        config->Init(NO_0, NO_127, NO_128);
        config->mMemorySegmentSize = IO_SIZE_64M;
        config->SetEvictMinSize(IO_SIZE_1K);
        config->SetLocalPath(basePth + std::to_string(cpId) + "/sst");
        config->SetEnableLocalRecovery(true);
        config->SetBackupPath(backupPath);
        mDB = BoostStateDBFactory::Create();
        mDB->Open(config);
        kVTable = std::dynamic_pointer_cast<KVTable>(CreateFromDB(StateType::VALUE, "kVTable", mDB));
        nsKVTable = std::dynamic_pointer_cast<NsKVTable>(CreateFromDB(StateType::SUB_VALUE, "nsKVTable", mDB));
        kMapTable = std::dynamic_pointer_cast<KMapTable>(CreateFromDB(StateType::MAP, "kMapTable", mDB));
        nsKMapTable = std::dynamic_pointer_cast<NsKMapTable>(CreateFromDB(StateType::SUB_MAP, "nsKMapTable", mDB));
        kListTable = std::dynamic_pointer_cast<KListTable>(CreateFromDB(StateType::LIST, "kListTable", mDB));
        nsKListTable = std::dynamic_pointer_cast<NsKListTable>(CreateFromDB(StateType::SUB_LIST, "nsKListTable", mDB));
        pqTable = mDB->CreatePQTable("stateName");
        pqTable1 = mDB->CreatePQTable("stateName1");
        std::string restorePath = basePth + std::to_string(cpId);
        std::unordered_map<std::string, std::string> pathMap;
        std::vector<std::string> restorePaths;
        restorePaths.emplace_back(restorePath);
        ASSERT_EQ(mDB->Restore(restorePaths, pathMap), BSS_OK);
        ValidateKV();
        DeleteCpFile();
    }
}

TEST_F(TestDB, test_cp_lsm_and_get_return_ok)
{
    std::vector<OperationConfig> operationsConfig = { { 0.9, KVPut }, { 0.1, KVRemove } };
    uint64_t cpId = 0;
    std::string currentDir = GetCurrentWorkingDirectory();
    std::string basePth = currentDir + "/cp/";
    mkdir(basePth.c_str(), mode_t(NO_777));
    for (int k = 0; k < NO_3; ++k) {
        cpId++;
        for (uint32_t j = 0; j < NO_10000; j++) {
            executeOperation(operationsConfig);
        }
        ForceEvictToLsm();

        // 测试同步snapshot流程
        std::string cpPth = basePth + std::to_string(cpId);
        mkdir(cpPth.c_str(), mode_t(NO_777));
        ASSERT_NE(mDB->CreateSyncCheckpoint(cpPth, cpId), nullptr);
        ASSERT_EQ(mDB->CreateAsyncCheckpoint(cpId, false), BSS_OK);
        if (k < NO_2) {
            continue;
        }
        CloseAllDb();
        ConfigRef config = std::make_shared<Config>();
        config->Init(NO_0, NO_127, NO_128);
        config->mMemorySegmentSize = IO_SIZE_64M;
        config->SetEvictMinSize(IO_SIZE_1K);
        config->SetLocalPath(basePth + std::to_string(cpId) + "/sst");
        config->SetBackendUID("cp-test");
        mDB = BoostStateDBFactory::Create();
        mDB->Open(config);
        std::string restorePath = basePth + std::to_string(cpId);
        std::unordered_map<std::string, std::string> pathMap;
        kVTable = nullptr;
        kVTable = std::dynamic_pointer_cast<KVTable>(CreateFromDB(StateType::VALUE, "kVTable", mDB));
        nsKVTable = std::dynamic_pointer_cast<NsKVTable>(CreateFromDB(StateType::SUB_VALUE, "nsKVTable", mDB));
        kMapTable = std::dynamic_pointer_cast<KMapTable>(CreateFromDB(StateType::MAP, "kMapTable", mDB));
        nsKMapTable = std::dynamic_pointer_cast<NsKMapTable>(CreateFromDB(StateType::SUB_MAP, "nsKMapTable", mDB));
        kListTable = std::dynamic_pointer_cast<KListTable>(CreateFromDB(StateType::LIST, "kListTable", mDB));
        nsKListTable = std::dynamic_pointer_cast<NsKListTable>(CreateFromDB(StateType::SUB_LIST, "nsKListTable", mDB));
        pqTable = mDB->CreatePQTable("stateName");
        pqTable1 = mDB->CreatePQTable("stateName1");
        std::vector<std::string> restorePaths;
        restorePaths.emplace_back(restorePath);
        ASSERT_EQ(mDB->Restore(restorePaths, pathMap), BSS_OK);
        ValidateKV();
    }

    for (int k = 0; k < NO_3; ++k) {
        cpId++;
        for (uint32_t j = 0; j < NO_10000; j++) {
            executeOperation(operationsConfig);
        }
        ForceEvictToLsm();
        ValidateKV();
    }
    DeleteCpFile();
}

TEST_F(TestDB, test_sp_lsm_and_get_return_ok)
{
    std::vector<OperationConfig> operationsConfig = {
        { 0.9, KVPut }, { 0.1, KVRemove }, { 0.9, PQTableAdd }, { 0.1, PQTableRemove }
    };
    for (uint32_t j = 0; j < NO_10000; j++) {
        executeOperation(operationsConfig);
    }
    ForceEvictToLsm();

    SavepointDataView *dataView = mDB->TriggerSavepoint();
    auto iterator = dataView->SavepointIterator();
    ASSERT_EQ(dataView != nullptr, true);
    ConfigRef config = std::make_shared<Config>();
    config->Init(NO_0, NO_127, NO_128);
    config->mMemorySegmentSize = IO_SIZE_64M;
    config->SetEvictMinSize(IO_SIZE_1K);
    config->SetTaskSlotFlag(NO_1);
    BoostStateDBPtr nDB = BoostStateDBFactory::Create();
    nDB->Open(config);
    pqTable = mDB->CreatePQTable("stateName");
    kVTable = nullptr;
    kVTable = std::dynamic_pointer_cast<KVTable>(CreateFromDB(StateType::VALUE, "kVTable", nDB));
    while (iterator->HasNext()) {
        BinaryKeyValueItemRef item = iterator->Next();
        uint32_t keyHashCode = HashForTest(item->mKey, item->mKeyLength);
        BinaryData priKey(item->mKey, item->mKeyLength);
        BinaryData val(item->mValue, item->mValueLength);
        if (item->mStateType == PQ) {
            pqTable->AddKey(priKey, keyHashCode);
        } else {
            kVTable->Put(keyHashCode, priKey, val);
        }
    }
    ValidatePQ();
    ValidateKV();
    nDB->Close();
    delete nDB;
    nDB = nullptr;
    DeleteCpFile();
}

TEST_F(TestDB, test_sp_kmap_and_get_return_ok)
{
    std::vector<OperationConfig> operationsConfig = { { 0.9, KMapPut }, { 0.1, KMapRemove } };
    for (uint32_t i = 0; i < NO_8; i++) {
        for (uint32_t j = 0; j < NO_10000; j++) {
            executeOperation(operationsConfig);
        }
        if (i < NO_3) {
            ForceEvictToLsm();
        } else if (i < NO_5) {
            ForceFlushToSlice();
        }
    }

    SavepointDataView *dataView = mDB->TriggerSavepoint();
    auto iterator = dataView->SavepointIterator();
    ASSERT_EQ(dataView != nullptr, true);
    uint32_t count = 0;
    while (iterator->HasNext()) {
        BinaryKeyValueItemRef item = iterator->Next();
        std::vector<uint8_t> key(item->mKey, item->mKey + item->mKeyLength);
        std::vector<uint8_t> secKey(item->mMapKey, item->mMapKey + item->mMapKeyLength);
        std::vector<uint8_t> value(item->mValue, item->mValue + item->mValueLength);
        ASSERT_TRUE(mKMap[key][secKey] == value);
        count++;
    }
    uint32_t expectCount = 0;
    for (auto &it : mKMap) {
        for (const auto &item : it.second) {
            expectCount++;
        }
    }
    ASSERT_TRUE(expectCount == count);
}

TEST_F(TestDB, test_cp_expansion_and_get_return_ok)
{
    std::vector<OperationConfig> operationsConfig = { { 0.9, KVPut },      { 0.1, KVRemove },     { 0.9, NsKVPut },
                                                      { 0.1, NsKVRemove }, { 0.9, KMapPut },      { 0.1, KMapRemove },
                                                      { 0.9, NsKMapPut },  { 0.1, NsKMapRemove }, { 0.1, KListPut },
                                                      { 0.8, KListAdd },   { 0.1, KListRemove },  { 0.1, NsKListPut },
                                                      { 0.8, NsKListAdd }, { 0.1, NsKListRemove } };
    uint64_t cpId = 0;
    std::string currentDir = GetCurrentWorkingDirectory();
    std::string basePth = currentDir + "/cp/";
    mkdir(basePth.c_str(), mode_t(NO_777));
    cpId++;
    const auto &oldConfig = mDB->GetConfig();
    for (uint32_t i = 0; i < NO_8; i++) {
        for (uint32_t j = 0; j < NO_10000; j++) {
            executeOperation(operationsConfig, oldConfig.GetStartGroup(), oldConfig.GetEndGroup());
        }
        if (i < NO_3) {
            ForceEvictToLsm();
        } else if (i < NO_5) {
            ForceFlushToSlice();
        }
    }

    // 测试同步snapshot流程
    std::string cpPth = basePth + std::to_string(cpId);
    mkdir(cpPth.c_str(), mode_t(NO_777));
    ASSERT_NE(mDB->CreateSyncCheckpoint(cpPth, cpId), nullptr);
    ASSERT_EQ(mDB->CreateAsyncCheckpoint(cpId, false), BSS_OK);
    std::string restorePath;
    CloseAllDb();
    RestoreDb(basePth, cpId, restorePath);
    std::vector<std::string> restorePaths;
    restorePaths.emplace_back(restorePath);
    std::unordered_map<std::string, std::string> pathMap;
    ASSERT_EQ(mDB->Restore(restorePaths, pathMap), BSS_OK);
    std::unordered_map<std::string, std::string> pathMap1;
    ASSERT_EQ(mDB1->Restore(restorePaths, pathMap1), BSS_OK);
    ValidateKVTwoDB();
    CleanCurrentVersion();
    DeleteCpFile();
}

TEST_F(TestDB, test_cp_scale_in_and_get_return_ok)
{
    PrepareForScaleIn();
    std::vector<OperationConfig> operationsConfig = { { 0.9, KVPut },      { 0.1, KVRemove },     { 0.9, NsKVPut },
                                                      { 0.1, NsKVRemove }, { 0.9, KMapPut },      { 0.1, KMapRemove },
                                                      { 0.9, NsKMapPut },  { 0.1, NsKMapRemove }, { 0.1, KListPut },
                                                      { 0.8, KListAdd },   { 0.1, KListRemove },  { 0.1, NsKListPut },
                                                      { 0.8, NsKListAdd }, { 0.1, NsKListRemove } };
    uint64_t cpId = 0;
    std::string currentDir = GetCurrentWorkingDirectory();
    std::string basePth = currentDir + "/cp/";
    mkdir(basePth.c_str(), mode_t(NO_777));
    cpId++;
    std::vector<std::string> restorePaths;
    const auto &db1Config = mDB->GetConfig();
    const auto &db2Config = mDB1->GetConfig();
    for (uint32_t i = 0; i < NO_8; i++) {
        for (uint32_t j = 0; j < NO_10000; j++) {
            if (j % NO_2 == 0) {
                executeOperation(operationsConfig, db1Config.GetStartGroup(), db1Config.GetEndGroup());
            } else {
                executeOperation(operationsConfig, db2Config.GetStartGroup(), db2Config.GetEndGroup(), false);
            }
        }
        if (i < NO_3) {
            ForceEvictToLsm(true);
        } else if (i < NO_5) {
            ForceFlushToSlice(true);
        }
    }
    std::string db1Pth = basePth + "/db1/";
    mkdir(db1Pth.c_str(), mode_t(NO_777));
    std::string cpPth1 = db1Pth + std::to_string(cpId);
    mkdir(cpPth1.c_str(), mode_t(NO_777));
    ASSERT_NE(mDB->CreateSyncCheckpoint(cpPth1, cpId), nullptr);
    ASSERT_EQ(mDB->CreateAsyncCheckpoint(cpId, false), BSS_OK);

    std::string db2Pth = basePth + "/db2/";
    mkdir(db2Pth.c_str(), mode_t(NO_777));
    std::string cpPth2 = db2Pth + std::to_string(cpId);
    mkdir(cpPth2.c_str(), mode_t(NO_777));
    ASSERT_NE(mDB1->CreateSyncCheckpoint(cpPth2, cpId), nullptr);
    ASSERT_EQ(mDB1->CreateAsyncCheckpoint(cpId, false), BSS_OK);
    CloseAllDb();
    ConfigRef config = std::make_shared<Config>();
    config->Init(NO_0, NO_127, NO_128);
    config->mMemorySegmentSize = IO_SIZE_64M;
    config->SetLocalPath(basePth + std::to_string(cpId) + "/sst");
    config->SetBackendUID("cp-test-0");
    mDB = BoostStateDBFactory::Create();
    mDB->Open(config);
    std::unordered_map<std::string, std::string> pathMap;
    kVTable = nullptr;
    kVTable = std::dynamic_pointer_cast<KVTable>(CreateFromDB(StateType::VALUE, "kVTable", mDB));
    nsKVTable = std::dynamic_pointer_cast<NsKVTable>(CreateFromDB(StateType::SUB_VALUE, "nsKVTable", mDB));
    kMapTable = std::dynamic_pointer_cast<KMapTable>(CreateFromDB(StateType::MAP, "kMapTable", mDB));
    nsKMapTable = std::dynamic_pointer_cast<NsKMapTable>(CreateFromDB(StateType::SUB_MAP, "nsKMapTable", mDB));
    kListTable = std::dynamic_pointer_cast<KListTable>(CreateFromDB(StateType::LIST, "kListTable", mDB));
    nsKListTable = std::dynamic_pointer_cast<NsKListTable>(CreateFromDB(StateType::SUB_LIST, "nsKListTable", mDB));
    restorePaths.emplace_back(cpPth1);
    restorePaths.emplace_back(cpPth2);
    ASSERT_EQ(mDB->Restore(restorePaths, pathMap), BSS_OK);
    ValidateKV();
    CleanCurrentVersion();
    DeleteCpFile();
}

TEST_F(TestDB, test_put_add_map)
{
    for (int i = 0; i < 10000; ++i) {
        std::vector<uint8_t> key = GetRandomData(sourceKey);
        std::vector<uint8_t> subKey = GetRandomData(sourceSubKey);
        uint32_t keyHashCode = HashForTest(key.data(), key.size());

        BinaryData priKey(key.data(), key.size());
        BinaryData secKey(subKey.data(), subKey.size());
        BinaryData val;
        kMapTable->Get(keyHashCode, priKey, secKey, val);
        if (val.IsNull()) {
            ASSERT_TRUE(mKMap[key][subKey].empty());
            uint32_t value = 1;
            std::vector<uint8_t> valVec(reinterpret_cast<uint8_t *>(&value),
                                        reinterpret_cast<uint8_t *>(&value) + sizeof(uint32_t));
            mKMap[key][subKey] = valVec;
            BinaryData rval(valVec.data(), valVec.size());
            kMapTable->Put(keyHashCode, priKey, secKey, rval);
        } else {
            uint32_t old = *reinterpret_cast<uint32_t *>(const_cast<uint8_t *>(val.Data()));
            ASSERT_EQ(old, *reinterpret_cast<uint32_t *>(mKMap[key][subKey].data()));
            uint32_t newVal = old + 1;
            std::vector<uint8_t> valVec(reinterpret_cast<uint8_t *>(&newVal),
                                        reinterpret_cast<uint8_t *>(&newVal) + sizeof(uint32_t));
            mKMap[key][subKey] = valVec;
            BinaryData rval(valVec.data(), valVec.size());
            kMapTable->Put(keyHashCode, priKey, secKey, rval);
        }
        kMapTable->Get(keyHashCode, priKey, secKey, val);
        uint32_t old = *reinterpret_cast<uint32_t *>(const_cast<uint8_t *>(val.Data()));
        ASSERT_EQ(old, *reinterpret_cast<uint32_t *>(mKMap[key][subKey].data()));
    }
}

TEST_F(TestDB, test_put_kv_kmap_to_lsm_store_and_get_return_ok_with_blob)
{
    PrepareDbWithBlob();
    // Q4 Q7 Q9 Q11 Q15 Q16
    std::vector<OperationConfig> operationsConfig = {
        { 0.9, KVPut },   { 0.1, KVRemove },   { 0.9, NsKVPut },   { 0.1, NsKVRemove },
        { 0.9, KMapPut }, { 0.1, KMapRemove }, { 0.9, NsKMapPut }, { 0.1, NsKMapRemove },
    };
    for (uint32_t i = 0; i < NO_5; i++) {
        for (uint32_t j = 0; j < NO_5000; j++) {
            executeOperation(operationsConfig);
        }
        ForceEvictToLsm();
        ValidateKV();
        ValidateNsKV();
        ValidateKMap();
        ValidateNsKMap();
    }
    CleanCurrentVersion();
    ValidateFiles();
}

TEST_F(TestDB, test_put_kv_kmap_nskv_to_all_table_and_get_return_ok_with_blob)
{
    PrepareDbWithBlob();
    // Q7
    std::vector<OperationConfig> operationsConfig = {
        { 0.9, KVPut }, { 0.1, KVRemove }, { 0.9, NsKVPut }, { 0.1, NsKVRemove }, { 0.9, KMapPut }, { 0.1, KMapRemove },
    };
    for (uint32_t i = 0; i < NO_5; i++) {
        for (uint32_t j = 0; j < NO_5000; j++) {
            executeOperation(operationsConfig);
        }
        if (i < NO_2) {
            ForceEvictToLsm();
        } else if (i < NO_3) {
            ForceFlushToSlice();
        }
        ValidateKV();
        ValidateNsKV();
        ValidateKMap();
    }
    CleanCurrentVersion();
    ValidateFiles();
}

TEST_F(TestDB, test_put_kmap_to_all_table_and_get_return_ok_with_blob)
{
    PrepareDbWithBlob();
    // Q7
    std::vector<OperationConfig> operationsConfig = { { 0.5, KMapPut }, { 0.8, KMapRemove } };
    for (uint32_t i = 0; i < NO_5; i++) {
        for (uint32_t j = 0; j < NO_5000; j++) {
            executeOperation(operationsConfig);
        }
        if (i < NO_2) {
            ForceEvictToLsm();
        } else if (i < NO_3) {
            ForceFlushToSlice();
        }
        ValidateKMap();
    }
    CleanCurrentVersion();
    ValidateFiles();
}

TEST_F(TestDB, test_pqTable_add)
{
    std::vector<OperationConfig> operationsConfig = {
        { 0.9, PQTableAdd },
        { 0.1, PQTableRemove },
    };
    for (uint32_t i = 0; i < NO_3; i++) {
        for (uint32_t j = 0; j < NO_10000; j++) {
            executeOperation(operationsConfig);
        }
        ValidatePQ();
    }
    CleanCurrentVersion();
    ValidateFiles();
}

TEST_F(TestDB, test_pqTable_add_lsm)
{
    std::vector<OperationConfig> operationsConfig = {
        { 0.9, PQTableAdd },
        { 0.1, PQTableRemove },
    };
    for (uint32_t i = 0; i < NO_7; i++) {
        for (uint32_t j = 0; j < NO_10000; j++) {
            executeOperation(operationsConfig);
        }
        if (i < NO_3) {
            pqTable->TriggerSegmentFlush(true);
        }
        ValidatePQ();
    }
    CleanCurrentVersion();
    ValidateFiles();
}

TEST_F(TestDB, test_pq_iterator_open_close)
{
    std::vector<OperationConfig> operationsConfig = {
        { 0.9, PQTableAdd },
        { 0.1, PQTableRemove },
    };
    for (uint32_t i = 0; i < NO_7; i++) {
        for (uint32_t j = 0; j < NO_10000; j++) {
            executeOperation(operationsConfig);
        }
        if (i < NO_3) {
            pqTable->TriggerSegmentFlush(true);
        }
    }
    for (int i = 0; i < NO_1000; i++) {
        std::vector<uint8_t> randomData = GetRandomData();
        uint32_t hashcode = HashForTest(randomData.data(), randomData.size());
        uint32_t groupId = hashcode % 128;
        std::vector<uint8_t> prefix = GetPrefix(groupId);
        BinaryData data(prefix.data(), prefix.size());
        auto it = pqTable->KeyIterator(data);
        it->Close();
        delete it;
    }
    CleanCurrentVersion();
    ValidateFiles();
}

TEST_F(TestDB, test_cp_lsm_and_get_return_ok_with_blob)
{
    PrepareDbWithBlob();
    std::vector<OperationConfig> operationsConfig = { { 0.9, KVPut }, { 0.1, KVRemove } };
    uint64_t cpId = 0;
    std::string currentDir = GetCurrentWorkingDirectory();
    std::string basePth = currentDir + "/cp/";
    mkdir(basePth.c_str(), mode_t(NO_777));
    for (int k = 0; k < NO_3; ++k) {
        cpId++;
        for (uint32_t j = 0; j < NO_10000; j++) {
            executeOperation(operationsConfig);
        }
        ForceEvictToLsm();

        // 测试同步snapshot流程
        std::string cpPth = basePth + std::to_string(cpId);
        mkdir(cpPth.c_str(), mode_t(NO_777));
        ASSERT_NE(mDB->CreateSyncCheckpoint(cpPth, cpId), nullptr);
        ASSERT_EQ(mDB->CreateAsyncCheckpoint(cpId, false), BSS_OK);
        if (k < NO_2) {
            continue;
        }
        CloseAllDb();
        ConfigRef config = std::make_shared<Config>();
        config->Init(NO_0, NO_127, NO_128);
        config->mMemorySegmentSize = IO_SIZE_64M;
        config->SetEvictMinSize(IO_SIZE_1K);
        config->SetEnableKVSeparate(true);
        config->SetBlobDefaultBlockSize(IO_SIZE_16K);
        config->SetBlobFileSize(IO_SIZE_64K);
        config->SetLocalPath(basePth + std::to_string(cpId) + "/sst");
        config->SetBackendUID("cp-test");
        config->SetEnableKVSeparate(true);
        config->SetBlobDefaultBlockSize(IO_SIZE_16K);
        config->SetBlobFileSize(IO_SIZE_64K);
        config->mEnableTombstone = true;
        config->mMaxBlobNumInMemCache = 1000;
        config->mTombstoneDataBlockSize = IO_SIZE_64K;
        config->mTombstoneFileSize = IO_SIZE_256K;
        config->mTombstoneLevel0CompactionFileNum = 4;
        config->mBlobMinCompactionThreshold = 0;
        config->mBlobMaxSpaceAmplificationRatio = 0;
        mDB = BoostStateDBFactory::Create();
        mDB->Open(config);
        std::string restorePath = basePth + std::to_string(cpId);
        std::unordered_map<std::string, std::string> pathMap;
        kVTable = nullptr;
        kVTable = std::dynamic_pointer_cast<KVTable>(CreateFromDB(StateType::VALUE, "kVTable", mDB));
        nsKVTable = std::dynamic_pointer_cast<NsKVTable>(CreateFromDB(StateType::SUB_VALUE, "nsKVTable", mDB));
        kMapTable = std::dynamic_pointer_cast<KMapTable>(CreateFromDB(StateType::MAP, "kMapTable", mDB));
        nsKMapTable = std::dynamic_pointer_cast<NsKMapTable>(CreateFromDB(StateType::SUB_MAP, "nsKMapTable", mDB));
        kListTable = std::dynamic_pointer_cast<KListTable>(CreateFromDB(StateType::LIST, "kListTable", mDB));
        nsKListTable = std::dynamic_pointer_cast<NsKListTable>(CreateFromDB(StateType::SUB_LIST, "nsKListTable", mDB));
        std::vector<std::string> restorePaths;
        restorePaths.emplace_back(restorePath);
        ASSERT_EQ(mDB->Restore(restorePaths, pathMap), BSS_OK);
        ValidateKV();
    }
    DeleteCpFile();
}

// 性能基准: SubTableEntryIterator.open (AbstractKMapTable::EntryIterator) 大量open, fresh路径, stderr输出耗时
TEST_F(TestDB, EntryIteratorOpenPerfFresh)
{
    const uint32_t numPriKeys = 200;
    const uint32_t secPerKey = 20;
    std::vector<std::vector<uint8_t>> priKeys;
    priKeys.reserve(numPriKeys);
    for (uint32_t p = 0; p < numPriKeys; ++p) {
        std::vector<uint8_t> priKey(8);
        for (uint32_t b = 0; b < 8; ++b) {
            priKey[b] = static_cast<uint8_t>((p * 7U + b * 3U) & 0xFF);
        }
        priKeys.push_back(priKey);
        for (uint32_t s = 0; s < secPerKey; ++s) {
            std::vector<uint8_t> secKey(4);
            for (uint32_t b = 0; b < 4; ++b) {
                secKey[b] = static_cast<uint8_t>((s * 5U + b * 11U + 1U) & 0xFF);
            }
            std::vector<uint8_t> value(64, static_cast<uint8_t>((p + s) & 0xFF));
            uint32_t keyHashCode = HashForTest(priKey.data(), priKey.size());
            BinaryData bPri(priKey.data(), priKey.size());
            BinaryData bSec(secKey.data(), secKey.size());
            BinaryData bVal(value.data(), value.size());
            ASSERT_EQ(kMapTable->Put(keyHashCode, bPri, bSec, bVal), BSS_OK);
        }
    }
    // data stays in the fresh table (not flushed) to exercise the GetMapIterator + open path.

    const uint32_t iters = 100;
    auto t0 = std::chrono::steady_clock::now();
    uint64_t totalEntries = 0;
    uint64_t opens = 0;
    for (uint32_t iter = 0; iter < iters; ++iter) {
        for (auto &pk : priKeys) {
            uint32_t keyHashCode = HashForTest(pk.data(), pk.size());
            BinaryData bPri(pk.data(), pk.size());
            auto *entryIterator = kMapTable->EntryIterator(keyHashCode, bPri);
            ++opens;
            if (entryIterator != nullptr) {
                while (entryIterator->HasNext()) {
                    entryIterator->Next();
                    ++totalEntries;
                }
                delete entryIterator;
            }
        }
    }
    auto t1 = std::chrono::steady_clock::now();
    auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count();
    fprintf(stderr,
            "\n[PERF] EntryIteratorOpenPerfFresh: priKeys=%u secPerKey=%u iters=%u opens=%llu totalEntries=%llu "
            "totalTime=%.3f ms (%.1f ns/open)\n",
            numPriKeys, secPerKey, iters, (unsigned long long)opens, (unsigned long long)totalEntries, ns / 1e6,
            opens > 0 ? (double)ns / opens : 0.0);
    ASSERT_GT(totalEntries, 0ULL);
}

namespace {
std::vector<uint8_t> PerfGlmKey(uint32_t value, uint8_t tag)
{
    return { tag, static_cast<uint8_t>(value >> 24U), static_cast<uint8_t>(value >> 16U),
             static_cast<uint8_t>(value >> 8U), static_cast<uint8_t>(value) };
}

std::vector<uint8_t> PerfGlmValue(uint32_t primary, uint32_t secondary, size_t size = 64)
{
    std::vector<uint8_t> value(size, static_cast<uint8_t>((primary * 17U + secondary * 31U) & 0xffU));
    if (size >= sizeof(primary) + sizeof(secondary)) {
        std::memcpy(value.data(), &primary, sizeof(primary));
        std::memcpy(value.data() + sizeof(primary), &secondary, sizeof(secondary));
    }
    return value;
}

uint32_t PerfGlmEnv(const char *name, uint32_t defaultValue)
{
    const char *raw = std::getenv(name);
    if (raw == nullptr || *raw == '\0') {
        return defaultValue;
    }
    char *end = nullptr;
    unsigned long parsed = std::strtoul(raw, &end, 10);
    return (end != raw && *end == '\0' && parsed > 0 && parsed <= UINT32_MAX) ? static_cast<uint32_t>(parsed)
                                                                                 : defaultValue;
}

uint64_t PerfGlmReadEntries(const KMapTableRef &table, const std::vector<uint8_t> &primaryKey, bool readAll)
{
    BinaryData primary(primaryKey.data(), primaryKey.size());
    auto *iterator = table->EntryIterator(HashForTest(primaryKey.data(), primaryKey.size()), primary);
    if (iterator == nullptr) {
        return 0;
    }
    uint64_t count = 0;
    while (iterator->HasNext()) {
        auto entry = iterator->Next();
        if (entry != nullptr) {
            ++count;
        }
        if (!readAll) {
            break;
        }
    }
    delete iterator;
    return count;
}

void PerfGlmExpectEntries(const KMapTableRef &table, const std::vector<uint8_t> &primaryKey,
                          const std::map<std::vector<uint8_t>, std::vector<uint8_t>> &expected)
{
    BinaryData primary(primaryKey.data(), primaryKey.size());
    auto *iterator = table->EntryIterator(HashForTest(primaryKey.data(), primaryKey.size()), primary);
    ASSERT_NE(iterator, nullptr);
    std::map<std::vector<uint8_t>, std::vector<uint8_t>> actual;
    while (iterator->HasNext()) {
        auto entry = iterator->Next();
        ASSERT_NE(entry, nullptr);
        const auto &secondary = entry->key.SecKey();
        actual.emplace(std::vector<uint8_t>(secondary.KeyData(), secondary.KeyData() + secondary.KeyLen()),
                       std::vector<uint8_t>(entry->value.ValueData(),
                                            entry->value.ValueData() + entry->value.ValueLen()));
    }
    delete iterator;
    EXPECT_EQ(actual, expected);
}

void PerfGlmPut(const KMapTableRef &table, const std::vector<uint8_t> &primaryKey,
                const std::vector<uint8_t> &secondaryKey, const std::vector<uint8_t> &value)
{
    BinaryData primary(primaryKey.data(), primaryKey.size());
    BinaryData secondary(secondaryKey.data(), secondaryKey.size());
    BinaryData binaryValue(value.data(), value.size());
    ASSERT_EQ(table->Put(HashForTest(primaryKey.data(), primaryKey.size()), primary, secondary, binaryValue), BSS_OK);
}
}  // namespace

TEST_F(TestDB, PerfGlmIteratorCorrectnessAcrossStorageLayers)
{
    auto primaryKey = PerfGlmKey(7, 0xa1);
    std::map<std::vector<uint8_t>, std::vector<uint8_t>> expected;
    for (uint32_t i = 0; i < 257; ++i) {
        auto secondaryKey = PerfGlmKey(i, 0xb2);
        auto value = PerfGlmValue(7, i);
        PerfGlmPut(kMapTable, primaryKey, secondaryKey, value);
        expected.emplace(std::move(secondaryKey), std::move(value));
    }

    // Fresh-table hit and the optimized empty-iterator fast path.
    PerfGlmExpectEntries(kMapTable, primaryKey, expected);
    auto missingKey = PerfGlmKey(9999, 0xa1);
    EXPECT_EQ(PerfGlmReadEntries(kMapTable, missingKey, true), 0U);

    // Slice and LSM paths exercise prefix iteration and lazy data-block decoding.
    ForceFlushToSlice();
    PerfGlmExpectEntries(kMapTable, primaryKey, expected);
    ForceEvictToLsm();
    PerfGlmExpectEntries(kMapTable, primaryKey, expected);

    // Verify that fresh updates/deletes still override entries returned lazily from LSM.
    auto updatedKey = PerfGlmKey(3, 0xb2);
    auto removedKey = PerfGlmKey(5, 0xb2);
    auto updatedValue = PerfGlmValue(77, 3, 96);
    PerfGlmPut(kMapTable, primaryKey, updatedKey, updatedValue);
    BinaryData primary(primaryKey.data(), primaryKey.size());
    BinaryData secondary(removedKey.data(), removedKey.size());
    ASSERT_EQ(kMapTable->Remove(HashForTest(primaryKey.data(), primaryKey.size()), primary, secondary), BSS_OK);
    expected[updatedKey] = updatedValue;
    expected.erase(removedKey);
    PerfGlmExpectEntries(kMapTable, primaryKey, expected);
}

TEST_F(TestDB, PerfGlmBlobIteratorCorrectness)
{
    PrepareDbWithBlob();
    auto primaryKey = PerfGlmKey(11, 0xc3);
    std::map<std::vector<uint8_t>, std::vector<uint8_t>> expected;
    for (uint32_t i = 0; i < 32; ++i) {
        auto secondaryKey = PerfGlmKey(i, 0xd4);
        auto value = PerfGlmValue(11, i, IO_SIZE_32K);
        PerfGlmPut(kMapTable, primaryKey, secondaryKey, value);
        expected.emplace(std::move(secondaryKey), std::move(value));
    }
    ForceEvictToLsm();
    PerfGlmExpectEntries(kMapTable, primaryKey, expected);
}

TEST_F(TestDB, DISABLED_PerfGlmIteratorMicroBenchmark)
{
    const uint32_t primaryCount = PerfGlmEnv("BSS_PERF_GLM_PRI_KEYS", 512);
    const uint32_t secondaryCount = PerfGlmEnv("BSS_PERF_GLM_SEC_KEYS", 64);
    const uint32_t rounds = PerfGlmEnv("BSS_PERF_GLM_ROUNDS", 7);
    std::vector<std::vector<uint8_t>> primaryKeys;
    primaryKeys.reserve(primaryCount);
    for (uint32_t primary = 0; primary < primaryCount; ++primary) {
        auto primaryKey = PerfGlmKey(primary, 0xe5);
        primaryKeys.emplace_back(primaryKey);
        for (uint32_t secondary = 0; secondary < secondaryCount; ++secondary) {
            PerfGlmPut(kMapTable, primaryKey, PerfGlmKey(secondary, 0xf6), PerfGlmValue(primary, secondary));
        }
    }

    auto run = [&](const char *name, bool missing, bool readAll) {
        std::vector<double> nsPerOpen;
        uint64_t checksum = 0;
        for (uint32_t round = 0; round < rounds; ++round) {
            auto begin = std::chrono::steady_clock::now();
            for (uint32_t i = 0; i < primaryCount; ++i) {
                auto key = missing ? PerfGlmKey(primaryCount + i, 0xe5) : primaryKeys[i];
                checksum += PerfGlmReadEntries(kMapTable, key, readAll);
            }
            auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(
                               std::chrono::steady_clock::now() - begin)
                               .count();
            nsPerOpen.emplace_back(static_cast<double>(elapsed) / primaryCount);
        }
        std::sort(nsPerOpen.begin(), nsPerOpen.end());
        std::fprintf(stderr,
                     "[PERF_GLM] case=%s primary=%u secondary=%u rounds=%u median_ns_per_open=%.1f checksum=%llu\n",
                     name, primaryCount, secondaryCount, rounds, nsPerOpen[nsPerOpen.size() / 2],
                     static_cast<unsigned long long>(checksum));
    };

    // Warm up allocator, branch predictors, and iterator code before collecting medians.
    for (uint32_t i = 0; i < primaryCount; ++i) {
        (void)PerfGlmReadEntries(kMapTable, primaryKeys[i], false);
    }
    run("fresh_miss", true, false);
    run("fresh_first", false, false);
    run("fresh_full", false, true);

    ForceEvictToLsm();
    for (uint32_t i = 0; i < primaryCount; ++i) {
        (void)PerfGlmReadEntries(kMapTable, primaryKeys[i], false);
    }
    run("lsm_miss", true, false);
    run("lsm_first", false, false);
    run("lsm_full", false, true);
}
}  // namespace test_kv_table
}  // namespace test
}  // namespace bss
}  // namespace ock
