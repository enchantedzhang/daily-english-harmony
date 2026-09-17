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

#ifndef BOOST_SS_BUCKETGROUPMANAGER_H
#define BOOST_SS_BUCKETGROUPMANAGER_H

#include <cstdint>

#include "blob_store.h"
#include "bss_metric.h"
#include "iterator"
#include "lsm_store/file/file_store_id.h"
#include "lsm_store/file/file_store_impl.h"
#include "slice_table/bucket_group.h"
#include "slice_table/index/slice_bucket_index.h"
#include "snapshot/snapshot_restore_utils.h"

namespace ock {
namespace bss {
class BucketGroupIterator : public Iterator<BucketGroupRef> {
public:
    BResult Initialize(std::vector<BucketGroupRef> &bucketGroups);

    bool HasNext() override;

    BucketGroupRef Next() override;

private:
    std::vector<BucketGroupRef> mBucketGroups;
    std::vector<BucketGroupRef>::iterator iter;
};
using BucketGroupIteratorRef = std::shared_ptr<BucketGroupIterator>;

class BucketGroupManager {
public:
    class SliceFileStoreIterator : public Iterator<KeyValueRef> {
    public:
        explicit SliceFileStoreIterator(BucketGroupManager *bucketGroupManager, uint16_t stateId)
            : mBucketGroupManager(bucketGroupManager), mStateId(stateId)
        {
        }
        bool HasNext() override;
        KeyValueRef Next() override;

    private:
        void Advance();
        BucketGroupManager *mBucketGroupManager;
        uint32_t mIndex = 0;
        KeyValueIteratorRef mInnerIterator;
        uint16_t mStateId;
    };
    using SliceFileStoreIteratorRef = Ref<SliceFileStoreIterator>;

    ~BucketGroupManager()
    {
        std::vector<BucketGroupRef>().swap(mBucketGroups);
        LOG_INFO("Delete BucketGroupManager success");
    }

    std::shared_ptr<BucketGroupIterator> GetBucketGroups();

    uint32_t GetBucketGroupSize();

    // open db调用，用于调用fileStore的open，暂时不实现
    void Open();
    void Close();
    BResult Initialize(const ConfigRef &config, const SliceBucketIndexRef &sliceIndex,
                       const FileCacheManagerRef &fileCache, uint32_t bucketGroupNum, uint32_t bucketNum,
                       const MemManagerRef &memManager, const StateFilterManagerRef &stateFilterManager);

    KeyValueIteratorRef IteratorFileStoreData(uint16_t stateId);
    std::vector<BucketGroupRef> GetBucketGroupVector();
    void MarkLogicalSliceChainFlushed(const LogicalSliceChainRef &logicalSliceChain, const BucketGroupRef &bucketGroup);
    void SnapshotMeta(const FileOutputViewRef &localOutputView);
    static BResult RestoreMeta(const FileInputViewRef &reader, uint32_t totalBucketNum,
                               std::vector<BucketGroupRangeRef> &bucketGroupRanges);
    std::vector<BucketGroupRangeRef> GetBucketGroupRanges();
    LsmStoreRef GetLsmStoreByBucketIndex(uint32_t bucketIndex);
    uint32_t ComputeBucketGroupIndex(uint32_t bucketIndex);
    BResult RestoreFileStore(const std::vector<SliceTableRestoreMetaRef> &sliceTableRestoreMetaList,
                             std::unordered_map<std::string, std::string> &lazyPathMapping,
                             std::unordered_map<std::string, uint32_t> &restorePathFileIdMap, bool isLazyDownload);

    /**
     * For testing, force clean current version.
     */
    void ForceCleanCurrentVersion()
    {
        for (const auto &item : mBucketGroups) {
            auto fileStore = item->GetLsmStore();
            fileStore->ExitCompaction();
            fileStore->CleanVersion();
        }
    }

    inline FileCacheManagerRef GetFileCache()
    {
        return mFileCache;
    }

    inline SliceBucketIndexRef &GetSliceBucketIndex()
    {
        return mSliceBucketIndex;
    }

    inline void RegisterMetric(BoostNativeMetricPtr metricPtr)
    {
        for (const auto &item : mBucketGroups) {
            item->RegisterMetric(metricPtr);
        }
    }

    void RegisterTombstoneService(BlobStoreRef &blobStore, const KeyGroupUtilRef &keyGroupUtil)
    {
        for (const auto &item : mBucketGroups) {
            auto lsmStore = item->GetLsmStore();
            auto name = lsmStore->GetName();
            auto tombstoneService = blobStore->CreateTombstoneService(name, keyGroupUtil);
            lsmStore->RegisterTombstoneService(tombstoneService);
        }
    }

private:
    BResult AssignBucketToBucketGroup(const std::shared_ptr<Config> &config, const FileCacheManagerRef &fileCache,
                                      const StateFilterManagerRef &stateFilterManager);

    static void HandleFileStoreOverLapping(const std::vector<SliceTableRestoreMetaRef> &sliceTableRestoreMetaList,
                                           FileStoreIDRef fileStoreID,
                                           std::vector<std::pair<FileInputViewRef, uint64_t>> &metaList);

    uint32_t mBucketNum = 0;
    SliceBucketIndexRef mSliceBucketIndex = nullptr;
    std::vector<BucketGroupRef> mBucketGroups;
    MemManagerRef mMemManager;
    FileCacheManagerRef mFileCache;
};
using BucketGroupManagerRef = std::shared_ptr<BucketGroupManager>;

}  // namespace bss
}  // namespace ock
#endif  // BOOST_SS_BUCKETGROUPMANAGER_H
