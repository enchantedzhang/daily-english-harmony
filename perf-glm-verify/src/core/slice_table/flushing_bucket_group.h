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

#ifndef BOOST_SS_FLUSHINGBUCKETGROUP_H
#define BOOST_SS_FLUSHINGBUCKETGROUP_H

#include <memory>
#include <vector>

#include "memory/full_sort_evictor.h"
#include "slice_table/slice/data_slice.h"

namespace ock {
namespace bss {

class FlushingBucketGroupIterator : public Iterator<std::vector<DataSliceRef>> {
public:
    BResult Initialize(std::vector<std::vector<DataSliceRef>> &dataSlices);

    bool HasNext() override;

    std::vector<DataSliceRef> Next() override;

private:
    std::vector<std::vector<DataSliceRef>> mDataSlices;
    std::vector<std::vector<DataSliceRef>>::iterator mIter;
};

class FlushingBucketGroup {
public:
    BResult Initialize(std::vector<SliceScore> &list, uint32_t bucketGroupId, const FullSortEvictorRef &evictor,
                       const FlushQueueForBucketGroupRef &flushQueueForBucketGroup);

    Ref<FlushingBucketGroupIterator> Iterator();

    BResult Complete(int32_t flushResult);

    inline FullSortEvictorRef &GetEvictor()
    {
        return mEvictor;
    }

    inline std::vector<SliceScore> &GetEvictList()
    {
        return mList;
    }

private:
    std::vector<std::vector<DataSliceRef>> mDataSlices;
    FullSortEvictorRef mEvictor;
    FlushQueueForBucketGroupRef mFlushQueueForBucketGroup;
    std::vector<SliceScore> mList;
    uint32_t mBucketGroupId = 0;
};

}  // namespace bss
}  // namespace ock

#endif  // BOOST_SS_FLUSHINGBUCKETGROUP_H
