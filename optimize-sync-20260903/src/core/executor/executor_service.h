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

#ifndef BOOST_STATE_STORE_EXECUTOR_SERVICE_H
#define BOOST_STATE_STORE_EXECUTOR_SERVICE_H

#include <atomic>
#include <condition_variable>
#include <list>
#include <mutex>
#include <thread>
#include <vector>

#include "common/bss_def.h"
#include "common/bss_log.h"
#include "include/bss_err.h"
#include "include/ref.h"

namespace ock {
namespace bss {
enum class RunnableType : char {
    NORMAL = 0,
    STOP = 1,
};

class Runnable;
using RunnablePtr = std::shared_ptr<Runnable>;
class Runnable {
public:
    Runnable() = default;
    virtual ~Runnable() = default;

    /* call SetRunFinish() when Run() finish,
        if you want to wait until the execution is complete */
    virtual void Run()
    {
    }

    void Type(RunnableType type)
    {
        mType = type;
    }

    RunnableType Type() const
    {
        return mType;
    }

    void SetTag(std::string tag)
    {
        mTag = std::move(tag);
    }

    std::string GetTag()
    {
        return mTag;
    }

private:
    RunnableType mType = RunnableType::NORMAL;
    std::string mTag = "default";
};

class RunnableQueue {
public:
    explicit RunnableQueue(uint32_t capacity) : mMaxCapacity(capacity), mQueue()
    {
    }

    virtual ~RunnableQueue()
    {
        std::lock_guard<std::mutex> lk(mMutex);
        mQueue.clear();
    }

    bool Enqueue(const RunnablePtr &task, bool flag = false)
    {
        std::unique_lock<std::mutex> lk(mMutex);
        if (task == nullptr) {
            LOG_ERROR("Unexpected nullptr error for task.");
            return false;
        }
        if (mQueue.size() >= mMaxCapacity && !flag) {
            LOG_WARN("Enqueue task:" << task->GetTag() << " failed, exceed capacity:" << mMaxCapacity);
            return false;
        }
        if (flag) {
            mQueue.push_front(task);
        } else {
            mQueue.push_back(task);
        }
        mCondVar.notify_one();
        return true;
    }

    bool EnqueueStop()
    {
        RunnablePtr task = std::make_shared<Runnable>();
        if (UNLIKELY(task == nullptr)) {
            LOG_ERROR("Unexpected nullptr error for task, maybe out of memory");
            return false;
        }

        task->Type(RunnableType::STOP);
        {
            std::unique_lock<std::mutex> lk(mMutex);
            mQueue.push_front(task);
            lk.unlock();
            mCondVar.notify_one();
        }

        return true;
    }

    void Dequeue(RunnablePtr &value)
    {
        std::unique_lock<std::mutex> lk(mMutex);
        mCondVar.wait(lk, [this] { return !mQueue.empty(); });
        value = mQueue.front();
        mQueue.pop_front();
    }

    uint32_t QueueSize()
    {
        std::unique_lock<std::mutex> lk(mMutex);
        return mQueue.size();
    }

private:
    uint32_t mMaxCapacity;
    std::list<RunnablePtr> mQueue;
    std::mutex mMutex;
    std::condition_variable mCondVar;
};

class ExecutorService;
using ExecutorServicePtr = std::shared_ptr<ExecutorService>;
class ExecutorService {
public:
    ~ExecutorService();
    ExecutorService(uint16_t threadNum, uint32_t queueCapacity);
    static ExecutorServicePtr Create(uint16_t threadNum, uint32_t queueCapacity = 10000);

    bool Start();
    void Stop();
    bool Execute(const RunnablePtr &runnable, bool flag = false);

    inline void SetThreadName(const std::string &name)
    {
        mThreadName = name;
    }

    uint32_t QueueSize()
    {
        return mRunnableQueue.QueueSize();
    }

private:
    void RunInThread();
    BResult DoRunnable(bool &flag);
    void FreeThread();

private:
    RunnableQueue mRunnableQueue;
    uint16_t mThreadNum;
    std::vector<std::thread *> mThreads;

    std::atomic<bool> mStarted;
    std::atomic<bool> mStopped;
    std::atomic<uint16_t> mStartedThreadNum;

    std::string mThreadName;
};
using ExecutorServiceRef = std::shared_ptr<ExecutorService>;
}  // namespace bss
}  // namespace ock

#endif