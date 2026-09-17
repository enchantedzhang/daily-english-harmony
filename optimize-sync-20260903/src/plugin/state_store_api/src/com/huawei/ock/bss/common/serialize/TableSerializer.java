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
import org.apache.flink.util.Preconditions;

import java.io.Serializable;
import java.util.Objects;

/**
 * TableSerializer
 *
 * @param <K1> key
 * @param <V>  value
 * @since BeiMing 25.0.T1
 */
public class TableSerializer<K1, V> implements Serializable {
    private static final long serialVersionUID = 6844799460196565050L;

    private TypeSerializer<K1> keySerializer;

    private TypeSerializer<V> valueSerializer;

    public TableSerializer(TypeSerializer<K1> keySerializer, TypeSerializer<V> valueSerializer) {
        Preconditions.checkNotNull(keySerializer);
        Preconditions.checkNotNull(valueSerializer);
        this.keySerializer = keySerializer;
        this.valueSerializer = valueSerializer;
    }

    public TypeSerializer<K1> getKeySerializer() {
        return this.keySerializer;
    }

    public TypeSerializer<V> getValueSerializer() {
        return this.valueSerializer;
    }

    public boolean equals(Object o) {
        if (this == o) {
            return true;
        }
        if (!(o instanceof TableSerializer)) {
            return false;
        }
        TableSerializer<?, ?> that = (TableSerializer<?, ?>) o;
        return (Objects.equals(this.keySerializer, that.keySerializer) && Objects.equals(this.valueSerializer,
            that.valueSerializer));
    }

    public int hashCode() {
        return Objects.hash(this.keySerializer, this.valueSerializer);
    }

    /**
     * 设置value序列化器
     *
     * @param valueSerializer 序列化器
     */
    public void setValueSerializer(TypeSerializer<V> valueSerializer) {
        this.valueSerializer = valueSerializer;
    }
}
