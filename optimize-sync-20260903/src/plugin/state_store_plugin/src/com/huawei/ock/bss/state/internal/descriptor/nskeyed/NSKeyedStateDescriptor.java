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

package com.huawei.ock.bss.state.internal.descriptor.nskeyed;

import com.huawei.ock.bss.state.internal.descriptor.InternalStateType;
import com.huawei.ock.bss.state.internal.namespace.NSKeyedState;

import org.apache.flink.api.common.typeutils.TypeSerializer;
import org.apache.flink.util.Preconditions;

import java.io.Serializable;
import java.util.Objects;

/**
 * 有namespace的描述符
 *
 * @since 2025年1月14日15:06:15
 */
public abstract class NSKeyedStateDescriptor<K, N, V, S extends NSKeyedState<K, N, V>> implements Serializable {
    private static final long serialVersionUID = 1L;

    private final String name;

    private final InternalStateType stateType;

    private final TypeSerializer<K> keySerializer;

    private final TypeSerializer<N> namespaceSerializer;

    private final TypeSerializer<V> valueSerializer;

    protected NSKeyedStateDescriptor(String name, InternalStateType stateType, TypeSerializer<K> keySerializer,
        TypeSerializer<N> namespaceSerializer, TypeSerializer<V> valueSerializer) {
        Preconditions.checkNotNull(name);
        Preconditions.checkNotNull(stateType);
        Preconditions.checkNotNull(keySerializer);
        Preconditions.checkNotNull(namespaceSerializer);
        Preconditions.checkNotNull(valueSerializer);

        this.name = name;
        this.stateType = stateType;
        this.keySerializer = keySerializer;
        this.namespaceSerializer = namespaceSerializer;
        this.valueSerializer = valueSerializer;
    }

    public String getName() {
        return this.name;
    }

    public InternalStateType getStateType() {
        return this.stateType;
    }

    public TypeSerializer<K> getKeySerializer() {
        return this.keySerializer;
    }

    public TypeSerializer<N> getNamespaceSerializer() {
        return this.namespaceSerializer;
    }

    public TypeSerializer<V> getValueSerializer() {
        return this.valueSerializer;
    }

    /**
     * snapshot调用，返回一个新的相同descriptor对象
     *
     * @return 复制对象
     */
    public abstract NSKeyedStateDescriptor<K, N, V, S> duplicate();

    /**
     * 重写hash方法
     *
     * @return hashcode
     */
    public int hashCode() {
        int result = Objects.hashCode(this.name);
        result = 31 * result + Objects.hashCode(this.keySerializer);
        result = 31 * result + Objects.hashCode(this.namespaceSerializer);
        result = 31 * result + Objects.hashCode(this.valueSerializer);
        return result;
    }
}
