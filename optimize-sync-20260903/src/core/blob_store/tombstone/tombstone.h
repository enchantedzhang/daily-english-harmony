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

#ifndef BOOST_SS_TOMBSTONE_H
#define BOOST_SS_TOMBSTONE_H

#include <memory>

namespace ock {
namespace bss {
struct Tombstone {
public:
    Tombstone(uint64_t blobId, uint16_t keyGroup) : mBlobId(blobId), mKeyGroup(keyGroup)
    {
    }

    inline uint64_t GetBlobId() const
    {
        return mBlobId;
    }

    inline uint16_t GetKeyGroup() const
    {
        return mKeyGroup;
    }

    bool Equals(std::shared_ptr<Tombstone> &other) const
    {
        if (other == nullptr) {
            return false;
        }
        return mBlobId == other->GetBlobId() && mKeyGroup == other->GetKeyGroup();
    }

private:
    uint64_t mBlobId = 0;
    uint16_t mKeyGroup = 0;
};
using TombstoneRef = std::shared_ptr<Tombstone>;
}  // namespace bss
}  // namespace ock

#endif  // BOOST_SS_TOMBSTONE_H