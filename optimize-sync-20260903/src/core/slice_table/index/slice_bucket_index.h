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

#ifndef BOOST_STATE_STORE_SLICEINDEX_H
#define BOOST_STATE_STORE_SLICEINDEX_H

#include <vector>

#include "hash_code_range.h"
#include "include/bss_err.h"
#include "include/config.h"
#include "slice_bucket_mapper.h"
#include "slice_index_context.h"
#include "slice_table/slice/logical_slice_chain.h"

namespace ock {
namespace bss {
constexpr uint32_t LOCKS_NUM = NO_1024;

class SliceBucketIndex {
public:
    SliceBucketIndex() = default;

    // 该构建函数仅state restore流程中用于构建恢复出来的sliceBucketIndex.
    explicit SliceBucketIndex(std::vector<LogicalSliceChainRef> &mappingTable)
    {
        mMappingTable = mappingTable;
        mIsRestoreBuild = true;
    }

    ~SliceBucketIndex()
    {
        std::vector<LogicalSliceChainRef>().swap(mMappingTable);
    }

    BResult Initialize(uint32_t totalBucketNum, const ConfigRef &config);

    inline LogicalSliceChainRef GetLogicalSliceChain(const Key &key)
    {
        uint32_t bucketIndex = mBucketMapper->MapUnchecked(key.KeyHashCode());
        return GetLogicChainedSlice(bucketIndex);
    }

    inline SliceIndexContextRef GetSliceIndexContext(uint32_t hashCode, bool createIfMissing)
    {
        if (UNLIKELY(mIsRestoreBuild)) {
            LOG_ERROR("Unsupported operation exception, restore build flag:" << mIsRestoreBuild);
            return nullptr;
        }
        uint32_t bucketIndex = mBucketMapper->MapUnchecked(hashCode);
        return InternalGetSliceIndexContext(bucketIndex, createIfMissing);
    }

    inline SliceIndexContextRef GetSliceIndexContext(uint32_t bucketIndex)
    {
        if (UNLIKELY(mIsRestoreBuild)) {
            LOG_ERROR("Unsupported operation exception, restore build flag:" << mIsRestoreBuild);
            return nullptr;
        }
        return InternalGetSliceIndexContext(bucketIndex, false);
    }

    inline uint32_t GetIndexCapacity()
    {
        return mIsRestoreBuild ? mMappingTable.size() : mTotalBucketNum;
    }

    inline LogicalSliceChainRef GetLogicChainedSlice(uint32_t bucketIndex)
    {
        ReadLocker<ReadWriteLock> lk(&mMappingLocks[bucketIndex % LOCKS_NUM]);
        if (UNLIKELY(bucketIndex >= mMappingTable.size())) {
            LOG_ERROR("Slice bucketIndex " << bucketIndex << " over limit of mapping table size");
            return nullptr;
        }
        return mMappingTable[bucketIndex];
    }

    inline void SetLogicChainedSlice(uint32_t bucketIndex, const LogicalSliceChainRef &logicalSliceChain)
    {
        if (!CheckIndex(bucketIndex)) {
            LOG_ERROR("Invalid bucketIndex: " << bucketIndex);
            return;
        }
        WriteLocker<ReadWriteLock> lk(&mMappingLocks[bucketIndex % LOCKS_NUM]);
        mMappingTable[bucketIndex] = logicalSliceChain;
    }

    inline LogicalSliceChainRef CreateLogicalChainedSlice()
    {
        if (UNLIKELY(mIsRestoreBuild)) {
            LOG_ERROR("Unsupported operation exception, restore build flag:" << mIsRestoreBuild);
            return nullptr;
        }
        LogicalSliceChainRef newLogicalSliceChain = std::make_shared<LogicalSliceChainImpl>();
        BResult ret = newLogicalSliceChain->Init(SliceStatus::NORMAL, mConfig->GetLogicTableDefaultChainLen());
        if (UNLIKELY(ret != BSS_OK)) {
            LOG_ERROR("Initialize logical slice chain failed, ret:" << ret);
            return nullptr;
        }
        return newLogicalSliceChain;
    }

    void UpdateLogicalSliceChain(uint32_t bucketIndex, const LogicalSliceChainRef &oldLogicalSliceChain,
                                 const LogicalSliceChainRef &newLogicalSliceChain);

    HashCodeRangeRef ComputeHashCodeRange(uint32_t startBucket, uint32_t endBucket);

    SliceIndexContextRef InternalGetSliceIndexContext(uint32_t bucketIndex, bool createIfMiss);

    uint64_t GetTotalSliceChainSize();

    uint64_t GetAvgSliceChainSize();

    uint64_t GetAvgSliceSize();

    std::function<bool(SliceKey)> GetSlotStateFilter(uint32_t slot)
    {
        auto mapper = mBucketMapper;
        auto taskRange = mapper->GetTaskRange();
        return [slot, mapper, taskRange](const SliceKey &key) {
            uint32_t hashCode = key.KeyHashCode();
            if (UNLIKELY(hashCode < taskRange->GetStartHashCode() || hashCode > taskRange->GetEndHashCode())) {
                return true;
            }
            const uint32_t bucketIndex = mapper->MapUnchecked(hashCode);
            return bucketIndex != slot;
        };
    }

    inline SliceBucketMapperRef GetBucketMapper() const
    {
        return mBucketMapper;
    }

    inline uint32_t GetSliceChainMappingSize() const
    {
        return mMappingTable.size();
    }

    inline std::string ToString() const
    {
        std::ostringstream oss;
        oss << "TotalBucketNum:" << mTotalBucketNum << ", bucketIndex info[";
        for (uint32_t idx = 0; idx < mMappingTable.size(); idx++) {
            auto chain = mMappingTable[idx];
            oss << "idx:" << idx << " status:" << static_cast<uint32_t>(chain->GetSliceStatus())
                << " end:" << chain->GetSliceChainTailIndex() << " base:" << chain->GetBaseSliceIndex()
                << " size:" << chain->GetSliceSize() << " fileSize:" << chain->GetFilePageSize() << " ";
        }
        oss << "]";
        return oss.str();
    }

    std::string ToString(uint32_t index) const
    {
        std::ostringstream oss;
        oss << " TotalBucketNum:" << mTotalBucketNum << ", tableSize:" << mMappingTable.size() << ", bucketIndex info[";
        auto chain = mMappingTable[index];
        oss << "idx:" << index << " status:" << static_cast<uint32_t>(chain->GetSliceStatus())
            << " end:" << chain->GetSliceChainTailIndex() << " base:" << chain->GetBaseSliceIndex()
            << " size:" << chain->GetSliceSize() << " fileSize:" << chain->GetFilePageSize() << " ]";
        return oss.str();
    }

    inline void ReleaseChainSlice(uint32_t bucketIndex, SliceAddressRef &sliceAddress)
    {
        if (!CheckIndex(bucketIndex)) {
            LOG_ERROR("Invalid bucketIndex: " << bucketIndex);
            return;
        }
        ReadLocker<ReadWriteLock> lk(
            &mMappingLocks[bucketIndex % LOCKS_NUM]);  // 互斥compaction流程去替换bucketIndex上的logicChain
        auto sliceChain = mMappingTable[bucketIndex];
        if (UNLIKELY(sliceChain == nullptr)) {
            LOG_ERROR("Invalid bucket index:" << bucketIndex);
            return;
        }
        sliceChain->ReleaseSliceAddress(sliceAddress);
    }

    inline bool CheckIndex(uint32_t bucketIndex) const
    {
        return bucketIndex < mMappingTable.size();
    }

    inline void LockWrite(uint32_t bucketIndex)
    {
        mLocks[bucketIndex % LOCKS_NUM].LockWrite();
    }

    inline void Unlock(uint32_t bucketIndex)
    {
        mLocks[bucketIndex % LOCKS_NUM].Unlock();
    }

private:
    ConfigRef mConfig = nullptr;
    SliceBucketMapperRef mBucketMapper = nullptr;
    uint32_t mTotalBucketNum = 0;
    std::array<ReadWriteLock, LOCKS_NUM> mLocks;
    std::array<ReadWriteLock, LOCKS_NUM> mMappingLocks;  // 该锁保护mMappingTable的修改.
    std::vector<LogicalSliceChainRef> mMappingTable;     // 挂载sliceChain的数组.
    bool mIsRestoreBuild = false;
};
using SliceBucketIndexRef = std::shared_ptr<SliceBucketIndex>;

}  // namespace bss
}  // namespace ock
#endif  // BOOST_STATE_STORE_SLICEINDEX_H
