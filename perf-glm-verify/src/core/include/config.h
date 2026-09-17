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

#ifndef BOOST_SS_CONFIG_H
#define BOOST_SS_CONFIG_H

#include <cstdint>
#include <memory>
#include <string>
#include <valarray>
#include <vector>

#include "bss_types.h"
#include "compress_algo.h"

namespace ock {
namespace bss {

class Config {
public:
    Config() = default;

    Config(uint32_t startGroup, uint32_t endGroup, uint32_t maxParallelism)
    {
        Init(startGroup, endGroup, maxParallelism);
    }

    void Init(uint32_t startGroup, uint32_t endGroup, uint32_t maxParallelism);

    // 获取最大并发度, 在openDB的时候指定.
    inline uint32_t GetMaxParallelism() const
    {
        return mMaxParallelism;
    }

    // 设置每个Bucket的slice标准大小.
    inline void SetSliceStandardSizePerBucket(uint32_t size)
    {
        mSliceStandardSizePerBucket = size;
    }

    // 获取每个Bucket的slice标准大小, 默认值为64KB.
    inline uint32_t GetSliceStandardSizePerBucket() const
    {
        return mSliceStandardSizePerBucket;
    }

    // 获取每个BucketGroup的有效负载, 默认值为256MB.
    inline uint64_t GetPayloadPerBucketGroup() const
    {
        return NO_256 * NO_1024 * NO_1024;
    }

    // 获取单个Slice的最大大小, 默认值为16KB.
    inline uint64_t GetSliceMaxSize() const
    {
        return NO_16 * NO_1024 * NO_1024;
    }

    // 获取LogicSliceChain的默认长度, 默认值为3.
    inline uint32_t GetLogicTableDefaultChainLen() const
    {
        return NO_3;
    }

    // 获取全部DB大小, 默认值为2G.
    inline uint64_t GetTotalDBSize() const
    {
        return mTotalDBSize;
    }

    // 获取LsmStore的内存占比, 默认值为0.2.
    inline float GetFileStoreMemoryFraction() const
    {
        return mFileMemoryRatio;
    }

    // 设置LsmStore的内存占比.
    inline void SetFileMemoryRatio(float fileMemoryRatio)
    {
        mFileMemoryRatio = fileMemoryRatio;
    }

    // 设置SliceTable的淘汰水位.
    inline void SetEvictMinSize(uint32_t size)
    {
        mEvictMinSize = size;
    }

    // 获取SliceTable的最低淘汰容量值, 默认值为32MB.
    inline uint32_t GetEvictMinSize() const
    {
        return mEvictMinSize;
    }

    // 获取LsmStore的Level层级数, 默认值为5.
    inline uint32_t GetFileStoreNumLevels() const
    {
        return NO_5;
    }

    // 获取LsmStore的Level层的基本容量, 默认值为256MB.
    inline uint64_t GetFileStoreMaxBaseLevelBytes() const
    {
        return mMaxBaseLevelBytes;
    }

    // 设置LsmStore的Level层的基本容量.
    inline void SetFileStoreMaxBaseLevelBytes(uint64_t size)
    {
        mMaxBaseLevelBytes = size;
    }

    // 获取LsmStore compaction的最大大小, 默认值为25倍FileSize.
    inline uint64_t GetFileStoreMaxCompactionSize() const
    {
        // 默认使用25倍File size.
        return NO_25 * GetFileBaseSize();
    }

    // 获取LsmStore单个文件的基本大小, 默认值为64MB.
    inline uint64_t GetFileBaseSize() const
    {
        return mFileBaseSize;
    }

    // 获取BlobStore单个文件的基本大小，默认值为256MB.
    inline uint64_t GetBlobFileSize() const
    {
        return mBlobFileSize;
    }

    // 设置BlobStore单个文件的基本大小
    inline void SetBlobFileSize(uint64_t blobFileSize)
    {
        mBlobFileSize = blobFileSize;
    }

    // 设置LsmStore单个文件的基本大小.
    inline void SetFileStoreFileBaseSize(uint64_t size)
    {
        mFileBaseSize = size;
    }

    // 获取LsmStore Level0的compaction触发文件数, 默认值为8.
    inline uint32_t GetFileStoreL0NumTrigger() const
    {
        return mFileStoreL0NumTrigger;
    }

    // 设置LsmStore Level0的compaction触发文件数.
    inline void SetFileStoreL0NumTrigger(uint32_t trigger)
    {
        mFileStoreL0NumTrigger = trigger;
    }

    // 获取LsmStore的倍数, 默认值为10.
    inline uint32_t GetFileStoreMultiple() const
    {
        return mFileStoreMultiple;
    }

    // 设置LsmStore的倍数.
    inline void SetFileStoreMultiple(uint32_t multiple)
    {
        mFileStoreMultiple = multiple;
    }

    // 获取LsmStore的最大父类重叠大小, 默认值为10倍FileSize.
    inline uint64_t GetFileStoreMaxGrandParentOverlapBytes() const
    {
        return NO_10 * GetFileBaseSize();
    }

    // 获取SliceTable的compaction门槛, 默认值为10.
    inline uint32_t GetInMemoryCompactionThreshold() const
    {
        return NO_10;
    }

    // 获取当前DB的起始Group, 在openDB的时候指定.
    inline uint32_t GetStartGroup() const
    {
        return mStartGroup;
    }

    // 获取当前DB的终止Group, 在openDB的时候指定.
    inline uint32_t GetEndGroup() const
    {
        return mEndGroup;
    }

    // 获取LsmStore中的单个DataBlock的大小, 默认值为16KB.
    inline uint32_t GetDataBlockSize() const
    {
        return NO_16 * NO_1024;
    }

    // 获取LsmStore中的Block的类型, 默认值为1.
    inline uint8_t GetBlockType() const
    {
        return 1;
    }

    // 获取LsmStore中Hash Index的负载率, 默认值为0.7F.
    inline float GetHashIndexLoadRatio() const
    {
        float loadRatio = 0.75F;
        return loadRatio;
    }

    // 获取LsmStore中IndexBlock的类型, 默认值为3.
    inline uint8_t GetIndexBlockType() const
    {
        return NO_3;
    }

    // 获取LsmStore中已分区Index的最大层数, 默认值为1.
    inline uint32_t GetPartitionedIndexMaxLevel() const
    {
        return NO_1;
    }

    // 获取LsmStore中已分区Index的Block大小, 默认值为64KB.
    inline uint32_t GetPartitionedIndexBlockSize() const
    {
        return NO_64 * NO_1024;
    }

    // 获取SubTask的最大并发数.
    inline uint32_t GetMaxNumberOfParallelSubtasks() const
    {
        return mMaxParallelism;
    }

    // 获取Table Factory的类型, 默认值为1, 仅支持HASH类型.
    inline uint8_t GetFileFactoryType() const
    {
        return 1;
    }

    // 获取SliceTable的淘汰水位, 默认值为0.8.
    inline float GetTotalMemHighMarkRatio() const
    {
        return mTotalMemHighMarkRatio;
    }

    // 设置SliceTable的淘汰水位.
    inline void SetTotalMemHighMarkRatio(float memoryRatio)
    {
        mTotalMemHighMarkRatio = memoryRatio;
    }

    // 获取FreshTable单个Memory segment的大小, 默认值为0.
    inline uint32_t GetMemorySegmentSize() const
    {
        return mMemorySegmentSize;
    }

    // 设置FreshTable的Memory segment大小.
    inline void SetMemorySegmentSize(uint32_t segmentSize)
    {
        mMemorySegmentSize = segmentSize;
    }

    // 设置全部DB的大小.
    inline void SetTotalDBSize(uint64_t totalDBSize)
    {
        mTotalDBSize = totalDBSize;
    }

    // 设置Heap类型的内存可用大小.
    inline void SetHeapAvailableSize(uint64_t overHeapSize)
    {
        mHeapAvailableSize = overHeapSize;
    }

    // 获取Heap类型的内存可用大小, 默认值为2GB.
    inline uint64_t GetHeapAvailableSize() const
    {
        return mHeapAvailableSize;
    }

    // 设置Task slot标记.
    inline void SetTaskSlotFlag(uint32_t taskSlotFlag)
    {
        mTaskSlotFlag = taskSlotFlag;
    }

    // 获取Task slot标记.
    inline uint32_t GetTaskSlotFlag() const
    {
        return mTaskSlotFlag;
    }

    inline void SetPeakFilterElemNum(int32_t num)
    {
        mPeakFilterElemNum = num;
    }

    inline uint32_t PeakFilterElemNum() const
    {
        return mPeakFilterElemNum;
    }

    // 设置本地checkpoint路径, 规定传入localPath的时候应该是/a/b的形式, 不能是/a/b/ , 否则递归创建会报错.
    inline void SetLocalPath(const std::string &localPath)
    {
        mLocalPath = localPath;
    }

    // 设置checkpoint的backend uid.
    inline void SetBackendUID(const std::string &backendUID)
    {
        mBackendUID = backendUID;
    }

    // 获取checkpoint本地路径.
    inline std::string &GetLocalPath()
    {
        return mLocalPath;
    }

    // 获取checkpoint远端路径.
    inline std::string &GetRemotePath()
    {
        return mRemotePath;
    }

    // 获取backend uid.
    inline std::string &GetBackendUID()
    {
        return mBackendUID;
    }

    // 获取LsmStore的compaction开关, 默认值为0.
    inline int32_t GetLsmStoreCompactionSwitch() const
    {
        return mLsmStoreCompactionSwitch;
    }

    // 设置LsmStore的compaction开关.
    inline void SetLsmStoreCompactionSwitch(int32_t uSwitch)
    {
        mLsmStoreCompactionSwitch = uSwitch;
    }

    // 获取ttlFilter开关, 默认值为false.
    inline bool GetTtlFilterSwitch() const
    {
        return mTtlFilterSwitch;
    }

    // 设置ttlFilter开关.
    inline void SetTtlFilterSwitch(bool ttlFilterSwitch)
    {
        mTtlFilterSwitch = ttlFilterSwitch;
    }

    inline bool GetZeroCopySwitch() const
    {
        return mZeroCopySwitch;
    }

    inline void SetZeroCopySwitch(bool zeroCopySwitch)
    {
        mZeroCopySwitch = zeroCopySwitch;
    }

    // 获取cacheIndexAndFilter开关, 默认值为true.
    inline bool GetCacheIndexAndFilterSwitch() const
    {
        return mCacheIndexAndFilterSwitch;
    }

    // 设置cacheIndexAndFilter开关.
    inline void SetCacheIndexAndFilterSwitch(bool cacheIndexAndFilterSwitch)
    {
        mCacheIndexAndFilterSwitch = cacheIndexAndFilterSwitch;
    }

    // 获取indexAndFilter的独立缓存在总缓存的比例, 默认值为0.0f.
    inline float GetCacheIndexAndFilterRatio() const
    {
        return mCacheIndexAndFilterRatio;
    }

    // 设置indexAndFilter的独立缓存在总缓存的比例, 默认值为0.0f.
    inline void SetCacheIndexAndFilterRatio(float cacheIndexAndFilterRatio)
    {
        mCacheIndexAndFilterRatio = cacheIndexAndFilterRatio;
    }

    // 获取lsm level前n层以后的默认压缩策略FileStoreCompressionPolicy, 默认值为level2之后压缩.
    inline CompressAlgo GetLsmStoreCompressionPolicy() const
    {
        return mLsmStoreCompressionPolicy;
    }

    // 设置lsm level前n层以后的默认压缩策略FileStoreCompressionPolicy
    inline void SetLsmStoreCompressionPolicy(const std::string &lsmStoreCompressionPolicy)
    {
        mLsmStoreCompressionPolicy = CompressAlgoUtil::CompressAlgoTransform(lsmStoreCompressionPolicy);
    }

    inline CompressAlgo GetFreshTableSnapshotCompressionPolicy() const
    {
        return mFreshTableSnapshotCompressionPolicy;
    }

    inline void SetFreshTableSnapshotCompressionPolicy(const std::string &freshTableSnapshotCompressionPolicy)
    {
        mFreshTableSnapshotCompressionPolicy =
            CompressAlgoUtil::CompressAlgoTransform(freshTableSnapshotCompressionPolicy);
    }

    inline CompressAlgo GetSliceTableSnapshotCompressionPolicy() const
    {
        return mSliceTableSnapshotCompressionPolicy;
    }

    inline void SetSliceTableSnapshotCompressionPolicy(const std::string &sliceTableSnapshotCompressionPolicy)
    {
        mSliceTableSnapshotCompressionPolicy =
            CompressAlgoUtil::CompressAlgoTransform(sliceTableSnapshotCompressionPolicy);
    }

    // 获取lsm level前n层的默认压缩策略FileStoreCompressionPolicy, 默认值为level2压缩.
    inline std::vector<CompressAlgo> GetCompressionLevelPolicy() const
    {
        return mCompressionLevelPolicy;
    }

    // 设置lsm level前n层的默认压缩策略FileStoreCompressionPolicy
    void SetCompressionLevelPolicy(const std::vector<std::string> &compressionLevelPolicy);

    inline void SetBackupPath(const std::string &SnapshotBackupPath)
    {
        mBackupPath = SnapshotBackupPath;
    }

    inline const std::string &GetBackupPath() const
    {
        return mBackupPath;
    }

    inline void SetEnableLocalRecovery(bool enableLocalRecovery)
    {
        mEnableLocalRecovery = enableLocalRecovery;
    }

    inline bool GetEnableLocalRecovery() const
    {
        return mEnableLocalRecovery;
    }

    inline void SetIsNewJob(bool isNewJob)
    {
        mIsNewJob = isNewJob;
    }

    inline bool GetIsNewJob() const
    {
        return mIsNewJob;
    }

    inline void SetMaxBlobNumInMemCache(uint32_t maxBlobNumInMemCache)
    {
        mMaxBlobNumInMemCache = maxBlobNumInMemCache;
    }

    inline void SetTombstoneDataBlockSize(uint32_t tombstoneDataBlockSize)
    {
        mTombstoneDataBlockSize = tombstoneDataBlockSize;
    }

    inline void SetTombstoneFileSize(uint32_t tombstoneFileSize)
    {
        mTombstoneFileSize = tombstoneFileSize;
    }

    inline uint64_t GetTombstoneLevelMaxSize(uint32_t levelId) const
    {
        if (levelId < 1) {
            return IO_SIZE_32M;
        }
        return IO_SIZE_32M * (uint64_t)std::pow(NO_10, levelId - 1);
    }

    inline uint32_t GetTombstoneCompactionGroupSize() const
    {
        return NO_4;
    }

    inline void SetEnableKVSeparate(bool IsKVSeparate)
    {
        mIsKVSeparate = IsKVSeparate;
    }

    inline bool GetEnableKVSeparate() const
    {
        return mIsKVSeparate;
    }

    inline void SetBlobValueSizeThreshold(uint32_t blobValueSizeThreshold)
    {
        mBlobValueSizeThreshold = blobValueSizeThreshold;
    }

    inline uint32_t GetBlobValueSizeThreshold() const
    {
        return mBlobValueSizeThreshold;
    }

    inline void SetBlobDefaultBlockSize(uint32_t defaultBlockSize)
    {
        mBlobDefaultBlockSize = defaultBlockSize;
    }

    inline uint32_t GetBlobDefaultBlockSize() const
    {
        return mBlobDefaultBlockSize;
    }

    inline double GetBlobFileCompactionMinDeleteRatio() const
    {
        return (mBlobMaxSpaceAmplificationRatio - 1.0) / mBlobMaxSpaceAmplificationRatio;
    }

    inline double GetBlobMaxSpaceAmplificationRatio() const
    {
        return mBlobMaxSpaceAmplificationRatio;
    }

    inline uint64_t GetBlobMinCompactionThreshold() const
    {
        return mBlobMinCompactionThreshold;
    }

    inline uint64_t GetBlobFileRetainTimeInMill() const
    {
        return mBlobFileRetainTimeInMill;
    }

    inline bool GetEnableTombstone() const
    {
        return mEnableTombstone;
    }

    uint64_t mTotalDBSize = IO_SIZE_2G;
    uint32_t mMemorySegmentSize = 0;
    uint64_t mHeapAvailableSize = IO_SIZE_2G;
    uint32_t mTaskSlotFlag = 0;
    int32_t mPeakFilterElemNum = 0;

    uint32_t mStartGroup = 0;
    uint32_t mEndGroup = 0;
    uint32_t mMaxParallelism = 0;
    uint32_t mEvictMinSize = IO_SIZE_32M;

    uint32_t mSliceStandardSizePerBucket = IO_SIZE_64K;  // 默认bucket大小为64KB
    float mTotalMemHighMarkRatio = 0.8;                  // slice淘汰水位
    float mFileMemoryRatio = 0.2;                        // file内存占总内存比例
    std::string mLocalPath;
    std::string mRemotePath;
    std::string mBackendUID;
    std::string mBackupPath;
    bool mEnableLocalRecovery;
    bool mIsNewJob = true;

    int32_t mLsmStoreCompactionSwitch = 1;
    bool mTtlFilterSwitch = false;
    bool mZeroCopySwitch = false;
    bool mIsKVSeparate = false;
    uint32_t mBlobDefaultBlockSize = IO_SIZE_16K;
    uint32_t mBlobValueSizeThreshold = 200;
    bool mCacheIndexAndFilterSwitch = true;
    float mCacheIndexAndFilterRatio = 0.0f;
    uint64_t mFileBaseSize = IO_SIZE_64M;
    uint64_t mBlobFileSize = IO_SIZE_128M;
    uint64_t mMaxBaseLevelBytes = IO_SIZE_256M;
    uint32_t mFileStoreMultiple = NO_10;
    uint32_t mFileStoreL0NumTrigger = NO_8;
    std::vector<CompressAlgo> mCompressionLevelPolicy = { CompressAlgo::NONE, CompressAlgo::NONE, CompressAlgo::LZ4 };
    CompressAlgo mLsmStoreCompressionPolicy = CompressAlgo::LZ4;
    CompressAlgo mFreshTableSnapshotCompressionPolicy = CompressAlgo::NONE;
    CompressAlgo mSliceTableSnapshotCompressionPolicy = CompressAlgo::NONE;
    uint32_t mMaxBlobNumInMemCache = TOMBSTONE_MEMTABLE_SIZE;
    uint32_t mTombstoneDataBlockSize = IO_SIZE_64K;
    uint32_t mTombstoneFileSize = IO_SIZE_64M;
    uint32_t mTombstoneLevel0CompactionFileNum = 4;
    uint64_t mBlobMinCompactionThreshold = IO_SIZE_1G;
    double mBlobMaxSpaceAmplificationRatio = 3.0;
    uint64_t mBlobFileRetainTimeInMill = 120000;
    bool mEnableTombstone = true;
};
using ConfigRef = std::shared_ptr<Config>;
}  // namespace bss
}  // namespace ock

#endif
