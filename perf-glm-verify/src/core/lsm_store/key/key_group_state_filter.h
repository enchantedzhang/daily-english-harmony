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

#ifndef BOOST_SS_KEY_GROUP_STATE_FILTER_H
#define BOOST_SS_KEY_GROUP_STATE_FILTER_H

#include <cstdint>
#include <memory>

namespace ock {
namespace bss {
class KeyGroupStateFilter {
public:
    KeyGroupStateFilter(int32_t startKeyGroup, int32_t endKeyGroup)
        : mStartKeyGroup(startKeyGroup), mEndKeyGroup(endKeyGroup)
    {
    }

    inline bool Filter(int32_t group) const
    {
        return (group < mStartKeyGroup || group > mEndKeyGroup);
    }

private:
    int32_t mStartKeyGroup = 0;
    int32_t mEndKeyGroup = 0;
};
using KeyGroupStateFilterRef = std::shared_ptr<KeyGroupStateFilter>;

}  // namespace bss
}  // namespace ock
#endif  // BOOST_SS_KEY_GROUP_STATE_FILTER_H