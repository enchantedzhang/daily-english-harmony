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

#ifndef BOOST_STATE_STORE_CONCURRENT_QUEUE_H
#define BOOST_STATE_STORE_CONCURRENT_QUEUE_H

#include <deque>
#include <mutex>

namespace ock {
namespace bss {

template <typename T> class ConcurrentDeque {
public:
    ConcurrentDeque() = default;
    ConcurrentDeque(ConcurrentDeque &&other) noexcept
    {
        std::lock_guard<std::mutex> lock(other.mMutex);
        mDeque = std::move(other.mDeque);
    }

    void PushFront(T const &item)
    {
        std::lock_guard<std::mutex> lock(mMutex);
        mDeque.push_front(item);
    }

    void PushBack(T const item)
    {
        std::lock_guard<std::mutex> lock(mMutex);
        mDeque.emplace_back(item);
    }

    bool TryPopFront(T &item)
    {
        std::lock_guard<std::mutex> lock(mMutex);
        if (!mDeque.empty()) {
            item = mDeque.front();
            mDeque.pop_front();
            return true;
        }
        return false;
    }

    bool Front(T &item)
    {
        std::lock_guard<std::mutex> lock(mMutex);
        if (!mDeque.empty()) {
            item = mDeque.front();
            return true;
        }
        return false;
    }

    bool PopFront(T &item)
    {
        std::lock_guard<std::mutex> lock(mMutex);
        if (!mDeque.empty()) {
            mDeque.pop_front();
            return true;
        }
        return false;
    }

    std::deque<T> GetDeque()
    {
        std::lock_guard<std::mutex> lock(mMutex);
        return mDeque;
    }

    bool TryPopBack(T &item)
    {
        std::lock_guard<std::mutex> lock(mMutex);
        if (!mDeque.empty()) {
            item = std::move(mDeque.back());
            mDeque.pop_back();
            return true;
        }
        return false;
    }

    std::reverse_iterator<typename std::deque<T>::iterator> rbegin()
    {
        std::lock_guard<std::mutex> lock(mMutex);
        return mDeque.rbegin();
    }

    std::reverse_iterator<typename std::deque<T>::iterator> rend()
    {
        std::lock_guard<std::mutex> lock(mMutex);
        return mDeque.rend();
    }

    bool Empty() const
    {
        std::lock_guard<std::mutex> lock(mMutex);
        return mDeque.empty();
    }

    void Clear()
    {
        std::lock_guard<std::mutex> lock(mMutex);
        std::deque<T>().swap(mDeque);
    }

    size_t Size() const
    {
        std::lock_guard<std::mutex> lock(mMutex);
        return mDeque.size();
    }

private:
    mutable std::mutex mMutex;
    std::deque<T> mDeque;
};

}  // namespace bss
}  // namespace ock

#endif