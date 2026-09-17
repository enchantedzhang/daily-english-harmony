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

#ifndef BOOST_SS_LOCK_H
#define BOOST_SS_LOCK_H

#include <pthread.h>

#include <atomic>

namespace ock {
namespace bss {

class ReadWriteLock {
public:
    ReadWriteLock()
    {
        pthread_rwlock_init(&mLock, nullptr);
    }
    ~ReadWriteLock()
    {
        pthread_rwlock_destroy(&mLock);
    }

    ReadWriteLock(const ReadWriteLock &) = delete;
    ReadWriteLock &operator=(const ReadWriteLock &) = delete;
    ReadWriteLock(ReadWriteLock &&) = delete;
    ReadWriteLock &operator=(ReadWriteLock &&) = delete;

    inline void LockRead()
    {
        pthread_rwlock_rdlock(&mLock);
    }
    inline void LockWrite()
    {
        pthread_rwlock_wrlock(&mLock);
    }
    inline void Unlock()
    {
        pthread_rwlock_unlock(&mLock);
    }

private:
    pthread_rwlock_t mLock{};
};

template <class T> class ReadLocker {
public:
    explicit ReadLocker(T *lock) : mLock(lock)
    {
        if (mLock != nullptr) {
            mLock->LockRead();
        }
    }

    ~ReadLocker()
    {
        if (mLock != nullptr) {
            mLock->Unlock();
        }
    }

private:
    ReadLocker(const ReadLocker &) = default;
    ReadLocker &operator=(const ReadLocker &) = default;
    ReadLocker(ReadLocker &&) noexcept = default;
    ReadLocker &operator=(ReadLocker &&) noexcept = default;

    T *mLock;
};

template <class T> class WriteLocker {
public:
    explicit WriteLocker(T *lock) : mLock(lock)
    {
        if (mLock != nullptr) {
            mLock->LockWrite();
        }
    }

    ~WriteLocker()
    {
        if (mLock != nullptr) {
            mLock->Unlock();
        }
    }

private:
    WriteLocker(const WriteLocker &) = default;
    WriteLocker &operator=(const WriteLocker &) = default;
    WriteLocker(WriteLocker &&) noexcept = default;
    WriteLocker &operator=(WriteLocker &&) noexcept = default;

    T *mLock;
};

class SpinLock {
public:
    SpinLock() = default;
    ~SpinLock() = default;

    SpinLock(const SpinLock &) = delete;
    SpinLock &operator=(const SpinLock &) = delete;
    SpinLock(SpinLock &&) = delete;
    SpinLock &operator=(SpinLock &&) = delete;

    inline void TryLock()
    {
        mFlag.test_and_set(std::memory_order_acquire);
    }

    inline void Lock()
    {
        while (mFlag.test_and_set(std::memory_order_acquire)) {
        }
    }

    inline void UnLock()
    {
        mFlag.clear(std::memory_order_release);
    }

private:
    std::atomic_flag mFlag = ATOMIC_FLAG_INIT;
};

}  // namespace bss
}  // namespace ock
#endif  // BOOST_SS_LOCK_H
