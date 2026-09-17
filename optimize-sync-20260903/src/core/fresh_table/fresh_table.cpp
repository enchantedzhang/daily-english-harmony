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
#include "fresh_table.h"

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <new>
#include <stdexcept>
#include <utility>

#include "fresh_restore_memory.h"
#include "snapshot/snapshot_compression_utils.h"

namespace ock {
namespace bss {
BResult AllocateFreshRestoreCompressedSegment(uint32_t length, uint32_t maxLength, MemorySegmentRef &segment)
{
    if (UNLIKELY(length == 0 || length > maxLength)) {
        LOG_ERROR("Invalid Fresh restore compressed scratch length:" << length << ", maxLength:" << maxLength);
        return BSS_INVALID_PARAM;
    }
    auto *temporary = static_cast<uint8_t *>(malloc(length));
    if (UNLIKELY(temporary == nullptr)) {
        LOG_ERROR("Allocate Fresh restore compressed scratch failed, length:" << length);
        return BSS_ALLOC_FAIL;
    }
    segment = MemorySegmentRef(new (std::nothrow) MemorySegment(length, temporary, true));
    if (UNLIKELY(segment == nullptr)) {
        free(temporary);
        return BSS_ALLOC_FAIL;
    }
    return BSS_OK;
}

namespace {
BResult ValidateFreshSnapshotCompressionMeta(CompressAlgo compressAlgo, uint32_t rawLength, uint32_t storedLength)
{
    if (rawLength == 0) {
        return storedLength == 0 ? BSS_OK : BSS_INVALID_PARAM;
    }
    if (compressAlgo == CompressAlgo::NONE) {
        if (storedLength == rawLength) {
            return BSS_OK;
        }
        LOG_ERROR("Invalid fresh table snapshot none compression meta, rawLength:" << rawLength << ", storedLength:"
                                                                                   << storedLength);
        return BSS_INVALID_PARAM;
    }
    if (compressAlgo == CompressAlgo::LZ4) {
        if (storedLength > 0 && storedLength < rawLength) {
            return BSS_OK;
        }
        LOG_ERROR("Invalid fresh table snapshot lz4 compression meta, rawLength:" << rawLength
                                                                                  << ", storedLength:" << storedLength);
        return BSS_INVALID_PARAM;
    }
    LOG_ERROR("Unsupported fresh table snapshot compression algo:" << static_cast<uint32_t>(compressAlgo));
    return BSS_NOT_SUPPORTED;
}

MemoryType GetFreshRestoreMemoryType(const MemManagerRef &memManager, uint32_t length)
{
    if (length > memManager->GetMemoryTypeMaxSize(MemoryType::SNAPSHOT)) {
        return MemoryType::RESTORE;
    }
    return MemoryType::SNAPSHOT;
}

BResult AllocateFreshRestoreSegment(const MemManagerRef &memManager, uint32_t length, MemorySegmentRef &segment)
{
    uint64_t maxFreshRestoreSize = std::max(memManager->GetMemoryTypeMaxSize(MemoryType::SNAPSHOT),
                                            memManager->GetMemoryTypeMaxSize(MemoryType::RESTORE));
    if (UNLIKELY(length == 0 || length > maxFreshRestoreSize)) {
        LOG_ERROR("Invalid fresh table restore memory length:" << length
                                                               << ", maxFreshRestoreSize:" << maxFreshRestoreSize);
        return BSS_INVALID_PARAM;
    }
    uintptr_t addr = 0;
    MemoryType memoryType = GetFreshRestoreMemoryType(memManager, length);
    RETURN_NOT_OK_NO_LOG(memManager->GetMemory(memoryType, length, addr));
    segment = MakeRef<MemorySegment>(length, reinterpret_cast<uint8_t *>(addr), memManager);
    if (UNLIKELY(segment == nullptr)) {
        memManager->ReleaseMemory(addr);
        LOG_ERROR("Make ref buffer failed, memorySegment is null.");
        return BSS_ALLOC_FAIL;
    }
    return BSS_OK;
}

BResult ReadFreshSnapshotSegment(const FileInputViewRef &fileInputView, const MemorySegmentRef &rawSegment,
                                 uint32_t rawLength, CompressAlgo compressAlgo, uint32_t storedLength,
                                 const MemManagerRef &memManager)
{
    RETURN_INVALID_PARAM_AS_NULLPTR(fileInputView);
    RETURN_INVALID_PARAM_AS_NULLPTR(rawSegment);
    RETURN_INVALID_PARAM_AS_NULLPTR(memManager);
    RETURN_NOT_OK(ValidateFreshSnapshotCompressionMeta(compressAlgo, rawLength, storedLength));
    if (compressAlgo == CompressAlgo::NONE) {
        auto ret = fileInputView->ReadMemorySegment(0, rawSegment, 0, rawLength);
        if (UNLIKELY(ret != BSS_OK)) {
            LOG_ERROR("Read fresh table raw snapshot failed, ret:" << ret << ", rawLength:" << rawLength);
            return BSS_ERR;
        }
        rawSegment->UpdatePosition(rawLength);
        return BSS_OK;
    }

    MemorySegmentRef storedSegment = nullptr;
    RETURN_NOT_OK(AllocateFreshRestoreCompressedSegment(storedLength, rawLength, storedSegment));
    auto ret = fileInputView->ReadMemorySegment(0, storedSegment, 0, storedLength);
    if (UNLIKELY(ret != BSS_OK)) {
        LOG_ERROR("Read fresh table compressed snapshot failed, ret:" << ret << ", storedLength:" << storedLength);
        return BSS_ERR;
    }
    ret = SnapshotCompressionUtils::Decompress(compressAlgo, storedSegment->GetSegment(), storedLength,
                                               rawSegment->GetSegment(), rawLength);
    if (UNLIKELY(ret != BSS_OK)) {
        LOG_ERROR("Decompress fresh table snapshot failed, ret:" << ret << ", rawLength:" << rawLength
                                                                 << ", storedLength:" << storedLength
                                                                 << ", algo:" << static_cast<uint32_t>(compressAlgo));
        return ret;
    }
    rawSegment->UpdatePosition(rawLength);
    return BSS_OK;
}
}  // namespace

FreshTable::~FreshTable()
{
    if (!mSnapshotQueue.Empty()) {
        mSnapshotQueue.Clear();
    }
    LOG_INFO("Delete freshTable object success.");
}

void FreshTable::Open()
{
    if (mIsOpened.load()) {
        return;
    }
    mIsOpened.store(true);
}

BResult FreshTable::Put(const Key &key, const Value &value)
{
    auto writer = [this, key, value]() -> BResult { return mActive->GetBinaryData()->Put(key, value); };
    uint32_t size = BoostHashMap::GetPriKeyLength(key) + BoostHashMap::GetValueLength(value);
    IncrementFreshKeyValueSize(key, value);
    if (UNLIKELY(ShouldForceRequireMemory(size))) {
        return RunWithForceRequireMemory(writer, size, MapLayerType::SINGLE_LAYER);
    } else {
        RequireMemoryUntilSuccess(writer);
        return BSS_OK;
    }
}

bool FreshTable::GetList(const Key &key, std::deque<Value> &result)
{
    // 1. 查找active memory segment.
    bool shouldContinue = VisitBinarySegmentMap(mActive, result, key);
    if (shouldContinue) {
        // 2. 查找flushing queue.
        ReadLocker<ReadWriteLock> lock(&mRwLock);
        for (auto it = mSnapshotQueue.rbegin(); it != mSnapshotQueue.rend() && shouldContinue; ++it) {
            shouldContinue = VisitBinarySegmentMap(*it, result, key);
        }
    }
    return shouldContinue;
}

KeyValueIteratorRef FreshTable::EntryIterator(const PriKeyFilter &priKeyFilter, const SecKeyFilter &secKeyFilter)
{
    std::vector<KeyValueIteratorRef> iterators;
    auto iterator = mActive->GetBinaryData()->EntryIterator(priKeyFilter, secKeyFilter);
    if (iterator != nullptr) {
        iterators.emplace_back(iterator);
    }

    ReadLocker<ReadWriteLock> lock(&mRwLock);
    for (auto it = mSnapshotQueue.rbegin(); it != mSnapshotQueue.rend(); ++it) {
        iterator = (*it)->GetBinaryData()->EntryIterator(priKeyFilter, secKeyFilter);
        if (iterator != nullptr) {
            iterators.emplace_back(iterator);
        }
    }

    return std::make_shared<ChainedIterator<KeyValueRef>>(std::move(iterators));
}

KeyValueIteratorRef FreshTable::GetMapIterator(const Key &key)
{
    std::vector<KeyValueIteratorRef> iterators;
    auto iterator = mActive->GetBinaryData()->MapEntryIterator(key);
    if (iterator != nullptr) {
        iterators.emplace_back(iterator);
    }

    ReadLocker<ReadWriteLock> lock(&mRwLock);
    for (auto it = mSnapshotQueue.rbegin(); it != mSnapshotQueue.rend(); ++it) {
        iterator = (*it)->GetBinaryData()->MapEntryIterator(key);
        if (iterator != nullptr) {
            iterators.emplace_back(iterator);
        }
    }

    return std::make_shared<ChainedIterator<KeyValueRef>>(std::move(iterators));
}

bool FreshTable::VisitBinarySegmentMap(const BoostSegmentRef &segment, std::deque<Value> &result, const Key &key)
{
    RETURN_FALSE_AS_NULLPTR(segment);
    auto BinaryRefDeque = segment->GetBinaryData()->GetList(key);
    for (auto &it : BinaryRefDeque) {
        uint8_t typeCode = it.ValueType();
        if (typeCode == ValueType::APPEND) {
            result.push_front(it);
            continue;
        } else if (typeCode == ValueType::PUT) {
            result.push_front(it);
            return false;
        } else {
            return false;
        }
    }
    return true;
}

BResult FreshTable::Append(const Key &key, const Value &value)
{
    auto writer = [this, key, value]() -> BResult { return mActive->GetBinaryData()->Append(key, value); };
    uint32_t size = BoostHashMap::GetPriKeyLength(key) + BoostHashMap::GetValueLength(value);
    IncrementFreshKeyValueSize(key, value);
    if (ShouldForceRequireMemory(size)) {
        return RunWithForceRequireMemory(writer, size, MapLayerType::SINGLE_LAYER);
    } else {
        RequireMemoryUntilSuccess(writer);
    }
    return BSS_OK;
}

bool FreshTable::GetKMap(const Key &key, Value &value)
{
    bool found = mActive->GetBinaryData()->GetKMap(key, value);
    if (found) {
        return true;
    }

    ReadLocker<ReadWriteLock> lock(&mRwLock);
    for (auto it = mSnapshotQueue.rbegin(); it != mSnapshotQueue.rend(); ++it) {
        found = (*it)->GetBinaryData()->GetKMap(key, value);
        if (found) {
            return true;
        }
    }
    return false;
}

bool FreshTable::Get(const Key &key, Value &value)
{
    bool found = mActive->GetBinaryData()->Get(key, value);
    if (found) {
        return true;
    }

    ReadLocker<ReadWriteLock> lock(&mRwLock);
    for (auto it = mSnapshotQueue.rbegin(); it != mSnapshotQueue.rend(); ++it) {
        found = (*it)->GetBinaryData()->Get(key, value);
        if (found) {
            return true;
        }
    }
    return false;
}

BResult FreshTable::Add(const KeyValue &keyValue)
{
    IncrementFreshKeyMapSize(keyValue);
    auto writer = [this, &keyValue]() -> BResult {
        if (UNLIKELY(mActive == nullptr || mActive->GetBinaryData() == nullptr)) {
            return BSS_INNER_RETRY;
        }
        return mActive->GetBinaryData()->Put(keyValue);
    };
    uint32_t size = BoostHashMap::GetKeyLength(keyValue.key) + BoostHashMap::GetValueLength(keyValue.value);
    if (ShouldForceRequireMemory(size)) {  // 待写入kV大小超过memorySegment容量的一半, 则新建memorySegment用来写入.
        return RunWithForceRequireMemory(writer, size, MapLayerType::SINGLE_LAYER);
    } else {
        RequireMemoryUntilSuccess(writer);
    }
    return BSS_OK;
}

void FreshTable::RequireMemoryUntilSuccess(const std::function<BResult()> &function)
{
    while (true) {
        // 1. 将kv写入到active segment中.
        BResult result = function();
        if (UNLIKELY(result != BSS_OK)) {
            // 2. 写入失败则将当前active segment淘汰掉, 然后重试.
            result = TriggerSegmentFlush();
            if (UNLIKELY(result != BSS_OK)) {
                LOG_WARN("Trigger segment flush failed, need inner retry, ret:" << result);
            }
            continue;
        }
        return;
    }
}

BResult FreshTable::RunWithForceRequireMemory(const std::function<BResult()> &function, uint32_t size,
                                              MapLayerType layerType)
{
    // 1. 新申请memorySegment替换掉当前active segment.
    auto ret = TriggerSegmentFlushWithForceInit(size, layerType);
    if (UNLIKELY(ret != BSS_OK)) {
        LOG_ERROR("Trigger segment flush with force initialize failed. ret:" << ret);
        return ret;
    }

    // 2. 将kv写入到active segment中.
    ret = function();
    if (UNLIKELY(ret != BSS_OK)) {
        LOG_WARN("Put failed, ret:" << ret);
        return ret;
    }

    // 3. 触发memory segment淘汰.
    RETURN_NOT_OK(TriggerSegmentFlush());
    return BSS_OK;
}

BResult FreshTable::TriggerSegmentFlushWithForceInit(uint32_t size, MapLayerType layerType)
{
    // 将activeSegment加入到Flushing queue中, 并触发Transform流程.
    RETURN_ERROR_AS_NULLPTR(mActive);
    mIsAlreadyTriggerTransform = true;
    AddFlushingSegment();
    mTransformTrigger();

    return NewActiveSegment(size, layerType);
}

BResult FreshTable::TriggerSegmentFlush()
{
    // 1. 将active segment加入到待淘汰队列中.
    mIsAlreadyTriggerTransform = true;
    AddFlushingSegment();
    mTransformTrigger();
    // 2. 新建memory segment替换掉当前的active segment.
    return InitNewActiveBinarySegment();
}

void FreshTable::EndSegmentFlush()
{
    WriteLocker<ReadWriteLock> lock(&mRwLock);
    BoostSegmentRef boostSegment;
    PollFlushingSegment(boostSegment);  // poll出待淘汰队列的队首的segment, 即释放该segment内存.
    // 由于只有一个异步线程flush, flush队列中的任务是串行的, 所以此处监控项更新不需要考虑多线程
    if (UNLIKELY(boostSegment == nullptr)) {
        LOG_WARN("boostSegment is nullptr.");
        return;
    }
    IncrementFreshFlush(boostSegment->Size());
    LOG_INFO("finish to flush fresh segment:" << boostSegment->GetSegmentId());
}

BResult FreshTable::InitNewActiveBinarySegment()
{
    uintptr_t addr = 0;
    uint32_t capacity = mMemManager->GetConfig()->GetMemorySegmentSize();
    RETURN_NOT_OK_NO_LOG(mMemManager->GetMemory(MemoryType::FRESH_TABLE, capacity, addr));
    MemorySegmentRef memorySegment = MakeRef<MemorySegment>(capacity, reinterpret_cast<uint8_t *>(addr), mMemManager);
    if (UNLIKELY(memorySegment == nullptr)) {
        mMemManager->ReleaseMemory(addr);
        LOG_ERROR("Make ref buffer failed, memorySegment is null.");
        return BSS_ALLOC_FAIL;
    }
    return CreateAndAssignBinarySegment(memorySegment);
}

BResult FreshTable::Initialize(const ConfigRef &config, const MemManagerRef &memManager,
                               const KeyGroupUtilRef &keyGroupUtil)
{
    RETURN_INVALID_PARAM_AS_NULLPTR(config);
    RETURN_INVALID_PARAM_AS_NULLPTR(memManager);
    mConfig = config;
    mMemManager = memManager;
    mKeyGroupUtil = keyGroupUtil;
    return InitNewActiveBinarySegment();
}

BResult FreshTable::Restore(const std::vector<RestoredDbMetaRef> &restoredDbMetas)
{
    auto memorySegmentSize = mConfig->GetMemorySegmentSize();
    std::vector<RestoredDbMetaRef> usefulMetas;
    for (const auto &dbMeta : restoredDbMetas) {  // 过滤掉不属于该KeyGroup的dbMeta.
        CONTINUE_LOOP_AS_NULLPTR(dbMeta);
        if (dbMeta->GetStartKeyGroup() <= mConfig->GetEndGroup() &&
            dbMeta->GetEndKeyGroup() >= mConfig->GetStartGroup()) {
            usefulMetas.push_back(dbMeta);
        }
    }
    uint64_t totalDataSize = 0;
    bool fastBulkLoad = (usefulMetas.size() == 1);  // 该字段表示是否满足快速批量加载的条件.
    for (const auto &dbMeta : usefulMetas) {
        auto currentMetaKeyGroup = std::make_shared<GroupRange>(dbMeta->GetStartKeyGroup(), dbMeta->GetEndKeyGroup());
        auto validKeyGroups = std::make_shared<GroupRange>(mConfig->GetStartGroup(), mConfig->GetEndGroup())
                                  ->Intersection(currentMetaKeyGroup);
        if (fastBulkLoad) {
            fastBulkLoad = currentMetaKeyGroup->Equals(std::make_shared<GroupRange>(validKeyGroups));
        }

        auto metaFileInputView = dbMeta->GetSnapshotMetaInputView();
        for (const auto &opInfo : dbMeta->GetRestoredSnapshotOperatorInfos()) {
            CONTINUE_LOOP_AS_NULLPTR(opInfo);
            SnapshotOperatorType opType = opInfo->GetSnapshotOperatorInfo()->GetSnapshotOperatorType();
            if (opType == SnapshotOperatorType::FRESH_TABLE) {
                metaFileInputView->Seek(opInfo->GetSnapshotOperatorMetaOffset());
                std::string address;
                RETURN_NOT_OK_AS_READ_ERROR(metaFileInputView->ReadUTF(address));  // 读取文件地址.
                uint32_t length = NO_0;
                RETURN_NOT_OK_AS_READ_ERROR(metaFileInputView->Read(length));  // 读取MemorySegment的数据长度.
                CompressAlgo compressAlgo = NONE;
                RETURN_NOT_OK_AS_READ_ERROR(metaFileInputView->Read(compressAlgo));  // 读取压缩标记.
                uint32_t compressLength = NO_0;
                RETURN_NOT_OK_AS_READ_ERROR(metaFileInputView->Read(compressLength));  // 读取压缩后长度.
                RETURN_NOT_OK(ValidateFreshSnapshotCompressionMeta(compressAlgo, length, compressLength));
                if (UNLIKELY(length == 0)) {
                    continue;
                }

                RETURN_NOT_OK(RestoreOpen());  // 申请activeSegment.
                if (fastBulkLoad) {
                    fastBulkLoad = (memorySegmentSize >= length);
                }
                FileInputViewRef fileInputView = std::make_shared<FileInputView>();
                RETURN_NOT_OK(fileInputView->Init(FileSystemType::LOCAL, std::make_shared<Path>(Uri(address))));
                LOG_DEBUG("Restore fresh table snapshot, algo:"
                          << static_cast<uint32_t>(compressAlgo) << ", rawLength:" << length
                          << ", storedLength:" << compressLength << ", fastBulkLoad:" << fastBulkLoad
                          << ", file:" << PathTransform::ExtractFileName(address));
                if (fastBulkLoad) {  // 执行快速批量加载.
                    MemorySegmentRef memorySegment = mActive->GetMemorySegment();
                    auto ret = ReadFreshSnapshotSegment(fileInputView, memorySegment, length, compressAlgo,
                                                        compressLength, mMemManager);
                    if (UNLIKELY(ret != BSS_OK)) {
                        LOG_ERROR("Restore memory segment failed from snapshot file, ret:"
                                  << ret << ", file path:" << fileInputView->GetFilePath()->ExtractFileName());
                        return BSS_ERR;
                    }
                    totalDataSize += length;
                    uint32_t indexNodeSize = mActive->GetBinaryData()->Size();
                    uint32_t bucketCount = mActive->GetBinaryData()->BucketCount();
                    LOG_INFO("Restore fresh table fast bulk load, length:"
                             << length << ", indexNodeSize:" << indexNodeSize << ", bucketCount:" << bucketCount
                             << ", file address:" << PathTransform::ExtractFileName(address));
                    break;
                } else {
                    MemorySegmentRef memorySegment = nullptr;
                    RETURN_NOT_OK(AllocateFreshRestoreSegment(mMemManager, length, memorySegment));
                    BoostSegmentRef boostSegment = std::make_shared<BoostSegment>();
                    RETURN_NOT_OK(boostSegment->Init(0, memorySegment, 0));

                    auto ret = ReadFreshSnapshotSegment(fileInputView, boostSegment->GetMemorySegment(), length,
                                                        compressAlgo, compressLength, mMemManager);
                    if (UNLIKELY(ret != BSS_OK)) {
                        LOG_ERROR("Restore memory segment failed from snapshot file, ret:"
                                  << ret << ", file path:" << fileInputView->GetFilePath()->ExtractFileName());
                        return BSS_ERR;
                    }
                    totalDataSize += length;
                    RETURN_NOT_OK(FillDataByMemorySegment(boostSegment, validKeyGroups));
                    LOG_INFO("Restore fresh table slow load, length:"
                             << length << ", keyGroupsStart:" << validKeyGroups.GetStartGroup()
                             << ", keyGroupsEnd:" << validKeyGroups.GetEndGroup()
                             << ", file address:" << PathTransform::ExtractFileName(address));
                }
            }
        }
    }
    LOG_DEBUG("Restore fresh table success, totalDataSize:" << totalDataSize);
    return BSS_OK;
}

BResult FreshTable::RestoreOpen()
{
    if (mIsOpened.load()) {
        return BSS_OK;
    }
    mIsOpened.store(true);
    auto ret = InitNewActiveBinarySegment();
    if (UNLIKELY(ret != BSS_OK)) {
        LOG_ERROR("Failed to initialize fresh table active segment, ret:" << ret);
    }
    return ret;
}

BResult FreshTable::NewActiveSegment(uint32_t size, FreshTable::MapLayerType layerType)
{
    uint32_t capacity;
    if (layerType == MapLayerType::DUAL_LAYER) {
        if (mLinkedListCapacity > 0) {
            capacity = NO_74 + NO_34 + NO_4 + size;
        } else {
            capacity = NO_74 * NO_2 + NO_4 + size;
        }
    } else {
        capacity = NO_74 + NO_4 + size;
    }
    void *addr = malloc(capacity);
    if (addr == nullptr) {
        LOG_ERROR("Malloc addr failed, size:" << capacity << ".");
        IncrementFreshSegmentCreateFail();
        return BSS_ALLOC_FAIL;
    }
    MemorySegmentRef memorySegment = MakeRef<MemorySegment>(capacity, reinterpret_cast<uint8_t *>(addr), true);
    if (UNLIKELY(memorySegment == nullptr)) {
        free(reinterpret_cast<void *>(addr));
        LOG_ERROR("Make ref buffer failed, memorySegment is null.");
        IncrementFreshSegmentCreateFail();
        return BSS_ALLOC_FAIL;
    }

    WriteLocker<ReadWriteLock> lock(&mRwLockActive);
    mActive = std::make_shared<BoostSegment>();  // 替换freshTable的active memorySegment.
    return mActive->Init(mSegmentId++, memorySegment, 0);
}

BResult FreshTable::FillDataByMemorySegment(const BoostSegmentRef &boostSegment, GroupRange &validKeyGroups)
{
    // 1. 构建binary segment的迭代器iterator.
    auto iteratorPrimary = boostSegment->GetBinaryData()->KVIterator();
    RETURN_ERROR_AS_NULLPTR(iteratorPrimary);

    // 2. 遍历迭代器每个元数, 根据keyType类型调用不同的write接口将数据写入freshTable中.
    while (iteratorPrimary->HasNext()) {
        auto kvPair = iteratorPrimary->Next();
        FreshKeyNodePtr key = kvPair.first;
        RETURN_ALLOC_FAIL_AS_NULLPTR(key);
        FreshValueNodePtr value = kvPair.second;
        RETURN_ALLOC_FAIL_AS_NULLPTR(value);
        PriKey primaryKey;
        primaryKey.Parse(key);  // 构建primaryKey获取stateId和stateType.
        uint16_t stateId = primaryKey.mStateId;
        StateType stateType = StateId::GetStateType(stateId);

        BinaryKey binaryKey;
        binaryKey.Parse(primaryKey, stateType == VALUE);  // 构建binaryKey获取keyGroup.
        auto curGroup = mKeyGroupUtil->ComputeKeyGroupForKeyHash(binaryKey.mKeyHashCode);
        if (!validKeyGroups.ContainsGroup(static_cast<int32_t>(curGroup))) {  // 过滤掉不属于该KeyGroup的kv数据.
            continue;
        }

        // 3. 根据不同的类型调用不同的write接口.
        if (StateTypeUtil::HasSecKey(stateType)) {  // 3.1 KMap类型.
            BoostHashMapRef hashMap = MakeRef<BoostHashMap>();
            uint32_t offset = value->MapData() - boostSegment->GetMemorySegment()->GetSegment();
            RETURN_NOT_OK(hashMap->Init(boostSegment->GetMemorySegment(), offset, false));
            auto iteratorSecondary = hashMap->KVIterator();
            RETURN_ERROR_AS_NULLPTR(iteratorSecondary);
            while (iteratorSecondary->HasNext()) {
                auto entry = iteratorSecondary->Next();
                FreshKeyNodePtr entryKey = entry.first;
                FreshValueNodePtr entryVal = entry.second;
                BinaryData priKey(binaryKey.mPrimaryKey.mKeyData, binaryKey.mPrimaryKey.mKeyDataLength);
                BinaryData secKey(entryKey->SecKeyData(), entryKey->SecKeyDataLen());
                KeyValue keyValue;
                QueryKey queryKey(stateId, binaryKey.mKeyHashCode, priKey, secKey);
                keyValue.key = queryKey;
                Value putVal;
                putVal.Init(entryVal->ValueType(), entryVal->ValueDataLen(), entryVal->Value(), entryVal->ValueSeqId());
                keyValue.value = putVal;
                RETURN_NOT_OK(Add(keyValue));
            }
        } else {
            ValueType valType = value->ValueType();
            // 3.2 KList类型.
            if (valType == ValueType::APPEND) {
                value->VisitAsList(*(boostSegment->GetMemorySegment()), [&](FreshValueNode &curVal) -> BResult {
                    BinaryData priKey(binaryKey.mPrimaryKey.mKeyData, binaryKey.mPrimaryKey.mKeyDataLength);
                    QueryKey putKey(stateId, binaryKey.mKeyHashCode, priKey);
                    Value addVal;
                    addVal.Init(curVal.ValueType(), curVal.ValueDataLen(), curVal.Value(), curVal.ValueSeqId());
                    auto ret = Append(putKey, addVal);
                    if (UNLIKELY(ret != BSS_OK)) {
                        LOG_ERROR("Append to fresh table failed, ret:" << ret);
                    }
                    return true;
                });
            } else {
                // 3.3 KValue类型.
                BinaryData priKey(binaryKey.mPrimaryKey.mKeyData, binaryKey.mPrimaryKey.mKeyDataLength);
                QueryKey putKey(stateId, binaryKey.mKeyHashCode, priKey);
                Value putValue;
                putValue.Init(value->ValueType(), value->ValueDataLen(), value->Value(), value->ValueSeqId());
                RETURN_NOT_OK(Put(putKey, putValue));
            }
        }
    }
    return BSS_OK;
}

}  // namespace bss
}  // namespace ock
