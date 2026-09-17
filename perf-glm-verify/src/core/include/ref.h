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

#ifndef BOOST_STATE_STORE_UTIL_REF_H
#define BOOST_STATE_STORE_UTIL_REF_H

#include <atomic>
#include <functional>

namespace ock {
namespace bss {
// base class of the smart pointer
class Referable {
public:
    explicit Referable() : mRefCount(0)
    {
    }

    virtual ~Referable() = default;

    inline long IncRefCount()
    {
        return ++mRefCount;
    }
    inline long DecRefCount()
    {
        return --mRefCount;
    }
    inline long GetRefCount() const
    {
        return mRefCount;
    }
    inline void SetRefCount(long count)
    {
        mRefCount = count;
    }

private:
    std::atomic<long> mRefCount;
};

// smart pointer class
template <class T> class Ref {
    static_assert(std::is_base_of<Referable, T>::value, "T must inherit from Referable");

public:
    Ref() = default;

    Ref(const Ref<T> &other) : mObj(nullptr)
    {
        Acquire(other.mObj);
        mObj = other.mObj;
    }

    // fix: can't be explicit
    Ref(T *newObj) : mObj(newObj)
    {
        Acquire(mObj);
    }

    // 定义一个别名模板，简化 std::enable_if 的使用
    template <typename U> using EnableIfBaseOf = typename std::enable_if<std::is_base_of<T, U>::value, int>::type;

    // 接受子类 Ref 对象的构造函数，用于自动转换
    template <typename U> Ref(const Ref<U> &other, EnableIfBaseOf<U> = 0) : mObj(nullptr)
    {
        T *basePtr = static_cast<T *>(other.Get());
        Acquire(basePtr);
        mObj = basePtr;
    }

    virtual ~Ref()
    {
        Release();
    }

    inline T *operator->() const
    {
        return mObj;
    }

    inline Ref<T> &operator=(const Ref<T> &other)
    {
        if (this != &other) {
            Release();
            Acquire(other.mObj);
            mObj = other.mObj;
        }
        return *this;
    }

    inline Ref<T> &operator=(T *newObj)
    {
        if (mObj != newObj) {
            Release();
            Acquire(newObj);
            mObj = newObj;
        }
        return *this;
    }

    inline bool IsNull() const
    {
        return mObj == nullptr;
    }

    inline bool operator==(const Ref<T> &other) const
    {
        return mObj == other.mObj;
    }

    inline bool operator==(T *other) const
    {
        return mObj == other;
    }

    inline bool operator<(const Ref<T> &other) const
    {
        return mObj < other.mObj;
    }

    inline bool operator<(T *other) const
    {
        return mObj < other;
    }

    inline bool operator!=(const Ref<T> &other) const
    {
        return mObj != other.mObj;
    }

    inline bool operator!=(T *other) const
    {
        return mObj != other;
    }

    // important: cannot assign to c++ origin pointer, only to be ref pointer object
    inline T *Get() const
    {
        return mObj;
    }

    inline void Set(T *newObj)
    {
        if (mObj == newObj) {
            return;
        }
        Release();
        Acquire(newObj);
        mObj = newObj;
    }

    inline long GetRefCount() const
    {
        return mObj ? mObj->GetRefCount() : 0;
    }

    // construction for std::move
    Ref(Ref<T> &&other) noexcept : mObj(other.mObj)
    {
        other.mObj = nullptr;
    }

    const T &operator*() const noexcept
    {
        return *mObj;
    }

    T &operator*() noexcept
    {
        return *mObj;
    }

    Ref<T> &operator=(Ref<T> &&other) noexcept
    {
        if (this != &other) {
            Release();
            mObj = other.mObj;
            other.mObj = nullptr;
        }
        return *this;
    }

private:
    T *mObj = nullptr;

    inline void Release()
    {
        if (mObj != nullptr && mObj->DecRefCount() == 0) {
            delete mObj;
            mObj = nullptr;
        }
    }

    inline void Acquire(T *newObj)
    {
        if (newObj != nullptr) {
            newObj->IncRefCount();
        }
    }
};

template <typename T, typename... Args> inline Ref<T> MakeRef(Args &&...args)
{
    return Ref<T>(new T(std::forward<Args>(args)...));
}

template <typename T> class EnableRefFromThis {
protected:
    EnableRefFromThis() = default;
    ~EnableRefFromThis() = default;

    Ref<T> RefFromThis()
    {
        return Ref<T>(static_cast<T *>(this));
    }
};

// 为 Ref 实现类似 std::static_pointer_cast 的功能
template <typename To, typename From> Ref<To> StaticPointerCast(const Ref<From> &from)
{
    To *toPtr = static_cast<To *>(from.Get());
    return Ref<To>(toPtr);
}

// 为 Ref 实现类似 std::dynamic_pointer_cast 的功能
template <typename To, typename From> Ref<To> DynamicPointerCast(const Ref<From> &from)
{
    To *toPtr = dynamic_cast<To *>(from.Get());
    return Ref<To>(toPtr);
}

// 定义一个通用的 Ref 哈希函数模板
template <class T> struct RefHash {
    std::size_t operator()(const Ref<T> &ref) const
    {
        return std::hash<T *>()(ref.Get());
    }
};
}  // namespace bss
}  // namespace ock

#endif
