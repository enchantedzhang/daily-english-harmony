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

#ifndef BOOST_SS_STATE_ID_H
#define BOOST_SS_STATE_ID_H

#include "include/state_type.h"
#include "io/file_input_view.h"

namespace ock {
namespace bss {
struct StateId {
    StateId() = default;

    explicit StateId(uint16_t raw)
    {
        mRaw = raw;
    }

    inline static uint16_t Of(uint32_t seq, StateType type)
    {
        return (seq & 0x1FFF) | static_cast<uint32_t>(type) << NO_13;
    }

    inline bool operator==(const StateId &other) const
    {
        return mRaw == other.mRaw;
    }

    inline static StateType GetStateType(uint16_t stateId)
    {
        return static_cast<StateType>((stateId >> NO_13) & 0x7);
    }

    inline static bool HasSecKey(uint16_t stateId)
    {
        return StateTypeUtil::HasSecKey(GetStateType(stateId));
    }

    inline static bool HasNameSpace(uint16_t stateId)
    {
        return StateTypeUtil::HasNameSpace(GetStateType(stateId));
    }

    inline static bool IsList(uint16_t stateId)
    {
        StateType stateType = GetStateType(stateId);
        return stateType == StateType::LIST || stateType == StateType::SUB_LIST;
    }

private:
    uint16_t mRaw = 0;
};
}  // namespace bss
}  // namespace ock
#endif  // BOOST_SS_STATE_ID_H
