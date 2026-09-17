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

#include "slice_table_restore_operation.h"

#include <iostream>
#include <limits>

#include "slice_table/bucket_group_range.h"
#include "slice_table/bucket_group_rescale_util.h"
#include "snapshot_compression_utils.h"

namespace ock {
namespace bss {
namespace {
BResult ValidateSliceSnapshotCompressionMeta(const SliceAddressRef &sliceAddress)
{
    RETURN_INVALID_PARAM_AS_NULLPTR(sliceAddress);
    uint32_t rawLength = sliceAddress->GetDataLen();
    uint32_t storedLength = sliceAddress->GetStoredDataLen();
    CompressAlgo compressAlgo = sliceAddress->GetCompressAlgo();
    if (UNLIKELY(rawLength == 0)) {
        LOG_ERROR("Invalid slice snapshot raw length, sliceId:" << sliceAddress->GetSliceId()
                                                                << ", rawLength:" << rawLength);
        return BSS_INVALID_PARAM;
    }
    if (compressAlgo == CompressAlgo::NONE) {
        if (storedLength == rawLength) {
            return BSS_OK;
        }
        LOG_ERROR("Invalid slice snapshot none compression meta, sliceId:"
                  << sliceAddress->GetSliceId() << ", rawLength:" << rawLength << ", storedLength:" << storedLength);
        return BSS_INVALID_PARAM;
    }
    if (compressAlgo == CompressAlgo::LZ4) {
        if (storedLength > 0 && storedLength < rawLength) {
            return BSS_OK;
        }
        LOG_ERROR("Invalid slice snapshot lz4 compression meta, sliceId:"
                  << sliceAddress->GetSliceId() << ", rawLength:" << rawLength << ", storedLength:" << storedLength);
        return BSS_INVALID_PARAM;
    }
    LOG_ERROR("Unsupported slice snapshot compression algo, sliceId:" << sliceAddress->GetSliceId() << ", algo:"
                                                                      << static_cast<uint32_t>(compressAlgo));
    return BSS_NOT_SUPPORTED;
}

BResult AllocateSliceRestoreSegment(const MemManagerRef &memManager, uint32_t length, MemorySegmentRef &segment)
{
    RETURN_INVALID_PARAM_AS_NULLPTR(memManager);
    uint64_t maxSliceRestoreSize = memManager->GetMemoryTypeMaxSize(MemoryType::SLICE_TABLE);
    if (UNLIKELY(length == 0 || length > maxSliceRestoreSize)) {
        LOG_ERROR("Invalid slice restore memory length:" << length << ", maxSliceRestoreSize:" << maxSliceRestoreSize);
        return BSS_INVALID_PARAM;
    }
    uintptr_t addr = 0;
    RETURN_NOT_OK_NO_LOG(memManager->GetMemory(MemoryType::SLICE_TABLE, length, addr));
    segment = MakeRef<MemorySegment>(length, reinterpret_cast<uint8_t *>(addr), memManager);
    if (UNLIKELY(segment == nullptr)) {
        memManager->ReleaseMemory(addr);
        LOG_ERROR("Make slice restore segment failed, memorySegment is null.");
        return BSS_ALLOC_FAIL;
    }
    return BSS_OK;
}

BResult ReadSliceSnapshotPayload(const FileInputViewRef &inputView, const SliceAddressRef &sliceAddress,
                                 const ByteBufferRef &rawBuffer, const MemManagerRef &memManager)
{
    RETURN_INVALID_PARAM_AS_NULLPTR(inputView);
    RETURN_INVALID_PARAM_AS_NULLPTR(rawBuffer);
    RETURN_INVALID_PARAM_AS_NULLPTR(memManager);
    RETURN_NOT_OK(ValidateSliceSnapshotCompressionMeta(sliceAddress));

    uint32_t rawLength = sliceAddress->GetDataLen();
    uint32_t storedLength = sliceAddress->GetStoredDataLen();
    CompressAlgo compressAlgo = sliceAddress->GetCompressAlgo();
    if (UNLIKELY(!rawBuffer->Valid() || rawLength > rawBuffer->Capacity())) {
        LOG_ERROR("Invalid slice restore raw buffer, sliceId:" << sliceAddress->GetSliceId() << ", rawLength:"
                                                               << rawLength << ", capacity:" << rawBuffer->Capacity());
        return BSS_INVALID_PARAM;
    }
    uint64_t startOffset = sliceAddress->GetStartOffset();
    if (UNLIKELY(startOffset > UINT32_MAX)) {
        LOG_ERROR("Invalid slice snapshot start offset, sliceId:" << sliceAddress->GetSliceId()
                                                                  << ", startOffset:" << startOffset);
        return BSS_INVALID_PARAM;
    }
    uint32_t readOffset = static_cast<uint32_t>(startOffset);
    if (compressAlgo == CompressAlgo::NONE) {
        auto ret = inputView->ReadByteBuffer(0, rawBuffer, readOffset, rawLength);
        if (UNLIKELY(ret != BSS_OK)) {
            LOG_ERROR("Read raw slice snapshot failed, ret:" << ret << ", sliceId:" << sliceAddress->GetSliceId()
                                                             << ", rawLength:" << rawLength);
            return BSS_ERR;
        }
        return BSS_OK;
    }

    MemorySegmentRef storedSegment = nullptr;
    RETURN_NOT_OK(AllocateSliceRestoreSegment(memManager, storedLength, storedSegment));
    auto ret = inputView->ReadMemorySegment(0, storedSegment, readOffset, storedLength);
    if (UNLIKELY(ret != BSS_OK)) {
        LOG_ERROR("Read compressed slice snapshot failed, ret:" << ret << ", sliceId:" << sliceAddress->GetSliceId()
                                                                << ", storedLength:" << storedLength);
        return BSS_ERR;
    }
    ret = SnapshotCompressionUtils::Decompress(compressAlgo, storedSegment->GetSegment(), storedLength,
                                               rawBuffer->Data(), rawLength);
    if (UNLIKELY(ret != BSS_OK)) {
        LOG_ERROR("Decompress slice snapshot failed, ret:"
                  << ret << ", sliceId:" << sliceAddress->GetSliceId() << ", rawLength:" << rawLength
                  << ", storedLength:" << storedLength << ", algo:" << static_cast<uint32_t>(compressAlgo));
        return ret;
    }
    double ratio = rawLength == 0 ? 0.0 : static_cast<double>(storedLength) * 100.0 / rawLength;
    LOG_DEBUG("SliceTable restore compression, sliceId:"
              << sliceAddress->GetSliceId() << ", algo:" << static_cast<uint32_t>(compressAlgo)
              << ", rawLength:" << rawLength << ", storedLength:" << storedLength << ", startOffset:" << readOffset
              << ", file:" << inputView->GetFilePath()->ExtractFileName() << ", ratio:" << ratio << "%");
    return BSS_OK;
}
}  // namespace

size_t SliceTableRestoreOperation::SliceRestoreCacheKeyHash::operator()(const SliceRestoreCacheKey &key) const
{
    size_t seed = std::hash<std::string>{}(key.localAddress);
    auto combine = [&seed](size_t value) { seed ^= value + 0x9e3779b9U + (seed << 6U) + (seed >> 2U); };
    combine(std::hash<uint64_t>{}(key.startOffset));
    combine(std::hash<uint32_t>{}(key.rawLength));
    combine(std::hash<uint32_t>{}(key.storedLength));
    combine(std::hash<uint8_t>{}(static_cast<uint8_t>(key.compressAlgo)));
    combine(std::hash<uint32_t>{}(key.checkSum));
    return seed;
}

SliceTableRestoreOperation::SliceRestoreCacheKey SliceTableRestoreOperation::BuildSliceRestoreCacheKey(
    const SliceAddressRef &sliceAddress)
{
    return { sliceAddress->GetLocalAddress(),  sliceAddress->GetStartOffset(),  sliceAddress->GetDataLen(),
             sliceAddress->GetStoredDataLen(), sliceAddress->GetCompressAlgo(), sliceAddress->GetCheckSum() };
}

SliceRef SliceTableRestoreOperation::DataSliceRestoreCache::Find(const SliceRestoreCacheKey &key)
{
    auto current = currentSlot.find(key);
    if (current != currentSlot.end()) {
        return current->second;
    }
    auto previous = previousSlot.find(key);
    if (previous == previousSlot.end()) {
        return nullptr;
    }
    currentSlot.emplace(key, previous->second);
    return previous->second;
}

void SliceTableRestoreOperation::DataSliceRestoreCache::Put(SliceRestoreCacheKey key, const SliceRef &slice)
{
    currentSlot.emplace(std::move(key), slice);
}

void SliceTableRestoreOperation::DataSliceRestoreCache::ClearPreviousSlot()
{
    previousSlot.clear();
}

void SliceTableRestoreOperation::DataSliceRestoreCache::CompleteSlot()
{
    previousSlot = std::move(currentSlot);
    currentSlot.clear();
}

void SliceTableRestoreOperation::DataSliceRestoreCache::Clear()
{
    previousSlot.clear();
    currentSlot.clear();
}

BResult SliceTableRestoreOperation::RestoreSliceBucketIndex(std::vector<RestoredDbMetaRef> &restoredDbMetas,
                                                            std::vector<SliceTableRestoreMetaRef> &restoreMetaList)
{
    for (const auto &dbMeta : restoredDbMetas) {
        if (dbMeta->GetStartKeyGroup() > mConfig->GetEndGroup() ||
            dbMeta->GetEndKeyGroup() < mConfig->GetStartGroup()) {  // 过滤掉不属于该KeyGroup的dbMeta.
            continue;
        }

        auto metaFileInputView = dbMeta->GetSnapshotMetaInputView();
        auto restoreMeta = std::make_shared<SliceTableRestoreMeta>(
            metaFileInputView, std::make_shared<GroupRange>(dbMeta->GetStartKeyGroup(), dbMeta->GetEndKeyGroup()),
            dbMeta->GetSnapshotVersion());
        // 查找sliceTable和fileStore的元数据偏移.
        for (const auto &opInfo : dbMeta->GetRestoredSnapshotOperatorInfos()) {
            SnapshotOperatorType opType = opInfo->GetSnapshotOperatorInfo()->GetSnapshotOperatorType();
            if (opType == SnapshotOperatorType::FRESH_TABLE) {
                continue;
            }
            if (opType == SnapshotOperatorType::SLICE_TABLE) {
                restoreMeta->SetSliceTableMetaOffset(static_cast<int64_t>(opInfo->GetSnapshotOperatorMetaOffset()));
                continue;
            }
            if (opType == SnapshotOperatorType::BLOB_STORE) {
                restoreMeta->SetBlobServiceMetaOffset(static_cast<int64_t>(opInfo->GetSnapshotOperatorMetaOffset()));
                continue;
            }
            if (opType == SnapshotOperatorType::FILE_STORE) {
                auto fileStoreSnapshotInfo =
                    std::dynamic_pointer_cast<FileStoreSnapshotOperatorInfo>(opInfo->GetSnapshotOperatorInfo());
                RETURN_NOT_OK_AS_FALSE(fileStoreSnapshotInfo == nullptr, BSS_ERR);
                std::vector<FileStoreIDRef> fileStoreIds = fileStoreSnapshotInfo->GetFileStoreIds();
                std::vector<uint64_t> fileMetaOffsets = fileStoreSnapshotInfo->GetFileMetaOffsets();
                if (UNLIKELY(fileStoreIds.size() != fileMetaOffsets.size())) {
                    LOG_ERROR("Restore file store meta failed, fileStoreIds size:"
                              << fileStoreIds.size() << ", fileMeta size:" << fileMetaOffsets.size());
                    return BSS_ERR;
                }
                for (uint32_t idx = 0; idx < fileStoreIds.size(); idx++) {
                    restoreMeta->AddFileStoreMeta(fileStoreIds[idx], fileMetaOffsets[idx]);
                }
            }
        }

        // 恢复slice bucket index.
        uint64_t snapshotId = restoredDbMetas[0]->GetSnapshotId();
        metaFileInputView->Seek(restoreMeta->GetSliceTableMetaOffset());
        SliceBucketGroupRangeGroupRef oldSliceSegmentGroup = nullptr;
        SliceBucketMapperRef oldMapper = nullptr;
        RETURN_NOT_OK(RestoreSliceBucketIndex(metaFileInputView, dbMeta->GetSnapshotVersion(), oldSliceSegmentGroup,
                                              snapshotId, dbMeta->GetStartKeyGroup(), dbMeta->GetEndKeyGroup(),
                                              oldMapper));

        // 缩放BucketGroup, 针对并行度并更场景.
        auto newSliceBucketGroupRangeGroup =
            std::make_shared<SliceBucketGroupRangeGroup>(mSliceBucketIndex->GetIndexCapacity(),
                                                         mBucketGroupManager->GetBucketGroupRanges(),
                                                         mSliceBucketIndex);
        RETURN_NOT_OK(BucketGroupRescaleUtil::Rescale(oldSliceSegmentGroup, newSliceBucketGroupRangeGroup,
                                                      mBucketGroupManager, oldMapper,
                                                      mSliceBucketIndex->GetBucketMapper(),
                                                      restoreMeta->GetRescaleMappingInfo()));
        restoreMetaList.push_back(restoreMeta);
        LOG_DEBUG("Restore slice bucket index success, bucket group:" << newSliceBucketGroupRangeGroup->ToString());
    }
    return BSS_OK;
}

BResult SliceTableRestoreOperation::RestoreSliceBucketIndex(const FileInputViewRef &reader, uint32_t snapshotVersion,
                                                            SliceBucketGroupRangeGroupRef &oldSliceSegmentGroup,
                                                            uint64_t snapshotId, uint32_t oldStartKeyGroup,
                                                            uint32_t oldEndKeyGroup, SliceBucketMapperRef &oldMapper)
{
    if (snapshotVersion >= NO_3) {
        uint8_t kvSeparateMode = 0;
        RETURN_NOT_OK_AS_READ_ERROR(reader->Read(kvSeparateMode));  // 读取kv分离标记
    }

    uint32_t oldBucketNum = 0;
    RETURN_NOT_OK_AS_READ_ERROR(reader->Read(oldBucketNum));  // 读取bucketNum
    if (UNLIKELY(oldBucketNum == 0)) {
        LOG_ERROR("Old bucket num resolve failed, oldBucketNum:" << oldBucketNum);
        return BSS_ERR;
    }
    if (UNLIKELY(oldBucketNum != mCurTotalBucketNum)) {
        LOG_ERROR("Restore with different slice bucket count is unsupported, old:" << oldBucketNum << ", current:"
                                                                                   << mCurTotalBucketNum);
        return BSS_NOT_SUPPORTED;
    }
    oldMapper = std::make_shared<SliceBucketMapper>();
    RETURN_NOT_OK(oldMapper->Initialize(mConfig->GetMaxParallelism(), oldStartKeyGroup, oldEndKeyGroup, oldBucketNum));

    // 1. 恢复sliceChain和sliceAddress.
    uint64_t sliceTotalSize = 0;
    std::vector<LogicalSliceChainRef> logicalSliceChainTable(oldBucketNum);
    for (uint32_t i = 0; i < oldBucketNum; i++) {
        uint8_t isEmptyChain = 0;
        RETURN_NOT_OK_AS_READ_ERROR(reader->Read(isEmptyChain));  // 读取emptyChain标记
        if (isEmptyChain == NO_1) {
            logicalSliceChainTable[i] = LogicalSliceChain::mEmptySliceChain;
        } else {
            LogicalSliceChainRef logicalSliceChain = std::make_shared<LogicalSliceChainImpl>(SliceStatus::NORMAL,
                                                                                             false);
            RETURN_NOT_OK(logicalSliceChain->Restore(reader, snapshotId));
            if (logicalSliceChain->HasFilePage()) {
                auto bucketGroup = GetBucketGroup();
                RETURN_ERROR_AS_NULLPTR(bucketGroup);
                auto filePage = std::make_shared<FilePage>(bucketGroup->GetLsmStore());
                std::vector<FilePageRef> filePages;
                filePages.push_back(filePage);
                logicalSliceChain->SetFilePages(filePages);
            }
            logicalSliceChainTable[i] = logicalSliceChain;
            sliceTotalSize += logicalSliceChain->GetSliceSize();
            LOG_DEBUG("Restore non-empty slice chain, idx:"
                      << i << ", logicSliceChainStatus:" << static_cast<uint32_t>(logicalSliceChain->GetSliceStatus())
                      << ", endSliceIndex:" << logicalSliceChain->GetSliceChainTailIndex() << ", baseSliceIndex:"
                      << logicalSliceChain->GetBaseSliceIndex() << ", sliceSize:" << logicalSliceChain->GetSliceSize()
                      << ", filePageSize:" << logicalSliceChain->GetFilePageSize());
        }
    }

    // 2. 恢复bucketGroupRange.
    std::vector<BucketGroupRangeRef> bucketGroupRanges;
    RETURN_NOT_OK(BucketGroupManager::RestoreMeta(reader, oldBucketNum, bucketGroupRanges));

    oldSliceSegmentGroup =
        std::make_shared<SliceBucketGroupRangeGroup>(oldBucketNum, bucketGroupRanges,
                                                     std::make_shared<SliceBucketIndex>(logicalSliceChainTable));
    LOG_DEBUG("Restore slice bucket group range success, oldBucketNum:"
              << oldBucketNum << ", newBucketNum:" << mCurTotalBucketNum << ", totalSize:" << sliceTotalSize
              << ", bucketGroupRangeSize:" << bucketGroupRanges.size());
    return BSS_OK;
}

BResult SliceTableRestoreOperation::RestoreBlobStore(std::vector<SliceTableRestoreMetaRef> &metaList,
                                                     std::unordered_map<std::string, uint32_t> &restorePathFileIdMap)
{
    return mSliceTable->RestoreBlobStore(metaList, restorePathFileIdMap);
}

void SliceTableRestoreOperation::TryEvictCompositeLogicalSliceChain(uint64_t chainMemory)
{
    auto evictMemoryWatermark = static_cast<uint64_t>(mSliceTable->GetMemHighMark());
    uint64_t usedMemory = mMemManager->GetMemoryUseSize(MemoryType::SLICE_TABLE);
    if (usedMemory + chainMemory >= evictMemoryWatermark) {
        LOG_INFO("Evict composite logical slice start, chainMemory:" << chainMemory << ", usedMemory:" << usedMemory
                                                                     << ", watermark:" << evictMemoryWatermark);
        auto ret = mSliceTable->TryCurrentDBEvict(0, true, true);
        if (UNLIKELY(ret != BSS_OK)) {
            LOG_ERROR("Force sync evict composite logical slice failed, ret:"
                      << ret << ", chainMemory:" << chainMemory << ", usedMemory:" << usedMemory
                      << ", watermark:" << evictMemoryWatermark);
        }
    }
}

BResult SliceTableRestoreOperation::LoadSlicesIntoSliceTable(uint32_t snapshotVersion, bool isFailOver)
{
    RETURN_INVALID_PARAM_AS_NULLPTR(mSliceBucketIndex);
    DataSliceRestoreCache cache;
    auto whole = mSliceBucketIndex->GetIndexCapacity();
    std::atomic<uint32_t> remaining{ whole };

    for (uint32_t i = 0; i < mSliceBucketIndex->GetIndexCapacity(); i++) {
        uint32_t slot = i;
        LogicalSliceChainRef sliceChain = mSliceBucketIndex->GetLogicChainedSlice(i);
        if (UNLIKELY(sliceChain == nullptr)) {
            cache.Clear();
            mSliceBucketIndex->SetLogicChainedSlice(slot, LogicalSliceChain::mEmptySliceChain);
            remaining.fetch_sub(1);
            continue;
        }
        if (sliceChain->IsNone()) {
            cache.Clear();
            remaining.fetch_sub(1);
        } else if (std::dynamic_pointer_cast<CompositeLogicalSliceChain>(sliceChain)->GetSliceSize() == 0) {
            cache.Clear();
            std::shared_ptr<LogicalSliceChainImpl> chain = std::make_shared<LogicalSliceChainImpl>();
            auto chains = std::dynamic_pointer_cast<CompositeLogicalSliceChain>(sliceChain)->GetSliceChains();
            for (auto &item : chains) {
                std::vector<FilePageRef> filePages;
                item->GetFilePages(filePages);
                chain->InsertAllFilePage(filePages);
            }
            mSliceBucketIndex->SetLogicChainedSlice(slot, chain);
            remaining.fetch_sub(1);
        } else {
            // 1. 根据compact compositeLogicalSlice所需内存判断是否淘汰SliceTable的数据进行淘汰.
            auto composite = std::dynamic_pointer_cast<CompositeLogicalSliceChain>(sliceChain);
            RETURN_ERROR_AS_NULLPTR(composite);
            bool useRollingCache = composite->GetCompositeNum() > NO_1 || composite->RequireForceCompaction();
            if (!useRollingCache) {
                cache.Clear();
            }
            TryEvictCompositeLogicalSliceChain(composite->GetCompositeSliceSize());
            // 2. 加载复合LogicSliceChain.
            RETURN_NOT_OK(TryLoadCompositeLogicalSliceChain(sliceChain, snapshotVersion, cache));
            if (useRollingCache) {
                cache.ClearPreviousSlot();
            } else {
                cache.Clear();
            }
            // 3. 同步合并CompositeLogicSliceChain.
            RETURN_NOT_OK(CompactCompositeLogicalSliceChain(slot));
            if (useRollingCache) {
                cache.CompleteSlot();
            }
        }
    }
    for (uint32_t slot = 0; slot < mSliceBucketIndex->GetIndexCapacity(); ++slot) {
        auto chain = mSliceBucketIndex->GetLogicChainedSlice(slot);
        if (UNLIKELY(chain == nullptr || std::dynamic_pointer_cast<CompositeLogicalSliceChain>(chain) != nullptr)) {
            LOG_ERROR("Restore left invalid target chain, slot:" << slot);
            return BSS_INNER_ERR;
        }
    }
    return BSS_OK;
}

BResult SliceTableRestoreOperation::TryLoadCompositeLogicalSliceChain(const LogicalSliceChainRef &sliceChain,
                                                                      uint32_t snapshotVersion)
{
    DataSliceRestoreCache cache;
    return TryLoadCompositeLogicalSliceChain(sliceChain, snapshotVersion, cache);
}

BResult SliceTableRestoreOperation::TryLoadCompositeLogicalSliceChain(const LogicalSliceChainRef &sliceChain,
                                                                      uint32_t snapshotVersion,
                                                                      DataSliceRestoreCache &cache)
{
    std::vector<LogicalSliceChainRef> sliceChainList = sliceChain->GetSliceChains();
    for (const auto &chain : sliceChainList) {
        RETURN_NOT_OK(LoadSlicesIntoLogicalSliceChain(chain, snapshotVersion, cache));
    }
    return BSS_OK;
}

BResult SliceTableRestoreOperation::LoadSlicesIntoLogicalSliceChain(const LogicalSliceChainRef &sliceChain,
                                                                    uint32_t snapshotVersion)
{
    DataSliceRestoreCache cache;
    return LoadSlicesIntoLogicalSliceChain(sliceChain, snapshotVersion, cache);
}

BResult SliceTableRestoreOperation::LoadSlicesIntoLogicalSliceChain(const LogicalSliceChainRef &sliceChain,
                                                                    uint32_t snapshotVersion,
                                                                    DataSliceRestoreCache &cache)
{
    uint32_t index = 0;
    IteratorRef<SliceAddressRef> sliceIterator = sliceChain->SliceIterator();
    RETURN_ALLOC_FAIL_AS_NULLPTR(sliceIterator);
    while (sliceIterator->HasNext()) {
        SliceAddressRef sliceAddress = sliceIterator->Next();
        if (sliceAddress == nullptr || sliceAddress->IsEvicted()) {  // 跳过已经被淘汰的sliceAddress.
            continue;
        }

        // 1. 读取dataSlice数据.
        RETURN_NOT_OK(ValidateSliceSnapshotCompressionMeta(sliceAddress));
        SliceRestoreCacheKey cacheKey = BuildSliceRestoreCacheKey(sliceAddress);
        SliceRef slice = cache.Find(cacheKey);
        if (slice == nullptr) {
            RETURN_NOT_OK(EnsureSliceRestoreMemory(sliceAddress, cache));
            auto buffer = MakeRef<ByteBuffer>(sliceAddress->GetDataLen(), MemoryType::SLICE_TABLE, mMemManager);
            if (UNLIKELY(buffer == nullptr || !buffer->Valid())) {
                LOG_ERROR("Alloc slice restore byte buffer failed, sliceId:"
                          << sliceAddress->GetSliceId() << ", dataLen:" << sliceAddress->GetDataLen());
                return BSS_ALLOC_FAIL;
            }
            auto restoreFilePath = std::make_shared<Path>(sliceAddress->GetLocalAddress());
            FileInputViewRef inputView = std::make_shared<FileInputView>();
            auto ret = inputView->Init(FileSystemType::LOCAL, restoreFilePath);
            if (UNLIKELY(ret != BSS_OK)) {
                LOG_ERROR("Load slice chain failed, because of init restore input view failed, ret:"
                          << ret << ", filePath:" << restoreFilePath->ExtractFileName());
                return ret;
            }
            ret = ReadSliceSnapshotPayload(inputView, sliceAddress, buffer, mMemManager);
            if (UNLIKELY(ret != BSS_OK)) {
                LOG_ERROR("Load slice chain failed, because of read snapshot payload failed, ret:"
                          << ret << ", filePath:" << restoreFilePath->ExtractFileName());
                return ret;
            }

            // 2. 构建slice.
            slice = std::make_shared<Slice>();
            slice->RestoreSliceUseByteBuffer(buffer, mMemManager);
            cache.Put(std::move(cacheKey), slice);
        }
        DataSliceRef dataSlice = std::make_shared<DataSlice>();
        dataSlice->Init(slice);
        dataSlice->SetChainIndex(index++);
        sliceAddress->SetDataSlice(dataSlice);
        // 更新slice链上占用内存
        mSliceTable->AddSliceUsedMemory(dataSlice->GetSize());
        if (mConfig->GetIsNewJob()) {
            // 清空localAddress，增量Checkpoint要用这个标识做判断.
            sliceAddress->SetLocalAddress("");
        }
    }
    return BSS_OK;
}

BResult SliceTableRestoreOperation::EnsureSliceRestoreMemory(const SliceAddressRef &sliceAddress,
                                                             DataSliceRestoreCache &cache)
{
    RETURN_INVALID_PARAM_AS_NULLPTR(sliceAddress);
    uint64_t rawLength = sliceAddress->GetDataLen();
    uint64_t storedLength = sliceAddress->GetStoredDataLen();
    uint64_t required = mMemManager->CalcSize(rawLength);
    if (sliceAddress->GetCompressAlgo() != CompressAlgo::NONE) {
        required += mMemManager->CalcSize(storedLength);
    }
    uint64_t capacity = mMemManager->GetMemoryTypeMaxSize(MemoryType::SLICE_TABLE);
    if (UNLIKELY(required > capacity)) {
        LOG_ERROR("Slice restore decode working set exceeds SliceTable capacity, rawLength:"
                  << rawLength << ", storedLength:" << storedLength << ", required:" << required
                  << ", capacity:" << capacity);
        return BSS_ALLOC_FAIL;
    }

    uint64_t usedMemory = mMemManager->GetMemoryUseSize(MemoryType::SLICE_TABLE);
    if (usedMemory <= capacity - required) {
        return BSS_OK;
    }
    LOG_DEBUG("Release previous restore cache for decode memory, rawLength:"
              << rawLength << ", storedLength:" << storedLength << ", usedMemory:" << usedMemory
              << ", capacity:" << capacity);
    cache.ClearPreviousSlot();
    usedMemory = mMemManager->GetMemoryUseSize(MemoryType::SLICE_TABLE);
    if (usedMemory > capacity - required) {
        auto ret = mSliceTable->TryCurrentDBEvict(0, true, true);
        if (UNLIKELY(ret != BSS_OK)) {
            LOG_ERROR("Force sync evict before slice restore decode failed, ret:"
                      << ret << ", rawLength:" << rawLength << ", storedLength:" << storedLength
                      << ", usedMemory:" << usedMemory << ", capacity:" << capacity);
        }
    }
    return BSS_OK;
}

BResult SliceTableRestoreOperation::DoCompactCompositeSlice(
    const std::vector<DataSliceRef> &canCompactSliceListReversed, DataSliceRef &compactedDataSlice, bool forceFilter,
    uint32_t bucketIndex)
{
    // 1. 执行Merge操作.
    std::vector<std::pair<SliceKey, Value>> resultVec;
    uint32_t compactionCount = 0L;
    auto retVal = SliceCompactionUtils::MergeDataSlicesForCompaction(
        resultVec, compactionCount, canCompactSliceListReversed, mMemManager, forceFilter,
        mSliceTable->GetStateFilterManager(), mSliceTable->GetSliceBucketIndex()->GetSlotStateFilter(bucketIndex), true,
        mSliceTable->GetTombstoneService());
    if (UNLIKELY((retVal != BSS_OK))) {
        LOG_WARN("Merge slice data for compaction failed or data empty, ret:" << retVal << ".");
        return retVal;
    }
    if (UNLIKELY(resultVec.empty())) {
        return BSS_OK;
    }

    // 2. 创建新的slice.
    auto dataSliceImpl = std::make_shared<DataSlice>();
    SliceRef slice = std::make_shared<Slice>();
    SliceCreateMeta meta = { 1, 0, compactionCount };
    auto ret = slice->Initialize(resultVec, meta, mMemManager, false);
    if (UNLIKELY(ret != BSS_OK)) {
        LOG_WARN("Initialize new slice failed, ret:" << ret << ".");
        return ret;
    }
    dataSliceImpl->Init(slice);
    compactedDataSlice = DataSliceRef(dataSliceImpl);
    return BSS_OK;
}

BResult SliceTableRestoreOperation::DoCompositeCompaction(const SliceIndexContextRef &sliceIndexContext)
{
    LogicalSliceChainRef logicalSliceChain = sliceIndexContext->GetLogicalSliceChain();
    uint32_t bucketIndex = sliceIndexContext->GetSliceIndexSlot();
    std::vector<DataSliceRef> canCompactSliceListReversed;
    std::vector<SliceAddressRef> sliceAddressReverseOrder;
    std::vector<LogicalSliceChainRef> sliceChains = logicalSliceChain->GetSliceChains();
    if (UNLIKELY(sliceChains.empty())) {
        return BSS_OK;
    }

    bool hasFilePage = false;
    for (const auto &singleChain : sliceChains) {
        CONTINUE_LOOP_AS_NULLPTR(singleChain);
        std::vector<FilePageRef> filePages;
        singleChain->GetFilePages(filePages);
        if (!filePages.empty()) {
            hasFilePage = true;
        }
        for (int32_t i = singleChain->GetSliceChainTailIndex(); i >= 0; --i) {
            SliceAddressRef sliceAddress = singleChain->GetSliceAddress(i);
            if (sliceAddress == nullptr || sliceAddress->IsEvicted() || sliceAddress->GetDataSlice() == nullptr) {
                continue;
            }
            sliceAddressReverseOrder.push_back(sliceAddress);
            canCompactSliceListReversed.push_back(sliceAddress->GetDataSlice());
        }
    }
    if (UNLIKELY(canCompactSliceListReversed.empty() && !hasFilePage)) {
        return BSS_OK;
    }

    DataSliceRef compactDataSlice = nullptr;
    auto ret = DoCompactCompositeSlice(canCompactSliceListReversed, compactDataSlice, true, bucketIndex);
    auto tombstoneService = mSliceTable->GetTombstoneService();
    if (tombstoneService != nullptr) {
        tombstoneService->Commit(ret == BSS_OK);
    }
    if (UNLIKELY(ret != BSS_OK)) {
        LOG_WARN("Do compact composite slice failed, ret:" << ret << ", bucketIndex:" << bucketIndex);
        return ret;
    }
    LOG_DEBUG("Do compact composite slice success, oldSliceSize:"
              << logicalSliceChain->GetSliceSize() << ", bucketIndex:" << bucketIndex
              << ", dataSliceSize:" << canCompactSliceListReversed.size());

    return ReplaceCompositeLogicalSlice(logicalSliceChain, bucketIndex, compactDataSlice, sliceAddressReverseOrder);
}

BResult SliceTableRestoreOperation::DirectReplaceCompositeSlice(const SliceIndexContextRef &sliceIndexContext)
{
    LogicalSliceChainRef logicalSliceChain = sliceIndexContext->GetLogicalSliceChain();
    uint32_t bucketIndex = sliceIndexContext->GetSliceIndexSlot();
    std::vector<LogicalSliceChainRef> sliceChains = logicalSliceChain->GetSliceChains();
    if (UNLIKELY(sliceChains.empty())) {
        return BSS_OK;
    }
    if (UNLIKELY(sliceChains.size() > NO_1)) {
        LOG_ERROR("Impossible, invalid composite slice chains num, size:" << sliceChains.size());
        return BSS_INNER_ERR;
    }

    LogicalSliceChainRef newLogicalSliceChain = sliceChains[0];
    mSliceBucketIndex->UpdateLogicalSliceChain(bucketIndex, logicalSliceChain, newLogicalSliceChain);
    return BSS_OK;
}

BResult SliceTableRestoreOperation::CompactCompositeLogicalSliceChain(uint32_t sliceIndexSlot)
{
    auto sliceIndexContext = mSliceBucketIndex->GetSliceIndexContext(sliceIndexSlot);
    if (UNLIKELY(sliceIndexContext == nullptr)) {
        LOG_ERROR("Get slice index context is nullptr");
        return BSS_ERR;
    }
    LogicalSliceChainRef logicalSliceChain = sliceIndexContext->GetLogicalSliceChain();
    if (UNLIKELY(logicalSliceChain->IsNone())) {
        return BSS_OK;
    }

    // 当compositeSliceChain仅有一个元素, 则不执行compaction, 直接将之替换为signalSliceChain.
    auto composite = std::dynamic_pointer_cast<CompositeLogicalSliceChain>(logicalSliceChain);
    RETURN_ERROR_AS_NULLPTR(composite);
    if (composite->GetCompositeNum() == NO_1 && !composite->RequireForceCompaction()) {
        return DirectReplaceCompositeSlice(sliceIndexContext);
    }

    if (!logicalSliceChain->CompareAndSetStatus(SliceStatus::NORMAL, SliceStatus::COMPACTING)) {
        LOG_ERROR("Failed to set slice status " << static_cast<uint8_t>(logicalSliceChain->GetSliceStatus()));
        return BSS_OK;
    }
    return DoCompositeCompaction(sliceIndexContext);
}

BResult SliceTableRestoreOperation::ReplaceCompositeLogicalSlice(LogicalSliceChainRef &logicalSliceChain,
                                                                 uint32_t sliceIndexSlot,
                                                                 const DataSliceRef &compactedDataSlice,
                                                                 std::vector<SliceAddressRef> &invalidSliceAddressList)
{
    constexpr uint64_t maxUsedMemory = static_cast<uint64_t>(std::numeric_limits<int64_t>::max());
    uint64_t loadedInputBytes = 0;
    for (const auto &sliceAddress : invalidSliceAddressList) {
        if (sliceAddress == nullptr) {
            continue;
        }
        uint64_t dataLen = sliceAddress->GetDataLen();
        if (UNLIKELY(dataLen > maxUsedMemory - loadedInputBytes)) {
            LOG_ERROR("Restore compacted input size exceeds used memory range, inputBytes:"
                      << loadedInputBytes << ", dataLen:" << dataLen << ", slot:" << sliceIndexSlot);
            return BSS_INVALID_PARAM;
        }
        loadedInputBytes += dataLen;
    }
    uint64_t outputBytes = compactedDataSlice == nullptr ? 0 : compactedDataSlice->GetSize();
    int64_t usedMemoryDelta = static_cast<int64_t>(outputBytes) - static_cast<int64_t>(loadedInputBytes);

    // 1. 创建新的LogicSliceChain.
    LogicalSliceChainRef compactedLogicalSliceChain = mSliceBucketIndex->CreateLogicalChainedSlice();
    RETURN_ERROR_AS_NULLPTR(compactedLogicalSliceChain);

    // 2. 创建新的DataSliceAddress. 过滤结果为空时保留合法空chain，避免生成零长度SliceAddress.
    if (compactedDataSlice != nullptr) {
        SliceAddressRef compactedSliceAddress = compactedLogicalSliceChain->CreateSlice(compactedDataSlice, 0);
        RETURN_ERROR_AS_NULLPTR(compactedSliceAddress);
        compactedSliceAddress->ReduceRequestCount(invalidSliceAddressList);
    }

    // 3. 设置新的LogicSliceChain的filePages和baseSliceIndex.
    std::vector<FilePageRef> filePages;
    logicalSliceChain->GetFilePages(filePages);
    compactedLogicalSliceChain->SetFilePages(filePages);
    if (compactedDataSlice != nullptr && compactedDataSlice->GetSize() > mConfig->GetSliceMaxSize()) {
        compactedLogicalSliceChain->SetBaseSliceIndex(NO_1);
    } else {
        compactedLogicalSliceChain->SetBaseSliceIndex(0);
    }

    // 4. 更新LogicSliceChain.
    SyncUpdateChain(logicalSliceChain, compactedLogicalSliceChain, sliceIndexSlot);
    if (UNLIKELY(mSliceBucketIndex->GetLogicChainedSlice(sliceIndexSlot) != compactedLogicalSliceChain)) {
        LOG_ERROR("Restore compacted logical slice chain replacement failed, slot:" << sliceIndexSlot);
        return BSS_INNER_ERR;
    }
    mSliceTable->AddSliceUsedMemory(usedMemoryDelta);
    return BSS_OK;
}

void SliceTableRestoreOperation::SyncUpdateChain(const LogicalSliceChainRef &oldLogicalSliceChain,
                                                 const LogicalSliceChainRef &newLogicalSliceChain,
                                                 uint32_t sliceIndexSlot)
{
    mSliceBucketIndex->UpdateLogicalSliceChain(sliceIndexSlot, oldLogicalSliceChain, newLogicalSliceChain);
}

}  // namespace bss
}  // namespace ock
