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

#include "index_block_hash_index_writer.h"

#include <cmath>

namespace ock {
namespace bss {
BResult IndexBlockHashIndexWriter::Build(OutputViewRef &outputView)
{
    uint32_t numDataBlocks = mStartHashVec.size();
    if (mStartHashVec.empty() || numDataBlocks - 1 >= mEndHashVec.size()) {
        LOG_ERROR("index out of bounds, mEndHashVec size is: " << mEndHashVec.size()
                                                               << ", mStartHashVec size is: " << mStartHashVec.size());
        return BSS_ERR;
    }
    uint64_t minHash = mStartHashVec.at(0);
    uint64_t maxHash = mEndHashVec.at(numDataBlocks - 1);
    uint32_t numBuckets = GetNumBuckets(numDataBlocks);
    uint64_t bucketSize = (maxHash - minHash + numBuckets) / numBuckets;

    uint64_t nextDataBlockIndex = 0;

    for (uint32_t i = 0; i < numBuckets; i++) {
        uint64_t inclusiveBucketMinHash = minHash + i * bucketSize;
        uint64_t exclusiveBucketMaxHash = inclusiveBucketMinHash + bucketSize;
        uint64_t startDataBlockIndex = nextDataBlockIndex;
        uint64_t endDataBlockIndex = startDataBlockIndex;

        while (endDataBlockIndex < numDataBlocks) {
            if (endDataBlockIndex >= mStartHashVec.size() || endDataBlockIndex >= mEndHashVec.size()) {
                LOG_ERROR("endDataBlockIndex out of bounds, mEndHashVec size is: "
                          << mEndHashVec.size() << ", mStartHashVec size is: " << mStartHashVec.size());
                return BSS_ERR;
            }
            uint64_t minh = mStartHashVec.at(endDataBlockIndex);
            uint64_t maxh = mEndHashVec.at(endDataBlockIndex);
            if (minh >= exclusiveBucketMaxHash) {
                break;
            }

            if (maxh < inclusiveBucketMinHash) {
                startDataBlockIndex = ++endDataBlockIndex;
                continue;
            }
            endDataBlockIndex++;
        }

        int32_t retVal = Build(outputView, startDataBlockIndex, endDataBlockIndex);
        RETURN_NOT_OK_NO_LOG(retVal);
        nextDataBlockIndex = endDataBlockIndex - 1;
    }

    auto ret = outputView->WriteUint32(static_cast<uint32_t>(minHash));
    RETURN_NOT_OK_NO_LOG(ret);
    ret = outputView->WriteUint32(static_cast<uint32_t>(maxHash));
    RETURN_NOT_OK_NO_LOG(ret);
    ret = outputView->WriteUint32(numBuckets);
    return ret;
}

BResult IndexBlockHashIndexWriter::Build(OutputViewRef &outputView, uint64_t startDataBlockIndex,
                                         uint64_t endDataBlockIndex)
{
    if (endDataBlockIndex < startDataBlockIndex) {
        LOG_ERROR("startDataBlockIndex must be greater than endDataBlockIndex, startDataBlockIndex: "
                  << startDataBlockIndex << ", endDataBlockIndex: " << endDataBlockIndex << ". ");
        return BSS_ERR;
    }
    uint64_t nblk = endDataBlockIndex - startDataBlockIndex;
    int32_t retVal;
    if (nblk > 0) {
        retVal = outputView->WriteUint64((startDataBlockIndex << NO_32) | nblk);
        RETURN_NOT_OK_NO_LOG(retVal);
    } else {
        int64_t defaultValue = -1L;
        retVal = outputView->Write(reinterpret_cast<uint8_t *>(&defaultValue), sizeof(defaultValue));
        RETURN_NOT_OK_NO_LOG(retVal);
    }
    return BSS_OK;
}

uint32_t IndexBlockHashIndexWriter::GetNumBuckets(uint32_t numEntries) const
{
    uint32_t numBuckets = std::ceil((numEntries / mLoadRatio));
    if (numBuckets == 0) {
        numBuckets = 1;
    }
    return numBuckets;
}

uint64_t HashIndexReader::GetBucket(uint32_t hash, const ByteBufferRef &buffer)
{
    auto primaryKeyHash = static_cast<uint64_t>(hash);
    if (primaryKeyHash < mMinHash || primaryKeyHash > mMaxHash || mBucketSize == 0) {
        return UINT64_MAX;
    }
    uint32_t bucketIndex = (primaryKeyHash - mMinHash) / mBucketSize;
    if (bucketIndex >= mNumBuckets) {
        return UINT64_MAX;
    }
    uint64_t bucket = 0;
    auto ret = buffer->ReadUint64(bucket, mBucketOffset + bucketIndex * NO_8);
    if (UNLIKELY(ret != BSS_OK)) {
        LOG_ERROR("Read bucket fail, pos: " << mBucketOffset + bucketIndex * NO_8 << ", ret: " << ret);
    }
    return bucket;
}

}  // namespace bss
}  // namespace ock