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
#ifndef BOOST_SS_ITERATOR_H
#define BOOST_SS_ITERATOR_H

#include <memory>
#include <stdexcept>
#include <vector>

#include "bss_log.h"
#include "include/ref.h"
namespace ock {
namespace bss {

enum Iterator_Result { Iterator_Result_Continue = 0, Iterator_Result_End = 1, Iterator_Result_Failed = 2 };

template <typename E> class Iterator : public Referable {
public:
    ~Iterator() override = default;

    virtual bool HasNext()
    {
        return false;
    }

    virtual E Next()
    {
        return {};
    }

    virtual void Close()
    {
    }

    virtual void PrintUsefulInfo()
    {
        LOG_INFO("virtual PrintUsefulInfo");
    }
};
template <typename E> using IteratorRef = Ref<Iterator<E>>;
template <typename E> using IteratorPtr = std::shared_ptr<Iterator<E>>;

template <typename E> class VectorIterator : public Iterator<E> {
public:
    // 构造函数：接受一个 std::vector 的引用（不获取所有权）
    explicit VectorIterator(std::vector<E> &vec) : mData(&vec, [](std::vector<E> *) {})
    {
    }  // 使用空删除器

    // 构造函数：接受一个 std::vector 的右值引用（移动语义，获取所有权）
    explicit VectorIterator(std::vector<E> &&vec)
        : mData(new std::vector<E>(std::move(vec)), [](std::vector<E> *ptr) { delete ptr; })
    {
    }

    // 检查是否还有下一个元素
    inline bool HasNext() override
    {
        return mIndex < mData->size();
    }

    // 获取下一个元素
    inline E Next() override
    {
        if (!HasNext()) {
            LOG_ERROR("No more elements in the vector.");
            return {};
        }
        return (*mData)[mIndex++];
    }

private:
    std::unique_ptr<std::vector<E>, void (*)(std::vector<E> *)> mData;  // 统一管理引用和所有权
    size_t mIndex = 0;                                                  // 当前迭代位置
};
template <typename E> using VectorIteratorRef = Ref<VectorIterator<E>>;

template <typename E> class ChainedIterator : public Iterator<E> {
public:
    ChainedIterator(std::vector<std::shared_ptr<Iterator<E>>> &&iterators)
        : mIterators(std::move(iterators)), mCurrentElement()
    {
        Advance();
    }

    bool HasNext() override
    {
        return mCurrentElement != nullptr;
    }

    E Next() override
    {
        if (!HasNext()) {
            throw std::out_of_range("No more elements in the chained iterator.");
        }
        E result = mCurrentElement;
        Advance();
        return result;
    }

    void Close() override
    {
        for (const auto &it : mIterators) {
            it->Close();
        }
    }

private:
    void Advance()
    {
        mCurrentElement = nullptr;
        while (mCurrentIterator < mIterators.size()) {
            if (mIterators[mCurrentIterator]->HasNext()) {
                mCurrentElement = mIterators[mCurrentIterator]->Next();
                break;
            }
            ++mCurrentIterator;
        }
    }

private:
    std::vector<std::shared_ptr<Iterator<E>>> mIterators;
    size_t mCurrentIterator = 0;
    E mCurrentElement;
};
}  // namespace bss
}  // namespace ock

#endif  // BOOST_SS_ITERATOR_H