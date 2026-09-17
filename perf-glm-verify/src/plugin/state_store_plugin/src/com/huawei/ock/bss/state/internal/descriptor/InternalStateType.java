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

package com.huawei.ock.bss.state.internal.descriptor;

import org.apache.flink.api.common.state.StateDescriptor;
import org.apache.flink.util.FlinkRuntimeException;

/**
 * 区分state类型
 *
 * @since 2025年1月17日19:53:19
 */
public enum InternalStateType {
    KEYED_VALUE(true, StateType.VALUE),

    KEYED_LIST(true, StateType.LIST),

    KEYED_MAP(true, StateType.MAP),

    KEYED_AGGREGATING(true, StateType.AGGREGATING),

    KEYED_REDUCING(true, StateType.REDUCING),

    NSKEYED_VALUE(false, StateType.VALUE),

    NSKEYED_LIST(false, StateType.LIST),

    NSKEYED_MAP(false, StateType.MAP),

    NSKEYED_AGGREGATING(false, StateType.AGGREGATING),

    NSKEYED_REDUCING(false, StateType.REDUCING);

    private final StateType stateType;

    private final boolean withoutNamespace;

    private enum StateType {VALUE, LIST, MAP, REDUCING, AGGREGATING;}

    InternalStateType(boolean withoutNamespace, StateType stateType) {
        this.withoutNamespace = withoutNamespace;
        this.stateType = stateType;
    }

    public StateType getStateType() {
        return this.stateType;
    }

    public boolean isKeyedState() {
        return this.withoutNamespace;
    }

    /**
     * 获取原本flink的标准状态
     *
     * @param stateType BSS的state状态
     * @return flink标准状态
     */
    public static StateDescriptor.Type getOriginStateType(InternalStateType stateType) {
        switch (stateType) {
            case KEYED_VALUE:
            case NSKEYED_VALUE:
                return StateDescriptor.Type.VALUE;
            case KEYED_LIST:
            case NSKEYED_LIST:
                return StateDescriptor.Type.LIST;
            case KEYED_MAP:
            case NSKEYED_MAP:
                return StateDescriptor.Type.MAP;
            case KEYED_AGGREGATING:
            case NSKEYED_AGGREGATING:
                return StateDescriptor.Type.AGGREGATING;
            case KEYED_REDUCING:
            case NSKEYED_REDUCING:
                return StateDescriptor.Type.REDUCING;
        }
        throw new FlinkRuntimeException("Unsupported state type " + stateType + ".");
    }
}
