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

package com.huawei.ock.bss.table;

import org.apache.flink.util.Preconditions;

/**
 * KeyPair
 *
 * @param <K0> key0
 * @param <K1> key1
 * @since BeiMing 25.0.T1
 */
public class KeyPair<K0, K1> {
    private final K0 firstKey;

    private final K1 secondKey;

    public KeyPair(K0 key0, K1 key1) {
        this.firstKey = (K0) Preconditions.checkNotNull(key0);
        this.secondKey = key1;
    }

    public K0 getFirstKey() {
        return this.firstKey;
    }

    public K1 getSecondKey() {
        return this.secondKey;
    }

    /**
     * equals
     *
     * @param obj obj
     * @return boolean
     */
    @SuppressWarnings("rawtypes")
    @Override
    public boolean equals(Object obj) {
        if (obj == this) {
            return true;
        }

        if (!(obj instanceof KeyPair)) {
            return false;
        }

        KeyPair other = (KeyPair) obj;
        return (this.firstKey.equals(other.firstKey) && this.secondKey.equals(other.secondKey));
    }

    /**
     * hashCode
     *
     * @return int
     */
    @Override
    public int hashCode() {
        return this.firstKey.hashCode() ^ this.secondKey.hashCode();
    }

    /**
     * keyHashCode
     *
     * @return int
     */
    public int keyHashCode() {
        return this.firstKey.hashCode();
    }

    /**
     * toString
     *
     * @return java.lang.String
     */
    @Override
    public String toString() {
        return "KeyPair{first=" + this.firstKey + ", second=" + this.secondKey + "}";
    }
}
