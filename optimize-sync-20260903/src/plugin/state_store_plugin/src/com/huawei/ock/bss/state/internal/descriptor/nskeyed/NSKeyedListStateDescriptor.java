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

import com.huawei.ock.bss.common.exception.BSSRuntimeException;
import com.huawei.ock.bss.state.internal.descriptor.InternalStateType;
import com.huawei.ock.bss.state.internal.namespace.NSKeyedListState;

import org.apache.flink.api.common.typeutils.TypeSerializer;
import org.apache.flink.api.common.typeutils.base.ListSerializer;

import java.util.List;

/**
 * 有namespace listState描述符
 *
 * @param <K> key
 * @param <N> namespace
 * @param <E> element
 * @since BeiMing 25.0.T1
 */
public class NSKeyedListStateDescriptor<K, N, E>
    extends NSKeyedStateDescriptor<K, N, List<E>, NSKeyedListState<K, N, E>> {
    public NSKeyedListStateDescriptor(String name, TypeSerializer<K> keySerializer,
        TypeSerializer<N> namespaceSerializer, TypeSerializer<E> elementSerializer) {
        super(name, InternalStateType.NSKEYED_LIST, keySerializer, namespaceSerializer,
            new ListSerializer<>(elementSerializer));
    }

    /**
     * 获取element序列化器
     *
     * @return ElementSerializer
     */
    public TypeSerializer<E> getElementSerializer() {
        if (!(getValueSerializer() instanceof ListSerializer)) {
            throw new BSSRuntimeException(
                "Failed to getElementSerializer, ValueStateSerializer not instance of ListSerializer.");
        }
        return ((ListSerializer<E>) getValueSerializer()).getElementSerializer();
    }

    /**
     * 复制一个当前对象的副本
     *
     * @return 当前对象的副本
     */
    @Override
    public NSKeyedListStateDescriptor<K, N, E> duplicate() {
        return new NSKeyedListStateDescriptor<>(getName(), getKeySerializer().duplicate(),
            getNamespaceSerializer().duplicate(), getElementSerializer().duplicate());
    }
}
