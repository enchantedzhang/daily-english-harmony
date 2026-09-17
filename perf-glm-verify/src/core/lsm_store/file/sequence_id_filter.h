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

#ifndef BOOST_SS_SEQUENCE_ID_FILTER_H
#define BOOST_SS_SEQUENCE_ID_FILTER_H

#include <cstdint>
#include <memory>

#include "include/bss_types.h"

namespace ock {
namespace bss {
class SequenceIdFilter {
public:
    virtual ~SequenceIdFilter() = default;

    virtual inline bool Filter(uint64_t paramLong)
    {
        return false;
    }

    virtual inline int64_t GetTtlTime()
    {
        return 0;
    }
};
using SequenceIdFilterRef = std::shared_ptr<SequenceIdFilter>;

class NoStateFilter : public SequenceIdFilter {
public:
    inline bool Filter(uint64_t seqId) override
    {
        return false;
    }

    inline int64_t GetTtlTime() override
    {
        return 0;
    }
};

class TtlStateFilter : public SequenceIdFilter {
public:
    explicit TtlStateFilter(int64_t ttl) : mTtl(ttl)
    {
    }

    inline bool Filter(uint64_t seqId) override
    {
        return IsExpired(seqId >> NO_16, TimeStampUtil::GetCurrentTime());
    }

    inline bool IsExpired(uint64_t ts, uint64_t currentTime) const
    {
        return (mTtl > 0 &&
                ts + std::min(static_cast<uint64_t>(INT64_MAX - ts), static_cast<uint64_t>(mTtl)) <= currentTime);
    }

    inline int64_t GetTtlTime() override
    {
        return mTtl;
    }

private:
    int64_t mTtl = 0;
};

}  // namespace bss
}  // namespace ock
#endif  // BOOST_SS_SEQUENCE_ID_FILTER_H
