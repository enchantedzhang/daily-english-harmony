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

#ifndef BOOST_SS_SKIPLIST_H
#define BOOST_SS_SKIPLIST_H

#include <algorithm>
#include <iostream>
#include <memory>
#include <random>
#include <stdexcept>
#include <vector>

#include "fixed_size_memory_pool.h"
#include "memory_segment.h"

namespace ock {
namespace bss {

template <typename Key> struct Node {
    Key mKey;
    Node<Key> **mForward;
    int32_t mTopLevel;

    Node(const Key &k, int32_t level) : mKey(k), mTopLevel(level)
    {
        mForward = new Node<Key> *[level + 1]();
    }

    ~Node()
    {
        delete[] mForward;
    }
};

template <typename Key, class Comparator> class SkipList {
public:
    // 构造函数 - 必须提供比较器
    explicit SkipList(const Comparator &comparator, FixedSizeMemoryPoolRef allocator, uint64_t seqId)
        : mComparator(comparator), mAllocator(allocator), mSeqId(seqId), mDistribution(0.0, 1.0)
    {
    }

    BResult Initialize()
    {
        mHead = NewNode(Key(), MAX_LEVEL);
        mTail = NewNode(Key(), MAX_LEVEL);
        if (!mHead || !mTail) {
            LOG_INFO("Failed to create head");
            return BSS_ERR;
        }

        for (int i = 0; i <= MAX_LEVEL; ++i) {
            mHead->mForward[i] = mTail;
        }
        mCurrentMaxLevel = 0;
        mSize = 0;
        return BSS_OK;
    }

    // 析构函数
    ~SkipList()
    {
        Clear();
    }

    // 禁止拷贝
    SkipList(const SkipList &) = delete;
    SkipList &operator=(const SkipList &) = delete;

    inline uint64_t GetSeqId()
    {
        return mSeqId;
    }

    BResult Put(const Key &key)
    {
        std::vector<Node<Key> *> update(MAX_LEVEL + 1, nullptr);
        Node<Key> *current = mHead;

        for (int level = mCurrentMaxLevel; level >= 0; --level) {
            while (current->mForward[level] != mTail && mComparator(current->mForward[level]->mKey, key) < 0) {
                current = current->mForward[level];
            }
            update[level] = current;
        }

        current = current->mForward[0];
        if (current != mTail && mComparator(current->mKey, key) == 0) {
#ifndef FLAG_FOR_UT
            mAllocator->Free(const_cast<uint8_t *>(current->mKey.Data()));
#endif
            current->mKey = key;  // 更新键值
            return BSS_OK;        // 如果键已存在，返回
        }

        int newLevel = RandomLevel();
        if (newLevel > mCurrentMaxLevel) {
            for (int level = mCurrentMaxLevel + 1; level <= newLevel; ++level) {
                update[level] = mHead;
            }
            mCurrentMaxLevel = newLevel;
        }

        Node<Key> *newNode = NewNode(key, newLevel);
        if (!newNode) {
            return BSS_ALLOC_FAIL;
        }

        for (int level = 0; level <= newLevel; ++level) {
            newNode->mForward[level] = update[level]->mForward[level];
            update[level]->mForward[level] = newNode;
        }

        mSize++;
        return BSS_OK;
    }

    bool Contains(const Key &key) const
    {
        Node<Key> *current = mHead;
        for (int level = mCurrentMaxLevel; level >= 0; --level) {
            while (current->mForward[level] != mTail && mComparator(current->mForward[level]->mKey, key) < 0) {
                current = current->mForward[level];
            }
        }

        current = current->mForward[0];
        return current != mTail && mComparator(current->mKey, key) == 0;
    }

    BResult Remove(const Key &key)
    {
        std::vector<Node<Key> *> update(MAX_LEVEL + 1, nullptr);
        Node<Key> *current = mHead;
        for (int level = mCurrentMaxLevel; level >= 0; --level) {
            while (current->mForward[level] != mTail && mComparator(current->mForward[level]->mKey, key) < 0) {
                current = current->mForward[level];
            }
            update[level] = current;
        }

        current = current->mForward[0];
        if (current == mTail || mComparator(current->mKey, key) != 0) {
            return BSS_OK;
        }

        for (int level = 0; level <= current->mTopLevel; ++level) {
            if (update[level]->mForward[level] != current) {
                break;
            }
            update[level]->mForward[level] = current->mForward[level];
        }
        while (mCurrentMaxLevel > 0 && mHead->mForward[mCurrentMaxLevel] == mTail) {
            mCurrentMaxLevel--;
        }
#ifndef FLAG_FOR_UT
        mAllocator->Free(const_cast<uint8_t *>(current->mKey.Data()));
#endif
        delete current;
        mSize--;
        return BSS_OK;
    }

    bool First(Key &key) const
    {
        Node<Key> *firstNode = mHead->mForward[0];
        if (firstNode == mTail) {
            return false;
        }
        key = firstNode->mKey;
        return true;
    }

    bool Last(Key &key) const
    {
        Node<Key> *current = mHead;
        for (int level = mCurrentMaxLevel; level >= 0; --level) {
            while (current->mForward[level] != mTail) {
                current = current->mForward[level];
            }
        }
        if (current == mHead) {
            return false;
        }
        key = current->mKey;
        return true;
    }

    // 获取大于等于指定键的最小键
    bool Ceiling(const Key &key, Node<Key> *&result) const
    {
        Node<Key> *current = mHead;

        for (int level = mCurrentMaxLevel; level >= 0; --level) {
            while (current->mForward[level] != mTail && mComparator(current->mForward[level]->mKey, key) < 0) {
                current = current->mForward[level];
            }
        }

        current = current->mForward[0];
        if (current == mTail) {
            return false;
        }

        result = current;
        return true;
    }

    int32_t Size() const
    {
        return mSize.load();
    }

    bool Empty() const
    {
        return Size() == 0;
    }

    // 清空SkipList
    void Clear()
    {
        Node<Key> *current = mHead->mForward[0];
        while (current != mTail) {
            Node<Key> *next = current->mForward[0];
            delete current;
            current = next;
        }
        for (int32_t i = 0; i <= MAX_LEVEL; ++i) {
            mHead->mForward[i] = mTail;
        }
        delete mHead;
        delete mTail;
        mCurrentMaxLevel.store(0);
        mSize.store(0);
    }

    // 迭代器支持
    class Iterator {
    public:
        Iterator() = default;
        Iterator(Node<Key> *start, Node<Key> *end) : mCurrent(start), mTail(end)
        {
        }

        bool HasNext() const
        {
            return mCurrent != nullptr;
        }

        const Key &GetKey() const
        {
            return mCurrent->mKey;
        }

        Key Next()
        {
            Node<Key> *temp = mCurrent;
            if (mCurrent != nullptr && mCurrent != mTail) {
                mCurrent = mCurrent->mForward[0];
            }
            if (mCurrent == mTail) {
                mCurrent = nullptr;
            }
            return temp->mKey;
        }

    private:
        Node<Key> *mCurrent;
        Node<Key> *mTail;
    };

    Iterator NewIterator(const Key &startKey) const
    {
        Node<Key> *start = nullptr;
        if (!Ceiling(startKey, start)) {
            start = mHead->mForward[0];
        }
        return Iterator(start, mTail);
    }

    Iterator NewIterator() const
    {
        return Iterator(mHead->mForward[0], mTail);
    }

    BResult GetMemAddr(uint32_t length, uint8_t *&addr)
    {
        return mAllocator->Allocate(length, addr);
    }

    FixedSizeMemoryPoolRef GetAllocator()
    {
        return mAllocator;
    }

private:
    Node<Key> *NewNode(const Key &key, int32_t level)
    {
        return new (std::nothrow) Node<Key>(key, level);
    }

    int32_t RandomLevel()
    {
        int32_t level = 0;
        while (mDistribution(mGenerator) < PROBABILITY && level < MAX_LEVEL - 1) {
            level++;
        }
        return level;
    }

private:
    static constexpr int32_t MAX_LEVEL = 12;
    static constexpr double PROBABILITY = 0.5;

    Node<Key> *mHead;
    Node<Key> *mTail;
    std::atomic<int32_t> mSize{ 0 };
    std::atomic<int32_t> mCurrentMaxLevel{ 0 };

    Comparator mComparator;
    FixedSizeMemoryPoolRef mAllocator;
    uint64_t mSeqId;

    std::default_random_engine mGenerator;
    std::uniform_real_distribution<double> mDistribution;
};
}  // namespace bss
}  // namespace ock
#endif  // BOOST_SS_SKIPLIST_H