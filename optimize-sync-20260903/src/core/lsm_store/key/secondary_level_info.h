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

#ifndef BOOST_SS_SECONDARY_LEVEL_INFO_H
#define BOOST_SS_SECONDARY_LEVEL_INFO_H

#include "common/block.h"
#include "full_key_util.h"
#include "primary_key_info.h"

namespace ock {
namespace bss {

class SecondaryLevelInfo {
public:
    virtual BResult GetPair(PrimaryKeyInfo &primaryKeyInfo, const Key &key, Value &value, ByteBufferRef byteBuffer) = 0;

    virtual uint16_t GetStateId(const ByteBufferRef &byteBuffer) = 0;
};
using SecondaryLevelInfoRef = std::shared_ptr<SecondaryLevelInfo>;

class SingleSecondaryLevelInfo : public SecondaryLevelInfo {
public:
    SingleSecondaryLevelInfo(uint32_t secondaryKeyOffset, const MemManagerRef &memManager, FileProcHolder holder)
        : mSecondaryKeyOffset(secondaryKeyOffset), mMemManager(memManager), mHolder(holder)
    {
    }

    BResult GetPair(PrimaryKeyInfo &primaryKeyInfo, const Key &key, Value &value, ByteBufferRef byteBuffer) override;

    uint16_t GetStateId(const ByteBufferRef &byteBuffer) override
    {
        return FullKeyUtil::ReadStateId(byteBuffer, mSecondaryKeyOffset);
    }

private:
    uint32_t mSecondaryKeyOffset;
    MemManagerRef mMemManager = nullptr;
    FileProcHolder mHolder;
};

class MultiSecondaryLevelInfo : public SecondaryLevelInfo,
                                public std::enable_shared_from_this<MultiSecondaryLevelInfo> {
public:
    MultiSecondaryLevelInfo(uint32_t numBytesForSecondaryKeyIndexElement, uint32_t numSecondaryKeys,
                            uint32_t secondaryKeyIndexOffset, uint32_t firstSecondaryKeyOffset,
                            const MemManagerRef &memManager, FileProcHolder holder)
        : mNumBytesForSecondaryKeyIndexElement(numBytesForSecondaryKeyIndexElement),
          mNumSecondaryKeys(numSecondaryKeys),
          mSecondaryKeyIndexOffset(secondaryKeyIndexOffset),
          mFirstSecondaryKeyOffset(firstSecondaryKeyOffset),
          mMemManager(memManager),
          mHolder(holder)
    {
    }

    BResult GetPair(PrimaryKeyInfo &primaryKeyInfo, const Key &key, Value &value, ByteBufferRef byteBuffer) override;

    uint16_t GetStateId(const ByteBufferRef &byteBuffer) override
    {
        return FullKeyUtil::ReadStateId(byteBuffer, mFirstSecondaryKeyOffset);
    }

private:
    BlockRef mDataBlock = nullptr;
    uint32_t mNumBytesForSecondaryKeyIndexElement;
    uint32_t mNumSecondaryKeys;
    uint32_t mSecondaryKeyIndexOffset;
    uint32_t mFirstSecondaryKeyOffset;
    MemManagerRef mMemManager = nullptr;
    FileProcHolder mHolder;
};

}  // namespace bss
}  // namespace ock
#endif  // BOOST_SS_SECONDARY_LEVEL_INFO_H