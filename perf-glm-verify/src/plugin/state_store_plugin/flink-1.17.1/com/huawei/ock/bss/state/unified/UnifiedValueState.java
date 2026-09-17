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

package com.huawei.ock.bss.state.unified;

import com.huawei.ock.bss.OckDBKeyedStateBackend;
import com.huawei.ock.bss.state.internal.KeyedValueState;
import com.huawei.ock.bss.state.internal.descriptor.InternalStateType;
import com.huawei.ock.bss.state.internal.descriptor.keyed.KeyedValueStateDescriptor;

import org.apache.flink.api.common.state.StateDescriptor;
import org.apache.flink.api.common.typeutils.TypeSerializer;
import org.apache.flink.runtime.state.VoidNamespace;
import org.apache.flink.runtime.state.heap.InternalKeyContext;
import org.apache.flink.runtime.state.internal.InternalValueState;
import org.apache.flink.util.Preconditions;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import java.io.IOException;
import java.util.Arrays;

/**
 * UnifiedValueState
 *
 * @param <K> key
 * @param <V> value
 * @since 2025年1月14日14:53:32
 */
public class UnifiedValueState<K, V> extends UnifiedKeyedState<K, V>
    implements InternalValueState<K, VoidNamespace, V> {
    private static final Logger LOG = LoggerFactory.getLogger(UnifiedValueState.class);

    private final InternalKeyContext<K> keyContext;

    private final KeyedValueState<K, V> keyedState;

    private final V defaultValue;

    public UnifiedValueState(InternalKeyContext<K> keyContext, KeyedValueState<K, V> keyedState,
        V defaultValue) {
        Preconditions.checkNotNull(keyContext);
        Preconditions.checkNotNull(keyedState);

        this.keyContext = keyContext;
        this.keyedState = keyedState;
        this.defaultValue = defaultValue;
    }

    @Override
    public V value() throws IOException {
        // InternalKeyContext cannot set null to key, so will not get null key
        V value = this.keyedState.get(this.keyContext.getCurrentKey());
        if (value == null) {
            return this.defaultValue;
        }
        return value;
    }

    @Override
    public void update(V value) throws IOException {
        if (value == null) {
            clear();
        } else {
            this.keyedState.put(this.keyContext.getCurrentKey(), value);
        }
    }

    @Override
    @SuppressWarnings("unchecked")
    public TypeSerializer<K> getKeySerializer() {
        return this.keyedState.getDescriptor().getKeySerializer();
    }

    @Override
    @SuppressWarnings("unchecked")
    public TypeSerializer<V> getValueSerializer() {
        return this.keyedState.getDescriptor().getValueSerializer();
    }

    @Override
    public byte[] getSerializedValue(byte[] serializedKeyAndNamespace, TypeSerializer<K> keySerializer,
        TypeSerializer<VoidNamespace> namespaceSerializer, TypeSerializer<V> valueSerializer) throws Exception {
        if (serializedKeyAndNamespace == null || serializedKeyAndNamespace.length == 0 || keySerializer == null
            || namespaceSerializer == null || valueSerializer == null) {
            LOG.error("Failed to getSerializedValue for serializedKeyAndNamespace: {}, "
                    + "keySerializer: {}, namespaceSerializer: {}, valueSerializer: {}.",
                Arrays.toString(serializedKeyAndNamespace), keySerializer, namespaceSerializer, valueSerializer);
            return null;
        }
        return this.keyedState.getSerializedValue(serializedKeyAndNamespace, keySerializer, valueSerializer);
    }

    @Override
    public void clear() {
        this.keyedState.remove(this.keyContext.getCurrentKey());
    }

    /**
     * 外部调用获取此类的对象
     *
     * @param stateDesc 状态描述符
     * @param keySerializer key序列化器
     * @param backend stateBackend
     * @param <K> key
     * @param <V> value
     * @param <S> state
     * @param <IS> UnifiedState
     * @return 本类实例
     * @throws Exception exception
     */
    @SuppressWarnings("unchecked")
    public static <K, V, S extends org.apache.flink.api.common.state.State, IS extends S> IS create(
        StateDescriptor<S, V> stateDesc, TypeSerializer<K> keySerializer, OckDBKeyedStateBackend<K> backend)
        throws Exception {
        KeyedValueStateDescriptor<K, V> keyedValueStateDescriptor =
            new KeyedValueStateDescriptor<>(stateDesc.getName(), keySerializer, stateDesc.getSerializer(),
                InternalStateType.KEYED_VALUE);
        keyedValueStateDescriptor.setStateTtlConfig(stateDesc.getTtlConfig());
        KeyedValueState<K, V> keyedValueState = backend.createKeyedValueState(keyedValueStateDescriptor);
        LOG.info("Creating a new state, stateType: {}", stateDesc.getType());
        return (IS) new UnifiedValueState<>(backend.getKeyContext(), keyedValueState, stateDesc.getDefaultValue());
    }
}
