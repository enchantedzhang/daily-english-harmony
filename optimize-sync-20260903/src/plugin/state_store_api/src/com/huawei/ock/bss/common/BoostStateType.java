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

package com.huawei.ock.bss.common;

import com.huawei.ock.bss.common.exception.BSSRuntimeException;

/**
 * BoostStateType 状态类型
 *
 * @since BeiMing 25.0.T1
 */
public enum BoostStateType {
    PQ(0),
    VALUE(1),
    LIST(2),
    MAP(3),
    SUB_VALUE(4),
    SUB_LIST(5),
    SUB_MAP(6);

    BoostStateType(int code) {
        this.code = code;
    }

    int code;

    /**
     * getCode
     *
     * @return int
     */
    public int getCode() {
        return this.code;
    }

    /**
     * isSorted
     *
     * @return boolean
     */
    public boolean isSorted() {
        return (this.code == 3 || this.code == 7);
    }

    /**
     * int转换为枚举类型
     *
     * @param code code
     * @return com.huawei.ock.bss.common.BoostStateType
     */
    public static BoostStateType of(int code) {
        switch (code) {
            case 0:
                return PQ;
            case 1:
                return VALUE;
            case 2:
                return LIST;
            case 3:
                return MAP;
            case 4:
                return SUB_VALUE;
            case 5:
                return SUB_LIST;
            case 6:
                return SUB_MAP;
            default:
                throw new BSSRuntimeException("NOT a valid state type for raw value: " + code);
        }
    }

    public static boolean isSortedState(int code) {
        // don't support sorted state.
        return false;
    }
}