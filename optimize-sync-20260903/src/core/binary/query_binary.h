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

#ifndef BOOST_SS_LOOKUP_BINARY_H
#define BOOST_SS_LOOKUP_BINARY_H

#include <deque>

#include "binary_data.h"
#include "hash.h"
#include "key/key.h"
#include "value/value.h"

namespace ock {
namespace bss {

struct QueryKey : public Key {
public:
    QueryKey(uint16_t stateId, uint32_t keyHashCode, const BinaryData &priKey, const BinaryData &secKey,
             BufferRef &buffer)
        : QueryKey(stateId, keyHashCode, priKey, secKey)
    {
        mBuffer = buffer;
    }

    QueryKey(uint16_t stateId, uint32_t keyHashCode, const BinaryData &priKey, const BinaryData &secKey)
    {
        mHasSecKey = StateId::HasSecKey(stateId);
        mHasNameSpace = StateId::HasNameSpace(stateId);

        mPriKey.StateId(stateId);
        mPriKey.KeyLen(priKey.Length());
        mPriKey.KeyData(priKey.Data());
        mPriKey.HashCode(keyHashCode ^ NsHashCode());

        mSecKey.KeyLen(secKey.Length());
        mSecKey.KeyData(secKey.Data());
        mSecKey.HashCode(HashCode::Hash(secKey.Data(), secKey.Length()));

        mMixedHashCode = stateId ^ mPriKey.HashCode() ^ mSecKey.HashCode();
    }

    QueryKey(uint16_t stateId, uint32_t keyHashCode, const BinaryData &priKey, BufferRef &buffer)
        : QueryKey(stateId, keyHashCode, priKey)
    {
        mBuffer = buffer;
    }

    QueryKey(uint16_t stateId, uint32_t keyHashCode, const BinaryData &priKey)
    {
        mHasSecKey = StateId::HasSecKey(stateId);
        mHasNameSpace = StateId::HasNameSpace(stateId);

        mPriKey.StateId(stateId);
        mPriKey.KeyLen(priKey.Length());
        mPriKey.KeyData(priKey.Data());
        mPriKey.HashCode(keyHashCode ^ NsHashCode());

        mMixedHashCode = stateId ^ mPriKey.HashCode();
    }

    QueryKey(uint16_t stateId, uint32_t keyHashCode, uint32_t secHashCode, const BinaryData &priKey,
             const BinaryData &secKey)
    {
        mHasSecKey = StateId::HasSecKey(stateId);
        mHasNameSpace = StateId::HasNameSpace(stateId);

        mPriKey.StateId(stateId);
        mPriKey.KeyLen(priKey.Length());
        mPriKey.KeyData(priKey.Data());
        mPriKey.HashCode(keyHashCode ^ NsHashCode());

        mSecKey.KeyLen(secKey.Length());
        mSecKey.KeyData(secKey.Data());
        mSecKey.HashCode(secHashCode);

        mMixedHashCode = stateId ^ mPriKey.HashCode() ^ mSecKey.HashCode();
    }
};

struct CompositeValue : public Value {
public:
    inline void AddValue(Value &value)
    {
        mValues.emplace_back(value);
    }

    inline bool IsEmpty() const
    {
        return mValues.empty();
    }

    inline void GetValues(std::deque<Value> &values) const
    {
        values = std::move(mValues);
    }

private:
    std::deque<Value> mValues;
};
}  // namespace bss
}  // namespace ock

#endif  // BOOST_SS_LOOKUP_BINARY_H
