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

#ifndef BOOST_SS_VALUE_H
#define BOOST_SS_VALUE_H
#include <iomanip>

#include "binary/byte_buffer.h"
#include "binary/key/key.h"
#include "bss_types.h"
#include "value_type.h"

namespace ock {
namespace bss {
struct Value {
public:
    /**
     * Initialize value.
     * @param valueType value type.
     * @param valueLen value length.
     * @param valueData value data.
     * @param seqId value seq id.
     * @param buffer buffer object, store value data, for lifecycle management of this value.
     */
    inline void Init(uint8_t valueType, uint32_t valueLen, const uint8_t *valueData, uint64_t seqId,
                     const BufferRef &buffer)
    {
        mValueType = valueType;
        mValueLen = valueLen;
        mValueData = valueData;
        mSeqId = seqId;
        mBuffer = buffer;
    }

    inline void Init(uint8_t valueType, uint32_t valueLen, const uint8_t *valueData, uint64_t seqId)
    {
        Init(valueType, valueLen, valueData, seqId, nullptr);
    }

    inline void Init(const uint8_t *valueData, uint32_t valueLen, const BufferRef &buffer)
    {
        Init(NO_0, valueLen, valueData, NO_0, buffer);
    }

    /**
     * Initialize value.
     * @param valueType value type.
     * @param seqId value seq id.
     */
    inline void Init(uint8_t valueType, uint64_t seqId)
    {
        mValueType = valueType;
        mSeqId = seqId;
    }

    /**
     * Initialize value by other value.
     * @param other other value.
     */
    inline void Init(const Value &other)
    {
        mValueType = other.mValueType;
        mValueLen = other.mValueLen;
        mValueData = other.mValueData;
        mSeqId = other.mSeqId;
        mBuffer = other.mBuffer;
    }

    inline bool IsNull() const
    {
        return mBuffer == nullptr || mValueData == nullptr;
    }

    /**
     * Get value type, it define in @ValueType
     * @return value type.
     */
    inline uint8_t ValueType() const
    {
        return mValueType;
    }

    /**
     * Get value length.
     * @return value length.
     */
    inline uint32_t ValueLen() const
    {
        return mValueLen;
    }

    /**
     * Get value data.
     * @return value data, it is store in BufferRef.
     */
    inline const uint8_t *ValueData() const
    {
        return mValueData;
    }

    /**
     * Get value seq id, seq id is for updating value, the newer value has bigger id.
     * @return seq id.
     */
    inline uint64_t SeqId() const
    {
        return mSeqId;
    }

    /**
     * Set the seqId
     * @param seqId seqId
     */
    inline void SetSeqId(uint64_t seqId)
    {
        mSeqId = seqId;
    }

    /**
     * Buffer object to store value data, it used for lifecycle management of this value.
     * @return buffer object.
     */
    inline BufferRef Buffer() const
    {
        return mBuffer;
    }

    using BufferAllocator = std::function<BufferRef(uint32_t size)>;
    /**
     * Merge with older value.
     * @param olderValue older value.
     * @param allocator allocator for allocate merged value buffer.
     * @return return BSS_OK if success, else return BSS_ERR.
     */
    inline BResult MergeWithOlderValue(const Value &olderValue, BufferAllocator allocator)
    {
        if (mValueType == APPEND) {
            if (olderValue.mValueType == DELETE) {
                // update value type.
                mValueType = PUT;
                return BSS_OK;
            } else {
                // merge value
                auto mergedValue = MergeValue(*this, olderValue, allocator);
                if (mergedValue.second == nullptr) {
                    LOG_WARN("Failed to merge value." << olderValue.ToString() << ", " << ToString());
                    return BSS_ALLOC_FAIL;
                }
                // update value.
                mValueType = olderValue.ValueType();
                mValueLen = mergedValue.first;
                mValueData = mergedValue.second->Data();
                // update buffer.
                mBuffer = mergedValue.second;
                return BSS_OK;
            }
        }
        return BSS_OK;
    }

    /**
     * Merge with newer value.
     * @param newerValue newer value.
     * @param allocator allocator for allocate merged value buffer.
     * @return return BSS_OK if success, else return BSS_ERR.
     */
    inline BResult MergeWithNewerValue(const Key &key, const Value &newerValue, BufferAllocator allocator,
                                       std::function<void(const Key &, const Value &)> action)
    {
        mSeqId = newerValue.mSeqId;
        if (newerValue.mValueType == APPEND) {
            if (mValueType == DELETE) {
                // update value type.
                mValueType = PUT;
                mValueData = newerValue.mValueData;
                mValueLen = newerValue.mValueLen;
                mBuffer = newerValue.mBuffer;
                return BSS_OK;
            } else {
                // merge value
                auto mergedValue = MergeValue(newerValue, *this, allocator);
                if (mergedValue.second == nullptr) {
                    LOG_WARN("Failed to merge value." << ToString() << ", " << newerValue.ToString());
                    return BSS_ALLOC_FAIL;
                }
                // update value.
                mValueLen = mergedValue.first;
                mValueData = mergedValue.second->Data();
                // update buffer.
                mBuffer = mergedValue.second;
                return BSS_OK;
            }
        }
        action(key, *this);
        mValueType = newerValue.mValueType;
        mValueData = newerValue.mValueData;
        mValueLen = newerValue.mValueLen;
        mBuffer = newerValue.mBuffer;
        return BSS_OK;
    }

    /**
     * For printing value.
     * @return value string.
     */
    inline std::string ToString() const
    {
        std::ostringstream oss;
        oss << "ValueType: " << static_cast<int>(mValueType) << ", ValueLen: " << mValueLen << ", seqId: " << mSeqId
            << ", ValueData: [";
        if (UNLIKELY(mValueData != nullptr)) {
            uint32_t printLen = std::min(NO_10, mValueLen);
            for (uint32_t i = 0; i < printLen; i++) {
                oss << std::hex << std::uppercase << std::setw(NO_2) << std::setfill('0')
                    << static_cast<int>(mValueData[i]);
            }
        } else {
            oss << "null";
        }

        oss << "]";
        return oss.str();
    }

protected:
    uint64_t mSeqId{ INVALID_U64 };
    uint8_t mValueType{ INVALID_U8 };
    uint8_t mReserved[3]{};
    uint32_t mValueLen{ 0 };
    const uint8_t *mValueData{ nullptr };

    BufferRef mBuffer{ nullptr };

private:
    /**
     * Merge two value together, older value is before newer value in merged value.
     * @param newerValue newer value.
     * @param olderValue older value.
     * @param allocator allocator for allocate merged value buffer.
     * @return merged value length and buffer object.
     */
    inline static std::pair<uint32_t, BufferRef> MergeValue(const Value &newerValue, const Value &olderValue,
                                                            BufferAllocator allocator)
    {
        // allocate new buffer for merged value.
        uint32_t olderValueLen = olderValue.ValueLen();
        uint32_t newerValueLen = newerValue.ValueLen();
        if (UNLIKELY(UINT32_MAX - olderValueLen < newerValueLen)) {
            LOG_ERROR("Merge value too long:" << olderValueLen << "," << newerValueLen);
            return { 0, nullptr };
        }
        uint32_t mergedValueLen = olderValueLen + newerValueLen;
        BufferRef buffer = allocator(mergedValueLen);
        if (UNLIKELY(buffer == nullptr)) {
            LOG_WARN("Failed to allocate buffer. length:" << mergedValueLen);
            return { 0, nullptr };
        }

        // copy older value to buffer.
        errno_t ret1 = memcpy_s(buffer->Data(), mergedValueLen, olderValue.ValueData(), olderValueLen);
        if (UNLIKELY(ret1 != EOK)) {
            LOG_ERROR("memcpy_s failed for older value, ret = " << ret1);
            return { 0, nullptr };
        }
        // append newer value to buffer.
        errno_t ret2 = memcpy_s(buffer->Data() + olderValueLen, mergedValueLen - olderValueLen, newerValue.ValueData(),
                                newerValueLen);
        if (UNLIKELY(ret2 != EOK)) {
            LOG_ERROR("memcpy_s failed for newer value, ret = " << ret2);
            return { 0, nullptr };
        }
        return { mergedValueLen, buffer };
    }
};
using ValuePtr = Value *;
using ValueRef = std::shared_ptr<Value>;
using BlobValueTransformFunc = std::function<BResult(uint64_t, uint32_t, uint64_t, Value &)>;
}  // namespace bss
}  // namespace ock
#endif  // BOOST_SS_VALUE_H
