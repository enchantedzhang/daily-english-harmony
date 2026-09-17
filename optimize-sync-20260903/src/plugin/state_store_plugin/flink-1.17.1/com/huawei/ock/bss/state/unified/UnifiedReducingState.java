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

import org.apache.flink.api.common.functions.ReduceFunction;
import org.apache.flink.api.common.state.ReducingState;
import org.apache.flink.api.common.state.ReducingStateDescriptor;
import org.apache.flink.api.common.state.StateDescriptor;
import org.apache.flink.api.common.typeutils.TypeSerializer;
import org.apache.flink.runtime.state.VoidNamespace;
import org.apache.flink.runtime.state.heap.InternalKeyContext;
import org.apache.flink.runtime.state.internal.InternalReducingState;
import org.apache.flink.util.Preconditions;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import java.util.Arrays;
import java.util.Collection;

/**
 * UnifiedReducingState
 *
 * @param <K> key
 * @param <T> 输入值/中间值/结果
 * @since 2025年1月14日14:53:47
 */
public class UnifiedReducingState<K, T>
    extends UnifiedKeyedState<K, T> implements ReducingState<T>, InternalReducingState<K, VoidNamespace, T> {
    private static final Logger LOG = LoggerFactory.getLogger(UnifiedReducingState.class);

    private final InternalKeyContext<K> keyContext;

    private final KeyedValueState<K, T> keyedState;

    private final ReduceFunction<T> reduceFunction;

    public UnifiedReducingState(InternalKeyContext<K> keyContext, KeyedValueState<K, T> keyedState,
        ReduceFunction<T> reduceFunction) {
        Preconditions.checkNotNull(keyContext);
        Preconditions.checkNotNull(keyedState);
        Preconditions.checkNotNull(reduceFunction);

        this.keyContext = keyContext;
        this.keyedState = keyedState;
        this.reduceFunction = reduceFunction;
    }

    @Override
    public void mergeNamespaces(VoidNamespace voidNamespace, Collection<VoidNamespace> collection) {
        throw new UnsupportedOperationException("mergeNamespaces not supported in keyedState.");
    }

    @Override
    public T getInternal() {
        return this.keyedState.get(this.keyContext.getCurrentKey());
    }

    @Override
    public void updateInternal(T value) {
        this.keyedState.put(this.keyContext.getCurrentKey(), value);
    }

    @Override
    public T get() throws Exception {
        return getInternal();
    }

    @Override
    public void add(T value) throws Exception {
        if (value == null) {
            return;
        }
        this.keyedState.reduce(this.keyContext.getCurrentKey(), value, this.reduceFunction);
    }

    @Override
    @SuppressWarnings("unchecked")
    public TypeSerializer<K> getKeySerializer() {
        return this.keyedState.getDescriptor().getKeySerializer();
    }

    @Override
    @SuppressWarnings("unchecked")
    public TypeSerializer<T> getValueSerializer() {
        return this.keyedState.getDescriptor().getValueSerializer();
    }

    @Override
    public byte[] getSerializedValue(byte[] serializedKeyAndNamespace, TypeSerializer<K> keySerializer,
        TypeSerializer<VoidNamespace> namespaceSerializer, TypeSerializer<T> valueSerializer) throws Exception {
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
    @SuppressWarnings({"unchecked", "rawtypes"})
    public static <K, V, S extends org.apache.flink.api.common.state.State, IS extends S> IS create(
        StateDescriptor<S, V> stateDesc, TypeSerializer<K> keySerializer, OckDBKeyedStateBackend<K> backend)
        throws Exception {
        KeyedValueStateDescriptor<K, V> keyedValueStateDescriptor =
            new KeyedValueStateDescriptor<>(stateDesc.getName(), keySerializer, stateDesc.getSerializer(),
                InternalStateType.KEYED_REDUCING);
        keyedValueStateDescriptor.setStateTtlConfig(stateDesc.getTtlConfig());
        KeyedValueState<K, V> keyedValueState = backend.createKeyedValueState(keyedValueStateDescriptor);
        LOG.info("Creating a new state, stateType: {}", stateDesc.getType());
        return (IS) new UnifiedReducingState<>(backend.getKeyContext(), keyedValueState,
            ((ReducingStateDescriptor) stateDesc).getReduceFunction());
    }
}
