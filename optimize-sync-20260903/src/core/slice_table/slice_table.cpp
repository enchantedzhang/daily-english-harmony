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

#include "slice_table.h"

#include <memory>
#include <stack>

#include "compaction/slice_compactor.h"
#include "db/boost_state_dbgroup.h"
#include "slice_table/slice/compare_slice_key.h"
#include "slice_table/slice/data_slice.h"
#include "snapshot/sorted_key_value_merging_iterator.h"

namespace ock {
namespace bss {
BResult SliceTable::TryCurrentDBEvict(int64_t addedSize, bool isSync, bool isForce, uint32_t minSize)
{
    mEvictManager->AddSliceUsedMemory(addedSize);
    return mEvictManager->TryEvict(isSync, isForce, minSize);
}

BResult SliceTable::TryCurrentSlotEvict(int64_t addedSize, bool isSync, bool isForce, uint32_t minSize)
{
    mEvictManager->AddSliceUsedMemory(addedSize);
    auto boostDbGroup = BoostStateDbGroupMgr::GetBoostStateDbGroup(mConfig->GetTaskSlotFlag());
    if (UNLIKELY(boostDbGroup == nullptr)) {
        // 如果boostDbGroup对象已经删除则使用自身对象做淘汰.
        return mEvictManager->TryEvict(isSync, isForce, minSize);
    }
    return boostDbGroup->TryEvict(isSync, isForce, minSize);
}

BResult SliceTable::TryCompact(const SliceIndexContextRef &sliceIndexContext,
                               const CompactCompletedNotify &compactCompletedNotify)
{
    if (UNLIKELY(mIsCompaction.load(std::memory_order_acquire) == SliceTableCompaction::CLOSED)) {
        LOG_DEBUG("Snapshot is running, skip compaction.");
        return BSS_OK;
    }
    if (UNLIKELY(sliceIndexContext == nullptr)) {
        LOG_ERROR("Impassable, received compact slice index context is nullptr.");
        return BSS_INVALID_PARAM;
    }

    LogicalSliceChainRef currentLogicalSliceChain = sliceIndexContext->GetLogicalSliceChain();
    if (UNLIKELY(currentLogicalSliceChain == nullptr)) {
        LOG_ERROR("Current logic slice chain is nullptr.");
        return BSS_INNER_ERR;
    }

    const uint32_t bucketIndex = sliceIndexContext->GetSliceIndexSlot();
    if (UNLIKELY(currentLogicalSliceChain != mSliceBucketIndex->GetLogicChainedSlice(bucketIndex))) {
        LOG_ERROR("Current logic slice chain not matched.");
        return BSS_INNER_ERR;
    }

    int32_t tailIndex = currentLogicalSliceChain->GetSliceChainTailIndex();
    if (tailIndex == -1) {  // tail == -1 表示没有slice，不需要compact
        return BSS_OK;
    }

    auto tailSliceChainIndex = static_cast<uint32_t>(tailIndex);
    uint32_t normalSliceNum = (tailSliceChainIndex <= currentLogicalSliceChain->GetBaseSliceIndex()) ?
                                  0 :
                                  tailSliceChainIndex - currentLogicalSliceChain->GetBaseSliceIndex();
    if (normalSliceNum + 1 < mConfig->GetInMemoryCompactionThreshold()) {
        LOG_DEBUG("Not satisfied compaction condition, normalSliceNum:" << normalSliceNum << ", threshold:"
                                                                        << mConfig->GetInMemoryCompactionThreshold());
        return TryCleanUpEvictedSlices(currentLogicalSliceChain, bucketIndex);
    }
    if (UNLIKELY(!currentLogicalSliceChain->CompareAndSetStatus(SliceStatus::NORMAL, SliceStatus::COMPACTING))) {
        LOG_DEBUG("Before execute slice chain compact, set slice chain status failed, current status:"
                  << static_cast<uint32_t>(currentLogicalSliceChain->GetSliceStatus()));
        return BSS_OK;
    }

    // compact slice.
    BResult result = mCompactManager->AsyncCompactSlice(sliceIndexContext, compactCompletedNotify);
    if (UNLIKELY(result != BSS_OK)) {
        if (UNLIKELY(!currentLogicalSliceChain->CompareAndSetStatus(SliceStatus::COMPACTING, SliceStatus::NORMAL))) {
            LOG_ERROR("After execute slice chain compact failed, set slice chain status failed, current status:"
                      << static_cast<uint32_t>(currentLogicalSliceChain->GetSliceStatus()));
        }
    }
    return result;
}

int32_t SliceTable::AddSlice(const SliceIndexContextRef &curSliceIndexContext,
                             std::vector<std::pair<SliceKey, Value>> &dataList, uint64_t version)
{
    LogicalSliceChainRef currentLogicalSliceChain = curSliceIndexContext->GetLogicalSliceChain();
    if (currentLogicalSliceChain == nullptr || currentLogicalSliceChain->IsNone()) {
        LOG_ERROR("BUG! add slice receive NO_SLICE request");
        return -1;
    }
    if (dataList.empty()) {
        return 0;
    }

    SliceRef slice = std::make_shared<Slice>();
    SliceCreateMeta meta = { 1 };
    BResult ret = slice->Initialize(dataList, meta, mMemManager);
    if (ret != BSS_OK) {
        LOG_WARN("slice init with empty value!");
        return -1;
    }

    auto dataSliceImpl = std::make_shared<DataSlice>();
    dataSliceImpl->Init(slice);
    //  将DataSlice转换为SliceAddress并加入对应的chain中
    SliceAddressRef sliceAddress = currentLogicalSliceChain->CreateSlice(dataSliceImpl, mAccessRecorder->AccessCount());
    sliceAddress->AddRequestCount(GetSnapshotVersion() & 0xFFFFL);
    auto sliceSize = dataSliceImpl->GetSize();
    if (UNLIKELY(sliceSize > INT32_MAX)) {
        LOG_ERROR("slice size is:" << sliceSize << ", larger than INT32_MAX!");
        return -1;
    }
    return static_cast<int32_t>(sliceSize);
}

BResult SliceTable::AddSlice(const SliceIndexContextRef &curSliceIndexContext, RawDataSlice &rawDataSlice,
                             uint32_t &addSize, bool &forceEvict)
{
    if (UNLIKELY(rawDataSlice.GetSliceData().empty())) {
        return BSS_OK;
    }

    SliceRef slice = std::make_shared<Slice>();
    SliceCreateMeta meta = { 1 };
    BResult ret = slice->Initialize(rawDataSlice, meta, mMemManager, forceEvict, shared_from_this());
    if (UNLIKELY(ret != BSS_OK)) {
        LOG_LIMIT_WARN("Initialize slice failed, ret:" << ret << ", requires internal retry.");
        return ret;
    }

    DataSliceRef dataSlice = std::make_shared<DataSlice>();
    dataSlice->Init(slice);
    // 将DataSlice转换为SliceAddress并加入对应的chain中
    auto currentLogicalSliceChain = curSliceIndexContext->GetLogicalSliceChain();
    auto sliceAddress = currentLogicalSliceChain->CreateSlice(dataSlice, mAccessRecorder->AccessCount());
    RETURN_ERROR_AS_NULLPTR(sliceAddress);
    sliceAddress->AddRequestCount(GetSnapshotVersion() & 0xFFFFL);
    addSize = dataSlice->GetSize();
    return BSS_OK;
}

BResult SliceTable::WriteValueToBlobStore(FreshValueNodePtr &curVal, uint32_t keyHash, uint16_t stateId,
                                          uint64_t &blobId)
{
    RETURN_ERROR_AS_NULLPTR(curVal);
    if (!mIsKVSeparate || curVal->ValueType() == ValueType::DELETE) {
        return BSS_OK;
    }
    if (curVal->ValueDataLen() > mKvThreshold) {
        RETURN_ERROR_AS_NULLPTR(mConfig);
        RETURN_ERROR_AS_NULLPTR(mStateFilterManager);
        RETURN_ERROR_AS_NULLPTR(mBlobStore);
        uint32_t keyGroup = mKeyGroupUtil->ComputeKeyGroupForKeyHash(keyHash);
        int64_t tableTtl = 0;
        uint64_t expireTime = 0;
        if (mConfig->GetTtlFilterSwitch()) {
            tableTtl = mStateFilterManager->GetTtlTime(stateId);
        }
        if (tableTtl > 0) {
            expireTime = SeqIDUtils::GetTimestamp(curVal->ValueSeqId()) + static_cast<uint64_t>(tableTtl);
        }
        BResult result;
        do {
            result = mBlobStore->WriteBlobValue(curVal->Value(), curVal->ValueDataLen(), expireTime, keyGroup, blobId);
            if (UNLIKELY(result == BSS_ALLOC_FAIL)) {
                // 基于当前slot, 异步淘汰SliceTable数据
                TryCurrentSlotEvict(0, false, true);
            }
        } while (UNLIKELY(result == BSS_ALLOC_FAIL) && (usleep(NO_100), 1));

        if (UNLIKELY(result != BSS_OK)) {
            LOG_ERROR("Blob store write failed, keyHash: " << keyHash << ", value len: " << curVal->ValueDataLen()
                                                           << ", result: " << result);
            return result;
        }
    }
    return BSS_OK;
}

BResult SliceTable::GetValueFromBlobStore(uint64_t blobId, uint32_t keyHashCode, uint64_t seqId, Value &value)
{
    auto keyGroup = mKeyGroupUtil->ComputeKeyGroupForKeyHash(keyHashCode);
    RETURN_INVALID_PARAM_AS_NULLPTR(mBlobStore);
    BResult result = mBlobStore->GetBlobValue(blobId, keyGroup, value);
    if (UNLIKELY(result != BSS_OK)) {
        LOG_ERROR("Get blob value failed, blobId: " << blobId << ", keyGroup: " << keyGroup
                                                    << ", keyHashCode: " << keyHashCode << ", result: " << result);
    }
    value.SetSeqId(seqId);
    return result;
}

BResult SliceTable::Initialize(const ConfigRef &config, const FileCacheManagerRef &fileCache,
                               const MemManagerRef &memManager, const StateFilterManagerRef &stateFilterManager)
{
    RETURN_ERROR_AS_NULLPTR(config);
    mConfig = config;
    mIsKVSeparate = config->GetEnableKVSeparate();
    mKvThreshold = config->GetBlobValueSizeThreshold() > sizeof(uint64_t) ? config->GetBlobValueSizeThreshold() :
                                                                            sizeof(uint64_t);
    mMemManager = memManager;
    mFileCache = fileCache;
    mStateFilterManager = stateFilterManager;
    mKeyGroupUtil = stateFilterManager->GetKeyGroupUtil();

    uint32_t bucketNum = ComputeIndexBucketNum(IO_SIZE_64M, config);
    SliceBucketIndexRef sliceIndexHash = std::make_shared<SliceBucketIndex>();
    BResult sliceIndexRet = sliceIndexHash->Initialize(bucketNum, config);
    if (sliceIndexRet != BSS_OK) {
        return sliceIndexRet;
    }
    mSliceBucketIndex = sliceIndexHash;
    RETURN_ERROR_AS_NULLPTR(mSliceBucketIndex);
    uint32_t bucketGroupNum = ComputeBucketGroupNum(bucketNum, config);
    auto ret = mBucketGroupManager->Initialize(config, mSliceBucketIndex, fileCache, bucketGroupNum, bucketNum,
                                               mMemManager, mStateFilterManager);
    RETURN_NOT_OK(ret);
    // create access recorder.
    mAccessRecorder = std::make_shared<AccessRecorder>();

    // create and initialize evict.
    mEvictManager = std::make_shared<EvictManager>();

    ret = mEvictManager->Initialize(config, mBucketGroupManager, mAccessRecorder);
    if (ret != BSS_OK) {
        LOG_ERROR("initialize evict manager failed!");
        return BSS_ERR;
    }

    // create and initialize compactor.
    mCompactManager = std::make_shared<SliceCompactionTrigger>();
    ret = mCompactManager->Init(config, mSliceBucketIndex, mMemManager, mStateFilterManager);
    if (ret != BSS_OK) {
        LOG_ERROR("initialize compact manager failed!");
        mEvictManager->Exit();
        return BSS_ERR;
    }

    if (mIsKVSeparate) {
        RETURN_NOT_OK(InitializeBlobStore());
    }

    return BSS_OK;
}

BResult SliceTable::InitializeBlobStore()
{
    mBlobStore = std::make_shared<BlobStore>();
    uint64_t fileMemLimit = mMemManager->GetMemoryTypeMaxSize(MemoryType::FILE_STORE);
    float indexCacheRatio = 0.0f;
    if (mConfig->GetCacheIndexAndFilterSwitch()) {
        indexCacheRatio = mConfig->GetCacheIndexAndFilterRatio();
    }
    auto blockCache = BlockCacheManager::Instance()->CreateBlockCache(mConfig->GetTaskSlotFlag(), fileMemLimit,
                                                                      indexCacheRatio);
    mNeedReleaseBlockCache = true;
    RETURN_NOT_OK(mBlobStore->Init(mMemManager, mFileCache, mConfig, blockCache));
    auto tombstoneService = mBlobStore->CreateTombstoneService("SliceMerge", mKeyGroupUtil);
    mTombstoneService = tombstoneService;
    mCompactManager->RegisterTombstoneService(tombstoneService);
    RegisterTombstoneService();
    return BSS_OK;
}

void SliceTable::Exit()
{
    if (mCompactManager != nullptr) {
        mCompactManager->Exit();
    }

    if (mEvictManager != nullptr) {
        mEvictManager->Exit();
        mEvictManager = nullptr;  // 置空避免循环引用.
    }
}

uint32_t SliceTable::ComputeIndexBucketNum(uint64_t totalMem, const ConfigRef &config)
{
    uint32_t sliceSizePerBucket = config->GetSliceStandardSizePerBucket();
    uint32_t bucketNum = std::max(static_cast<uint64_t>(1), (totalMem / sliceSizePerBucket));
    if (bucketNum < NO_10) {
        LOG_WARN("The memory mSize totalMem="
                 << totalMem << ",mBucketNum= " << bucketNum
                 << "is too small which may affect the performance of boost-state-store. Please consider increasing "
                    "the memory used by BSS.");
    }

    uint32_t upper = BssMath::RoundUpToPowerOfTwo(bucketNum);
    uint32_t lower = BssMath::RoundDownToPowerOfTwo(bucketNum);

    LOG_INFO("totalMem = " << totalMem << ", sliceSizePerBucket = " << sliceSizePerBucket
                           << ", mBucketNum = " << bucketNum);

    if (bucketNum <= lower / NO_2 + upper / NO_2) {
        return lower;
    }
    return upper;
}

uint32_t SliceTable::ComputeBucketGroupNum(uint32_t bucketNum, const ConfigRef &config)
{
    uint32_t sliceSizePerBucket = config->GetSliceStandardSizePerBucket();
    uint64_t payloadPerBucketGroup = config->GetPayloadPerBucketGroup();
    return std::max(static_cast<uint64_t>(1),
                    bucketNum / std::max(static_cast<uint64_t>(1), payloadPerBucketGroup / sliceSizePerBucket));
}

BResult SliceTable::InternalGetList(const Key &key, std::deque<Value> &result,
                                    std::vector<SectionsReadContextRef> &readMetas)
{
    mAccessRecorder->Record();
    LogicalSliceChainRef logicalSliceChain = mSliceBucketIndex->GetLogicalSliceChain(key);
    if (UNLIKELY(logicalSliceChain == nullptr)) {
        LOG_ERROR("Get logical slice chain failed, hashCode:" << key.KeyHashCode());
        return BSS_ERR;
    }
    if (logicalSliceChain->IsEmpty() && !logicalSliceChain->HasFilePage()) {
        LOG_DEBUG("Logical slice chain is empty.");
        AddSliceMiss();
        AddSliceReadCount();
        return BSS_NOT_EXISTS;
    }

    AddSliceReadCount();

    int32_t curIndex = logicalSliceChain->GetSliceChainTailIndex();
    uint32_t readLength = 0;
    Value finalResult;
    std::stack<Value> mergingValues;
    bool isFound = false;
    GetFromSliceChain(key, logicalSliceChain, curIndex, readLength, mergingValues, finalResult, isFound);
    AddSliceListHitMiss(mergingValues, isFound);

    // 从LsmStore中读取List value.
    if (!isFound) {
        auto ret = GetFromFile(key, logicalSliceChain, finalResult, mergingValues, readMetas);
        if (ret != BSS_OK && mergingValues.empty()) {
            return ret;
        }
    }

    MergeList(result, finalResult, mergingValues);
    return BSS_OK;
}

BResult SliceTable::MergeList(std::deque<Value> &result, Value finalResult, std::stack<Value> &mergingValues) const
{
    if (!finalResult.IsNull()) {
        result.push_back(finalResult);
    }
    while (!mergingValues.empty()) {
        Value later = mergingValues.top();
        mergingValues.pop();

        if (result.empty() || result.back().SeqId() < later.SeqId()) {
            result.push_back(later);
        }
    }
    if (!result.empty()) {
        ValueType type = static_cast<ValueType>(result.front().ValueType());
        if (type == DELETE && result.size() > 1) {
            result.pop_front();
        }
    }
    return BSS_OK;
}

BResult SliceTable::GetFromFile(const Key &key, LogicalSliceChainRef &logicalSliceChain, Value &finalResult,
                                std::stack<Value> &mergingValues, std::vector<SectionsReadContextRef> &readMetas) const
{
    RETURN_INVALID_PARAM_AS_NULLPTR(logicalSliceChain);
    std::vector<FilePageRef> filePages;
    logicalSliceChain->GetFilePages(filePages);
    if (UNLIKELY(filePages.empty())) {
        return BSS_OK;
    }

    BResult ret = BSS_OK;
    bool hasSectionReadMeta = false;
    for (uint32_t i = 0; i < filePages.size() && filePages[i] != nullptr; i++) {
        std::deque<Value> values;
        auto sectionsReadMeta = std::make_shared<SectionsReadMeta>();
        ret = filePages[i]->Get(key, values, sectionsReadMeta, hasSectionReadMeta);
        if (ret != BSS_OK || values.empty()) {
            break;
        }
        if (values.begin()->ValueType() == DELETE) {
            break;
        }
        if (values.begin()->ValueType() == APPEND) {
            while (!values.empty()) {
                mergingValues.push(*values.begin());
                values.pop_front();
            }
        } else {
            finalResult = *values.begin();
            break;
        }
        if (sectionsReadMeta->mCurrent != nullptr) {
            hasSectionReadMeta = true;
            sectionsReadMeta->key = key;
            SectionsReadContextRef sectionsReadContext = std::make_shared<SectionsReadContext>();
            sectionsReadContext->sectionsReadMeta = sectionsReadMeta;
            sectionsReadContext->filePage = filePages[i];
            readMetas.emplace_back(sectionsReadContext);
        }
    }
    return ret;
}

void SliceTable::GetFromSliceChain(const Key &key, LogicalSliceChainRef &logicalSliceChain, int32_t curIndex,
                                   uint32_t readLength, std::stack<Value> &mergingValues, Value &finalResult,
                                   bool &isFound)
{
    RETURN_AS_NULLPTR(logicalSliceChain);
    while (curIndex >= 0) {
        SliceAddressRef sliceAddress = logicalSliceChain->GetSliceAddress(curIndex);
        if (UNLIKELY(sliceAddress == nullptr)) {
            break;
        }
        auto dataSlice = sliceAddress->GetDataSlice();
        if (UNLIKELY(dataSlice == nullptr)) {
            break;
        }
        if (sliceAddress->IsEvicted()) {
            break;
        }
        readLength++;
        if (!GetFromSlice(key, mergingValues, finalResult, sliceAddress, dataSlice, isFound)) {
            AddSliceReadMetric(readLength);
            break;
        }
        curIndex--;
    }
    AddSliceReadMetric(readLength);
}

bool SliceTable::GetFromSlice(const Key &key, std::stack<Value> &mergingValues, Value &finalResult,
                              SliceAddressRef &sliceAddress, DataSliceRef &dataSlice, bool &isFound) const
{
    RETURN_FALSE_AS_NULLPTR(dataSlice);
    Value value;
    BResult result = dataSlice->Get(key, value);
    if (result == BSS_OK) {
        if (LIKELY(sliceAddress != nullptr)) {
            sliceAddress->AddRequestCount(NO_1);
        }
        if (value.ValueType() == DELETE) {
            isFound = true;
            return false;
        }
        if (value.ValueType() == APPEND) {
            mergingValues.push(value);
        } else {
            finalResult = value;
            isFound = true;
            return false;
        }
    }
    // MERGE要继续遍历
    return true;
}

KeyValueIteratorRef SliceTable::EntryIterator(const KeyFilter &keyFilter, uint16_t stateId,
                                              BlobValueTransformFunc &blobValueTransformFunc)
{
    // get value from blob store
    auto self = shared_from_this();
    auto blobValueTransformFunc2 = [self](uint64_t blobId, uint32_t keyHashCode, uint64_t seqId,
                                          Value &originalValue) -> BResult {
        return self->GetValueFromBlobStore(blobId, keyHashCode, seqId, originalValue);
    };
    blobValueTransformFunc = std::move(blobValueTransformFunc2);
    return EntryIterator(keyFilter, stateId);
}

KeyValueIteratorRef SliceTable::PrefixIterator(const Key &prefixKey, BlobValueTransformFunc &blobValueTransformFunc)
{
    // get value from blob store
    auto self = shared_from_this();
    auto blobValueTransformFunc2 = [self](uint64_t blobId, uint32_t keyHashCode, uint64_t seqId,
                                          Value &originalValue) -> BResult {
        return self->GetValueFromBlobStore(blobId, keyHashCode, seqId, originalValue);
    };
    blobValueTransformFunc = std::move(blobValueTransformFunc2);
    return PrefixIterator(prefixKey);
}

KeyValueIteratorRef SliceTable::EntryIterator(const KeyFilter &keyFilter, uint16_t stateId)
{
    SortedKeyValueMergingIteratorRef iterator = std::make_shared<SortedKeyValueMergingIterator>();
    if (UNLIKELY(iterator == nullptr)) {
        LOG_ERROR("MakeRef iterator failed!");
        return nullptr;
    }
    KeyValueIteratorRef keyValueIterator = mBucketGroupManager->IteratorFileStoreData(stateId);
    RETURN_NULLPTR_AS_NULLPTR(keyValueIterator);
    iterator->Init(GetIterator(), keyValueIterator, mMemManager);
    return std::make_shared<SortedKeyValueMergingIteratorV2>(iterator, keyFilter);
}

KeyValueIteratorRef SliceTable::PrefixIterator(const Key &prefixKey)
{
    mAccessRecorder->Record();
    LogicalSliceChainRef logicalSliceChain = GetSliceBucketIndex()->GetLogicalSliceChain(prefixKey);
    if (UNLIKELY(logicalSliceChain == nullptr)) {
        LOG_ERROR("Logical slice chain is nullptr!");
        return nullptr;
    }

    if (logicalSliceChain->IsEmpty()) {
        LOG_DEBUG("Logical slice chain is empty!");
        return nullptr;
    }

    auto iterator = std::make_shared<SliceTablePrefixIterator>();
    if (UNLIKELY(iterator == nullptr)) {
        LOG_ERROR("Failed to create prefix iterator!");
        return nullptr;
    }
    BResult ret = iterator->Init(logicalSliceChain, prefixKey);
    if (UNLIKELY(ret != BSS_OK)) {
        LOG_ERROR("Failed to initialize prefix iterator!");
        return nullptr;
    }

    return iterator;
}

BResult SliceTable::Open()
{
    mBucketGroupManager->Open();
    return BSS_OK;
}

void SliceTable::Close()
{
    if (mBlobStore != nullptr) {
        mBlobStore->Close();
    }
    mBucketGroupManager->Close();
    if (mBoostNativeMetric != nullptr) {
        mBoostNativeMetric->SetSliceChainAvgSize(nullptr);
        mBoostNativeMetric->SetSliceAvgSize(nullptr);
    }
    mBoostNativeMetric = nullptr;
}

BResult SliceTablePrefixIterator::Init(const LogicalSliceChainRef &logicalSliceChain, const Key &prefixKey)
{
    if (UNLIKELY(logicalSliceChain == nullptr)) {
        LOG_ERROR("slice Table is nullptr!");
        return BSS_ERR;
    }
    mLogicalSliceChain = logicalSliceChain;
    int32_t curChainIndex = logicalSliceChain->GetSliceChainTailIndex();
    while (curChainIndex >= 0) {
        SliceAddressRef sliceAddress = logicalSliceChain->GetSliceAddress(curChainIndex);
        if (UNLIKELY(sliceAddress == nullptr)) {
            LOG_ERROR("sliceAddress is nullptr!");
            return BSS_ERR;
        }
        if (sliceAddress->IsEvicted()) {
            break;
        }
        DataSliceRef dataSlice = sliceAddress->GetDataSlice();
        if (UNLIKELY(dataSlice == nullptr)) {
            LOG_ERROR("dataSlice is nullptr!");
            return BSS_ERR;
        }
        mSliceAddressAndDataSlices.emplace_back(sliceAddress, dataSlice);
        --curChainIndex;
    }
    mPrefixKey = prefixKey;
    mCurIndex = 0;
    mLastDataSlice = nullptr;
    return BSS_OK;
}

bool SliceTablePrefixIterator::HasNext()
{
    if (mCurrent == nullptr && !mClosed) {
        Advance();
    }
    return !mClosed && mCurrent != nullptr;
}

KeyValueRef SliceTablePrefixIterator::Next()
{
    if (!HasNext()) {
        LOG_ERROR("NoSuchElementException");
        return {};
    }
    auto ret = mCurrent;
    mCurrent = nullptr;
    return ret;
}

void SliceTablePrefixIterator::Advance()
{
    mCurrent = nullptr;
    while (mCurIndex < mSliceAddressAndDataSlices.size() || mCurrentSliceTableKvIterator != nullptr) {
        // 在当前的slice中迭代
        while (mCurrentSliceTableKvIterator != nullptr && mLastDataSlice != nullptr &&
               mCurrentSliceTableKvIterator->HasNext()) {
            auto keyValue = mCurrentSliceTableKvIterator->Next();
            auto secKeyNode = const_cast<SecKeyNode *>(&(keyValue->key.SecKey()));
            if (mVisited.find(secKeyNode) == mVisited.end()) {
                mVisited.emplace(secKeyNode, keyValue);
                mCurrent = keyValue;
                return;
            }
        }

        // 先进入这个循环，给mCurrentSliceTableMap赋值，然后跳出这个循环，进入上面的循环在当前slice中进行迭代
        SetSliceIter();
    }
    std::vector<FilePageRef> filePages;
    mLogicalSliceChain->GetFilePages(filePages);
    if (filePages.empty()) {
        return;
    }

    for (; mCurrentFilePageMap != nullptr || mFileIndex < filePages.size(); mFileIndex++) {
        while (mCurrentFilePageMap != nullptr && mCurrentFilePageMap->HasNext()) {
            auto keyValue = mCurrentFilePageMap->Next();
            auto secKeyNode = const_cast<SecKeyNode *>(&(keyValue->key.SecKey()));
            if (mVisited.find(secKeyNode) == mVisited.end()) {
                mCurrent = keyValue;
                return;
            }
        }
        if (mCurrentFilePageMap != nullptr && !mCurrentFilePageMap->HasNext()) {
            mCurrentFilePageMap->Close();
        }
        SetFileIter(filePages);
    }

    if (mCurrent == nullptr && !mClosed) {
        mClosed = true;
    }
}

void SliceTablePrefixIterator::SetFileIter(const std::vector<FilePageRef> &filePages)
{
    mCurrentFilePageMap = nullptr;
    for (auto i = mFileIndex; mCurrentFilePageMap == nullptr && i < filePages.size(); i++) {
        if (filePages[i] == nullptr) {
            mFileIndex = filePages.size();
            break;
        }
        mCurrentFilePageMap = filePages[i]->PrefixIterator(mPrefixKey, false);
    }
}

void SliceTablePrefixIterator::SetSliceIter()
{
    size_t sliceSize = static_cast<uint32_t>(mSliceAddressAndDataSlices.size());
    for (mCurrentSliceTableKvIterator = nullptr; mCurrentSliceTableKvIterator == nullptr && mCurIndex < sliceSize;
         ++mCurIndex) {
        const SliceAddressRef &sliceAddress = mSliceAddressAndDataSlices[mCurIndex].first;
        const DataSliceRef &dataSlice = mSliceAddressAndDataSlices[mCurIndex].second;
        if (sliceAddress->IsEvicted()) {
            mLastDataSlice = nullptr;
            mCurIndex = sliceSize;
            break;
        }
        mCurrentSliceTableKvIterator = dataSlice->GetSlice()->SubIterator(mPrefixKey, false);
        mLastDataSlice = dataSlice;
    }
}

FileStoreSnapshotOperatorRef SliceTable::PrepareFileStoreSnapshot(uint64_t operatorId, uint64_t snapshotId)
{
    auto bucketGroupIterator = mBucketGroupManager->GetBucketGroups();
    std::vector<LsmStoreRef> lsmStores;
    while (bucketGroupIterator->HasNext()) {
        auto bucketGroup = bucketGroupIterator->Next();
        lsmStores.emplace_back(bucketGroup->GetLsmStore());
    }
    return std::make_shared<FileStoreSnapshotOperator>(operatorId, snapshotId, lsmStores, mFileCache);
}

BlobStoreSnapshotOperatorRef SliceTable::PrepareBlobStoreSnapshot(uint64_t operatorId, uint64_t snapshotId)
{
    BlobStoreSnapshotOperatorRef blobStoreSnapshotOperator = nullptr;
    if (mIsKVSeparate) {
        blobStoreSnapshotOperator = std::make_shared<BlobStoreSnapshotOperator>(operatorId, snapshotId, mBlobStore,
                                                                                mFileCache);
        RETURN_NULLPTR_AS_NOT_OK(mBlobStore->SyncSnapshot(snapshotId, blobStoreSnapshotOperator));
    }
    return blobStoreSnapshotOperator;
}

void SliceTable::AddSliceTableSnapshot(uint64_t snapshotId, const SliceTableSnapshotRef &sliceTableSnapshot)
{
    std::lock_guard<std::mutex> lock(mRunningSnapshotMutex);
    mRunningSnapshot.emplace(snapshotId, sliceTableSnapshot);
}

bool SliceTable::IsSnapshotIdExist(uint64_t snapshotId)
{
    std::lock_guard<std::mutex> lock(mRunningSnapshotMutex);
    return mRunningSnapshot.find(snapshotId) != mRunningSnapshot.end();
}

void SliceTable::EraseSnapshotId(uint64_t snapshotId)
{
    std::lock_guard<std::mutex> lock(mRunningSnapshotMutex);
    mRunningSnapshot.erase(snapshotId);
}

BResult SliceTable::RestoreBlobStore(const std::vector<SliceTableRestoreMetaRef> &metaList,
                                     std::unordered_map<std::string, uint32_t> &restorePathFileIdMap)
{
    bool rescale = metaList.size() != 1;
    std::vector<std::pair<FileInputViewRef, int64_t>> metaVec;
    for (const auto &item : metaList) {
        FileInputViewRef fileInputView = std::make_shared<FileInputView>();
        RETURN_NOT_OK(fileInputView->Init(FileSystemType::LOCAL, item->GetMetaInputView()->GetFilePath()));
        metaVec.emplace_back(fileInputView, item->GetBlobServiceMetaOffset());
    }
    if (mBlobStore != nullptr) {
        return mBlobStore->Restore(metaVec, restorePathFileIdMap, rescale);
    }
    return BSS_OK;
}

void SliceTable::RegisterTombstoneService()
{
    mBucketGroupManager->RegisterTombstoneService(mBlobStore, mKeyGroupUtil);
}
}  // namespace bss
}  // namespace ock
