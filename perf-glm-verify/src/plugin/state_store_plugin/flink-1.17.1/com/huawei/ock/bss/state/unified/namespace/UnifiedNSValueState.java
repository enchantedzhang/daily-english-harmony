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

package com.huawei.ock.bss.state.unified.namespace;

import com.huawei.ock.bss.OckDBKeyedStateBackend;
import com.huawei.ock.bss.state.internal.descriptor.InternalStateType;
import com.huawei.ock.bss.state.internal.descriptor.nskeyed.NSKeyedValueStateDescriptor;
import com.huawei.ock.bss.state.internal.namespace.NSKeyedValueState;

import org.apache.flink.api.common.state.StateDescriptor;
import org.apache.flink.api.common.typeutils.TypeSerializer;
import org.apache.flink.runtime.state.heap.InternalKeyContext;
import org.apache.flink.runtime.state.internal.InternalValueState;
import org.apache.flink.util.Preconditions;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import java.io.IOException;
import java.util.Arrays;

/**
 * UnifiedNSValueState
 *
 * @param <K> key
 * @param <N> namspace
 * @param <V> value
 * @since 2025年1月17日20:53:51
 */
public class UnifiedNSValueState<K, N, V> extends UnifiedNSKeyedState<K, N, V> implements InternalValueState<K, N, V> {
    private static final Logger LOG = LoggerFactory.getLogger(UnifiedNSValueState.class);

    private final InternalKeyContext<K> keyContext;

    private final NSKeyedValueState<K, N, V> nsKeyedState;

    private final V defaultValue;

    private N namespace;

    public UnifiedNSValueState(InternalKeyContext<K> keyContext, NSKeyedValueState<K, N, V> nsKeyedValueState,
        V defaultValue) {
        Preconditions.checkNotNull(keyContext);
        Preconditions.checkNotNull(nsKeyedValueState);

        this.keyContext = keyContext;
        this.nsKeyedState = nsKeyedValueState;
        this.defaultValue = defaultValue;
    }

    @Override
    public V value() throws IOException {
        V value = this.nsKeyedState.get(this.keyContext.getCurrentKey(), this.namespace);
        if (value == null) {
            return this.defaultValue;
        }
        return value;
    }

    @Override
    public void update(V value) throws IOException {
        if (value == null) {
            clear();
        }
        this.nsKeyedState.put(this.keyContext.getCurrentKey(), this.namespace, value);
    }

    @Override
    @SuppressWarnings({"unchecked"})
    public TypeSerializer<K> getKeySerializer() {
        return this.nsKeyedState.getDescriptor().getKeySerializer();
    }

    @Override
    @SuppressWarnings({"unchecked"})
    public TypeSerializer<N> getNamespaceSerializer() {
        return this.nsKeyedState.getDescriptor().getNamespaceSerializer();
    }

    @Override
    @SuppressWarnings({"unchecked"})
    public TypeSerializer<V> getValueSerializer() {
        return this.nsKeyedState.getDescriptor().getValueSerializer();
    }

    @Override
    public void setCurrentNamespace(N namespace) {
        if (namespace == null) {
            LOG.error("namespace should not be null.");
            return;
        }
        this.namespace = namespace;
    }

    @Override
    public byte[] getSerializedValue(byte[] serializedKeyAndNamespace, TypeSerializer<K> keySerializer,
        TypeSerializer<N> namespaceSerializer, TypeSerializer<V> valueSerializer) throws Exception {
        if (serializedKeyAndNamespace == null || serializedKeyAndNamespace.length == 0 || keySerializer == null
            || namespaceSerializer == null || valueSerializer == null) {
            LOG.error("Failed to getSerializedValue for serializedKeyAndNamespace: {}, "
                    + "keySerializer: {}, namespaceSerializer: {}, valueSerializer: {}.",
                Arrays.toString(serializedKeyAndNamespace), keySerializer, namespaceSerializer, valueSerializer);
            return null;
        }
        return this.nsKeyedState.getSerializedValue(serializedKeyAndNamespace, keySerializer, namespaceSerializer,
            valueSerializer);
    }

    @Override
    public void clear() {
        this.nsKeyedState.remove(this.keyContext.getCurrentKey(), this.namespace);
    }

    /**
     * 外部调用获取此类的对象
     *
     * @param stateDesc 状态描述符
     * @param keySerializer key序列化器
     * @param namespaceSerializer namespace序列化器
     * @param backend stateBackend
     * @param <K> key
     * @param <N> namespace
     * @param <V> value
     * @param <S> state
     * @param <IS> UnifiedState
     * @return 本类实例
     * @throws Exception exception
     */
    @SuppressWarnings({"unchecked"})
    public static <K, N, V, S extends org.apache.flink.api.common.state.State, IS extends S> IS create(
        StateDescriptor<S, V> stateDesc, TypeSerializer<K> keySerializer, TypeSerializer<N> namespaceSerializer,
        OckDBKeyedStateBackend<K> backend) throws Exception {
        NSKeyedValueStateDescriptor<K, N, V> nsKeyedValueStateDescriptor =
            new NSKeyedValueStateDescriptor<>(stateDesc.getName(), keySerializer, namespaceSerializer,
                stateDesc.getSerializer(), InternalStateType.NSKEYED_VALUE);
        NSKeyedValueState<K, N, V> nsKeyedValueState = backend.createNSKeyedValueState(nsKeyedValueStateDescriptor);
        LOG.info("Creating a new state, stateType: {}", stateDesc.getType());
        return (IS) new UnifiedNSValueState<>(backend.getKeyContext(), nsKeyedValueState, stateDesc.getDefaultValue());
    }
}
