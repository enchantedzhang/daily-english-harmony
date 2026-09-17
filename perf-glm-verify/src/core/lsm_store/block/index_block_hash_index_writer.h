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

#ifndef BOOST_SS_INDEX_BLOCK_HASH_INDEX_WRITER_H
#define BOOST_SS_INDEX_BLOCK_HASH_INDEX_WRITER_H

#include <memory>
#include <vector>

#include "common/io/output_view.h"
#include "include/bss_types.h"

namespace ock {
namespace bss {
class IndexBlockHashIndexWriter {
public:
    explicit IndexBlockHashIndexWriter(float loadRatio) : mLoadRatio(loadRatio)
    {
    }

    inline uint32_t CurrentEstimateSize() const
    {
        return mEstimateSize;
    }

    inline uint32_t IncrementalSize()
    {
        return NO_8;
    }

    BResult Build(OutputViewRef &outputView);

    BResult Build(OutputViewRef &outputView, uint64_t startDataBlockIndex, uint64_t endDataBlockIndex);

    inline void Reset()
    {
        mStartHashVec.clear();
        mEndHashVec.clear();
        mEstimateSize = 0;
    }

    inline void Add(uint32_t startHash, uint32_t endHash)
    {
        mStartHashVec.emplace_back(startHash);
        mEndHashVec.emplace_back(endHash);
        mEstimateSize += sizeof(startHash) + sizeof(endHash);
    }

private:
    uint32_t GetNumBuckets(uint32_t numEntries) const;

private:
    float mLoadRatio;
    std::vector<uint32_t> mStartHashVec;
    std::vector<uint32_t> mEndHashVec;
    uint32_t mEstimateSize = 0;
};
using IndexBlockHashIndexWriterRef = std::shared_ptr<IndexBlockHashIndexWriter>;

class HashIndexReader {
public:
    HashIndexReader(const ByteBufferRef &buffer, uint32_t backwardOffset)
    {
        uint32_t offset = buffer->Capacity() - backwardOffset;
        buffer->ReadUint32(mNumBuckets, offset - NO_4);
        buffer->ReadAt(reinterpret_cast<uint8_t *>(&mMinHash), sizeof(uint32_t), offset - NO_12);
        buffer->ReadAt(reinterpret_cast<uint8_t *>(&mMaxHash), sizeof(uint32_t), offset - NO_8);
        if (UNLIKELY(mNumBuckets == 0)) {
            LOG_ERROR("Unexpected: mNumBuckets is zero.");
            return;
        }
        mBucketSize = (mMaxHash - mMinHash + mNumBuckets) / mNumBuckets;
        mBucketOffset = offset - (mNumBuckets * NO_8 + NO_12);
    }

    inline uint64_t GetMinHash() const
    {
        return mMinHash;
    }

    inline uint32_t GetBucketOffset() const
    {
        return mBucketOffset;
    }

    uint64_t GetBucket(uint32_t hash, const ByteBufferRef &buffer);

private:
    uint32_t mNumBuckets = 0;
    uint64_t mBucketSize = 0;
    uint32_t mBucketOffset = 0;
    uint64_t mMinHash = 0;
    uint64_t mMaxHash = 0;
};
using HashIndexReaderRef = std::shared_ptr<HashIndexReader>;

}  // namespace bss
}  // namespace ock
#endif  // BOOST_SS_INDEX_BLOCK_HASH_INDEX_WRITER_H