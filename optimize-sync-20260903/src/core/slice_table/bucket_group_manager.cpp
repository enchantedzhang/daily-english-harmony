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

#include "bucket_group_manager.h"

#include "common/bss_log.h"

namespace ock {
namespace bss {

BResult BucketGroupManager::Initialize(const ConfigRef &config, const SliceBucketIndexRef &sliceIndex,
                                       const FileCacheManagerRef &fileCache, uint32_t bucketGroupNum,
                                       uint32_t bucketNum, const MemManagerRef &memManager,
                                       const StateFilterManagerRef &stateFilterManager)
{
    if (UNLIKELY(config == nullptr)) {
        LOG_ERROR("Config is nullptr!");
        return BSS_ERR;
    }
    if (UNLIKELY(sliceIndex == nullptr)) {
        LOG_ERROR("Slice bucket index is nullptr!");
        return BSS_ERR;
    }
    mSliceBucketIndex = sliceIndex;
    mBucketNum = bucketNum;
    mBucketGroups.resize(bucketGroupNum);
    mMemManager = memManager;
    mFileCache = fileCache;
    return AssignBucketToBucketGroup(config, fileCache, stateFilterManager);
}

void BucketGroupManager::Open()
{
    for (const BucketGroupRef &bucketGroup : mBucketGroups) {
        bucketGroup->GetLsmStore()->StartLsmCompaction();
    }
}

void BucketGroupManager::Close()
{
    for (const BucketGroupRef &bucketGroup : mBucketGroups) {
        bucketGroup->GetLsmStore()->Close();
    }
}

BResult BucketGroupManager::AssignBucketToBucketGroup(const std::shared_ptr<Config> &config,
                                                      const FileCacheManagerRef &fileCache,
                                                      const StateFilterManagerRef &stateFilterManager)
{
    auto bucketGroupNum = mBucketGroups.size();
    GroupRangeRef groupRange = std::make_shared<GroupRange>(config->GetStartGroup(), config->GetEndGroup());

    for (uint32_t index = 0; index < bucketGroupNum; ++index) {
        uint32_t startBucketId = (index * mBucketNum + bucketGroupNum - 1) / bucketGroupNum;
        uint32_t endBucketId = ((index + 1) * mBucketNum - 1) / bucketGroupNum;
        HashCodeRangeRef hashCodeRange = mSliceBucketIndex->ComputeHashCodeRange(startBucketId, endBucketId);
        FileStoreIDRef fileStoreId = std::make_shared<FileStoreID>(groupRange, index, hashCodeRange);

        // 初始化LsmStore, 创建FileStoreCompaction.
        auto fileFactory = FileFactory::CreateFileFactory(config, mMemManager);
        auto lsmStore = std::make_shared<LsmStore>(fileStoreId, config, fileFactory, fileCache, stateFilterManager,
                                                   mMemManager);
        auto ret = lsmStore->Initialize();
        RETURN_NOT_OK_NO_LOG(ret);
        LOG_INFO("Create file store success, index:" << index << ", fileStoreId:" << fileStoreId->ToString().c_str());

        BucketGroupRef bucketGroup = std::make_shared<BucketGroup>();
        ret = bucketGroup->Initialize(index, lsmStore, mSliceBucketIndex, startBucketId, endBucketId);
        RETURN_NOT_OK_NO_LOG(ret);
        mBucketGroups[index] = bucketGroup;
    }
    return BSS_OK;
}

uint32_t BucketGroupManager::GetBucketGroupSize()
{
    return mBucketGroups.size();
}

std::shared_ptr<BucketGroupIterator> BucketGroupManager::GetBucketGroups()
{
    std::shared_ptr<BucketGroupIterator> bucketGroupIterator = std::make_shared<BucketGroupIterator>();
    bucketGroupIterator->Initialize(mBucketGroups);
    return bucketGroupIterator;
}

KeyValueIteratorRef BucketGroupManager::IteratorFileStoreData(uint16_t stateId)
{
    return std::make_shared<SliceFileStoreIterator>(this, stateId);
}

std::vector<BucketGroupRef> BucketGroupManager::GetBucketGroupVector()
{
    return mBucketGroups;
}

void BucketGroupManager::MarkLogicalSliceChainFlushed(const LogicalSliceChainRef &logicalSliceChain,
                                                      const BucketGroupRef &bucketGroup)
{
    logicalSliceChain->InsertFilePageIfEmpty(std::make_shared<FilePage>(bucketGroup->GetLsmStore()));
}

BResult BucketGroupManager::RestoreMeta(const FileInputViewRef &reader, uint32_t totalBucketNum,
                                        std::vector<BucketGroupRangeRef> &bucketGroupRanges)
{
    uint32_t bucketGroupNum = 0;
    RETURN_NOT_OK_AS_READ_ERROR(reader->Read(bucketGroupNum));
    bucketGroupRanges.reserve(bucketGroupNum);
    for (uint32_t i = 0; i < bucketGroupNum; ++i) {
        BucketGroupRangeRef bucketGroupRange = nullptr;
        RETURN_NOT_OK(BucketGroup::Restore(reader, totalBucketNum, bucketGroupRange));
        bucketGroupRanges.push_back(bucketGroupRange);
    }
    return BSS_OK;
}

void BucketGroupManager::SnapshotMeta(const FileOutputViewRef &localOutputView)
{
    // bucketGroup的元数据内容: bucketGroup个数+每个bucketGroup的startIndex+endIndex+groupId
    localOutputView->WriteUint32(mBucketGroups.size());
    for (auto &bucketGroup : mBucketGroups) {
        bucketGroup->SnapshotMeta(localOutputView);
    }
}

std::vector<BucketGroupRangeRef> BucketGroupManager::GetBucketGroupRanges()
{
    std::vector<BucketGroupRangeRef> bucketGroupRanges;
    bucketGroupRanges.reserve(mBucketGroups.size());
    for (auto &mBucketGroup : mBucketGroups) {
        bucketGroupRanges.push_back(mBucketGroup->GetBucketGroupRange());
    }
    return bucketGroupRanges;
}

LsmStoreRef BucketGroupManager::GetLsmStoreByBucketIndex(uint32_t bucketIndex)
{
    if (UNLIKELY(bucketIndex >= mBucketNum)) {
        LOG_ERROR("UnValid bucket index:" << bucketIndex << ", bucket num :" << mBucketNum);
        return nullptr;
    }
    uint32_t bucketGroupIndex = ComputeBucketGroupIndex(bucketIndex);
    return mBucketGroups[bucketGroupIndex]->GetLsmStore();
}

uint32_t BucketGroupManager::ComputeBucketGroupIndex(uint32_t bucketIndex)
{
    return mBucketNum == 0 ? 0 : (bucketIndex * mBucketGroups.size() / mBucketNum);
}

BResult BucketGroupManager::RestoreFileStore(const std::vector<SliceTableRestoreMetaRef> &sliceTableRestoreMetaList,
                                             std::unordered_map<std::string, std::string> &lazyPathMapping,
                                             std::unordered_map<std::string, uint32_t> &restorePathFileIdMap,
                                             bool isLazyDownload)
{
    // 1. 恢复file store version info.
    for (const auto &bucketGroup : mBucketGroups) {
        LsmStoreRef lsmStore = bucketGroup->GetLsmStore();
        std::vector<std::pair<FileInputViewRef, uint64_t>> metaList;
        HandleFileStoreOverLapping(sliceTableRestoreMetaList, lsmStore->GetFileStoreId(), metaList);
        RETURN_NOT_OK(lsmStore->RestoreVersionInfo(metaList, lazyPathMapping, restorePathFileIdMap, isLazyDownload));
    }

    // 2. 恢复file store data.
    for (const auto &bucketGroup : mBucketGroups) {
        LsmStoreRef lsmStore = bucketGroup->GetLsmStore();
        RETURN_NOT_OK(lsmStore->RestoreData());
    }
    LOG_DEBUG("Restore file store success.");
    return BSS_OK;
}

void BucketGroupManager::HandleFileStoreOverLapping(
    const std::vector<SliceTableRestoreMetaRef> &sliceTableRestoreMetaList, FileStoreIDRef fileStoreID,
    std::vector<std::pair<FileInputViewRef, uint64_t>> &metaList)
{
    for (const auto &meta : sliceTableRestoreMetaList) {
        std::vector<uint64_t> metaOffsetList;
        meta->GetOverlapFileStoreMetaOffset(fileStoreID, metaOffsetList);
        for (const auto &offset : metaOffsetList) {
            metaList.emplace_back(meta->GetMetaInputView(), offset);
        }
    }
}

BResult BucketGroupIterator::Initialize(std::vector<BucketGroupRef> &bucketGroups)
{
    mBucketGroups = bucketGroups;
    iter = mBucketGroups.begin();
    return BSS_OK;
}

bool BucketGroupIterator::HasNext()
{
    return iter != mBucketGroups.end();
}

BucketGroupRef BucketGroupIterator::Next()
{
    auto bucketGroupEle = *iter;
    iter++;
    return bucketGroupEle;
}

bool BucketGroupManager::SliceFileStoreIterator::HasNext()
{
    Advance();
    return (mInnerIterator != nullptr && mInnerIterator->HasNext());
}

KeyValueRef BucketGroupManager::SliceFileStoreIterator::Next()
{
    if (mInnerIterator == nullptr || !mInnerIterator->HasNext()) {
        LOG_ERROR("no more elements in SliceFileStoreIterator");
        return {};
    }
    return mInnerIterator->Next();
}

void BucketGroupManager::SliceFileStoreIterator::Advance()
{
    if (mInnerIterator != nullptr && mInnerIterator->HasNext()) {
        return;
    }
    if (mInnerIterator != nullptr && !mInnerIterator->HasNext()) {
        mInnerIterator->Close();
    }

    mInnerIterator = nullptr;
    while (mIndex < mBucketGroupManager->GetBucketGroupVector().size()) {
        LsmStoreRef curLsmStore = mBucketGroupManager->GetBucketGroupVector()[mIndex]->GetLsmStore();
        mIndex++;
        mInnerIterator = curLsmStore->Iterator(mStateId);
        if (mInnerIterator != nullptr && mInnerIterator->HasNext()) {
            break;
        }
        mInnerIterator = nullptr;
    }
}

}  // namespace bss
}  // namespace ock