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

#ifndef BOOST_SS_BINARY_DATA_H
#define BOOST_SS_BINARY_DATA_H

#include <cstring>
#include <iomanip>
#include <memory>
#include <sstream>

#include "bss_def.h"
#include "bss_types.h"
#include "buffer.h"
#include "value_type.h"

namespace ock {
namespace bss {
struct BinaryData {
public:
    BinaryData() = default;
    inline void Init(const uint8_t *valueData, uint32_t valueLen, const BufferRef &buffer = nullptr)
    {
        mLength = valueLen;
        mData = valueData;
        mBuffer = buffer;
    }

    BinaryData(const uint8_t *data, uint32_t length, const BufferRef &buffer = nullptr)
        : mLength(length), mData(data), mBuffer(buffer)
    {
    }

    inline uint32_t Length() const
    {
        return mLength;
    }

    inline const uint8_t *Data() const
    {
        return mData;
    }

    inline BufferRef Buffer() const
    {
        return mBuffer;
    }

    inline bool IsNull()
    {
        return mData == nullptr;
    }

    inline bool operator==(const BinaryData &other) const
    {
        return mLength == other.mLength && mData && other.mData &&
               (mData == other.mData || memcmp(mData, other.mData, mLength) == 0);
    }

    inline std::string ToString() const
    {
        std::ostringstream oss;
        oss << "[";
        if (UNLIKELY(mData != nullptr)) {
            uint32_t printLen = std::min(NO_20, mLength);
            for (uint32_t i = 0; i < printLen; i++) {
                oss << std::hex << std::uppercase << std::setw(NO_2) << std::setfill('0') << static_cast<int>(mData[i]);
            }
        } else {
            oss << "null";
        }
        oss << "]";
        return oss.str();
    }

protected:
    uint32_t mLength{ 0 };
    const uint8_t *mData{ nullptr };
    BufferRef mBuffer{ nullptr };
};
using BinaryDataRef = std::shared_ptr<BinaryData>;

struct PQBinaryData {
    BinaryData mData;
    ValueType mValueType;
    uint32_t mHashCode;

    inline bool IsValid()
    {
        return !mData.IsNull();
    }

    inline const uint8_t *Data() const
    {
        return mData.Data();
    }
};

struct PQBinaryDataComparator {
    int operator()(const PQBinaryData &a, const PQBinaryData &b) const
    {
        return Compare(a.mData.Data(), a.mData.Length(), b.mData.Data(), b.mData.Length());
    }

    static int Compare(const PQBinaryData &a, const PQBinaryData &b)
    {
        return Compare(a.mData.Data(), a.mData.Length(), b.mData.Data(), b.mData.Length());
    }

    static int Compare(const uint8_t *a, uint32_t aLen, const uint8_t *b, uint32_t bLen)
    {
        int cmp = ComparePrefix(a, aLen, b, bLen);
        return cmp != 0 ? cmp : static_cast<int>(aLen) - static_cast<int>(bLen);
    }

    static int ComparePrefix(const uint8_t *a, uint32_t aLen, const uint8_t *b, uint32_t bLen)
    {
        uint32_t minLen = std::min(aLen, bLen);
        for (uint32_t i = 0; i < minLen; ++i) {
            // 将字节当作无符号数比较
            uint8_t aByte = a[i];
            uint8_t bByte = b[i];

            if (aByte != bByte) {
                return (aByte & 0xFF) - (bByte & 0xFF);
            }
        }
        return 0;
    }
    static int ComparePrefix(const PQBinaryData &a, const PQBinaryData &b)
    {
        return ComparePrefix(a.mData.Data(), a.mData.Length(), b.mData.Data(), b.mData.Length());
    }
};

struct PQBinaryDataSetComparator {
    bool operator()(const PQBinaryData &a, const PQBinaryData &b) const
    {
        return PQBinaryDataComparator::Compare(a, b) < 0;
    }
};
}  // namespace bss
}  // namespace ock

#endif