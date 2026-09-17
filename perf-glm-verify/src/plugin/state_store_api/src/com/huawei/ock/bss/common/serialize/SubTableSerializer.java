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

package com.huawei.ock.bss.common.serialize;

import org.apache.flink.api.common.typeutils.TypeSerializer;

import java.util.Objects;

/**
 * SubTableSerializer
 *
 * @param <K1> 一级key
 * @param <K2> 二级key
 * @param <V>  value
 * @since BeiMing 25.0.T1
 */
public class SubTableSerializer<K1, K2, V> extends TableSerializer<K1, V> {
    private final TypeSerializer<K2> key2Serializer;

    public SubTableSerializer(TypeSerializer<K1> key1Serializer, TypeSerializer<K2> key2Serializer,
        TypeSerializer<V> valueSerializer) {
        super(key1Serializer, valueSerializer);
        this.key2Serializer = key2Serializer;
    }

    public TypeSerializer<K2> getKey2Serializer() {
        return key2Serializer;
    }

    /**
     * equals
     *
     * @param o other
     * @return boolean
     */
    public boolean equals(Object o) {
        if (this == o) {
            return true;
        }
        if (!(o instanceof SubTableSerializer)) {
            return false;
        }
        if (!super.equals(o)) {
            return false;
        }
        SubTableSerializer<?, ?, ?> that = (SubTableSerializer<?, ?, ?>) o;
        return Objects.equals(this.key2Serializer, that.key2Serializer);
    }

    /**
     * hashCode
     *
     * @return int
     */
    public int hashCode() {
        return Objects.hash(super.hashCode(), this.key2Serializer);
    }
}
