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

#ifndef BOOST_SS_LRU_CACHE_H
#define BOOST_SS_LRU_CACHE_H

#include <memory>
#include <unordered_map>

#include "common/block.h"
#include "common/bss_log.h"
#include "util/bss_lock.h"

namespace ock {
namespace bss {
static const double TRIM_RATIO = 0.9;

class DLinkedNode {
public:
    explicit DLinkedNode() : key(0), value(nullptr), prev(nullptr), next(nullptr)
    {
    }

    DLinkedNode(uint64_t key, BlockRef value) : key(key), value(value), prev(nullptr), next(nullptr)
    {
    }

    // 禁止拷贝，只允许移动
    DLinkedNode(const DLinkedNode &) = delete;
    DLinkedNode &operator=(const DLinkedNode &) = delete;

    DLinkedNode(DLinkedNode &&other) noexcept
        : key(other.key), value(std::move(other.value)), prev(other.prev), next(other.next)
    {
        if (prev)
            prev->next = this;
        if (next)
            next->prev = this;
        // 重置 other 的 prev 和 next 指针
        other.prev = nullptr;
        other.next = nullptr;
    }

public:
    uint64_t key;
    BlockRef value;
    DLinkedNode *prev;
    DLinkedNode *next;
};

class LRUCache {
public:
    explicit LRUCache(uint64_t capacity) : mCapacity(capacity), mCurrentSize(0)
    {
        mHead = std::make_unique<DLinkedNode>();
        mTail = std::make_unique<DLinkedNode>();
        mHead->next = mTail.get();
        mTail->prev = mHead.get();
        LOG_INFO("Create LRUCache capacity:" << capacity << " success.");
    }

    ~LRUCache()
    {
        Close();
        std::unordered_map<uint64_t, std::unique_ptr<DLinkedNode>>().swap(mCache);
        std::unordered_map<ConfigRef, BoostNativeMetricPtr>().swap(mLruCachesMetricMap);
        LOG_INFO("Delete LRUCache success.");
    }

    BlockRef Get(const uint64_t &key)
    {
        mLock.Lock();
        auto iter = mCache.find(key);
        if (iter == mCache.end()) {
            mLock.UnLock();
            return nullptr;
        }

        DLinkedNode *node = iter->second.get();
        MoveToHead(node);
        mLock.UnLock();
        return node->value;
    }

    void Put(const uint64_t key, const BlockRef &value)
    {
        if (UNLIKELY(value == nullptr) || UNLIKELY(UINT64_MAX - value->BufferLen() < mCurrentSize)) {
            return;
        }

        mLock.Lock();
        auto iter = mCache.find(key);
        if (iter != mCache.end()) {
            DLinkedNode *node = iter->second.get();
            auto oldValueLen = node->value->BufferLen();

            UpdateCacheStat(node->value->GetBlockType(), false, oldValueLen);
            node->value = value;
            UpdateCacheStat(node->value->GetBlockType(), true, value->BufferLen());

            MoveToHead(node);
            mCurrentSize = mCurrentSize - oldValueLen + value->BufferLen();
        } else {
            auto nodePtr = std::make_unique<DLinkedNode>(key, value);
            DLinkedNode *node = nodePtr.get();

            mCache.emplace(key, std::move(nodePtr));
            AddToHead(node);

            mCurrentSize += value->BufferLen();
            UpdateCacheStat(value->GetBlockType(), true, value->BufferLen());
        }
        TrimToSize();
        mLock.UnLock();
    }

    BlockRef Remove(uint64_t key)
    {
        mLock.Lock();
        auto iter = mCache.find(key);
        if (iter != mCache.end()) {
            std::unique_ptr<DLinkedNode> nodePtr = std::move(iter->second);
            mCache.erase(iter);

            DLinkedNode *node = nodePtr.get();
            UpdateCacheStat(node->value->GetBlockType(), false, node->value->BufferLen());
            RemoveNode(node);

            mCurrentSize -= node->value->BufferLen();
            mLock.UnLock();
            return node->value;
        }
        mLock.UnLock();
        return nullptr;
    }

    void Close()
    {
        mLock.Lock();
        mCache.clear();
        mHead->next = mTail.get();
        mTail->prev = mHead.get();
        mCurrentSize = 0;
        mLock.UnLock();
    }

    void RemoveEldestEntry(uint64_t size)
    {
        mLock.Lock();
        uint64_t removeSize = 0;
        do {
            if (mCurrentSize > 0 && !mCache.empty()) {
                DLinkedNode *node = RemoveTailNode();
                if (UNLIKELY(node == nullptr)) {
                    break;
                }

                auto iter = mCache.find(node->key);
                if (iter != mCache.end()) {
                    UpdateCacheStat(iter->second->value->GetBlockType(), false, iter->second->value->BufferLen());
                    mCurrentSize -= iter->second->value->BufferLen();
                    removeSize += iter->second->value->BufferLen();
                    mCache.erase(iter);
                }
            } else {
                break;
            }
        } while (removeSize < size);
        if (UNLIKELY(removeSize < size)) {
            LOG_DEBUG("Remove eldest entry not enough, removeSize:" << removeSize << ", expectSize:" << size
                                                                    << ", currentSize:" << mCurrentSize);
        }
        mLock.UnLock();
    }

    inline void RegisterMetric(ConfigRef config, BoostNativeMetricPtr metricPtr)
    {
        if (UNLIKELY(metricPtr == nullptr)) {
            LOG_ERROR("RegisterMetric for lru cache failed, metricPtr is nullptr.");
            return;
        }
        mMetricEnabled = metricPtr->IsFileCacheMetricEnabled();
        if (!mMetricEnabled) {
            return;
        }
        mLock.Lock();
        mLruCachesMetricMap.emplace(config, metricPtr);
        mLock.UnLock();
    }

    inline void DeRegisterMetric(ConfigRef config)
    {
        if (!mMetricEnabled) {
            return;
        }
        mLock.Lock();
        mLruCachesMetricMap.erase(config);
        mLock.UnLock();
    }

    inline void UpdateCacheStat(BlockType type, bool isAdd, uint64_t size)
    {
        if (!mMetricEnabled) {
            return;
        }
        for (auto it = mLruCachesMetricMap.begin(); it != mLruCachesMetricMap.end(); it++) {
            CONTINUE_LOOP_AS_NULLPTR(it->second);
            it->second->UpdateCacheStat(type, isAdd, size);
        }
    }

private:
    void AddToHead(DLinkedNode *node)
    {
        node->prev = mHead.get();
        node->next = mHead->next;
        mHead->next->prev = node;
        mHead->next = node;
    }

    void RemoveNode(DLinkedNode *node)
    {
        if (node == mTail.get() || node == mHead.get() || node == nullptr) {
            return;
        }
        node->prev->next = node->next;
        node->next->prev = node->prev;

        // 重置指针，防止悬垂
        node->prev = nullptr;
        node->next = nullptr;
    }

    void MoveToHead(DLinkedNode *node)
    {
        RemoveNode(node);
        AddToHead(node);
    }

    DLinkedNode *RemoveTailNode()
    {
        DLinkedNode *node = mTail->prev;
        if (node == mHead.get() || node == nullptr) {
            return nullptr;
        }
        RemoveNode(node);
        return node;
    }

    void TrimToSize()
    {
        while ((mCurrentSize > (mCapacity * TRIM_RATIO)) && !mCache.empty()) {
            DLinkedNode *node = RemoveTailNode();
            if (node == nullptr) {
                return;
            }

            auto iter = mCache.find(node->key);
            if (iter != mCache.end()) {
                UpdateCacheStat(iter->second->value->GetBlockType(), false, iter->second->value->BufferLen());
                mCurrentSize -= iter->second->value->BufferLen();
                mCache.erase(iter);
                LOG_DEBUG("Trim eldest entry, key:" << node->key << " valueSize:" << node->value->BufferLen()
                                                    << ", currentSize:" << mCurrentSize);
            }
        }
    }

private:
    // 哈希表拥有节点的唯一所有权
    std::unordered_map<uint64_t, std::unique_ptr<DLinkedNode>> mCache;

    std::unique_ptr<DLinkedNode> mHead;
    std::unique_ptr<DLinkedNode> mTail;
    uint64_t mCapacity;
    uint64_t mCurrentSize;
    SpinLock mLock;
    bool mMetricEnabled = false;
    std::unordered_map<ConfigRef, BoostNativeMetricPtr> mLruCachesMetricMap;
};

}  // namespace bss
}  // namespace ock
#endif  // BOOST_SS_LRU_CACHE_H
