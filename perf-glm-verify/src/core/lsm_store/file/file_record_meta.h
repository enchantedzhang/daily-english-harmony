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

#ifndef BOOST_SS_FILE_RECORD_META_H
#define BOOST_SS_FILE_RECORD_META_H

#include <cstdint>

namespace ock {
namespace bss {
/**
 * A compact runtime record summary stored with FileMetaData.
 *
 * Compaction uses this summary to decide whether a
 * single-record ListState PUT
 * file can be adopted without opening the file. A zero record count marks an
 *
 * unavailable summary, which cannot participate in file reuse. The checkpoint
 * representation retains the version-7
 * fields, but min sequence id and value
 * type are derived when serializing and are not stored in this runtime
 * object.
 */
class FileRecordMeta {
public:
    FileRecordMeta() = default;

    FileRecordMeta(uint32_t recordCount, uint64_t maxSeqId, uint32_t singleValueLength)
        : mMaxSeqId(maxSeqId), mRecordCount(recordCount), mSingleValueLength(singleValueLength)
    {
    }

    inline void UpdateMaxSeqId(uint64_t seqId)
    {
        if (seqId > mMaxSeqId) {
            mMaxSeqId = seqId;
        }
    }

    inline void SetSingleValueLength(uint32_t valueLength)
    {
        mSingleValueLength = valueLength;
    }

    inline void Finalize(uint32_t recordCount)
    {
        mRecordCount = recordCount;
        if (recordCount != 1) {
            mSingleValueLength = 0;
        }
    }

    inline uint32_t GetRecordCount() const
    {
        return mRecordCount;
    }

    inline uint64_t GetMaxSeqId() const
    {
        return mMaxSeqId;
    }

    inline uint32_t GetSingleValueLength() const
    {
        return mSingleValueLength;
    }

    inline bool IsSingleRecord() const
    {
        return mRecordCount == 1;
    }

    inline bool IsAvailable() const
    {
        return mRecordCount != 0;
    }

    inline bool Equals(const FileRecordMeta &other) const
    {
        return mRecordCount == other.mRecordCount && mMaxSeqId == other.mMaxSeqId &&
               mSingleValueLength == other.mSingleValueLength;
    }

private:
    uint64_t mMaxSeqId = 0;
    uint32_t mRecordCount = 0;
    uint32_t mSingleValueLength = 0;
};
}  // namespace bss
}  // namespace ock

#endif  // BOOST_SS_FILE_RECORD_META_H
