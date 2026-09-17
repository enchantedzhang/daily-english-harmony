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

#ifndef BOOST_SS_SEQ_GENERATOR_H
#define BOOST_SS_SEQ_GENERATOR_H

#include <atomic>
#include <memory>

#include "common/bss_log.h"
#include "common/util/timestamp_util.h"

namespace ock {
namespace bss {
class SeqIDUtils {
public:
    static inline uint64_t GetTimestamp(uint64_t seqID)
    {
        return seqID >> NO_16;
    }

    static inline uint64_t GetCounts(uint64_t seqID)
    {
        return seqID & NO_MAX_VALUE16;
    }

    static inline uint64_t GenerateSeqID(uint64_t ts, uint64_t counts)
    {
        return (ts << NO_16) | counts;
    }

    static inline uint64_t NextSeqID(uint64_t lastSeqID, uint64_t currentTs)
    {
        if (currentTs > MAX_TIMESTAMP) {
            LOG_ERROR("Current time: " << currentTs << ", but max time: " << MAX_TIMESTAMP);
            return GenerateSeqID(MAX_TIMESTAMP, 0);
        }

        uint64_t lastTime = GetTimestamp(lastSeqID);
        uint64_t lastCounts = GetCounts(lastSeqID);
        // 达到90%容量时预警
        if (lastCounts > CURRENTTS_WARNING) {
            LOG_WARN("The counter is close to overflowing: " << lastCounts << "/" << NO_65535);
        }

        if (currentTs < lastTime) {
            if (timeBackCounts.fetch_add(1, std::memory_order_relaxed) % NO_100000 == 0) {
                LOG_WARN("Current time: " << currentTs << ", last time: " << lastTime
                                          << ", timeBackCounts: " << timeBackCounts.load(std::memory_order_relaxed));
            }
            currentTs = lastTime;
        }

        uint64_t counts = 0;
        if (currentTs == lastTime) {
            if (lastCounts >= NO_65535) {
                if (numOverflowCounts.fetch_add(1, std::memory_order_relaxed) % NO_100000 == 0) {
                    LOG_WARN("LastCounts: " << lastCounts << ", max counts: " << NO_65535
                                            << " under time: " << currentTs);
                }
                if (currentTs >= MAX_TIMESTAMP) {
                    LOG_ERROR("Current time: " << currentTs << ", but max time: " << MAX_TIMESTAMP);
                    return GenerateSeqID(MAX_TIMESTAMP, 0);
                }
                currentTs++;
                counts = 0;
            } else {
                counts = lastCounts + 1;
            }
        }
        return GenerateSeqID(currentTs, counts);
    }

private:
    static std::atomic<uint64_t> timeBackCounts;
    static std::atomic<uint64_t> numOverflowCounts;
};

class SeqGenerator {
public:
    ~SeqGenerator() = default;
    explicit SeqGenerator() = default;

    inline uint64_t GetSeqId()
    {
        return mSeqId.load(std::memory_order_relaxed);
    }

    inline uint64_t Next()
    {
        uint64_t currentTime = TimeStampUtil::GetCurrentTime();
        mSeqId = SeqIDUtils::NextSeqID(mSeqId, currentTime);
        return mSeqId.fetch_add(1);
    }

    inline void Restore(uint64_t seqId)
    {
        mSeqId.store(seqId);
    }

private:
    std::atomic<uint64_t> mSeqId{ 0 };
};

using SeqGeneratorRef = std::shared_ptr<SeqGenerator>;
}  // namespace bss
}  // namespace ock

#endif  // BOOST_SS_SEQ_GENERATOR_H
