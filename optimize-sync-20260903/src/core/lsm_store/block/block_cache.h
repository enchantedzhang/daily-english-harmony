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

#ifndef BOOST_SS_BLOCK_CACHE_H
#define BOOST_SS_BLOCK_CACHE_H

#include <memory>

#include "common/block.h"
#include "common/util/lru_cache.h"

namespace ock {
namespace bss {
class BlockCache {
public:
    explicit BlockCache(size_t capacity, float cacheIndexAndFilterRatio = 0.0f)
        : mLruCache(static_cast<size_t>(capacity * (1 - cacheIndexAndFilterRatio))),
          mLruCacheFilterAndIndex(static_cast<size_t>(capacity * cacheIndexAndFilterRatio))
    {
        if (cacheIndexAndFilterRatio > 0.001f) {
            LOG_INFO("Filter block and index block use independent cache pool, cacheIndexAndFilterRatio"
                     << cacheIndexAndFilterRatio);
            mUseOwnIndexCache = true;
        }
    }

    ~BlockCache() = default;

    inline bool IsDataBlock(BlockType blockType)
    {
        return blockType == BlockType::DATA || blockType == BlockType::BLOB;
    }

    inline BlockRef Get(uint64_t blockId, BlockType blockType)
    {
        return InnerGet(blockId, IsDataBlock(blockType));
    }

    inline void Put(uint64_t blockId, const BlockRef &block, BlockType blockType)
    {
        InnerPut(blockId, block, IsDataBlock(blockType));
    }

    inline BlockRef Remove(uint64_t blockId, BlockType blockType)
    {
        return InnerRemove(blockId, IsDataBlock(blockType));
    }

    inline void Revoke(uint64_t revokeSize)
    {
        mLruCache.RemoveEldestEntry(revokeSize);
    }

    inline BlockRef InnerGet(uint64_t blockId, bool isDataBlock)
    {
        if (mUseOwnIndexCache && !isDataBlock) {
            return mLruCacheFilterAndIndex.Get(blockId);
        }
        return mLruCache.Get(blockId);
    }

    inline void InnerPut(uint64_t blockId, const BlockRef &block, bool isDataBlock)
    {
        if (mUseOwnIndexCache && !isDataBlock) {
            mLruCacheFilterAndIndex.Put(blockId, block);
            return;
        }
        mLruCache.Put(blockId, block);
    }

    inline BlockRef InnerRemove(uint64_t blockId, bool isDataBlock)
    {
        if (mUseOwnIndexCache && !isDataBlock) {
            return mLruCacheFilterAndIndex.Remove(blockId);
        }
        return mLruCache.Remove(blockId);
    }

    inline void RegisterMetric(ConfigRef config, BoostNativeMetricPtr metricPtr)
    {
        mLruCache.RegisterMetric(config, metricPtr);
        mLruCacheFilterAndIndex.RegisterMetric(config, metricPtr);
    }

    inline void DeRegisterMetric(ConfigRef config)
    {
        mLruCache.DeRegisterMetric(config);
        mLruCacheFilterAndIndex.DeRegisterMetric(config);
    }

private:
    LRUCache mLruCache;
    LRUCache mLruCacheFilterAndIndex;
    bool mUseOwnIndexCache = false;
};
using BlockCacheRef = std::shared_ptr<BlockCache>;

class BlockCacheManager;
using BlockCacheManagerRef = std::shared_ptr<BlockCacheManager>;
class BlockCacheManager {
public:
    static BlockCacheManagerRef Instance()
    {
        static BlockCacheManagerRef instance = std::make_shared<BlockCacheManager>();
        return instance;
    }

    BlockCacheRef CreateBlockCache(uint32_t slotId, size_t capacity, float cacheIndexAndFilterRatio)
    {
        std::lock_guard<std::mutex> lock(mLock);
        auto iter = mBlockCacheMap.find(slotId);
        if (iter != mBlockCacheMap.end()) {
            iter->second.first++;
            return iter->second.second;
        }
        auto blockCache = std::make_shared<BlockCache>(capacity, cacheIndexAndFilterRatio);
        mBlockCacheMap.emplace(slotId, std::make_pair(1, blockCache));
        LOG_INFO("Create block cache success, slotId:" << slotId << ", capacity:" << capacity);
        return blockCache;
    }

    BlockCacheRef GetBlockCache(uint32_t slotId)
    {
        std::lock_guard<std::mutex> lock(mLock);
        auto iter = mBlockCacheMap.find(slotId);
        if (iter != mBlockCacheMap.end()) {
            return iter->second.second;
        }
        LOG_WARN("Unexpected: block cache of slot:" << slotId << " not exist.");
        return nullptr;
    }

    void DeleteBlockCache(uint32_t slotId)
    {
        std::lock_guard<std::mutex> lock(mLock);
        if (mBlockCacheMap.empty()) {
            return;
        }
        auto iter = mBlockCacheMap.find(slotId);
        if (iter == mBlockCacheMap.end()) {
            return;
        }
        iter->second.first--;
        if (iter->second.first == 0) {
            mBlockCacheMap.erase(iter);
            LOG_INFO("Delete block cache success, slotId:" << slotId);
        }
    }

private:
    std::mutex mLock;
    std::unordered_map<uint32_t, std::pair<uint32_t, BlockCacheRef>> mBlockCacheMap;
};

}  // namespace bss
}  // namespace ock
#endif  // BOOST_SS_BLOCK_CACHE_H