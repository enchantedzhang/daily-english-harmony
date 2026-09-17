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

#include "logical_slice_chain.h"

#include "blob_store/blob_store.h"
#include "common/bss_log.h"

namespace ock {
namespace bss {
LogicalSliceChainRef LogicalSliceChain::mEmptySliceChain = std::make_shared<LogicalSliceChainImpl>(SliceStatus::NORMAL,
                                                                                                   true);

BResult LogicalSliceChainImpl::Init(SliceStatus status, uint32_t defaultChainLen)
{
    if (UNLIKELY(defaultChainLen < NO_3)) {
        LOG_ERROR("Default chain len too small, defaultChainLen:" << defaultChainLen);
        return BSS_ERR;
    }
    mSliceStatus.store(status);
    mChainEndIndex.store(-1);
    mBaseSliceIndex.store(0);
    mSliceSize.store(0);
    return BSS_OK;
}

BResult LogicalSliceChainImpl::Initialize(
    const LogicalSliceChainRef &logicalSliceChain, int32_t startIndex, int32_t endIndex,
    std::unordered_map<SliceAddressRef, DataSliceRef, SliceAddressHash, SliceAddressEqual> &copiedDataSlice,
    bool deepCopySliceAddress, bool hasFilePage)
{
    if (UNLIKELY(logicalSliceChain == nullptr)) {
        LOG_ERROR("Input current logical slice chain is nullptr.");
        return BSS_INVALID_PARAM;
    }

    // 1. 计算sliceChain的baseIndex和endIndex.
    mBaseSliceIndex.store(0);  // start-base 之间是flush的slice，需要复制
    int32_t chainEndIndex = endIndex;
    if (chainEndIndex > logicalSliceChain->GetSliceChainTailIndex()) {
        chainEndIndex = logicalSliceChain->GetSliceChainTailIndex();
    }
    if (UNLIKELY(startIndex > chainEndIndex)) {
        LOG_ERROR("Verify logical slice chain invalid range failed, startIndex:"
                  << startIndex << ", endIndex:" << endIndex << ", chainTailIndex:"
                  << logicalSliceChain->GetSliceChainTailIndex() << ", final chainEndIndex:" << chainEndIndex);
        return BSS_ERR;
    }
    int32_t snapshotChainLen = chainEndIndex - startIndex + 1;
    mChainEndIndex.store(snapshotChainLen - 1);

    // 2. 拷贝dataSlice.
    if (!logicalSliceChain->GetSliceAddresses().empty()) {
        auto ret = CopySliceAddresses(logicalSliceChain, startIndex, endIndex, deepCopySliceAddress, snapshotChainLen);
        RETURN_NOT_OK_NO_LOG(ret);
    }

    // 3. 填充copyDataSliceMap, sliceStatus, sliceSize, filePage.
    IteratorRef<SliceAddressRef> iterator = SliceIterator();
    RETURN_NOT_OK_AS_FALSE(iterator == nullptr, BSS_ALLOC_FAIL);
    uint32_t snapshotSliceSize = 0;
    while (iterator->HasNext()) {
        SliceAddressRef sliceAddress = iterator->Next();
        DataSliceRef dataSlice = sliceAddress->GetDataSlice();
        CONTINUE_LOOP_AS_NULLPTR(dataSlice);
        copiedDataSlice.emplace(sliceAddress, dataSlice);
        snapshotSliceSize += sliceAddress->GetDataLen();
    }
    mSliceStatus.store(logicalSliceChain->GetSliceStatus());
    mSliceSize.store(snapshotSliceSize);

    LOG_DEBUG("Logic slice chain info, baseSliceIndex:"
              << mBaseSliceIndex.load() << ", chainEndIndex:" << mChainEndIndex.load()
              << ", sliceStatus:" << static_cast<uint32_t>(mSliceStatus.load()) << ", sliceSize:" << mSliceSize.load()
              << ", hasFilePage:" << hasFilePage);
    return BSS_OK;
}

BResult LogicalSliceChainImpl::CaptureSnapshotView(LogicalSliceChainSnapshotView &snapshotView)
{
    ReadLocker<ReadWriteLock> lk(&mRwLock);
    snapshotView = LogicalSliceChainSnapshotView{};
    snapshotView.mTailIndex = mChainEndIndex.load();
    snapshotView.mSliceStatus = mSliceStatus.load();
    if (snapshotView.mTailIndex < 0) {
        return BSS_OK;
    }
    if (UNLIKELY(static_cast<uint64_t>(snapshotView.mTailIndex) >= mSliceAddresses.size())) {
        snapshotView.mInvalidIndex = snapshotView.mTailIndex;
        LOG_ERROR("Capture logical slice chain snapshot failed, tailIndex:"
                  << snapshotView.mTailIndex << ", sliceAddressSize:" << mSliceAddresses.size());
        return BSS_ERR;
    }

    // 1. 在sliceChain读锁内从前往后找到第一个状态为非evicted的slice.
    for (int32_t index = 0; index <= snapshotView.mTailIndex; index++) {
        const SliceAddressRef &sliceAddress = mSliceAddresses[index];
        if (UNLIKELY(sliceAddress == nullptr)) {
            snapshotView.mInvalidIndex = index;
            LOG_ERROR("Capture logical slice chain snapshot failed, slice address is nullptr, index:" << index);
            return BSS_INVALID_PARAM;
        }
        SliceStatus sliceStatus = sliceAddress->GetSliceStatus();
        if (snapshotView.mHeadIndex < 0 && sliceStatus == SliceStatus::EVICTED) {
            snapshotView.mHasEvictedSlice = true;
            continue;
        }
        if (UNLIKELY(sliceStatus == SliceStatus::EVICTED)) {
            snapshotView.mInvalidIndex = index;
            LOG_ERROR("Capture logical slice chain snapshot failed, got evicted slice after snapshot head, index:"
                      << index << ", headIndex:" << snapshotView.mHeadIndex);
            return BSS_ERR;
        }

        // 2. 一次性预留空间并捕获强引用和对应状态, 避免锁内扩容及锁外状态漂移.
        if (snapshotView.mHeadIndex < 0) {
            snapshotView.mHeadIndex = index;
            uint32_t snapshotChainLen = static_cast<uint32_t>(snapshotView.mTailIndex - index + 1);
            snapshotView.mSliceAddresses.reserve(snapshotChainLen);
            snapshotView.mSliceStatuses.reserve(snapshotChainLen);
        }
        snapshotView.mSliceAddresses.emplace_back(sliceAddress);
        snapshotView.mSliceStatuses.emplace_back(sliceStatus);
    }
    return BSS_OK;
}

BResult LogicalSliceChainImpl::InitializeSnapshot(
    const LogicalSliceChainSnapshotView &snapshotView,
    std::unordered_map<SliceAddressRef, DataSliceRef, SliceAddressHash, SliceAddressEqual> &copiedDataSlice,
    bool hasFilePage)
{
    if (UNLIKELY(snapshotView.mSliceAddresses.empty() ||
                 snapshotView.mSliceAddresses.size() != snapshotView.mSliceStatuses.size())) {
        LOG_ERROR("Initialize logical slice chain snapshot failed, sliceAddressSize:"
                  << snapshotView.mSliceAddresses.size() << ", sliceStatusSize:" << snapshotView.mSliceStatuses.size());
        return BSS_INVALID_PARAM;
    }

    // 1. 根据已捕获的sliceAddress初始化快照链索引.
    mBaseSliceIndex.store(0);
    mChainEndIndex.store(static_cast<int32_t>(snapshotView.mSliceAddresses.size()) - 1);

    // 2. 在原sliceChain锁外深拷贝dataSlice.
    RETURN_NOT_OK_NO_LOG(CopySnapshotSliceAddresses(snapshotView));

    // 3. 填充copyDataSliceMap, sliceStatus, sliceSize, filePage.
    uint32_t snapshotSliceSize = 0;
    for (const auto &sliceAddress : mSliceAddresses) {
        if (UNLIKELY(sliceAddress == nullptr || sliceAddress == SliceAddress::mInvalidAddress)) {
            LOG_ERROR("Initialize logical slice chain snapshot failed, slice address is invalid.");
            return BSS_ERR;
        }
        DataSliceRef dataSlice = sliceAddress->GetDataSlice();
        if (UNLIKELY(dataSlice == nullptr)) {
            LOG_ERROR("Initialize logical slice chain snapshot failed, data slice is nullptr, sliceId:"
                      << sliceAddress->GetSliceId());
            return BSS_ERR;
        }
        copiedDataSlice.emplace(sliceAddress, dataSlice);
        snapshotSliceSize += sliceAddress->GetDataLen();
    }
    mSliceStatus.store(snapshotView.mSliceStatus);
    mSliceSize.store(snapshotSliceSize);

    LOG_DEBUG("Logic slice chain snapshot info, baseSliceIndex:"
              << mBaseSliceIndex.load() << ", chainEndIndex:" << mChainEndIndex.load()
              << ", sliceStatus:" << static_cast<uint32_t>(mSliceStatus.load()) << ", sliceSize:" << mSliceSize.load()
              << ", hasFilePage:" << hasFilePage);
    return BSS_OK;
}

IOResult LogicalSliceChainImpl::Get(const Key &key, Value &value, BlobValueTransformFunc getFromBlobFunc,
                                    BoostNativeMetricPtr &metricPtr)
{
    bool found;
    // 1. get from slice chain.
    int32_t tailIndex = GetSliceChainTailIndex();
    for (int32_t curIndex = tailIndex; curIndex >= 0; curIndex--) {
        SliceAddressRef sliceAddress = GetSliceAddress(curIndex);
        if (UNLIKELY(sliceAddress == nullptr)) {
            return IO_ERR;
        }
        if (sliceAddress->IsEvicted()) {
            break;
        }

        auto slice = sliceAddress->GetSlice();
        if (UNLIKELY(slice == nullptr)) {
            return IO_ERR;
        }
        found = slice->Get(key, value);
        if (found) {
            if (metricPtr != nullptr && metricPtr->IsSliceMetricEnabled()) {
                metricPtr->AddSliceReadSize(tailIndex - curIndex);
            }
            sliceAddress->AddRequestCount(NO_1);  // 增加命中次数，evict淘汰时候会查询该值算热度淘汰
            if (value.ValueType() == DELETE) {
                value.Init(DELETE, 0, nullptr, value.SeqId(), nullptr);
                return SLICE_FOUND_DELETE;
            }
            auto findBlobValue = BlobStore::ToBlobValue(key, value, getFromBlobFunc);
            return findBlobValue ? SLICE_FOUND : NOT_FOUND;
        }
    }
    if (metricPtr != nullptr && metricPtr->IsSliceMetricEnabled()) {
        metricPtr->AddSliceReadSize(tailIndex + 1);
    }

    // 2. get from file page.
    ReadLocker<ReadWriteLock> lk(&mFileRwLock);
    for (const auto &filePage : mFilePage) {
        found = filePage->Get(key, value);
        if (found) {
            auto findBlobValue = BlobStore::ToBlobValue(key, value, getFromBlobFunc);
            return findBlobValue ? LSM_FOUND : NOT_FOUND;
        }
    }
    return NOT_FOUND;
}

SliceAddressRef LogicalSliceChainImpl::CreateSlice(const DataSliceRef &dataSlice, uint64_t readAccessNumber)
{
    if (UNLIKELY(dataSlice == nullptr)) {
        LOG_ERROR("Data slice is nullptr.");
        return nullptr;
    }

    if (UNLIKELY(mChainEndIndex.load() >= static_cast<int32_t>(mSliceAddresses.size()))) {
        LOG_ERROR("Current logic slice overflow.");
        return nullptr;
    }

    SliceAddressRef sliceAddress = std::make_shared<SliceAddress>();
    sliceAddress->Init(dataSlice, readAccessNumber);

    WriteLocker<ReadWriteLock> lk(&mRwLock);
    mSliceAddresses.push_back(sliceAddress);
    mChainEndIndex.fetch_add(1);
    sliceAddress->SetChainIndex(mChainEndIndex.load());
    mSliceSize.fetch_add(dataSlice->GetSize());
    return sliceAddress;
}

uint32_t LogicalSliceChainImpl::InsertSlice(SliceAddressRef sliceAddress)
{
    if (UNLIKELY(sliceAddress == nullptr)) {
        LOG_ERROR("Insert slice failed, slice address is nullptr.");
        return INVALID_U32;
    }

    if (UNLIKELY(sliceAddress->GetDataSlice() == nullptr)) {
        LOG_WARN("Insert slice with nullptr data slice.");
    }

    WriteLocker<ReadWriteLock> lk(&mRwLock);
    mSliceAddresses.push_back(sliceAddress);
    mChainEndIndex.fetch_add(1);
    sliceAddress->SetChainIndex(mChainEndIndex.load());
    mSliceSize += sliceAddress->GetDataLen();
    LOG_DEBUG("Insert slice, endIndex:" << mChainEndIndex.load() << ", size:" << mSliceAddresses.size());
    return 0U;
}

void LogicalSliceChainImpl::ReleaseSliceAddress(SliceAddressRef &sliceAddress)
{
    WriteLocker<ReadWriteLock> lk(&mRwLock);
    bool find = false;
    // 从前往后遍历找到做flush的那个sliceAddress.
    for (auto &it : mSliceAddresses) {
        if (it == sliceAddress) {
            it = SliceAddress::mInvalidAddress;  // 插入一个无效的sliceAddress, 老的slice的资源释放.
            find = true;
            break;
        }
    }
    if (!find) {
        LOG_WARN("Not found evict slice.");
    }
}

BResult LogicalSliceChainImpl::SetBaseSliceIndex(uint32_t index)
{
    if (UNLIKELY(index > static_cast<uint32_t>(mChainEndIndex.load() + 1))) {
        LOG_ERROR("SetBaseSliceIndex error, index too large! index:" << index);
        return BSS_ERR;
    }
    mBaseSliceIndex.store(index);
    return BSS_OK;
}

IteratorRef<SliceAddressRef> LogicalSliceChainImpl::SliceIterator()
{
    ReadLocker<ReadWriteLock> lk(&mRwLock);
    if (mSliceAddresses.empty()) {
        return MakeRef<EmptySliceAddressIterator>();
    }
    auto iterator = MakeRef<SliceAddressIterator>();
    iterator->Initialize(mSliceAddresses);
    return iterator;
}

BResult LogicalSliceChainImpl::CopySliceAddresses(const LogicalSliceChainRef &logicalSliceChain, int32_t startIndex,
                                                  int32_t endIndex, bool deepCopySliceAddress,
                                                  uint32_t snapshotChainLen)
{
    mSliceAddresses.reserve(snapshotChainLen);
    if (deepCopySliceAddress) {
        for (uint32_t i = 0; i < snapshotChainLen; i++) {
            auto sliceAddress = logicalSliceChain->GetSliceAddress(startIndex + static_cast<int32_t>(i));
            if (UNLIKELY(sliceAddress == nullptr)) {
                LOG_ERROR("Get logical slice chain addresses failed, index:" << (startIndex + static_cast<int32_t>(i)));
                return BSS_ERR;
            }
            if (UNLIKELY(!sliceAddress->IsEvicted() && sliceAddress->GetDataSlice() == nullptr)) {
                LOG_ERROR("Copy slice address failed, data slice is nullptr, index:"
                          << (startIndex + static_cast<int32_t>(i)) << ", sliceId:" << sliceAddress->GetSliceId());
                return BSS_ERR;
            }
            auto copySliceAddress = std::make_shared<SliceAddress>(*sliceAddress);
            copySliceAddress->SetFatherSlice(sliceAddress);
            mSliceAddresses.emplace_back(copySliceAddress);
            LOG_DEBUG("Deep copy slice address success, sliceId:"
                      << mSliceAddresses[i]->GetSliceId() << ", dataLen:" << mSliceAddresses[i]->GetDataLen()
                      << ", checkSum:" << mSliceAddresses[i]->GetCheckSum());
        }
    } else {
        auto sliceAddresses = logicalSliceChain->GetSliceAddresses();  // 浅拷贝, 仅拷贝sliceAddress的智能指针.
        auto endIterator = std::copy(sliceAddresses.begin() + startIndex,
                                     sliceAddresses.begin() + startIndex + snapshotChainLen, mSliceAddresses.begin());
        if (endIterator != mSliceAddresses.end()) {
            LOG_ERROR("Copy slice address failed, startIndex:" << startIndex << ", endIndex:" << endIndex
                                                               << ", snapshotChainLen:" << snapshotChainLen);
            return BSS_ERR;
        }
    }
    return BSS_OK;
}

BResult LogicalSliceChainImpl::CopySnapshotSliceAddresses(const LogicalSliceChainSnapshotView &snapshotView)
{
    mSliceAddresses.reserve(snapshotView.mSliceAddresses.size());
    for (uint32_t index = 0; index < snapshotView.mSliceAddresses.size(); index++) {
        SliceAddressRef sourceSliceAddress = snapshotView.mSliceAddresses[index];
        if (UNLIKELY(sourceSliceAddress == nullptr || sourceSliceAddress == SliceAddress::mInvalidAddress ||
                     snapshotView.mSliceStatuses[index] == SliceStatus::EVICTED ||
                     sourceSliceAddress->GetDataSlice() == nullptr)) {
            LOG_ERROR("Copy snapshot slice address failed, index:" << index);
            return BSS_ERR;
        }
        auto copiedSliceAddress = std::make_shared<SliceAddress>(*sourceSliceAddress,
                                                                 snapshotView.mSliceStatuses[index]);
        if (UNLIKELY(copiedSliceAddress->GetDataSlice() == nullptr)) {
            LOG_ERROR("Copy snapshot data slice failed, index:" << index
                                                                << ", sliceId:" << sourceSliceAddress->GetSliceId());
            return BSS_ERR;
        }
        copiedSliceAddress->SetFatherSlice(sourceSliceAddress);
        mSliceAddresses.emplace_back(copiedSliceAddress);
        LOG_DEBUG("Deep copy snapshot slice address success, sliceId:"
                  << copiedSliceAddress->GetSliceId() << ", dataLen:" << copiedSliceAddress->GetDataLen()
                  << ", checkSum:" << copiedSliceAddress->GetCheckSum());
    }
    return BSS_OK;
}

BResult LogicalSliceChainImpl::Restore(const FileInputViewRef &reader, uint64_t snapshotId)
{
    RETURN_NOT_OK_AS_READ_ERROR(reader->Read(mChainEndIndex));  // 读取endIndex.
    mSliceAddresses.resize(mChainEndIndex + NO_1);
    RETURN_NOT_OK_AS_READ_ERROR(reader->Read(mBaseSliceIndex));  // 读取baseIndex.
    uint16_t evictedSliceCntAfterBase = 0;
    RETURN_NOT_OK_AS_READ_ERROR(reader->Read(evictedSliceCntAfterBase));  // 读取evictedNum.

    if (UNLIKELY(mBaseSliceIndex + evictedSliceCntAfterBase > static_cast<uint32_t>(mChainEndIndex.load() + 1))) {
        LOG_ERROR("ReStore logic chain fail, baseIndex:"
                  << mBaseSliceIndex << ", evict num:" << evictedSliceCntAfterBase << ", endIndex:" << mChainEndIndex);
        return BSS_ERR;
    }

    // 1. 处理淘汰的sliceAddress.
    for (uint32_t i = 0; i < mBaseSliceIndex.load() + evictedSliceCntAfterBase; i++) {
        SliceAddressRef sliceAddress = std::make_shared<SliceAddress>();
        sliceAddress->ForceEvict();
        mSliceAddresses[i] = sliceAddress;
    }

    // 2. 处理正常的sliceAddress.
    for (auto i = static_cast<int32_t>(mBaseSliceIndex.load() + evictedSliceCntAfterBase); i <= mChainEndIndex.load();
         i++) {
        SliceAddressRef sliceAddress = nullptr;
        RETURN_NOT_OK(SliceAddress::Restore(reader, sliceAddress));
        mSliceAddresses[i] = sliceAddress;
        mSliceSize.fetch_add(sliceAddress->GetDataLen());
        LOG_DEBUG("Restore normal slice address, idx:"
                  << i << ", sliceLen:" << sliceAddress->GetDataLen() << ", checkSum:" << sliceAddress->GetCheckSum()
                  << ", localAddress:" << sliceAddress->GetLocalAddress()
                  << ", startOffset:" << sliceAddress->GetStartOffset() << ", sliceId:" << sliceAddress->GetSliceId());
    }

    // 3. 处理filePage.
    uint8_t isFilePageNotEmpty = 0;
    RETURN_NOT_OK_AS_READ_ERROR(reader->Read(isFilePageNotEmpty));
    if (isFilePageNotEmpty == NO_1) {
        uint32_t filePageLen = 0;
        RETURN_NOT_OK_AS_READ_ERROR(reader->Read(filePageLen));
        mFilePage.resize(filePageLen);
    }
    LOG_DEBUG("SliceTable restore snapshot meta success, checkpoint:"
              << snapshotId << ", evictedSliceCount:" << evictedSliceCntAfterBase << ", slice chain:" << ToString());
    return BSS_OK;
}

void LogicalSliceChainImpl::SnapshotMeta(uint64_t snapshotId, const FileOutputViewRef &localOutputView,
                                         SnapshotMetaRef &snapshotMeta)
{
    LOG_DEBUG("SliceTable write snapshot meta, checkpointId:" << snapshotId << ", slice chain:" << ToString());
    // sliceChain的元数据内容: endIndex+baseIndex+被淘汰slice个数+sliceAddress元数据信息+filePage标记+非空filePage个数.
    localOutputView->WriteInt32(mChainEndIndex.load());
    localOutputView->WriteUint32(mBaseSliceIndex.load());
    ReadLocker<ReadWriteLock> lk(&mRwLock);
    if (mSliceAddresses.size() < static_cast<uint32_t>(mChainEndIndex.load() + NO_1)) {
        LOG_ERROR("Invalid slice addresses size:" << mSliceAddresses.size() << ", endIndex:" << mChainEndIndex.load());
        return;
    }
    // 计算从baseIndex开始的被淘汰slice的数量.
    uint16_t evictedSliceCntAfterBase = 0;
    for (auto idx = static_cast<int32_t>(mBaseSliceIndex.load()); idx <= mChainEndIndex.load(); idx++) {
        if (!mSliceAddresses[idx]->IsEvicted()) {
            break;
        }
        evictedSliceCntAfterBase++;
    }
    localOutputView->WriteUint16(evictedSliceCntAfterBase);

    // 将从baseIndex+evictedSliceCntAfterBase开始的sliceAddress元数据写入文件中.
    for (auto idx = static_cast<int32_t>(mBaseSliceIndex.load() + evictedSliceCntAfterBase);
         idx <= mChainEndIndex.load(); idx++) {
        mSliceAddresses[idx]->SnapshotMeta(localOutputView, snapshotMeta);
    }

    // 该字段表示是否为挂载filePage, 0表示没有filePage, 1表示有filePage.
    uint32_t pageSize = GetFilePageSize();
    if (pageSize == 0) {
        localOutputView->WriteUint8(NO_0);
    } else {
        localOutputView->WriteUint8(NO_1);
        localOutputView->WriteUint32(pageSize);
    }
}

LogicalSliceChainRef LogicalSliceChainImpl::DeepCopy(bool allowMetadataOnly)
{
    // 深拷贝SliceChain, DataSlice仅拷贝智能指针.
    auto copyLogicSliceChain = std::make_shared<LogicalSliceChainImpl>();
    copyLogicSliceChain->mSliceStatus.store(mSliceStatus.load());
    copyLogicSliceChain->mChainEndIndex.store(mChainEndIndex.load());
    copyLogicSliceChain->mBaseSliceIndex.store(mBaseSliceIndex.load());
    copyLogicSliceChain->mSliceSize.store(mSliceSize.load());
    if (!mSliceAddresses.empty()) {
        copyLogicSliceChain->mSliceAddresses.resize(mSliceAddresses.size());
        for (uint32_t i = 0; i < mSliceAddresses.size(); i++) {
            if (UNLIKELY(mSliceAddresses[i] == nullptr)) {
                LOG_ERROR("sliceAddress should not be null! and index in chain = " << i);
                return nullptr;
            }
            if (UNLIKELY(!allowMetadataOnly && !mSliceAddresses[i]->IsEvicted() &&
                         mSliceAddresses[i]->GetDataSlice() == nullptr)) {
                LOG_ERROR("Deep copy slice chain failed, data slice is nullptr, index:"
                          << i << ", sliceId:" << mSliceAddresses[i]->GetSliceId());
                return nullptr;
            }
            copyLogicSliceChain->mSliceAddresses[i] = std::make_shared<SliceAddress>(*mSliceAddresses[i],
                                                                                     allowMetadataOnly);
            copyLogicSliceChain->mSliceAddresses[i]->SetFatherSlice(mSliceAddresses[i]);
        }
    }
    ReadLocker<ReadWriteLock> lk(&mFileRwLock);
    if (!mFilePage.empty()) {
        copyLogicSliceChain->mFilePage.resize(mFilePage.size());
        std::copy(mFilePage.begin(), mFilePage.end(), copyLogicSliceChain->mFilePage.begin());
    }
    return copyLogicSliceChain;
}

void LogicalSliceChainImpl::RestoreFilePage(LsmStoreRef lsmStore)
{
    WriteLocker<ReadWriteLock> lk(&mFileRwLock);
    if (mFilePage.empty()) {
        return;
    }
    for (auto &filePage : mFilePage) {
        filePage = std::make_shared<FilePage>(lsmStore);
    }
}

BResult SliceAddressIterator::Initialize(std::vector<SliceAddressRef> sliceAddresses)
{
    mSliceAddresses = sliceAddresses;
    mIter = mSliceAddresses.begin();
    return BSS_OK;
}

bool SliceAddressIterator::HasNext()
{
    return mIter != mSliceAddresses.end();
}

SliceAddressRef SliceAddressIterator::Next()
{
    auto ret = *mIter;
    mIter++;
    return ret;
}

void SliceAddressIterator::Advance()
{
}

int32_t CompositeLogicalSliceChain::GetSliceChainTailIndex()
{
    if (UNLIKELY(mSliceChains.empty())) {
        return 0;
    }
    int32_t maxIndex = 0;
    for (const auto &chain : mSliceChains) {
        int32_t currentTailIndex = chain->GetSliceChainTailIndex();
        if (currentTailIndex > maxIndex) {
            maxIndex = currentTailIndex;
        }
    }
    return maxIndex;
}

uint32_t CompositeLogicalSliceChain::GetBaseSliceIndex()
{
    if (UNLIKELY(mSliceChains.empty())) {
        return 0;
    }
    uint32_t maxIndex = 0;
    for (const auto &chain : mSliceChains) {
        uint32_t currentBaseIndex = chain->GetBaseSliceIndex();
        if (currentBaseIndex > maxIndex) {
            maxIndex = currentBaseIndex;
        }
    }
    return maxIndex;
}

bool CompositeLogicalSliceChain::HasFilePage()
{
    for (const auto &chain : mSliceChains) {
        if (chain->HasFilePage()) {
            return true;
        }
    }
    return false;
}

void CompositeLogicalSliceChain::GetFilePages(std::vector<FilePageRef> &result)
{
    std::unordered_set<FilePageRef, FilePageHash, FilePageEqual> visited;
    for (const auto &chain : mSliceChains) {
        std::vector<FilePageRef> currentPages;
        chain->GetFilePages(currentPages);
        for (const FilePageRef &page : currentPages) {
            if (visited.find(page) == visited.end()) {
                visited.insert(page);
                result.push_back(page);
            }
        }
    }
}

uint32_t CompositeLogicalSliceChain::GetFilePageSize()
{
    std::vector<FilePageRef> result;
    std::unordered_set<FilePageRef, FilePageHash, FilePageEqual> visited;
    for (const auto &chain : mSliceChains) {
        std::vector<FilePageRef> currentPages;
        chain->GetFilePages(currentPages);
        for (const FilePageRef &page : currentPages) {
            if (visited.find(page) == visited.end()) {
                visited.insert(page);
                result.push_back(page);
            }
        }
    }
    return result.size();
}

uint32_t CompositeLogicalSliceChain::GetCurrentSliceChainLen()
{
    uint32_t sum = 0;
    for (const auto &chain : mSliceChains) {
        sum += chain->GetCurrentSliceChainLen();
    }
    return sum;
}

}  // namespace bss
}  // namespace ock
