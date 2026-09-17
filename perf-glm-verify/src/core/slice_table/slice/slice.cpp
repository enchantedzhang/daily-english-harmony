/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */
#include "slice.h"

#include <algorithm>
#include <vector>
#if defined(__aarch64__)
#ifdef BUILD_SVE
#include <arm_sve.h>
#endif
#endif

#include "binary/slice_binary.h"
#include "common/util/bss_math.h"
#include "compare_slice_key.h"
#include "include/bss_err.h"
#include "include/bss_types.h"
#include "slice_table.h"

namespace ock {
namespace bss {
BResult ValueSpace::Put(uint32_t index, FreshValueNodePtr &value, const MemorySegment &freshSegment, bool isValue,
                        SliceTableManagerRef sliceTable, uint32_t keyHashCode, uint16_t stateId, uint64_t &seqId)
{
    uint32_t count = 0;
    bool deleteOrPut = false;
    std::vector<FreshValueNode *> nodes;
    RETURN_ERROR_AS_NULLPTR(value);
    value->VisitAsList(freshSegment, [&](FreshValueNode &curVal) {
        seqId = std::max(seqId, curVal.ValueSeqId());
        if (curVal.ValueType() == ValueType::DELETE) {
            deleteOrPut = true;
            return false;
        } else if (curVal.ValueType() == ValueType::PUT) {
            deleteOrPut = true;
            nodes.emplace_back(&curVal);
            return false;
        }
        nodes.emplace_back(&curVal);
        return true;
    });

    bool isKvSeparate = false;
    for (auto node = nodes.rbegin(); node != nodes.rend(); ++node) {
        count++;
        uint32_t len = (*node)->ValueDataLen();
        if (UNLIKELY(sliceTable != nullptr) && isValue && sliceTable->NeedKvSeparate(len)) {
            uint64_t blobId = 0;
            BResult result = sliceTable->WriteValueToBlobStore((*node), keyHashCode, stateId, blobId);
            if (UNLIKELY(result != BSS_OK)) {
                LOG_ERROR("Write value to blob store failed, ret: " << result);
                return result;
            }
            len = sizeof(uint64_t);
            mBuffer->WriteAt(reinterpret_cast<const uint8_t *>(&blobId), len, mValueDataBaseOffset + mWritenBytes);
            isKvSeparate = true;
        } else {
            mBuffer->WriteAt((*node)->Value(), len, mValueDataBaseOffset + mWritenBytes);
        }
        mWritenBytes += len;
    }
    auto valType = isKvSeparate ? ValueType::SEPARATE : value->ValueType();
    if (!isValue) {
        valType = count ? (deleteOrPut ? ValueType::PUT : ValueType::APPEND) : ValueType::DELETE;
    } else if (count > 1) {
        LOG_ERROR("Value type should not have more than one element");
    }
    // value offset;
    mValueOffsets[index] = (static_cast<uint8_t>(valType) << VALUE_INDICATOR_OFFSET) | (mWritenBytes & 0xFFFFFFF);
    return BSS_OK;
}

BResult Slice::Initialize(std::vector<std::pair<SliceKey, Value>> &kvPairs, const SliceCreateMeta &meta,
                          const MemManagerRef &memManager, bool forceMemory)
{
    if (kvPairs.empty()) {
        return BSS_INVALID_PARAM;
    }
    mMemManager = memManager;

    // create and initialize slice buffer.
    std::vector<std::pair<SliceKey, uint32_t>> sortedKeySlotList;
    BResult result = CreateAndInitBuffer(meta, kvPairs, sortedKeySlotList, forceMemory);
    RETURN_NOT_OK_NO_LOG(result);

    // write kv pair and sorted key slot index to buffer.
    result = FillBuffer(kvPairs, sortedKeySlotList);
    RETURN_NOT_OK(result);

    mInit = true;
    return BSS_OK;
}

BResult Slice::CreateAndInitBuffer(const SliceCreateMeta &meta, std::vector<std::pair<SliceKey, Value>> &kvPairs,
                                   std::vector<std::pair<SliceKey, uint32_t>> &sortedKeySlotList, bool forceMemory)
{
    uint32_t kvCount = kvPairs.size();

    // confirm index width.
    uint32_t indexCount = BssMath::RoundUpToPowerOfTwo(kvCount);
    uint32_t indexWidth = NO_4;
    if (indexCount > BYTE4_MAX_SLOT_SIZE) {
        indexWidth = NO_8;
    }

    // sort by rotate right HashCode.
    uint32_t rightShiftBits = NO_31 - BssMath::NumberOfLeadingZeros(indexCount);
    // sort kvPairs
    std::vector<std::pair<uint32_t, size_t>> sortedPairs;
    sortedPairs.reserve(kvPairs.size());
    for (size_t index = 0; index < kvPairs.size(); ++index) {
        const auto &pair = kvPairs[index];
        uint32_t rotatedHash = BssMath::RotateRight(pair.first.MixedHashCode(), rightShiftBits);
        sortedPairs.emplace_back(rotatedHash, index);
    }
    std::sort(sortedPairs.begin(), sortedPairs.end(), [](const auto &a, const auto &b) { return a.first < b.first; });
    std::vector<std::pair<SliceKey, Value>> newKvPairs;
    newKvPairs.reserve(kvPairs.size());
    for (const auto &pair : sortedPairs) {
        newKvPairs.emplace_back(std::move(kvPairs[pair.second].first), std::move(kvPairs[pair.second].second));
    }
    kvPairs = std::move(newKvPairs);

    // calculate buffer size
    uint32_t totalKeySize = 0;
    uint32_t totalValueSize = 0;
    StateIdInterval stateIdInterval;
    for (uint32_t slot = 0; slot < kvPairs.size(); slot++) {
        auto &pair = kvPairs.at(slot);
        auto &key = pair.first;
        totalKeySize += key.GetSerializeLength();

        // select sorted key.
        auto stateType = StateId::GetStateType(key.StateId());
#if SUPPORT_SLICE_SORT
        if (StateTypeUtil::HasSecKey(stateType)) {
            sortedKeySlotList.emplace_back(key, slot);
        }
#endif
        stateIdInterval.Update(key.StateId());
        // get value size.
        auto &value = pair.second;
        if (value.ValueType() != DELETE) {
            totalValueSize += value.ValueLen();
        }
    }

    uint32_t bufferSize = 0;
    // header
    bufferSize += sizeof(SliceHead);
    // index
    uint32_t indexBase = bufferSize;
    bufferSize += indexCount * indexWidth;
    // index
    uint32_t sortedIndexBase = bufferSize;
    uint32_t sortedKeyCount = sortedKeySlotList.size();
    bufferSize += sortedKeyCount * sizeof(uint32_t);
    // hash code
    uint32_t hashCodeBase = bufferSize;
    bufferSize += kvCount * sizeof(uint32_t);
    // seq id
    uint32_t seqIdBase = bufferSize;
    bufferSize += kvCount * sizeof(uint64_t);
    // key offset
    uint32_t keyOffsetBase = bufferSize;
    bufferSize += kvCount * sizeof(uint32_t);
    // keys
    bufferSize += totalKeySize;
    // value offset;
    uint32_t valueOffsetBase = bufferSize;
    bufferSize += kvCount * sizeof(uint32_t);
    // values
    bufferSize += totalValueSize;

    // create buffer
    uintptr_t address;
    auto retVal = mMemManager->GetMemory(MemoryType::SLICE_TABLE, bufferSize, address, forceMemory, mTimeWaitMemory);
    if (retVal != BSS_OK) {
        LOG_WARN("Alloc memory for init slice table failed, len:" << bufferSize << ", force:" << forceMemory);
        return retVal;
    }
    mBuffer = MakeRef<ByteBuffer>(reinterpret_cast<uint8_t *>(address), bufferSize, mMemManager);
    if (UNLIKELY(mBuffer == nullptr)) {
        mMemManager->ReleaseMemory(address);
        LOG_ERROR("Make ref buffer failed, newByteBuffer is null.");
        return BSS_ALLOC_FAIL;
    }

    // initialize header;
    uint8_t *data = mBuffer->Data();
    mHeader = reinterpret_cast<SliceHead *>(data);
    FormatHeader(mHeader, meta, kvCount, indexCount, sortedKeyCount, keyOffsetBase, valueOffsetBase, stateIdInterval);

    // initialize space
    mHashCodeSpace = std::make_shared<HashCodeSpace>(mBuffer, hashCodeBase);
    mSeqIdSpace = std::make_shared<SeqIdSpace>(mBuffer, seqIdBase);
    mIndexSpace = std::make_shared<IndexSpace>(mBuffer, indexBase, indexCount, indexWidth, false);
    mSortedIndexSpace = std::make_shared<SortedIndexSpace>(mBuffer, sortedIndexBase);
    mKeySpace = std::make_shared<KeySpace>(mBuffer, keyOffsetBase, kvCount);
    mValueSpace = std::make_shared<ValueSpace>(mBuffer, valueOffsetBase, kvCount);

    mSliceSpace.Init(mBuffer, kvCount, sortedKeyCount, totalKeySize, false);

    return BSS_OK;
}

BResult Slice::Initialize(RawDataSlice &rawDataSlice, const SliceCreateMeta &meta, const MemManagerRef &memManager,
                          bool &forceEvict, SliceTableManagerRef sliceTable)
{
    mMemManager = memManager;
    uint32_t kvCount = rawDataSlice.GetSliceData().size();
    if (UNLIKELY(kvCount == 0)) {
        return BSS_INVALID_PARAM;
    }

    // create and initialize slice buffer.
    std::vector<std::pair<BinaryKey, uint32_t>> sortedKeySlotList;
    RETURN_NOT_OK_NO_LOG(CreateAndInitBuffer(meta, rawDataSlice, sortedKeySlotList, forceEvict, sliceTable));
    RETURN_NOT_OK(FillBuffer(rawDataSlice, sortedKeySlotList, sliceTable));
    mInit = true;
    return BSS_OK;
}

namespace {

uint32_t GetTotalValLen(const MemorySegment &freshSegment, FreshValueNodePtr &value)
{
    uint32_t len = 0;
    value->VisitAsList(freshSegment, [&](FreshValueNode &curVal) {
        if (curVal.ValueType() != ValueType::DELETE) {
            len += curVal.ValueDataLen();
        }
        return curVal.ValueType() != ValueType::DELETE && curVal.ValueType() != ValueType::PUT;
    });
    return len;
}

}  // namespace

BResult Slice::CreateAndInitBuffer(const SliceCreateMeta &meta, RawDataSlice &rawDataSlice,
                                   std::vector<std::pair<BinaryKey, uint32_t>> &sortedKeySlotList, bool &forceEvict,
                                   SliceTableManagerRef sliceTable)
{
    auto &kvPairs = rawDataSlice.GetSliceData();
    auto &indexVec = rawDataSlice.GetVectorGroup()->GetIndexVec();
    auto &mixHashCodeVec = rawDataSlice.GetVectorGroup()->GetMixHashCodeVec();
    uint32_t kvCount = kvPairs.size();

    // confirm index width.
    uint32_t indexCount = BssMath::RoundUpToPowerOfTwo(kvCount);
    uint32_t indexWidth = NO_4;
    if (indexCount > BYTE4_MAX_SLOT_SIZE) {
        indexWidth = NO_8;
    }

    // sort by rotate right HashCode.
    uint32_t rightShiftBits = NO_31 - BssMath::NumberOfLeadingZeros(indexCount);
    std::vector<uint32_t> rotatedHashes;
    rotatedHashes.reserve(indexVec.size());
    for (uint32_t index : indexVec) {
        rotatedHashes.emplace_back(BssMath::RotateRight(mixHashCodeVec[index], rightShiftBits));
    }
    std::sort(indexVec.begin(), indexVec.end(),
              [&](uint32_t a, uint32_t b) { return rotatedHashes[a] < rotatedHashes[b]; });

    // calculate buffer size
    uint32_t totalKeySize = 0;
    uint32_t totalValueSize = 0;
    StateIdInterval stateIdInterval;
    uint32_t slot = 0;
    for (uint32_t index : indexVec) {
        auto &pair = kvPairs[index];
        auto &key = pair.first;
        totalKeySize += key.GetSerializeLength();

        // select sorted key.
        auto stateType = StateId::GetStateType(key.mPrimaryKey.mStateId);
#if SUPPORT_SLICE_SORT
        if (StateTypeUtil::HasSecKey(stateType)) {
            sortedKeySlotList.emplace_back(key, slot);
        }
        slot++;
#endif
        stateIdInterval.Update(key.mPrimaryKey.mStateId);
        // get value size.
        auto &value = pair.second;
        uint32_t valueLen = GetTotalValLen(*rawDataSlice.GetFreshSegment(), value);
        if (UNLIKELY(sliceTable != nullptr) && key.mIsValue && sliceTable->NeedKvSeparate(valueLen)) {
            // 开启KV分离，只存储blobId
            valueLen = sizeof(uint64_t);
        }
        totalValueSize += valueLen;
        forceEvict = forceEvict || (valueLen > IO_SIZE_4M && StateId::IsList(key.mPrimaryKey.mStateId));
    }

    uint32_t bufferSize = 0;
    // header
    bufferSize += sizeof(SliceHead);
    // index
    uint32_t indexBase = bufferSize;
    bufferSize += indexCount * indexWidth;
    // index
    uint32_t sortedIndexBase = bufferSize;
    uint32_t sortedKeyCount = sortedKeySlotList.size();
    bufferSize += sortedKeyCount * sizeof(uint32_t);
    // hash code
    uint32_t hashCodeBase = bufferSize;
    bufferSize += kvCount * sizeof(uint32_t);
    // seq id
    uint32_t seqIdBase = bufferSize;
    bufferSize += kvCount * sizeof(uint64_t);
    // key offset
    uint32_t keyOffsetBase = bufferSize;
    bufferSize += kvCount * sizeof(uint32_t);
    // keys
    bufferSize += totalKeySize;
    // value offset;
    uint32_t valueOffsetBase = bufferSize;
    bufferSize += kvCount * sizeof(uint32_t);
    // values
    bufferSize += totalValueSize;

    // create buffer
    uintptr_t addr = NO_0;
    BResult ret = mMemManager->GetMemoryDirect(MemoryType::SLICE_TABLE, bufferSize, addr);
    RETURN_NOT_OK_NO_LOG(ret);
    mBuffer = MakeRef<ByteBuffer>(reinterpret_cast<uint8_t *>(addr), bufferSize, mMemManager);
    if (UNLIKELY(mBuffer == nullptr)) {
        mMemManager->ReleaseMemory(addr);
        LOG_ERROR("Make ref buffer failed, byteBuffer is null.");
        return BSS_ALLOC_FAIL;
    }
    // initialize header;
    uint8_t *data = mBuffer->Data();
    mHeader = reinterpret_cast<SliceHead *>(data);
    FormatHeader(mHeader, meta, kvCount, indexCount, sortedKeyCount, keyOffsetBase, valueOffsetBase, stateIdInterval);

    // initialize space
    mHashCodeSpace = std::make_shared<HashCodeSpace>(mBuffer, hashCodeBase);
    mSeqIdSpace = std::make_shared<SeqIdSpace>(mBuffer, seqIdBase);
    mIndexSpace = std::make_shared<IndexSpace>(mBuffer, indexBase, indexCount, indexWidth, false);
    mSortedIndexSpace = std::make_shared<SortedIndexSpace>(mBuffer, sortedIndexBase);
    mKeySpace = std::make_shared<KeySpace>(mBuffer, keyOffsetBase, kvCount);
    mValueSpace = std::make_shared<ValueSpace>(mBuffer, valueOffsetBase, kvCount);

    mSliceSpace.Init(mBuffer, kvCount, sortedKeyCount, totalKeySize, false);
    return BSS_OK;
}

void Slice::FormatHeader(SliceHead *header, const SliceCreateMeta &meta, uint32_t keyCount, uint32_t indexCount,
                         uint32_t sortedKeyCount, uint32_t keyOffsetBase, uint32_t valueOffsetBase,
                         StateIdInterval stateIdInterval)
{
    header->sliceId = meta.logicSliceId;
    header->keyCount = keyCount;
    header->indexCount = indexCount;
    header->keyOffsetBase = keyOffsetBase;
    header->valueOffsetBase = valueOffsetBase;
    header->version = meta.version;
    header->sortedKeyCount = sortedKeyCount;
    header->formatVersion = 0;
    header->stateIdInterval = stateIdInterval;
}

BResult Slice::FillBuffer(const std::vector<std::pair<SliceKey, Value>> &kvPairs,
                          std::vector<std::pair<SliceKey, uint32_t>> &sortedKeySlotList)
{
    // put key and value.
    uint32_t curIndex = 0;
    for (auto &kv : kvPairs) {
        RETURN_NOT_OK(PutKv(curIndex++, kv));
    }

    // sort by primary and secondary key.
    if (!sortedKeySlotList.empty()) {
        std::sort(sortedKeySlotList.begin(), sortedKeySlotList.end(),
                  [](const std::pair<SliceKey, uint32_t> &first, const std::pair<SliceKey, uint32_t> &second) {
                      return first.first.ComparePrimaryKeyAndSecondKey(second.first);
                  });
        // put sorted key index.
        uint32_t slot = 0;
        for (const auto &sortedKeySlot : sortedKeySlotList) {
            mSortedIndexSpace->Put(slot++, sortedKeySlot.second);
        }
    }

    return BSS_OK;
}

BResult Slice::FillBuffer(RawDataSlice &rawDataSlice, std::vector<std::pair<BinaryKey, uint32_t>> &sortedKeySlotList,
                          SliceTableManagerRef sliceTable)
{
    // put key and value.
    uint32_t curIndex = 0;
    auto &kvPairs = rawDataSlice.GetSliceData();
    auto &indexVec = rawDataSlice.GetVectorGroup()->GetIndexVec();
    uint32_t indexNum = indexVec.size();
    const auto &freshSegment = *rawDataSlice.GetFreshSegment();
    auto &mixHashCodeVec = rawDataSlice.GetVectorGroup()->GetMixHashCodeVec();
    std::vector<uint32_t> indexIdVec;
    indexIdVec.reserve(indexNum * sizeof(uint32_t));
    // hash code
#if defined(__aarch64__)
#ifdef BUILD_SVE
    {
        const uint32_t *indicesPtr = indexVec.data();
        const uint32_t *srcVecPtr = mixHashCodeVec.data();
        const auto &dataPtr = mHashCodeSpace->GetDataPtr();
        const auto &srcIndexIdVecPtr = indexIdVec.data();
        const uint32_t vl = svcntw();
        const uint32_t mainLoopEnd = indexNum & ~(vl - 1);  // 对齐边界
        const svbool_t allPred = svptrue_b32();
        const svuint32_t maskVec = svdup_n_u32(mHeader->indexCount - 1);

        for (uint32_t i = 0; i < mainLoopEnd; i += vl) {
            svuint32_t index_vec = svld1_u32(allPred, indicesPtr + i);
            svuint32_t data_vec = svld1_gather_u32index_u32(allPred, srcVecPtr, index_vec);
            svst1_u32(allPred, dataPtr + i, data_vec);
            svuint32_t indexId_vec = svand_u32_z(allPred, data_vec, maskVec);
            svst1_u32(allPred, srcIndexIdVecPtr + i, indexId_vec);
        }

        uint32_t tailStart = mainLoopEnd;
        if (tailStart < indexNum) {
            const svbool_t tailPred = svwhilelt_b32(tailStart, indexNum);
            svuint32_t index_vec = svld1_u32(tailPred, indicesPtr + tailStart);
            svuint32_t data_vec = svld1_gather_u32index_u32(tailPred, srcVecPtr, index_vec);
            svst1_u32(tailPred, dataPtr + tailStart, data_vec);
            svuint32_t indexId_vec = svand_u32_z(tailPred, data_vec, maskVec);
            svst1_u32(tailPred, srcIndexIdVecPtr + tailStart, indexId_vec);
        }
    }
#else
    uint32_t hashIndex = 0;
    for (uint32_t index : indexVec) {
        mHashCodeSpace->Put(hashIndex, mixHashCodeVec[index]);
        indexIdVec.emplace_back(mixHashCodeVec[index] & (mHeader->indexCount - 1));
        hashIndex++;
    }
#endif
#else
    uint32_t hashIndex = 0;
    for (uint32_t index : indexVec) {
        std::string arr(IO_SIZE_256K, '\0');
        char *arrPtr = &arr[0];
        auto ret = memset_s(arrPtr, IO_SIZE_256K, 'a', IO_SIZE_256K);
        if (UNLIKELY(ret != EOK)) {
            LOG_ERROR("arrPtr memset failed, ret: " << ret << ".");
            return BSS_ERR;
        }
        mHashCodeSpace->Put(hashIndex, mixHashCodeVec[index]);
        indexIdVec.emplace_back(mixHashCodeVec[index] & (mHeader->indexCount - 1));
        hashIndex++;
    }
#endif

    for (uint32_t index : indexVec) {
        auto &kv = kvPairs[index];
        // update index;
        mIndexSpace->Put(indexIdVec[curIndex], curIndex);

        // key
        BinaryKey key = kv.first;
        mKeySpace->Put(curIndex, key);

        // value
        uint64_t seqId = 0;
        RETURN_NOT_OK(mValueSpace->Put(curIndex, kv.second, freshSegment, kv.first.mIsValue, sliceTable,
                                       key.KeyHashCode(), key.StateId(), seqId));

        // seqId;
        mSeqIdSpace->Put(curIndex, seqId);
        curIndex++;
    }
    // sort by primary and secondary key.
    if (!sortedKeySlotList.empty()) {
        std::sort(sortedKeySlotList.begin(), sortedKeySlotList.end(),
                  [](const std::pair<BinaryKey, uint32_t> &first, const std::pair<BinaryKey, uint32_t> &second) {
                      return first.first.ComparePrimaryKeyAndSecondKey(second.first) < 0;
                  });
        // put sorted key index.
#if defined(__aarch64__)
#ifdef BUILD_SVE
        {
            const uint32_t total = sortedKeySlotList.size();
            uint32_t *destPtr = mSortedIndexSpace->GetDataPtr();
            std::pair<BinaryKey, uint32_t> pair;
            const uint32_t structSize = sizeof(std::pair<BinaryKey, uint32_t>);
            const uint32_t secondOffset = reinterpret_cast<const char *>(&pair.second) -
                                          reinterpret_cast<const char *>(&pair);
            auto basePtr = reinterpret_cast<const uint32_t *>(sortedKeySlotList.data());

            const uint32_t vl = svcntw();
            const uint32_t mainLoopEnd = total & ~(vl - 1);  // 对齐边界
            const svbool_t allPred = svptrue_b32();

            for (uint32_t i = 0; i < mainLoopEnd; i += vl) {
                svuint32_t indices = svindex_u32(i, 1);
                // index * structSize + secondOffset
                svuint32_t byteOffsets = svmad_u32_x(allPred, indices, svdup_u32(structSize), svdup_u32(secondOffset));
                svuint32_t data = svld1_gather_u32offset_u32(allPred, basePtr, byteOffsets);
                svst1(allPred, &destPtr[i], data);
            }

            uint32_t tailStart = mainLoopEnd;
            if (tailStart < total) {
                const svbool_t tailPred = svwhilelt_b32(tailStart, total);
                svuint32_t indices = svindex_u32(tailStart, 1);
                // index * structSize + secondOffset
                svuint32_t byteOffsets = svmad_u32_x(tailPred, indices, svdup_u32(structSize), svdup_u32(secondOffset));
                svuint32_t data = svld1_gather_u32offset_u32(tailPred, basePtr, byteOffsets);
                svst1(tailPred, &destPtr[tailStart], data);
            }
        }
#else
        uint32_t slot = 0;
        for (const auto &sortedKeySlot : sortedKeySlotList) {
            mSortedIndexSpace->Put(slot++, sortedKeySlot.second);
        }
#endif
#else
        uint32_t slot = 0;
        for (const auto &sortedKeySlot : sortedKeySlotList) {
            mSortedIndexSpace->Put(slot++, sortedKeySlot.second);
        }
#endif
    }
    return BSS_OK;
}

void Slice::GetSliceKVMap(SliceKVMap &dataMap, bool skipDeleted)
{
    uint32_t keyCount = mHeader->keyCount;

    dataMap.clear();
    dataMap.reserve(keyCount);

    for (uint32_t i = 0; i < keyCount; ++i) {
        auto sliceKey = GetBinaryKey(i);
        Value value = mValueSpace->Get(i, mSeqIdSpace->Get(i));
        if (skipDeleted && (value.ValueType() == DELETE)) {
            continue;
        }
        dataMap[sliceKey] = value;
    }
}

KeyValueIteratorRef Slice::SubIterator(const Key &prefixKey, bool skipDeleted)
{
    if (mHeader->keyCount == 0 || mHeader->sortedKeyCount == 0) {
        return nullptr;
    }
    if (!mHeader->stateIdInterval.Contains(prefixKey.PriKey().StateId())) {
        return nullptr;
    }

    uint32_t startSlot = FindStartSortedIndexSlot(prefixKey);
    if (startSlot >= mHeader->sortedKeyCount) {
        return nullptr;
    }
    uint32_t endSlot = FindEndSortedIndexSlot(prefixKey);
    if (endSlot < startSlot || endSlot == INVALID_U32_INDEX) {
        return nullptr;
    }

    std::vector<KeyValueRef> keyValues;
    for (uint32_t slot = startSlot; slot <= endSlot; ++slot) {
        auto keyValue = MakeRef<KeyValue>();
        mSliceSpace.GetKeyValue(mSliceSpace.GetIndexBySortIndex(slot), keyValue);
        if (!skipDeleted || keyValue->value.ValueType() != DELETE) {
            keyValues.emplace_back(std::move(keyValue));
        }
    }
    return std::make_shared<KeyValueVectorIterator>(std::move(keyValues));
}

SliceKey Slice::GetBinaryKey(uint32_t keyCursor)
{
    SliceKey sliceKey;
    uint32_t mixHashCode = mHashCodeSpace->Get(keyCursor);
    mKeySpace->Get(keyCursor, mixHashCode, sliceKey);
    return sliceKey;
}

uint32_t Slice::GetCompactionCount()
{
    if (!mInit) {
        return 0;
    }
    return mHeader->keyCount;
}

BResult Slice::BytesSize(uint32_t &size)
{
    if (!mInit) {
        return BSS_NOT_READY;
    }
    size = mBuffer->Capacity();
    return BSS_OK;
}

void Slice::RestoreSliceUseByteBuffer(ByteBufferRef byteBuffer, MemManagerRef memManager)
{
    mInit = true;
    mBuffer = byteBuffer;
    mMemManager = memManager;

    // 按照byteBuffer写进去的格式初始化header信息以及几个baseOffset数据
    uint8_t *data = mBuffer->Data();
    mHeader = reinterpret_cast<SliceHead *>(data);

    uint32_t kvCount = mHeader->keyCount;
    uint32_t indexCount = mHeader->indexCount;
    // confirm index width.
    uint32_t indexWidth = NO_4;
    if (indexCount > BYTE4_MAX_SLOT_SIZE) {
        indexWidth = NO_8;
    }

    uint32_t bufferSize = 0;
    // header
    bufferSize += sizeof(SliceHead);
    // index
    uint32_t indexBase = bufferSize;
    bufferSize += indexCount * indexWidth;

    // sorted key index
    uint32_t sortedIndexBase = bufferSize;
    bufferSize += mHeader->sortedKeyCount * sizeof(uint32_t);

    // hash code
    uint32_t hashCodeBase = bufferSize;
    bufferSize += kvCount * sizeof(uint32_t);
    // seq id
    uint32_t seqIdBase = bufferSize;
    // key offset
    uint32_t keyOffsetBase = mHeader->keyOffsetBase;
    // value offset;
    uint32_t valueOffsetBase = mHeader->valueOffsetBase;

    // initialize space
    mHashCodeSpace = std::make_shared<HashCodeSpace>(mBuffer, hashCodeBase);

    mSeqIdSpace = std::make_shared<SeqIdSpace>(mBuffer, seqIdBase);

    mIndexSpace = std::make_shared<IndexSpace>(mBuffer, indexBase, indexCount, indexWidth, true);

    mSortedIndexSpace = std::make_shared<SortedIndexSpace>(mBuffer, sortedIndexBase);

    mKeySpace = std::make_shared<KeySpace>(mBuffer, keyOffsetBase, kvCount);
    mValueSpace = std::make_shared<ValueSpace>(mBuffer, valueOffsetBase, kvCount);

    // 计算totalKeySize, 用于恢复mSliceSpace
    uint32_t totalKeySize = valueOffsetBase - kvCount * sizeof(uint32_t) - keyOffsetBase;
    mSliceSpace.Init(mBuffer, kvCount, mHeader->sortedKeyCount, totalKeySize, true);
}

bool Slice::Get(const Key &key, Value &value)
{
    if (UNLIKELY(!mInit)) {
        return false;
    }

    uint32_t targetMixedHashCode = key.MixedHashCode();
    uint32_t indexLen = mHeader->indexCount;
    uint32_t indexSlot = targetMixedHashCode & (indexLen - 1);

    uint32_t count = 0;
    uint32_t startSlot = 0;
    if (!mIndexSpace->Get(indexSlot, count, startSlot)) {
        return false;
    }

    bool useBinarySearch = (count > NO_3 && mHeader->formatVersion >= 0);
    if (useBinarySearch) {
        return BinarySearch(key, targetMixedHashCode, startSlot, count, value);
    }

    return LinearSearch(key, targetMixedHashCode, startSlot, count, value);
}

uint32_t Slice::FindStartSortedIndexSlot(const Key &startKey)
{
    if (UNLIKELY(mHeader->sortedKeyCount == 0)) {
        return 0;
    }

    uint32_t low = 0;
    uint32_t high = mHeader->sortedKeyCount - 1;

    auto &startPriKey = startKey.PriKey();
    while (low <= high) {
        uint32_t mid = low + ((high - low) >> NO_1);
        int32_t cmp = mSliceSpace.FindPriKeyBySortedIndex(mid, startPriKey);
        if (cmp < 0) {
            low = mid + 1;
            continue;
        }
        if (mid == 0) {
            return 0;
        }
        high = mid - 1;
    }
    return high + 1;
}

uint32_t Slice::FindEndSortedIndexSlot(const Key &endKey)
{
    uint32_t sortedKeyCount = mSliceSpace.SortedKeyCount();
    if (UNLIKELY(sortedKeyCount == 0)) {
        return 0;
    }

    uint32_t low = 0;
    uint32_t high = sortedKeyCount - 1;

    auto &endPriKey = endKey.PriKey();
    while (low <= high) {
        uint32_t mid = low + ((high - low) >> NO_1);
        int32_t cmp = mSliceSpace.FindPriKeyBySortedIndex(mid, endPriKey);
        if (cmp > 0) {
            if (mid == 0) {
                break;
            }
            high = mid - 1;
            continue;
        }
        low = mid + 1;
    }
    return low == 0 ? INVALID_U32_INDEX : low - 1;
}

BResult Slice::BinarySearchBound(uint32_t targetMixedHashCode, uint32_t startSlot, uint32_t indexCount,
                                 uint32_t &lowerBound, uint32_t &upperBound)
{
    uint32_t low = 0;
    uint32_t high = indexCount;
    uint32_t mid = 0;
    uint32_t mixedHashCode = 0;
    while (low < high) {
        mid = low + ((high - low) >> NO_1);
        mixedHashCode = mSliceSpace.GetKeyMixedHashCode(startSlot + mid);
        if (mixedHashCode < targetMixedHashCode) {
            low = mid + 1;
        } else {
            high = mid;
        }
    }

    if (low == indexCount || mSliceSpace.GetKeyMixedHashCode(startSlot + low) != targetMixedHashCode) {
        return false;
    }

    lowerBound = low;
    upperBound = lowerBound;
    while (upperBound < indexCount && mSliceSpace.GetKeyMixedHashCode(startSlot + upperBound) == targetMixedHashCode) {
        upperBound++;
    }
    return true;
}

void KeySpace::Get(uint32_t index, uint32_t mixHashCode, SliceKey &sliceKey)
{
    uint32_t keyEndOffset = mKeyOffsets[index];
    uint32_t keyStartOffset = 0;
    if (index == 0) {
        keyStartOffset = 0;
    } else {
        keyStartOffset = mKeyOffsets[index - 1];
    }
    sliceKey.Unpack(mBuffer, mKeyDataBaseOffset + keyStartOffset, keyEndOffset - keyStartOffset, mixHashCode);
}
}  // namespace bss
}  // namespace ock
