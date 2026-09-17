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

#ifndef BOOST_STATE_STORE_SLICEADDRESS_H
#define BOOST_STATE_STORE_SLICEADDRESS_H
#include <atomic>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <memory>
#include <random>

#include "common/io/file_input_view.h"
#include "common/io/file_output_view.h"
#include "data_slice.h"
#include "include/bss_err.h"
#include "include/compress_algo.h"
#include "include/ref.h"
#include "slice_status.h"
#include "snapshot/snapshot_meta.h"

namespace ock {
namespace bss {
class SliceAddress;
using SliceAddressRef = std::shared_ptr<SliceAddress>;
class SliceAddress {
public:
    explicit SliceAddress(uint64_t sliceId = GenerateRandomNumber())
    {
        mSliceId = sliceId;
    }

    SliceAddress(uint32_t dataLen, uint32_t checkSum, uint64_t accessNumber, uint64_t startOffset = 0,
                 uint64_t sliceId = GenerateRandomNumber())
        : mOriDataLen(dataLen),
          mStoredDataLen(dataLen),
          mCheckSum(checkSum),
          mBorn(accessNumber),
          mStartOffset(startOffset),
          mSliceId(sliceId)
    {
    }

    SliceAddress(SliceAddress &other, bool allowMetadataOnly = false)
        : mOriDataLen(other.mOriDataLen),
          mStoredDataLen(other.mStoredDataLen),
          mCompressAlgo(other.mCompressAlgo),
          mCheckSum(other.mCheckSum),
          mBorn(other.mBorn),
          mHitCount(other.mHitCount),
          mLocalAddress(other.mLocalAddress),
          mStartOffset(other.mStartOffset),
          mSliceId(other.mSliceId)
    {
        mSliceStatus.store(other.mSliceStatus.load());
        if (LIKELY(other.mDataSlice != nullptr)) {
            mDataSlice = std::make_shared<DataSlice>(*other.mDataSlice);
        } else if (!allowMetadataOnly && !other.IsEvicted()) {
            LOG_ERROR("Copy slice address failed, data slice is nullptr, dataLen:"
                      << other.mOriDataLen << ", status:" << static_cast<uint32_t>(other.mSliceStatus.load())
                      << ", checkSum:" << other.mCheckSum);
        }
    }

    SliceAddress(SliceAddress &other, SliceStatus snapshotStatus) : SliceAddress(other)
    {
        mSliceStatus.store(snapshotStatus);
    }

    SliceAddress &operator=(const SliceAddress &other) = delete;

    void Init(const DataSliceRef &dataSlice, uint64_t accessNumber);

    inline void SetChainIndex(uint32_t chainIndex)
    {
        if (mDataSlice != nullptr) {
            mDataSlice->SetChainIndex(chainIndex);
        }
    }

    inline uint32_t GetDataLen() const
    {
        return mOriDataLen;
    }

    inline uint32_t GetStoredDataLen() const
    {
        return mStoredDataLen;
    }

    inline CompressAlgo GetCompressAlgo() const
    {
        return mCompressAlgo;
    }

    inline uint32_t GetCheckSum() const
    {
        return mCheckSum;
    }

    inline DataSliceRef GetDataSlice()
    {
        return (mDataSlice == nullptr || mDataSlice->GetSlice() == nullptr ||
                mDataSlice->GetSlice()->GetByteBuffer() == nullptr) ?
                   nullptr :
                   mDataSlice;
    }

    inline SliceRef GetSlice()
    {
        return mDataSlice == nullptr ? nullptr : mDataSlice->GetSlice();
    }

    inline bool IsEvicted()
    {
        return mSliceStatus.load() == SliceStatus::EVICTED;
    }

    void SetFatherSlice(SliceAddressRef &sliceAddress)
    {
        std::lock_guard<std::mutex> lk(mStatusLock);
        mFatherSlice = sliceAddress;
    }

    inline bool IsTriggerFlush()
    {
        return mSliceStatus.load() == SliceStatus::FLUSHING;
    }

    float Score(uint64_t tick);

    void ReduceRequestCount(std::vector<SliceAddressRef> &invalidSliceAddressList);

    inline void AddRequestCount(uint32_t requestCount)
    {
        mHitCount += requestCount;
    }

    void SnapshotMeta(FileOutputViewRef localOutputView, SnapshotMetaRef &snapshotMeta);

    static BResult Restore(const FileInputViewRef &reader, SliceAddressRef &sliceAddress);

    inline void SetLocalAddress(std::string path)
    {
        mLocalAddress = path;
    }

    inline void SetRawLocalAddress(std::string path)
    {
        if (mFatherSlice == nullptr) {
            return;
        }
        mFatherSlice->SetLocalAddress(path);
        SetLocalAddress(path);
    }

    inline uint64_t GetSliceId() const
    {
        return mSliceId;
    }

    inline void SetStartOffset(uint64_t startOffset)
    {
        mStartOffset = startOffset;
    }

    inline void SetRawStartOffset(uint64_t startOffset)
    {
        if (mFatherSlice == nullptr) {
            return;
        }
        mFatherSlice->SetStartOffset(startOffset);
        SetStartOffset(startOffset);
    }

    inline uint64_t GetStartOffset() const
    {
        return mStartOffset;
    }

    inline void SetSnapshotCompressionMeta(uint32_t storedDataLen, CompressAlgo compressAlgo)
    {
        mStoredDataLen = storedDataLen;
        mCompressAlgo = compressAlgo;
    }

    inline void SetRawSnapshotCompressionMeta(uint32_t storedDataLen, CompressAlgo compressAlgo)
    {
        if (mFatherSlice != nullptr) {
            mFatherSlice->SetSnapshotCompressionMeta(storedDataLen, compressAlgo);
        }
        SetSnapshotCompressionMeta(storedDataLen, compressAlgo);
    }

    inline bool SetStatus(SliceEvent event)
    {
        std::lock_guard<std::mutex> lk(mStatusLock);
        SliceStatus expect;
        SliceStatus target;
        if (event == SliceEvent::FLUSH) {
            expect = SliceStatus::NORMAL;
            target = SliceStatus::FLUSHING;
        } else if (event == SliceEvent::COMPACT) {
            expect = SliceStatus::NORMAL;
            target = SliceStatus::COMPACTING;
        } else if (event == SliceEvent::EVICT) {
            expect = SliceStatus::FLUSHING;
            target = SliceStatus::EVICTED;
        } else if (event == SliceEvent::FLUSH_BACK) {
            expect = SliceStatus::FLUSHING;
            target = SliceStatus::NORMAL;
        } else if (event == SliceEvent::COMPACT_BACK) {
            expect = SliceStatus::COMPACTING;
            target = SliceStatus::NORMAL;
        } else {
            return false;
        }
        return mSliceStatus.compare_exchange_strong(expect, target);
    }

    inline void ForceEvict()
    {
        mSliceStatus.store(SliceStatus::EVICTED);
    }

    inline void SetDataSlice(DataSliceRef dataSlice)
    {
        mDataSlice = dataSlice;
    }

    inline std::string GetLocalAddress()
    {
        return mLocalAddress;
    }

    static SliceAddressRef GetInvalidAddress()
    {
        SliceAddressRef newSliceAddress = std::make_shared<SliceAddress>();
        newSliceAddress->ForceEvict();
        return newSliceAddress;
    }

    inline SliceStatus GetSliceStatus()
    {
        return mSliceStatus.load();
    }

    inline bool IsFlush()
    {
        return mSliceStatus.load() == SliceStatus::FLUSHING || mSliceStatus.load() == SliceStatus::EVICTED;
    }

    static SliceAddressRef mInvalidAddress;

private:
    inline static uint64_t GenerateRandomNumber()
    {
        return mGenerateSliceId.fetch_add(1, std::memory_order_relaxed);
    }

private:
    std::atomic<SliceStatus> mSliceStatus{ SliceStatus::NORMAL };
    static std::atomic<uint64_t> mGenerateSliceId;
    uint32_t mOriDataLen = 0;
    uint32_t mStoredDataLen = 0;
    CompressAlgo mCompressAlgo = CompressAlgo::NONE;
    uint32_t mCheckSum = 12568;  // 当前设置为魔术字:12568.
    uint64_t mBorn = 0;
    uint32_t mHitCount = 0;
    std::string mLocalAddress;  // snapshot文件地址.
    uint64_t mStartOffset = 0;  // snapshot文件中的偏移.
    uint64_t mSliceId = GenerateRandomNumber();
    DataSliceRef mDataSlice = nullptr;
    SliceAddressRef mFatherSlice = nullptr;
    std::mutex mStatusLock;
};

struct SliceAddressHash {
    uint32_t operator()(const SliceAddressRef sliceAddress) const
    {
        return std::hash<SliceAddress *>()(sliceAddress.get());
    }
};

struct SliceAddressEqual {
    bool operator()(const SliceAddressRef a, const SliceAddressRef b) const
    {
        return a->GetDataSlice() == b->GetDataSlice();
    }
};

}  // namespace bss
}  // namespace ock

#endif  // BOOST_STATE_STORE_SLICEADDRESS_H
