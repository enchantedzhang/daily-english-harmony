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
import com.huawei.ock.bss.state.internal.namespace.NSKeyedValueState;

import org.apache.flink.api.common.typeutils.TypeSerializer;

/**
 * NSKeyedValueStateDescriptor
 *
 * @param <K> key
 * @param <N> namespace
 * @param <V> value
 * @since 2025年1月14日15:06:36
 */
public class NSKeyedValueStateDescriptor<K, N, V> extends NSKeyedStateDescriptor<K, N, V, NSKeyedValueState<K, N, V>> {
    private static final long serialVersionUID = 1L;

    public NSKeyedValueStateDescriptor(String name, TypeSerializer<K> keySerializer,
        TypeSerializer<N> namespaceSerializer, TypeSerializer<V> valueSerializer, InternalStateType stateType) {
        super(name, stateType, keySerializer, namespaceSerializer, valueSerializer);
    }

    /**
     * snapshot调用，返回一个新的相同descriptor对象
     *
     * @return 复制对象
     */
    public NSKeyedValueStateDescriptor<K, N, V> duplicate() {
        return new NSKeyedValueStateDescriptor<>(getName(), getKeySerializer().duplicate(),
            getNamespaceSerializer().duplicate(), getValueSerializer().duplicate(), getStateType());
    }
}
