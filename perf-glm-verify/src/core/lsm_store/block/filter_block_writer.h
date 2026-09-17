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

#ifndef BOOST_SS_FILTER_BLOCK_WRITER_H
#define BOOST_SS_FILTER_BLOCK_WRITER_H

#include <memory>
#include <set>
#include <unordered_set>
#include <vector>

#include "binary/slice_binary.h"
#include "common/block.h"

namespace ock {
namespace bss {

enum FilterPolicyType : uint8_t {
    NONE_FILTER = 0,
    MIXED_HASH_FILTER = 1,
    PRIMARY_HASH_FILTER = 2,
};

class FilterBlockWriter {
public:
    explicit FilterBlockWriter(FilterPolicyType type) : mFilterType(type)
    {
    }

    virtual ~FilterBlockWriter() = default;

    virtual void Add(uint32_t hashCode) = 0;

    virtual uint32_t CalcFilterBlockLen() = 0;

    virtual BResult Finish(uint8_t *output, uint32_t outputSize)
    {
        return BSS_INNER_ERR;
    }

protected:
    FilterPolicyType mFilterType;
};
using FilterBlockWriterRef = std::shared_ptr<FilterBlockWriter>;

class HashFilterBlockWriter : public FilterBlockWriter {
public:
    ~HashFilterBlockWriter() override
    {
        LOG_DEBUG("Delete HashFilterBlockWriter success.");
    }

    explicit HashFilterBlockWriter(FilterPolicyType type = FilterPolicyType::MIXED_HASH_FILTER)
        : FilterBlockWriter(type)
    {
    }

    void Add(uint32_t hashCode) override
    {
        mHashCodes.emplace_back(hashCode);
    }

    int32_t Finish(uint8_t *output, uint32_t outputSize) override;

    uint32_t CalcFilterBlockLen() override
    {
        if (UNLIKELY(mHashCodes.size() == 0)) {
            LOG_ERROR("mHashCode size cannot be 0.");
            return 0;
        }
        double bits = -(static_cast<double>(mHashCodes.size()) * log(NO_0_0_1) / log(NO_2_0) / log(NO_2_0));
        mProbes = static_cast<uint32_t>(bits / static_cast<double>(mHashCodes.size()) * log(NO_2_0));
        mProbes = mProbes > NO_30 ? NO_30 : mProbes < NO_1 ? NO_1 : mProbes;
        if (bits < NO_64) {
            bits = NO_64;
        }
        mFilterDataLen = (bits + NO_7) / NO_8;
        LOG_DEBUG("bits: " << bits << " mProbes: " << mProbes << " hashVecSize: " << mHashCodes.size());
        return mFilterDataLen + NO_2;
    }

private:
    std::vector<uint32_t> mHashCodes;
    uint32_t mProbes = 0;
    uint32_t mFilterDataLen = 0;
};

class FilterBlock : public Block {
public:
    explicit FilterBlock(ByteBufferRef &byteBuffer) : Block(byteBuffer)
    {
    }

    BResult Init()
    {
        uint32_t capacity = mBuffer->Capacity();
        if (UNLIKELY(capacity < NO_2)) {
            LOG_ERROR("Init filter block fail, buffer is empty.");
            return BSS_ERR;
        }
        uint8_t type = 0;
        auto ret = mBuffer->ReadUint8(type, capacity - NO_1);
        RETURN_NOT_OK(ret);
        mLength = capacity - NO_1;
        ret = mBuffer->ReadAt(&mProbes, NO_1, mLength - 1);
        RETURN_NOT_OK(ret);
        return BSS_OK;
    }

    ~FilterBlock() override = default;

    BlockType GetBlockType() override
    {
        return BlockType::FILTER;
    }

    bool KeyMayMatch(uint32_t mixHashCode);

private:
    uint32_t mLength = 0;
    uint8_t mProbes = 0;
};
using FilterBlockRef = std::shared_ptr<FilterBlock>;

}  // namespace bss
}  // namespace ock
#endif  // BOOST_SS_FILTER_BLOCK_WRITER_H