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

#ifndef BOOST_SS_BSS_METRIC_H
#define BOOST_SS_BSS_METRIC_H

#include <algorithm>
#include <unordered_map>
#include <vector>

#include "bss_err.h"
#include "bss_log.h"
#include "bss_types.h"

namespace ock {
namespace bss {
// type num = (METRIC_TYPE_BUTT - METRIC_TYPE_HEAD - 1)
enum class MetricType : int32_t {
    // [0, 100) MemoryManager
    MEMORY_MAX_FRESH = 0,
    MEMORY_MAX_SLICE = 1,
    MEMORY_MAX_FILE = 2,
    MEMORY_MAX_SNAPSHOT = 3,
    MEMORY_MAX_DB = 4,
    MEMORY_MAX_BORROW_HEAP = 5,
    MEMORY_USED_FRESH = 6,
    MEMORY_USED_SLICE = 7,
    MEMORY_USED_FILE = 8,
    MEMORY_USED_SNAPSHOT = 9,
    MEMORY_USED_DB = 10,
    MEMORY_USED_BORROW_HEAP = 11,

    // [100, 200) FreshTable
    FRESH_HIT_COUNT = 100,
    FRESH_MISS_COUNT = 101,
    FRESH_RECORD_COUNT = 102,
    FRESH_FLUSHING_RECORD_COUNT = 103,
    FRESH_FLUSHING_SEGMENT_COUNT = 104,
    FRESH_FLUSHED_RECORD_COUNT = 105,
    FRESH_FLUSHED_SEGMENT_COUNT = 106,
    FRESH_SEGMENT_CREATE_FAIL_COUNT = 107,
    FRESH_FLUSH_COUNT = 108,
    FRESH_BINARY_KEY_SIZE = 109,
    FRESH_BINARY_VALUE_SIZE = 110,
    FRESH_BINARY_MAP_NODE_SIZE = 111,
    FRESH_WASTED_SIZE = 112,

    // [200, 300) SliceTable
    SLICE_HIT_COUNT = 200,
    SLICE_MISS_COUNT = 201,
    SLICE_READ_COUNT = 202,
    SLICE_READ_AVG_SIZE = 203,
    SLICE_EVICT_SIZE = 204,
    SLICE_EVICT_WAITING_COUNT = 205,
    SLICE_COMPACTION_COUNT = 206,
    SLICE_COMPACTION_SLICE_COUNT = 207,
    SLICE_COMPACTION_AVG_SLICE_COUNT = 208,
    SLICE_CHAIN_AVG_SIZE = 209,
    SLICE_AVG_SIZE = 210,

    // [300, 400) FileCache
    INDEX_BLOCK_HIT_COUNT = 301,
    INDEX_BLOCK_HIT_SIZE = 302,
    INDEX_BLOCK_MISS_COUNT = 303,
    INDEX_BLOCK_MISS_SIZE = 304,
    INDEX_BLOCK_CACHE_COUNT = 305,
    INDEX_BLOCK_CACHE_SIZE = 306,
    DATA_BLOCK_HIT_COUNT = 307,
    DATA_BLOCK_HIT_SIZE = 308,
    DATA_BLOCK_MISS_COUNT = 309,
    DATA_BLOCK_MISS_SIZE = 310,
    DATA_BLOCK_CACHE_COUNT = 311,
    DATA_BLOCK_CACHE_SIZE = 312,
    FILTER_HIT_COUNT = 313,
    FILTER_HIT_SIZE = 314,
    FILTER_MISS_COUNT = 315,
    FILTER_MISS_SIZE = 316,
    FILTER_CACHE_COUNT = 317,
    FILTER_CACHE_SIZE = 318,
    FILTER_SUCCESS_COUNT = 319,
    FILTER_EXIST_SUCCESS_COUNT = 320,
    FILTER_EXIST_FAIL_COUNT = 321,

    // [400, 600) FileStore
    LSM_FLUSH_COUNT = 400,
    LSM_FLUSH_SIZE = 401,
    LSM_COMPACTION_COUNT = 402,
    LSM_HIT_COUNT = 403,
    LSM_MISS_COUNT = 404,
    LEVEL0_HIT_COUNT = 405,
    LEVEL0_MISS_COUNT = 406,
    LEVEL1_HIT_COUNT = 407,
    LEVEL1_MISS_COUNT = 408,
    LEVEL2_HIT_COUNT = 409,
    LEVEL2_MISS_COUNT = 410,
    ABOVE_LEVEL2_HIT_COUNT = 411,
    ABOVE_LEVEL2_MISS_COUNT = 412,
    LEVEL0_FILE_SIZE = 413,
    LEVEL1_FILE_SIZE = 414,
    LEVEL2_FILE_SIZE = 415,
    LEVEL3_FILE_SIZE = 416,
    ABOVE_LEVEL3_FILE_SIZE = 417,
    LSM_FILE_SIZE = 418,
    LSM_COMPACTION_READ_SIZE = 419,
    LSM_COMPACTION_WRITE_SIZE = 420,
    LEVEL0_COMPACTION_RATE = 421,
    LEVEL1_COMPACTION_RATE = 422,
    LEVEL2_COMPACTION_RATE = 423,
    LEVEL3_COMPACTION_RATE = 424,
    LSM_COMPACTION_RATE = 425,
    LSM_FILE_COUNT = 439,

    // [600, 700) restore
    RESTORE_LAZY_DOWNLOAD_TIME = 600
};

constexpr size_t METRIC_NUM = 104;

enum class LayerType : uint8_t {
    MEMORY_MANAGER = 0,
    FRESH_TABLE = 1,
    SLICE_TABLE = 2,
    LSM_STORE = 3,
    LSM_CACHE = 4,
    RESTORE = 5,
    LAYER_TYPE_BUTT = 6
};

using IOResult = int8_t;

enum SliceReadResult : int8_t { NOT_FOUND = -2, SLICE_FOUND_DELETE = -1, IO_ERR = 0, SLICE_FOUND = 1, LSM_FOUND = 2 };

class BoostNativeMetric {
public:
    BoostNativeMetric(uint32_t switchMap)
    {
        for (uint8_t i = 0; i < static_cast<uint8_t>(LayerType::LAYER_TYPE_BUTT); i++) {
            mMetricLayers.emplace(static_cast<LayerType>(i), GetFlag(i, switchMap));
        }
        mMetrics.reserve(METRIC_NUM);
    }

    static inline bool GetFlag(uint8_t pos, uint32_t flags)
    {
        if (UNLIKELY(pos >= NO_31)) {
            LOG_ERROR("Failed to get flag, pos out of range.");
            return false;
        }
        return (flags & (1 << pos)) != 0;
    }

    BResult Init()
    {
        if (IsFreshMetricEnabled()) {
            InitFreshTableMetric();
        }
        if (IsSliceMetricEnabled()) {
            InitSliceTableMetric();
        }
        if (IsFileCacheMetricEnabled()) {
            InitFileCacheMetric();
        }
        if (IsMemoryMetricEnabled()) {
            InitMemoryManagerMetric();
        }
        if (IsFileStoreMetricEnabled()) {
            InitLsmMetric();
        }
        if (IsRestoreMetricEnabled()) {
            mMetrics.emplace(MetricType::RESTORE_LAZY_DOWNLOAD_TIME,
                             [this]() -> uint64_t { return mLazyDownloadTime; });
        }
        return BSS_OK;
    }

    void InitFreshTableMetric()
    {
        mMetrics.emplace(MetricType::FRESH_HIT_COUNT, [this]() -> uint64_t { return mFreshHitCount; });
        mMetrics.emplace(MetricType::FRESH_MISS_COUNT, [this]() -> uint64_t { return mFreshMissCount; });
        mMetrics.emplace(MetricType::FRESH_RECORD_COUNT, [this]() -> uint64_t {
            if (mFreshRecordCount == nullptr) {
                return 0;
            }
            return mFreshRecordCount();
        });
        mMetrics.emplace(MetricType::FRESH_FLUSHING_RECORD_COUNT, [this]() -> uint64_t {
            if (mFreshFlushingRecordCount == nullptr) {
                return 0;
            }
            return mFreshFlushingRecordCount();
        });
        mMetrics.emplace(MetricType::FRESH_FLUSHING_SEGMENT_COUNT, [this]() -> uint64_t {
            if (mFreshFlushingSegmentCount == nullptr) {
                return 0;
            }
            return mFreshFlushingSegmentCount();
        });
        mMetrics.emplace(MetricType::FRESH_FLUSHED_RECORD_COUNT,
                         [this]() -> uint64_t { return mFreshFlushedRecordCount; });
        mMetrics.emplace(MetricType::FRESH_FLUSHED_SEGMENT_COUNT,
                         [this]() -> uint64_t { return mFreshFlushedSegmentCount; });
        mMetrics.emplace(MetricType::FRESH_SEGMENT_CREATE_FAIL_COUNT,
                         [this]() -> uint64_t { return mFreshSegmentCreateFailCount; });
        mMetrics.emplace(MetricType::FRESH_FLUSH_COUNT, [this]() -> uint64_t { return mFreshFlushCount; });
        mMetrics.emplace(MetricType::FRESH_BINARY_KEY_SIZE, [this]() -> uint64_t { return mFreshBinaryKeySize; });
        mMetrics.emplace(MetricType::FRESH_BINARY_VALUE_SIZE, [this]() -> uint64_t { return mFreshBinaryValueSize; });
        mMetrics.emplace(MetricType::FRESH_BINARY_MAP_NODE_SIZE,
                         [this]() -> uint64_t { return mFreshBinaryMapNodeSize; });
        mMetrics.emplace(MetricType::FRESH_WASTED_SIZE, [this]() -> uint64_t { return mFreshWastedSize; });
    }

    void InitSliceTableMetric()
    {
        mMetrics.emplace(MetricType::SLICE_HIT_COUNT, [this]() -> uint64_t { return mSliceHitCount; });
        mMetrics.emplace(MetricType::SLICE_MISS_COUNT, [this]() -> uint64_t { return mSliceMissCount; });
        mMetrics.emplace(MetricType::SLICE_READ_COUNT, [this]() -> uint64_t { return mSliceReadCount; });
        mMetrics.emplace(MetricType::SLICE_READ_AVG_SIZE, [this]() -> uint64_t {
            if (mSliceReadCount == 0) {
                return 0;
            }
            // 向上取整
            return (mSliceReadSize + mSliceReadCount - 1) / mSliceReadCount;
        });
        mMetrics.emplace(MetricType::SLICE_EVICT_SIZE, [this]() -> uint64_t { return mSliceEvictSize; });
        mMetrics.emplace(MetricType::SLICE_EVICT_WAITING_COUNT, [this]() -> uint64_t {
            if (mSliceEvictWaitingCount == nullptr) {
                return 0;
            }
            return mSliceEvictWaitingCount();
        });
        mMetrics.emplace(MetricType::SLICE_COMPACTION_COUNT, [this]() -> uint64_t { return mSliceCompactionCount; });
        mMetrics.emplace(MetricType::SLICE_COMPACTION_SLICE_COUNT,
                         [this]() -> uint64_t { return mSliceCompactionSliceCount; });
        mMetrics.emplace(MetricType::SLICE_COMPACTION_AVG_SLICE_COUNT, [this]() -> uint64_t {
            if (mSliceCompactionCount == 0) {
                return 0;
            }
            // 向上取整
            return (mSliceCompactionSliceCount + mSliceCompactionCount - 1) / mSliceCompactionCount;
        });
        mMetrics.emplace(MetricType::SLICE_CHAIN_AVG_SIZE, [this]() -> uint64_t {
            if (mSliceChainAvgSize == nullptr) {
                return 0;
            }
            return mSliceChainAvgSize();
        });
        mMetrics.emplace(MetricType::SLICE_AVG_SIZE, [this]() -> uint64_t {
            if (mSliceAvgSize == nullptr) {
                return 0;
            }
            return mSliceAvgSize();
        });
    }

    void InitMemoryManagerMetric()
    {
        mMetrics.emplace(MetricType::MEMORY_MAX_FRESH, [this]() -> uint64_t { return mMemoryFreshMax; });
        mMetrics.emplace(MetricType::MEMORY_MAX_SLICE, [this]() -> uint64_t { return mMemorySliceMax; });
        mMetrics.emplace(MetricType::MEMORY_MAX_FILE, [this]() -> uint64_t { return mMemoryFileMax; });
        mMetrics.emplace(MetricType::MEMORY_MAX_SNAPSHOT, [this]() -> uint64_t { return mMemorySnapshotMax; });
        mMetrics.emplace(MetricType::MEMORY_MAX_DB, [this]() -> uint64_t { return mMemoryTotalMax; });
        mMetrics.emplace(MetricType::MEMORY_MAX_BORROW_HEAP, std::bind(&BoostNativeMetric::GetMemoryHeapMax, this));
        mMetrics.emplace(MetricType::MEMORY_USED_FRESH, std::bind(&BoostNativeMetric::GetMemoryFreshUsed, this));
        mMetrics.emplace(MetricType::MEMORY_USED_SLICE, std::bind(&BoostNativeMetric::GetMemorySliceUsed, this));
        mMetrics.emplace(MetricType::MEMORY_USED_FILE, std::bind(&BoostNativeMetric::GetMemoryFileUsed, this));
        mMetrics.emplace(MetricType::MEMORY_USED_SNAPSHOT, std::bind(&BoostNativeMetric::GetMemorySnapshotUsed, this));
        mMetrics.emplace(MetricType::MEMORY_USED_DB, std::bind(&BoostNativeMetric::GetMemoryTotalUsed, this));
        mMetrics.emplace(MetricType::MEMORY_USED_BORROW_HEAP, std::bind(&BoostNativeMetric::GetMemoryHeapUsed, this));
    }

    void InitFileCacheMetric()
    {
        for (uint8_t i = 0; i < static_cast<uint8_t>(BlockType::BUTT); ++i) {
            mCacheCount[i] = 0;
            mCacheSize[i] = 0;
        }
        mMetrics.emplace(MetricType::INDEX_BLOCK_HIT_COUNT, [this]() -> uint64_t { return mIndexBlockHitCount; });
        mMetrics.emplace(MetricType::INDEX_BLOCK_HIT_SIZE, [this]() -> uint64_t { return mIndexBlockHitSize; });
        mMetrics.emplace(MetricType::INDEX_BLOCK_MISS_COUNT, [this]() -> uint64_t { return mIndexBlockMissCount; });
        mMetrics.emplace(MetricType::INDEX_BLOCK_MISS_SIZE, [this]() -> uint64_t { return mIndexBlockMissSize; });
        mMetrics.emplace(MetricType::INDEX_BLOCK_CACHE_COUNT,
                         [this]() -> uint64_t { return mCacheCount[static_cast<uint8_t>(BlockType::INDEX)]; });
        mMetrics.emplace(MetricType::INDEX_BLOCK_CACHE_SIZE,
                         [this]() -> uint64_t { return mCacheSize[static_cast<uint8_t>(BlockType::INDEX)]; });
        mMetrics.emplace(MetricType::DATA_BLOCK_HIT_COUNT, [this]() -> uint64_t { return mDataBlockHitCount; });
        mMetrics.emplace(MetricType::DATA_BLOCK_HIT_SIZE, [this]() -> uint64_t { return mDataBlockHitSize; });
        mMetrics.emplace(MetricType::DATA_BLOCK_MISS_COUNT, [this]() -> uint64_t { return mDataBlockMissCount; });
        mMetrics.emplace(MetricType::DATA_BLOCK_MISS_SIZE, [this]() -> uint64_t { return mDataBlockMissSize; });
        mMetrics.emplace(MetricType::DATA_BLOCK_CACHE_COUNT,
                         [this]() -> uint64_t { return mCacheCount[static_cast<uint8_t>(BlockType::DATA)]; });
        mMetrics.emplace(MetricType::DATA_BLOCK_CACHE_SIZE,
                         [this]() -> uint64_t { return mCacheSize[static_cast<uint8_t>(BlockType::DATA)]; });
        mMetrics.emplace(MetricType::FILTER_HIT_COUNT, [this]() -> uint64_t { return mFilterHitCount; });
        mMetrics.emplace(MetricType::FILTER_HIT_SIZE, [this]() -> uint64_t { return mFilterHitSize; });
        mMetrics.emplace(MetricType::FILTER_MISS_COUNT, [this]() -> uint64_t { return mFilterMissCount; });
        mMetrics.emplace(MetricType::FILTER_MISS_SIZE, [this]() -> uint64_t { return mFilterMissSize; });
        mMetrics.emplace(MetricType::FILTER_CACHE_COUNT,
                         [this]() -> uint64_t { return mCacheCount[static_cast<uint8_t>(BlockType::FILTER)]; });
        mMetrics.emplace(MetricType::FILTER_CACHE_SIZE,
                         [this]() -> uint64_t { return mCacheSize[static_cast<uint8_t>(BlockType::FILTER)]; });
        mMetrics.emplace(MetricType::FILTER_SUCCESS_COUNT, [this]() -> uint64_t { return mFilterSuccessCount; });
        mMetrics.emplace(MetricType::FILTER_EXIST_SUCCESS_COUNT,
                         [this]() -> uint64_t { return mFilterExistSuccessCount; });
        mMetrics.emplace(MetricType::FILTER_EXIST_FAIL_COUNT, [this]() -> uint64_t { return mFilterExistFailCount; });
    }

    BResult Close()
    {
        mMetricLayers.clear();
        mMetrics.clear();
        LOG_INFO("Bss Metric Closed.");
        return BSS_OK;
    }

    int64_t GetMetrics(MetricType metricType)
    {
        auto it = mMetrics.find(metricType);
        if (it == mMetrics.end()) {
            LOG_ERROR("Failed to get metrics, not enabled in config, MetricType:" << static_cast<int32_t>(metricType)
                                                                                  << ".");
            return INT64_MAX;
        }
        return it->second();
    }

    bool IsFreshMetricEnabled()
    {
        return mMetricLayers[LayerType::FRESH_TABLE];
    }

    bool IsSliceMetricEnabled()
    {
        return mMetricLayers[LayerType::SLICE_TABLE];
    }

    bool IsFileCacheMetricEnabled()
    {
        return mMetricLayers[LayerType::LSM_CACHE];
    }

    bool IsMemoryMetricEnabled()
    {
        return mMetricLayers[LayerType::MEMORY_MANAGER];
    }

    bool IsRestoreMetricEnabled()
    {
        return mMetricLayers[LayerType::RESTORE];
    }

    bool IsFileStoreMetricEnabled()
    {
        return mMetricLayers[LayerType::LSM_STORE];
    }

    void InitLsmMetric()
    {
        mLevelCompactionRead.resize(NO_4);
        mLevelCompactionWrite.resize(NO_4);
        mLevelFileSize.resize(NO_5);
        mLevelHitCount.resize(NO_4);
        mLevelMissHitCount.resize(NO_4);
        mMetrics.emplace(MetricType::LSM_FLUSH_COUNT, [this]() -> uint64_t { return mLsmFlushCount; });
        mMetrics.emplace(MetricType::LSM_FLUSH_SIZE, [this]() -> uint64_t { return mLsmFlushSize; });
        mMetrics.emplace(MetricType::LSM_COMPACTION_COUNT, [this]() -> uint64_t { return mLsmCompactionCount; });
        mMetrics.emplace(MetricType::LSM_HIT_COUNT, [this]() -> uint64_t { return mLsmHitCount; });
        mMetrics.emplace(MetricType::LSM_MISS_COUNT, [this]() -> uint64_t { return mLsmMissCount; });
        mMetrics.emplace(MetricType::LEVEL0_HIT_COUNT, [this]() -> uint64_t { return mLevelHitCount[NO_0]; });
        mMetrics.emplace(MetricType::LEVEL0_MISS_COUNT, [this]() -> uint64_t { return mLevelMissHitCount[NO_0]; });
        mMetrics.emplace(MetricType::LEVEL1_HIT_COUNT, [this]() -> uint64_t { return mLevelHitCount[NO_1]; });
        mMetrics.emplace(MetricType::LEVEL1_MISS_COUNT, [this]() -> uint64_t { return mLevelMissHitCount[NO_1]; });
        mMetrics.emplace(MetricType::LEVEL2_HIT_COUNT, [this]() -> uint64_t { return mLevelHitCount[NO_2]; });
        mMetrics.emplace(MetricType::LEVEL2_MISS_COUNT, [this]() -> uint64_t { return mLevelMissHitCount[NO_2]; });
        mMetrics.emplace(MetricType::ABOVE_LEVEL2_HIT_COUNT, [this]() -> uint64_t { return mLevelHitCount[NO_3]; });
        mMetrics.emplace(MetricType::ABOVE_LEVEL2_MISS_COUNT,
                         [this]() -> uint64_t { return mLevelMissHitCount[NO_3]; });
        mMetrics.emplace(MetricType::LSM_FILE_SIZE, [this]() -> uint64_t { return mLsmFileSize; });
        mMetrics.emplace(MetricType::LSM_FILE_COUNT, [this]() -> uint64_t { return mLsmFileCount; });
        for (uint32_t i = 0; i < NO_4; i++) {
            mMetrics.emplace(static_cast<MetricType>(static_cast<int32_t>(MetricType::LEVEL0_COMPACTION_RATE) + i),
                             [this, i]() -> uint64_t {
                                 return mLevelCompactionRead[i] == 0 ?
                                            0 :
                                            (static_cast<uint64_t>(mLevelCompactionWrite[i]) * NO_100) /
                                                mLevelCompactionRead[i];
                             });
        }
        mMetrics.emplace(MetricType::LSM_COMPACTION_RATE, [this]() -> uint64_t {
            return mTotalCompactionRead == 0 ?
                       0 :
                       (static_cast<uint64_t>(mTotalCompactionWrite) * NO_100) / mTotalCompactionRead;
        });
        for (uint32_t i = 0; i < NO_5; i++) {
            mMetrics.emplace(static_cast<MetricType>(static_cast<int32_t>(MetricType::LEVEL0_FILE_SIZE) + i),
                             [this, i]() -> uint64_t { return mLevelFileSize[i]; });
        }
        mMetrics.emplace(MetricType::LSM_COMPACTION_READ_SIZE, [this]() -> uint64_t { return mTotalCompactionRead; });
        mMetrics.emplace(MetricType::LSM_COMPACTION_WRITE_SIZE, [this]() -> uint64_t { return mTotalCompactionWrite; });
        LOG_INFO("Register metric to LSM success.");
    }

    void AddFreshHitCount()
    {
        mFreshHitCount++;
    }

    void AddFreshMissCount()
    {
        mFreshMissCount++;
    }

    void SetFreshRecordCount(std::function<uint64_t()> computeFreshRecordCount)
    {
        mFreshRecordCount = computeFreshRecordCount;
    }

    void SetFreshFlushingRecordCount(std::function<uint64_t()> computeFreshFlushingRecordCount)
    {
        mFreshFlushingRecordCount = computeFreshFlushingRecordCount;
    }

    void SetFreshFlushingSegmentCount(std::function<uint64_t()> computeFreshFlushingSegmentCount)
    {
        mFreshFlushingSegmentCount = computeFreshFlushingSegmentCount;
    }

    void AddFreshFlushedRecordCount(uint64_t freshFlushedRecordCount)
    {
        mFreshFlushedRecordCount += freshFlushedRecordCount;
    }

    void AddFreshFlushedSegmentCount()
    {
        mFreshFlushedSegmentCount++;
    }

    void AddFreshSegmentCreateFailCount()
    {
        mFreshSegmentCreateFailCount++;
    }

    void AddFreshFlushCount()
    {
        mFreshFlushCount++;
    }

    void AddFreshBinaryKeySize(uint64_t freshBinaryKeySize)
    {
        mFreshBinaryKeySize += freshBinaryKeySize;
    }

    void AddFreshBinaryValueSize(uint64_t freshBinaryValueSize)
    {
        mFreshBinaryValueSize += freshBinaryValueSize;
    }

    void ClearFreshBinaryKeyValue()
    {
        mFreshBinaryKeySize = 0;
        mFreshBinaryValueSize = 0;
        mFreshBinaryMapNodeSize = 0;
    }

    void AddFreshBinaryMapNodeSize(uint64_t freshBinaryMapNodeSize)
    {
        mFreshBinaryMapNodeSize += freshBinaryMapNodeSize;
    }

    void AddFreshWastedSize(uint64_t freshWastedSize)
    {
        mFreshWastedSize += freshWastedSize;
    }

    void AddSliceHitCount()
    {
        mSliceHitCount++;
    }

    void AddSliceMissCount()
    {
        mSliceMissCount++;
    }

    void AddSliceReadCount()
    {
        mSliceReadCount++;
    }

    void AddSliceReadSize(uint64_t size)
    {
        mSliceReadSize += size;
    }

    void AddSliceEvictSize(uint64_t sliceEvictSize)
    {
        mSliceEvictSize += sliceEvictSize;
    }

    void SubSliceEvictSize(uint64_t sliceEvictSize)
    {
        if (mSliceEvictSize > sliceEvictSize) {
            mSliceEvictSize -= sliceEvictSize;
        } else {
            mSliceEvictSize = 0;
        }
    }

    void SetSliceEvictWaitingCount(std::function<uint64_t()> computeSliceEvictWaitingCount)
    {
        mSliceEvictWaitingCount = computeSliceEvictWaitingCount;
    }

    void AddSliceCompactionCount()
    {
        mSliceCompactionCount++;
    }

    void AddSliceCompactionSliceCount(uint64_t sliceCompactionSliceCount)
    {
        mSliceCompactionSliceCount += sliceCompactionSliceCount;
    }

    void SetSliceChainAvgSize(std::function<uint64_t()> computeSliceChainAvgSize)
    {
        mSliceChainAvgSize = computeSliceChainAvgSize;
    }

    // slice总size / 所有sliceChain的总长度(1个slice代表1个长度)
    void SetSliceAvgSize(std::function<uint64_t()> computeSliceAvgSize)
    {
        mSliceAvgSize = computeSliceAvgSize;
    }

    // 转换为秒数
    void SetLazyDownloadTime(uint64_t lazyDownloadTime)
    {
        mLazyDownloadTime = lazyDownloadTime / NO_1000;
    }

    void AddHitStat(BlockType type, uint64_t size)
    {
        switch (type) {
            case BlockType::FILTER:
                mFilterHitCount++;
                mFilterHitSize += size;
                break;
            case BlockType::INDEX:
                mIndexBlockHitCount++;
                mIndexBlockHitSize += size;
                break;
            case BlockType::DATA:
                mDataBlockHitCount++;
                mDataBlockHitSize += size;
                break;
            default:
                LOG_WARN("not support blockType: " << static_cast<uint8_t>(type));
                break;
        }
    }

    void AddMissStat(BlockType type, uint64_t size)
    {
        switch (type) {
            case BlockType::FILTER:
                mFilterMissCount++;
                mFilterMissSize += size;
                break;
            case BlockType::INDEX:
                mIndexBlockMissCount++;
                mIndexBlockMissSize += size;
                break;
            case BlockType::DATA:
                mDataBlockMissCount++;
                mDataBlockMissSize += size;
                break;
            default:
                LOG_WARN("not support blockType: " << static_cast<uint8_t>(type));
                break;
        }
    }

    void UpdateCacheStat(BlockType type, bool isAdd, uint64_t size)
    {
        auto &count = mCacheCount[static_cast<uint8_t>(type)];
        auto &cacheSize = mCacheSize[static_cast<uint8_t>(type)];
        if (isAdd) {
            count.fetch_add(1);
            cacheSize.fetch_add(size);
        } else {
            count.fetch_sub(1);
            cacheSize.fetch_sub(size);
        }
    }

    inline void AddFilterSuccessCount()
    {
        mFilterSuccessCount++;
    }

    inline void AddFilterExistSuccessCount()
    {
        mFilterExistSuccessCount++;
    }

    inline void AddFilterExistFailCount()
    {
        mFilterExistFailCount++;
    }
    void AddLsmFlushCount()
    {
        mLsmFlushCount++;
    }

    void AddLsmFlushSize(uint64_t lsmFlushSize)
    {
        mLsmFlushSize += lsmFlushSize;
    }

    void AddLsmCompactionCount()
    {
        mLsmCompactionCount++;
    }

    void AddLsmHitCount()
    {
        mLsmHitCount++;
    }

    void AddLsmMissCount()
    {
        mLsmMissCount++;
    }

    void AddLevelHitCount(uint32_t levelId)
    {
        levelId = std::min(levelId, NO_3);
        mLevelHitCount[levelId]++;
    }

    void AddLevelMissHitCount(uint32_t levelId)
    {
        levelId = std::min(levelId, NO_3);
        mLevelMissHitCount[levelId]++;
    }

    void UpdateLevelFileMetric(uint64_t fileSize, uint32_t levelId, bool increment)
    {
        levelId = std::min(levelId, NO_4);
        if (increment) {
            mLevelFileSize[levelId] += fileSize;
            mLsmFileSize += fileSize;
            if (levelId == 0) {
                mLsmFlushCount++;
                mLsmFlushSize += fileSize;
            }
            mLsmFileCount++;
            return;
        }

        if (fileSize <= mLevelFileSize[levelId]) {
            mLevelFileSize[levelId] -= fileSize;
        } else {
            mLevelFileSize[levelId] = 0;
        }

        if (fileSize <= mLsmFileSize) {
            mLsmFileSize -= fileSize;
        } else {
            mLsmFileSize = 0;
        }
        if (mLsmFileCount > 0) {
            mLsmFileCount--;
        }
    }

    void SetLsmFileSize(uint64_t lsmFileSize)
    {
        mLsmFileSize = lsmFileSize;
    }

    void AddLsmCompactionReadSize(uint64_t lsmCompactionReadSize, uint32_t levelId)
    {
        levelId = std::min(levelId, NO_3);
        mLevelCompactionRead[levelId] += lsmCompactionReadSize;
        mTotalCompactionRead += lsmCompactionReadSize;
    }

    void AddLsmCompactionWriteSize(uint64_t lsmCompactionWriteSize, uint32_t levelId)
    {
        levelId = std::min(levelId, NO_3);
        mLevelCompactionWrite[levelId] += lsmCompactionWriteSize;
        mTotalCompactionWrite += lsmCompactionWriteSize;
    }

    void AddLsmCompactCount()
    {
        mLsmCompactionCount++;
    }

    uint64_t GetFileFlush() const
    {
        return mLsmFileCount;
    }

    uint64_t GetMemoryFreshUsed()
    {
        if (LIKELY(mUsedMemoryGetter != nullptr)) {
            return mUsedMemoryGetter(MemoryType::FRESH_TABLE);
        }
        return 0;
    }

    uint64_t GetMemorySliceUsed()
    {
        if (LIKELY(mUsedMemoryGetter != nullptr)) {
            return mUsedMemoryGetter(MemoryType::SLICE_TABLE);
        }
        return 0;
    }

    uint64_t GetMemoryFileUsed()
    {
        if (LIKELY(mUsedMemoryGetter != nullptr)) {
            return mUsedMemoryGetter(MemoryType::FILE_STORE);
        }
        return 0;
    }

    uint64_t GetMemorySnapshotUsed()
    {
        if (LIKELY(mUsedMemoryGetter != nullptr)) {
            return mUsedMemoryGetter(MemoryType::SNAPSHOT);
        }
        return 0;
    }

    uint64_t GetMemoryTotalUsed()
    {
        return GetMemoryFreshUsed() + GetMemorySliceUsed() + GetMemoryFileUsed() + GetMemorySnapshotUsed();
    }

    void SetMemoryFreshMax(uint64_t memoryFreshMax)
    {
        mMemoryFreshMax = memoryFreshMax;
    }

    void SetMemorySliceMax(uint64_t memorySliceMax)
    {
        mMemorySliceMax = memorySliceMax;
    }

    void SetMemoryFileMax(uint64_t memoryFileMax)
    {
        mMemoryFileMax = memoryFileMax;
    }

    void SetMemorySnapshotMax(uint64_t memorySnapshotMax)
    {
        mMemorySnapshotMax = memorySnapshotMax;
    }

    void SetMemoryTotalMax(uint64_t memoryTotalMax)
    {
        mMemoryTotalMax = memoryTotalMax;
    }

    uint64_t GetMemoryHeapUsed()
    {
        if (LIKELY(mUsedMemoryGetter != nullptr)) {
            return mUsedMemoryGetter(MemoryType::BORROW_HEAP);
        }
        return 0;
    }

    uint64_t GetMemoryHeapMax()
    {
        if (LIKELY(mMaxMemoryGetter != nullptr)) {
            return mMaxMemoryGetter(MemoryType::BORROW_HEAP);
        }
        return 0;
    }

    void SetUsedMemoryGetter(std::function<uint64_t(MemoryType)> usedMemoryGetter)
    {
        mUsedMemoryGetter = usedMemoryGetter;
    }

    void SetMaxMemoryGetter(std::function<uint64_t(MemoryType)> maxMemoryGetter)
    {
        mMaxMemoryGetter = maxMemoryGetter;
    }

private:
    // *** FreshTable metrics *** ///
    uint64_t mFreshHitCount = 0;
    uint64_t mFreshMissCount = 0;
    std::function<uint64_t()> mFreshRecordCount = nullptr;
    std::function<uint64_t()> mFreshFlushingRecordCount = nullptr;
    std::function<uint64_t()> mFreshFlushingSegmentCount = nullptr;
    uint64_t mFreshFlushedRecordCount = 0;
    uint64_t mFreshFlushedSegmentCount = 0;
    uint64_t mFreshSegmentCreateFailCount = 0;
    uint64_t mFreshFlushCount = 0;
    uint64_t mFreshBinaryKeySize = 0;
    uint64_t mFreshBinaryValueSize = 0;
    uint64_t mFreshBinaryMapNodeSize = 0;
    uint64_t mFreshWastedSize = 0;
    // *** FreshTable metrics *** ///

    // *** SliceTable metrics *** ///
    uint64_t mSliceHitCount = 0;
    uint64_t mSliceMissCount = 0;
    uint64_t mSliceReadCount = 0;
    uint64_t mSliceEvictSize = 0;
    std::function<uint64_t()> mSliceEvictWaitingCount = nullptr;
    uint64_t mSliceCompactionCount = 0;
    uint64_t mSliceCompactionSliceCount = 0;
    std::function<uint64_t()> mSliceChainAvgSize = 0;
    std::function<uint64_t()> mSliceAvgSize = 0;
    uint64_t mSliceReadSize = 0;
    // *** SliceTable metrics *** ///

    uint64_t mLazyDownloadTime = 0;

    // *** LSM metrics *** //
    uint64_t mLsmFlushCount = 0;
    uint64_t mLsmFlushSize = 0;
    uint64_t mLsmCompactionCount = 0;
    uint64_t mLsmHitCount = 0;
    uint64_t mLsmMissCount = 0;
    std::vector<uint64_t> mLevelHitCount;
    std::vector<uint64_t> mLevelMissHitCount;
    std::vector<uint64_t> mLevelFileSize;
    uint64_t mLsmFileSize = 0;
    std::vector<uint64_t> mLevelCompactionRead;
    std::vector<uint64_t> mLevelCompactionWrite;
    uint64_t mTotalCompactionRead = 0;
    uint64_t mTotalCompactionWrite = 0;
    uint64_t mLsmFileCount = 0;

    // *** memory manager metrics *** //
    uint64_t mMemoryFreshMax = 0;
    uint64_t mMemorySliceMax = 0;
    uint64_t mMemoryFileMax = 0;
    uint64_t mMemorySnapshotMax = 0;
    uint64_t mMemoryTotalMax = 0;
    std::function<uint64_t(MemoryType)> mUsedMemoryGetter = nullptr;
    std::function<uint64_t(MemoryType)> mMaxMemoryGetter = nullptr;
    // *** memory manager metrics *** //

    std::unordered_map<LayerType, bool> mMetricLayers;
    std::unordered_map<MetricType, std::function<uint64_t()>> mMetrics;

    uint64_t mIndexBlockHitCount = 0;
    uint64_t mIndexBlockHitSize = 0;
    uint64_t mIndexBlockMissCount = 0;
    uint64_t mIndexBlockMissSize = 0;
    std::atomic<uint64_t> mCacheCount[static_cast<uint8_t>(BlockType::BUTT)];
    std::atomic<uint64_t> mCacheSize[static_cast<uint8_t>(BlockType::BUTT)];
    uint64_t mDataBlockHitCount = 0;
    uint64_t mDataBlockHitSize = 0;
    uint64_t mDataBlockMissCount = 0;
    uint64_t mDataBlockMissSize = 0;
    uint64_t mFilterHitCount = 0;
    uint64_t mFilterHitSize = 0;
    uint64_t mFilterMissCount = 0;
    uint64_t mFilterMissSize = 0;
    uint64_t mFilterSuccessCount = 0;
    uint64_t mFilterExistSuccessCount = 0;
    uint64_t mFilterExistFailCount = 0;
};

using BoostNativeMetricPtr = BoostNativeMetric *;

}  // namespace bss
}  // namespace ock
#endif  // BOOST_SS_BSS_METRIC_H
