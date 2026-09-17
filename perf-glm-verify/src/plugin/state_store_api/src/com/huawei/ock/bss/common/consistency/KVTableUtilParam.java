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

package com.huawei.ock.bss.common.consistency;

import com.huawei.ock.bss.common.conf.BoostConfig;
import com.huawei.ock.bss.common.memory.DirectBuffer;
import com.huawei.ock.bss.common.serialize.KVSerializerUtil;

import java.util.Arrays;

/**
 * debugMap KVTable的参数builder
 *
 * @param <K> KVTable的 key
 * @param <V> KVTable的 value
 * @since BeiMing 25.0.T1
 */
public class KVTableUtilParam<K, V> {
    private boolean actualContains;
    private int keyHashCode;
    private K key;
    private V value;
    private byte[] keyBytes;
    private byte[] valueBytes;

    private KVTableUtilParam() {
    }

    public boolean getActualContains() {
        return actualContains;
    }

    public int getKeyHashCode() {
        return keyHashCode;
    }

    public K getKey() {
        return key;
    }

    public V getValue() {
        return value;
    }

    public byte[] getKeyBytes() {
        return keyBytes;
    }

    public byte[] getValueBytes() {
        return valueBytes;
    }

    /**
     * getKeyStr 获取key字符串
     *
     * @return key string
     */
    public String getKeyStr() {
        return Arrays.toString(KVSerializerUtil.toUnsignedArray(keyBytes));
    }

    /**
     * getValueStr 获取value字符串
     *
     * @return value string
     */
    public String getValueStr() {
        return Arrays.toString(KVSerializerUtil.toUnsignedArray(valueBytes));
    }

    /**
     * Builder
     *
     * @param <K> KVTable的 key
     * @param <V> KVTable的 value
     */
    public static class Builder<K, V> {
        private boolean actualContains;
        private int keyHashCode;
        private K key;
        private V value;
        private byte[] keyBytes;
        private byte[] valueBytes;

        /**
         * build 构建参数
         *
         * @return debugMap参数builder
         */
        public KVTableUtilParam<K, V> build() {
            KVTableUtilParam<K, V> param = new KVTableUtilParam<>();
            param.actualContains = actualContains;
            param.keyHashCode = keyHashCode;
            param.key = key;
            param.value = value;
            param.keyBytes = keyBytes;
            param.valueBytes = valueBytes;
            return param;
        }

        public Builder<K, V> setActualContains(boolean actualContains) {
            this.actualContains = actualContains;
            return this;
        }

        public Builder<K, V> setKeyHashCode(int keyHashCode) {
            this.keyHashCode = keyHashCode;
            return this;
        }

        public Builder<K, V> setKey(K key) {
            this.key = key;
            return this;
        }

        public Builder<K, V> setValue(V value) {
            this.value = value;
            return this;
        }

        public Builder<K, V> setKeyBytes(DirectBuffer keyBuffer) {
            if (BoostConfig.DEBUG_MODE) {
                this.keyBytes = KVSerializerUtil.getCopyOfBuffer(keyBuffer);
            }
            return this;
        }
        public Builder<K, V> setValueBytes(DirectBuffer valueBuffer) {
            if (BoostConfig.DEBUG_MODE) {
                this.valueBytes = KVSerializerUtil.getCopyOfBuffer(valueBuffer);
            }
            return this;
        }
    }
}
