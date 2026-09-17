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

import com.huawei.ock.bss.common.memory.DirectBuffer;
import com.huawei.ock.bss.common.serialize.KVSerializerUtil;

import java.util.Arrays;

/**
 * debugMap MapTable的参数builder
 *
 * @param <K>  MapTable的 firstKey
 * @param <MK> MapTable的 secondKey
 * @since BeiMing 25.0.T1
 */
public class MapTableUtilParam<K, MK> {
    private boolean actualContains;
    private int keyHashCode;
    private K key;
    private MK mapKey;
    private DirectBuffer firstKey;
    private DirectBuffer secondKey;
    private DirectBuffer mapValue;
    private String tableName;
    private int iterCount;

    private MapTableUtilParam() {
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

    public MK getMapKey() {
        return mapKey;
    }

    /**
     * getFirstKeyStr 获取key字符串
     *
     * @return key string
     */
    public String getFirstKeyStr() {
        return Arrays.toString(KVSerializerUtil.toUnsignedArray(KVSerializerUtil.getCopyOfBuffer(firstKey)));
    }

    /**
     * getSecondKeyStr 获取subKey字符串
     *
     * @return key string
     */
    public String getSecondKeyStr() {
        return Arrays.toString(KVSerializerUtil.toUnsignedArray(KVSerializerUtil.getCopyOfBuffer(secondKey)));
    }

    public DirectBuffer getMapValue() {
        return mapValue;
    }

    public String getTableName() {
        return tableName;
    }

    public int getIterCount() {
        return iterCount;
    }

    /**
     * Builder
     *
     * @param <K>  MapTable的 firstKey
     * @param <MK> MapTable的 secondKey
     */
    public static class Builder<K, MK> {
        private boolean actualContains;
        private int keyHashCode;
        private K key;
        private MK mapKey;
        private DirectBuffer firstKey;
        private DirectBuffer secondKey;
        private DirectBuffer mapValue;
        private String tableName;
        private int iterCount;

        /**
         * build 构建参数
         *
         * @return debugMap参数builder
         */
        public MapTableUtilParam<K, MK> build() {
            MapTableUtilParam<K, MK> builder = new MapTableUtilParam<>();
            builder.actualContains = actualContains;
            builder.keyHashCode = keyHashCode;
            builder.key = key;
            builder.mapKey = mapKey;
            builder.firstKey = firstKey;
            builder.secondKey = secondKey;
            builder.mapValue = mapValue;
            builder.tableName = tableName;
            builder.iterCount = iterCount;
            return builder;
        }

        public Builder<K, MK> setActualContains(boolean actualContains) {
            this.actualContains = actualContains;
            return this;
        }

        public Builder<K, MK> setKeyHashCode(int keyHashCode) {
            this.keyHashCode = keyHashCode;
            return this;
        }

        public Builder<K, MK> setKey(K key) {
            this.key = key;
            return this;
        }

        public Builder<K, MK> setMapKey(MK mapKey) {
            this.mapKey = mapKey;
            return this;
        }

        public Builder<K, MK> setFirstKey(DirectBuffer firstKey) {
            this.firstKey = firstKey;
            return this;
        }

        public Builder<K, MK> setSecondKey(DirectBuffer secondKey) {
            this.secondKey = secondKey;
            return this;
        }

        public Builder<K, MK> setMapValue(DirectBuffer mapValue) {
            this.mapValue = mapValue;
            return this;
        }

        public Builder<K, MK> setTableName(String tableName) {
            this.tableName = tableName;
            return this;
        }

        public Builder<K, MK> setIterCount(int iterCount) {
            this.iterCount = iterCount;
            return this;
        }
    }
}
