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

#ifndef BOOST_STATE_STORE_LOGICALSLICECHAIN_H
#define BOOST_STATE_STORE_LOGICALSLICECHAIN_H

#include <cstdint>
#include <limits>
#include <memory>
#include <mutex>
#include <stack>
#include <vector>

#include "common/util/bss_lock.h"
#include "data_slice.h"
#include "include/bss_err.h"
#include "include/config.h"
#include "lsm_store/file/file_store_impl.h"
#include "slice_address.h"
#include "slice_status.h"
#include "snapshot/snapshot_meta.h"

namespace ock {
namespace bss {

class FilePage {
public:
    explicit FilePage(const LsmStoreRef &lsmStore) : mLsmStore(lsmStore)
    {
    }

    BResult inline Get(const Key &key, std::deque<Value> &values, SectionsReadMetaRef &sectionsReadMeta, bool flag)
    {
        auto ret = mLsmStore->Get(key, values, sectionsReadMeta, flag);
        return ret;
    }

    inline bool Get(const Key &key, Value &value)
    {
        auto ret = mLsmStore->Get(key, value);
        return ret;
    }

    inline KeyValueIteratorRef PrefixIterator(const Key &prefixKey, bool reverseOrder)
    {
        auto it = mLsmStore->PrefixIterator(prefixKey, reverseOrder);
        if (it == nullptr || !it->HasNext()) {
            LOG_DEBUG("Get prefix iterator failed." << prefixKey.ToString());
        }
        return it;
    };

    BResult GetByReadSection(FileMetaDataRef &fileMetaData, Key &key, Value &value)
    {
        return mLsmStore->GetByReadSection(fileMetaData->GetFileAddress(), key, value);
    }

    void CleanVersion(VersionPtr &version)
    {
        mLsmStore->ReleaseVersionFinally(version);
    }

private:
    LsmStoreRef mLsmStore = nullptr;
};
using FilePageRef = std::shared_ptr<FilePage>;

struct FilePageHash {
    uint32_t operator()(const std::shared_ptr<FilePage> &filePage) const
    {
        return std::hash<FilePage *>()(filePage.get());
    }
};

struct FilePageEqual {
    bool operator()(const std::shared_ptr<FilePage> &a, const std::shared_ptr<FilePage> &b) const
    {
        return a.get() == b.get();
    }
};

class EmptySliceAddressIterator : public Iterator<SliceAddressRef> {
public:
    bool HasNext() override
    {
        return false;
    }

    SliceAddressRef Next() override
    {
        return nullptr;
    }
};

class SliceAddressIterator : public Iterator<SliceAddressRef> {
public:
    BResult Initialize(std::vector<SliceAddressRef> sliceAddresses);

    bool HasNext() override;

    SliceAddressRef Next() override;

private:
    void Advance();
    std::vector<SliceAddressRef> mSliceAddresses;
    std::vector<SliceAddressRef>::iterator mIter;
};

struct LogicalSliceChainSnapshotView {
    int32_t mHeadIndex = -1;
    int32_t mTailIndex = -1;
    int32_t mInvalidIndex = -1;
    bool mHasEvictedSlice = false;
    SliceStatus mSliceStatus = SliceStatus::NORMAL;
    std::vector<SliceAddressRef> mSliceAddresses;
    std::vector<SliceStatus> mSliceStatuses;
};

class LogicalSliceChain {
public:
    static std::shared_ptr<LogicalSliceChain> mEmptySliceChain;

    LogicalSliceChain() = default;
    virtual ~LogicalSliceChain() = default;

    virtual BResult Init(SliceStatus status, uint32_t defaultChainLen) = 0;

    virtual BResult Initialize(
        const std::shared_ptr<LogicalSliceChain> &logicalSliceChain, int32_t startIndex, int32_t endIndex,
        std::unordered_map<SliceAddressRef, DataSliceRef, SliceAddressHash, SliceAddressEqual> &copiedDataSlice,
        bool deepCopySliceAddress, bool hasFilePage) = 0;

    virtual IOResult Get(const Key &key, Value &value, BlobValueTransformFunc func,
                         BoostNativeMetricPtr &metricPtr) = 0;

    virtual uint32_t GetSliceSize() = 0;

    virtual std::vector<SliceAddressRef> &GetSliceAddresses() = 0;

    virtual void ClearSliceAddresses() = 0;

    virtual uint32_t GetSliceChainCapacity() = 0;

    virtual SliceAddressRef GetSliceAddress(int32_t bucketIndex) = 0;

    virtual void SetSliceAddress(int32_t bucketIndex, const SliceAddressRef &addr) = 0;

    virtual SliceAddressRef CreateSlice(const DataSliceRef &dataSlice, uint64_t readAccessNumber) = 0;

    virtual void ReleaseSliceAddress(SliceAddressRef &sliceAddress) = 0;

    virtual uint32_t InsertSlice(SliceAddressRef sliceAddress) = 0;

    virtual void EmplaceSlice(SliceAddressRef sliceAddress) = 0;

    virtual int32_t GetSliceChainTailIndex() = 0;

    virtual void SetSliceChainTailIndex(int32_t index) = 0;

    virtual uint32_t GetBaseSliceIndex() = 0;

    virtual uint32_t GetFilePageSize() = 0;

    virtual BResult SetBaseSliceIndex(uint32_t index) = 0;

    virtual std::vector<std::shared_ptr<LogicalSliceChain>> GetSliceChains() = 0;

    virtual uint32_t GetCurrentSliceChainLen() = 0;

    virtual SliceStatus GetSliceStatus() = 0;

    virtual void SetSliceStatus(SliceStatus status) = 0;

    virtual IteratorRef<SliceAddressRef> SliceIterator() = 0;

    virtual bool CompareAndSetStatus(SliceStatus expectedPageStatus, SliceStatus targetStatus) = 0;

    virtual bool HasFilePage() = 0;

    virtual void GetFilePages(std::vector<FilePageRef> &currentPages) = 0;

    virtual void InsertFilePage(FilePageRef filePage) = 0;

    virtual void InsertFilePageIfEmpty(FilePageRef filePage) = 0;

    virtual void InsertAllFilePage(std::vector<FilePageRef> filePages) = 0;

    virtual void SetFilePages(std::vector<FilePageRef> filePages) = 0;

    virtual void SnapshotMeta(uint64_t snapshotId, const FileOutputViewRef &localOutputView,
                              SnapshotMetaRef &snapshotMeta) = 0;

    virtual BResult Restore(const FileInputViewRef &reader, uint64_t snapshotId) = 0;

    virtual void RestoreFilePage(LsmStoreRef lsmStore) = 0;

    virtual std::shared_ptr<LogicalSliceChain> DeepCopy(bool allowMetadataOnly = false) = 0;

    virtual bool IsNone() const = 0;

    virtual void NotifyNoneFlag(bool flag) = 0;

    virtual bool IsEmpty() = 0;

    virtual std::string ToString() const = 0;

    virtual bool IsSimpleSliceChain() = 0;

    virtual uint32_t GetCompositeNum() = 0;

    inline void SetCompactionToNormal()
    {
        mSliceStatus.store(SliceStatus::NORMAL);
        int32_t currentIndex = GetSliceChainTailIndex();
        auto baseIndex = static_cast<int32_t>(GetBaseSliceIndex());
        while (currentIndex >= baseIndex) {
            auto sliceAddress = GetSliceAddress(currentIndex);
            if (UNLIKELY(sliceAddress == nullptr)) {
                break;
            }
            sliceAddress->SetStatus(SliceEvent::COMPACT_BACK);
            currentIndex--;
        }
    }

public:
    std::atomic<SliceStatus> mSliceStatus{ SliceStatus::NORMAL };
};
using LogicalSliceChainRef = std::shared_ptr<LogicalSliceChain>;

class LogicalSliceChainImpl : public LogicalSliceChain {
public:
    LogicalSliceChainImpl() = default;

    explicit LogicalSliceChainImpl(SliceStatus sliceStatus, bool isNone) : mIsNone(isNone)
    {
        mSliceStatus.store(sliceStatus);
        mChainEndIndex.store(-1);
        mBaseSliceIndex.store(0);
        mSliceSize.store(0);
    }

    ~LogicalSliceChainImpl() override
    {
        std::vector<SliceAddressRef>().swap(mSliceAddresses);
        std::vector<FilePageRef>().swap(mFilePage);
    }

    BResult Init(SliceStatus status, uint32_t defaultChainLen) override;

    BResult Initialize(
        const std::shared_ptr<LogicalSliceChain> &logicalSliceChain, int32_t startIndex, int32_t endIndex,
        std::unordered_map<SliceAddressRef, DataSliceRef, SliceAddressHash, SliceAddressEqual> &copiedDataSlice,
        bool deepCopySliceAddress, bool hasFilePage) override;

    BResult CaptureSnapshotView(LogicalSliceChainSnapshotView &snapshotView);

    BResult InitializeSnapshot(
        const LogicalSliceChainSnapshotView &snapshotView,
        std::unordered_map<SliceAddressRef, DataSliceRef, SliceAddressHash, SliceAddressEqual> &copiedDataSlice,
        bool hasFilePage);

    IOResult Get(const Key &key, Value &value, BlobValueTransformFunc func, BoostNativeMetricPtr &metricPtr) override;

    uint32_t GetSliceSize() override
    {
        return mSliceSize;
    }

    std::vector<SliceAddressRef> &GetSliceAddresses() override
    {
        return mSliceAddresses;
    }

    void ClearSliceAddresses() override
    {
        mSliceAddresses.clear();
    }

    uint32_t GetSliceChainCapacity() override
    {
        return mSliceAddresses.size();
    }

    SliceAddressRef GetSliceAddress(int32_t bucketIndex) override
    {
        ReadLocker<ReadWriteLock> lk(&mRwLock);
        auto size = mSliceAddresses.size();
        if (UNLIKELY(bucketIndex < 0 || bucketIndex > mChainEndIndex.load())) {
            LOG_WARN("Logic slice overflow, bucketIndex:" << bucketIndex << ", baseIndex:" << mBaseSliceIndex.load()
                                                          << ", endIndex:" << mChainEndIndex.load()
                                                          << ", status:" << static_cast<uint32_t>(mSliceStatus.load())
                                                          << ", sliceSize:" << mSliceSize.load());
            return nullptr;
        }
        if (UNLIKELY(size < INT32_MAX && bucketIndex >= static_cast<int32_t>(size))) {
            LOG_WARN("Logic slice overflow, bucketIndex:" << bucketIndex << ", mSliceAddresses.size(): " << size);
            return nullptr;
        }
        return mSliceAddresses[bucketIndex];
    }

    void SetSliceAddress(int32_t bucketIndex, const SliceAddressRef &addr) override
    {
        ReadLocker<ReadWriteLock> lk(&mRwLock);
        if (UNLIKELY(bucketIndex < 0 || bucketIndex > mChainEndIndex.load())) {
            LOG_WARN("Logic slice overflow, bucketIndex:" << bucketIndex << ", endIndex:" << mChainEndIndex.load()
                                                          << ", status:" << static_cast<uint32_t>(mSliceStatus.load())
                                                          << ", sliceSize:" << mSliceSize.load());
            return;
        }
        mSliceAddresses[bucketIndex] = addr;
    }

    SliceAddressRef CreateSlice(const DataSliceRef &dataSlice, uint64_t readAccessNumber) override;

    void ReleaseSliceAddress(SliceAddressRef &sliceAddress) override;

    uint32_t InsertSlice(SliceAddressRef sliceAddress) override;

    void EmplaceSlice(SliceAddressRef sliceAddress) override
    {
        mSliceAddresses.emplace_back(sliceAddress);
    }

    int32_t GetSliceChainTailIndex() override
    {
        return mChainEndIndex.load();
    }

    void SetSliceChainTailIndex(int32_t index) override
    {
        mChainEndIndex.store(index);
    }

    uint32_t GetBaseSliceIndex() override
    {
        return mBaseSliceIndex.load();
    }

    BResult SetBaseSliceIndex(uint32_t index) override;

    std::vector<LogicalSliceChainRef> GetSliceChains() override
    {
        LOG_ERROR("Unsupported operation exception.");
        return {};
    }

    uint32_t GetCurrentSliceChainLen() override
    {
        return mChainEndIndex + 1;
    }

    SliceStatus GetSliceStatus() override
    {
        return mSliceStatus.load();
    }

    void SetSliceStatus(SliceStatus status) override
    {
        mSliceStatus.store(status);
    }

    IteratorRef<SliceAddressRef> SliceIterator() override;

    bool CompareAndSetStatus(SliceStatus expectedPageStatus, SliceStatus targetStatus) override
    {
        return mSliceStatus.compare_exchange_weak(expectedPageStatus, targetStatus);
    }

    bool HasFilePage() override
    {
        ReadLocker<ReadWriteLock> lk(&mFileRwLock);
        return (!mFilePage.empty()) && (mFilePage[0] != nullptr);
    }

    uint32_t GetFilePageSize() override
    {
        ReadLocker<ReadWriteLock> lk(&mFileRwLock);
        return mFilePage.size();
    }

    void GetFilePages(std::vector<FilePageRef> &currentPages) override
    {
        ReadLocker<ReadWriteLock> lk(&mFileRwLock);
        currentPages = mFilePage;
    }

    void InsertFilePage(FilePageRef filePage) override
    {
        WriteLocker<ReadWriteLock> lk(&mFileRwLock);
        mFilePage.push_back(filePage);
    }

    void InsertFilePageIfEmpty(FilePageRef filePage) override
    {
        WriteLocker<ReadWriteLock> lk(&mFileRwLock);
        if (mFilePage.empty()) {
            mFilePage.push_back(filePage);
        }
    }

    void InsertAllFilePage(std::vector<FilePageRef> filePages) override
    {
        WriteLocker<ReadWriteLock> lk(&mFileRwLock);
        for (auto &filePage : filePages) {
            mFilePage.push_back(filePage);
        }
    }

    void SetFilePages(std::vector<FilePageRef> filePages) override
    {
        WriteLocker<ReadWriteLock> lk(&mFileRwLock);
        mFilePage = filePages;
    }

    void SnapshotMeta(uint64_t snapshotId, const FileOutputViewRef &localOutputView,
                      SnapshotMetaRef &snapshotMeta) override;

    BResult Restore(const FileInputViewRef &reader, uint64_t snapshotId) override;

    void RestoreFilePage(LsmStoreRef lsmStore) override;

    LogicalSliceChainRef DeepCopy(bool allowMetadataOnly = false) override;

    bool IsNone() const override
    {
        return mIsNone;
    }

    void NotifyNoneFlag(bool flag) override
    {
        mIsNone = flag;
    }

    bool IsEmpty() override
    {
        return mIsNone || (mChainEndIndex == -1);
    }

    inline std::string ToString() const override
    {
        std::ostringstream oss;
        oss << "Info[";
        oss << "status:" << static_cast<uint32_t>(mSliceStatus.load()) << " end:" << mChainEndIndex.load()
            << " base:" << mBaseSliceIndex.load() << " size:" << mSliceSize.load() << " filePage:" << mFilePage.size()
            << "]";
        oss << ", Slice address status [";
        for (int32_t idx = 0; idx <= mChainEndIndex; idx++) {
            auto sliceAddress = mSliceAddresses[idx];
            if (sliceAddress == nullptr) {
                continue;
            }
            oss << static_cast<uint32_t>(sliceAddress->GetSliceStatus()) << " ";
        }
        oss << "]";
        return oss.str();
    }

    bool IsSimpleSliceChain() override
    {
        return true;
    }

    uint32_t GetCompositeNum() override
    {
        LOG_ERROR("Unsupported operation exception.");
        return NO_1;
    }

private:
    BResult CopySliceAddresses(const std::shared_ptr<LogicalSliceChain> &logicalSliceChain, int32_t startIndex,
                               int32_t endIndex, bool deepCopySliceAddress, uint32_t snapshotChainLen);

    BResult CopySnapshotSliceAddresses(const LogicalSliceChainSnapshotView &snapshotView);

private:
    std::atomic<int32_t> mChainEndIndex{ -1 };
    std::atomic<uint32_t> mBaseSliceIndex{ 0 };
    std::atomic<uint32_t> mSliceSize{ 0 };
    std::vector<SliceAddressRef> mSliceAddresses;
    std::vector<FilePageRef> mFilePage;
    ReadWriteLock mRwLock;
    ReadWriteLock mFileRwLock;
    bool mIsNone = false;
};

class CompositeLogicalSliceChain : public LogicalSliceChain {
public:
    CompositeLogicalSliceChain() = default;

    ~CompositeLogicalSliceChain() override = default;

    BResult Init(SliceStatus status, uint32_t defaultChainLen) override
    {
        LOG_ERROR("Unsupported operation exception.");
        return BSS_ERR;
    }

    BResult Initialize(
        const LogicalSliceChainRef &logicalSliceChain, int32_t startIndex, int32_t endIndex,
        std::unordered_map<SliceAddressRef, DataSliceRef, SliceAddressHash, SliceAddressEqual> &copiedDataSlice,
        bool deepCopySliceAddress, bool hasFilePage) override
    {
        LOG_ERROR("Unsupported operation exception.");
        return BSS_ERR;
    }

    IOResult Get(const Key &key, Value &value, BlobValueTransformFunc func, BoostNativeMetricPtr &metricPtr) override
    {
        LOG_ERROR("Unsupported operation exception.");
        return IO_ERR;
    }

    uint32_t GetSliceSize() override
    {
        return static_cast<uint32_t>(std::min<uint64_t>(mSliceSize, std::numeric_limits<uint32_t>::max()));
    }

    uint64_t GetCompositeSliceSize() const
    {
        return mSliceSize;
    }

    uint32_t GetSliceChainCapacity() override
    {
        uint32_t sum = 0;
        for (const auto &chain : mSliceChains) {
            sum += chain->GetSliceChainCapacity();
        }
        return sum;
    }

    SliceAddressRef GetSliceAddress(int32_t bucketIndex) override
    {
        LOG_ERROR("Unsupported operation exception.");
        return nullptr;
    }

    void SetSliceAddress(int32_t bucketIndex, const SliceAddressRef &addr) override
    {
        LOG_ERROR("Unsupported operation exception.");
    }

    std::vector<SliceAddressRef> &GetSliceAddresses() override
    {
        LOG_ERROR("Unsupported operation exception.");
        static std::vector<SliceAddressRef> emptyVec;
        return emptyVec;
    }

    void ClearSliceAddresses() override
    {
        LOG_ERROR("Unsupported operation exception.");
    }

    SliceAddressRef CreateSlice(const DataSliceRef &dataSlice, uint64_t readAccessNumber) override
    {
        LOG_ERROR("Unsupported operation exception.");
        return nullptr;
    }

    void ReleaseSliceAddress(SliceAddressRef &sliceAddress) override
    {
        LOG_ERROR("Unsupported operation exception.");
    }

    uint32_t InsertSlice(SliceAddressRef sliceAddress) override
    {
        LOG_ERROR("Unsupported operation exception.");
        return 0;
    }

    void EmplaceSlice(SliceAddressRef sliceAddress) override
    {
        LOG_ERROR("Unsupported operation exception.");
    }

    int32_t GetSliceChainTailIndex() override;

    void SetSliceChainTailIndex(int32_t index) override
    {
        LOG_ERROR("Unsupported operation exception.");
    }

    uint32_t GetBaseSliceIndex() override;

    uint32_t GetFilePageSize() override;

    BResult SetBaseSliceIndex(uint32_t index) override
    {
        LOG_ERROR("Unsupported operation exception.");
        return BSS_ERR;
    }

    uint32_t GetCurrentSliceChainLen() override;

    SliceStatus GetSliceStatus() override
    {
        return SliceStatus::COMPACTING;
    }

    void SetSliceStatus(SliceStatus status) override
    {
        LOG_ERROR("Unsupported operation exception.");
    }

    IteratorRef<SliceAddressRef> SliceIterator() override
    {
        if (mSliceChains.empty()) {
            return nullptr;
        }
        for (const auto &chain : mSliceChains) {
            return chain->SliceIterator();
        }
        return nullptr;
    }

    bool CompareAndSetStatus(SliceStatus expectedPageStatus, SliceStatus targetStatus) override
    {
        return true;
    }

    bool HasFilePage() override;

    void GetFilePages(std::vector<FilePageRef> &currentPages) override;

    void InsertFilePage(FilePageRef filePage) override
    {
        LOG_ERROR("Unsupported operation exception.");
    }

    void InsertFilePageIfEmpty(FilePageRef filePage) override
    {
        LOG_ERROR("Unsupported operation exception.");
    }

    void InsertAllFilePage(std::vector<FilePageRef> filePages) override
    {
        LOG_ERROR("Unsupported operation exception.");
    }

    void SetFilePages(std::vector<FilePageRef> filePages) override
    {
        LOG_ERROR("Unsupported operation exception.");
    }

    void SnapshotMeta(uint64_t snapshotId, const FileOutputViewRef &localOutputView,
                      SnapshotMetaRef &snapshotMeta) override
    {
        LOG_ERROR("Unsupported operation exception.");
    }

    BResult Restore(const FileInputViewRef &reader, uint64_t snapshotId) override
    {
        LOG_ERROR("Unsupported operation exception.");
        return BSS_ERR;
    }

    void RestoreFilePage(LsmStoreRef lsmStore) override
    {
        for (const auto &chain : mSliceChains) {
            chain->RestoreFilePage(lsmStore);
        }
    }

    LogicalSliceChainRef DeepCopy(bool allowMetadataOnly = false) override
    {
        LOG_ERROR("Unsupported operation exception.");
        return nullptr;
    }

    bool IsNone() const override
    {
        return false;
    }

    void NotifyNoneFlag(bool flag) override
    {
        LOG_ERROR("Unsupported operation exception.");
    }

    bool IsEmpty() override
    {
        return mSliceChains.empty();
    }

    std::string ToString() const override
    {
        return "CompositeLogicalSliceChain";
    }

    bool IsSimpleSliceChain() override
    {
        return false;
    }

    uint32_t GetCompositeNum() override
    {
        return mSliceChains.size();
    }

    BResult AddLogicalSliceChain(const LogicalSliceChainRef &sliceChain, bool requireForceCompaction = false)
    {
        if (UNLIKELY(sliceChain == nullptr)) {
            LOG_ERROR("Add logical slice chain failed because sliceChain is nullptr.");
            return BSS_INVALID_PARAM;
        }
        mSliceChains.push_back(sliceChain);
        mSliceSize += static_cast<uint64_t>(sliceChain->GetSliceSize());
        mRequireForceCompaction = mRequireForceCompaction || requireForceCompaction;
        return BSS_OK;
    }

    bool RequireForceCompaction() const
    {
        return mRequireForceCompaction;
    }

    std::vector<LogicalSliceChainRef> GetSliceChains() override
    {
        return mSliceChains;
    }

private:
    std::vector<LogicalSliceChainRef> mSliceChains;
    uint64_t mSliceSize = 0;
    bool mRequireForceCompaction = false;
};
using CompositeLogicalSliceChainRef = std::shared_ptr<CompositeLogicalSliceChain>;

struct SectionsReadContext {
    FilePageRef filePage;
    SectionsReadMetaRef sectionsReadMeta;
};
using SectionsReadContextRef = std::shared_ptr<SectionsReadContext>;

}  // namespace bss
}  // namespace ock
#endif  // BOOST_STATE_STORE_LOGICALSLICECHAIN_H
