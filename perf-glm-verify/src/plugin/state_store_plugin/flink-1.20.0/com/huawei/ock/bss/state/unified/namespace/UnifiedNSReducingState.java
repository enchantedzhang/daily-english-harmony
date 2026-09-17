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

import org.apache.flink.api.common.functions.ReduceFunction;
import org.apache.flink.api.common.state.AppendingState;
import org.apache.flink.api.common.state.ReducingStateDescriptor;
import org.apache.flink.api.common.state.StateDescriptor;
import org.apache.flink.api.common.typeutils.TypeSerializer;
import org.apache.flink.runtime.state.InternalKeyContext;
import org.apache.flink.runtime.state.internal.InternalReducingState;
import org.apache.flink.util.Preconditions;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import java.util.Arrays;
import java.util.Collection;

/**
 * UnifiedNSReducingState
 *
 * @param <K> key
 * @param <N> namespace
 * @param <T> IN/ACC/OUT
 * @since 2025年1月17日20:52:31
 */
public class UnifiedNSReducingState<K, N, T> extends UnifiedNSKeyedState<K, N, T>
    implements InternalReducingState<K, N, T>, AppendingState<T, T> {
    private static final Logger LOG = LoggerFactory.getLogger(UnifiedNSReducingState.class);

    private final InternalKeyContext<K> keyContext;

    private final NSKeyedValueState<K, N, T> nsKeyedState;

    private final ReduceFunction<T> reduceFunction;

    private N namespace;

    public UnifiedNSReducingState(InternalKeyContext<K> keyContext, NSKeyedValueState<K, N, T> nsKeyedValueState,
        ReduceFunction<T> reduceFunction) {
        Preconditions.checkNotNull(keyContext);
        Preconditions.checkNotNull(nsKeyedValueState);
        Preconditions.checkNotNull(reduceFunction);

        this.keyContext = keyContext;
        this.nsKeyedState = nsKeyedValueState;
        this.reduceFunction = reduceFunction;
    }

    @Override
    public void mergeNamespaces(N namespace, Collection<N> sources) throws Exception {
        if (sources == null || sources.isEmpty()) {
            return;
        }
        K key = this.keyContext.getCurrentKey();
        if (key == null) {
            return;
        }
        T merged = null;
        for (N source : sources) {
            T toMerge = this.nsKeyedState.getAndRemove(key, source);
            if (merged != null && toMerge != null) {
                merged = this.reduceFunction.reduce(merged, toMerge);
                continue;
            }
            if (merged == null) {
                merged = toMerge;
            }
        }
        if (merged != null) {
            this.nsKeyedState.reduce(key, namespace, merged, this.reduceFunction);
        }
    }

    @Override
    public T getInternal() throws Exception {
        return this.nsKeyedState.get(this.keyContext.getCurrentKey(), this.namespace);
    }

    @Override
    public void updateInternal(T value) throws Exception {
        this.nsKeyedState.put(this.keyContext.getCurrentKey(), this.namespace, value);
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
        this.nsKeyedState.reduce(this.keyContext.getCurrentKey(), this.namespace, value, this.reduceFunction);
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
    public TypeSerializer<T> getValueSerializer() {
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
        TypeSerializer<N> namespaceSerializer, TypeSerializer<T> valueSerializer) throws Exception {
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
    @SuppressWarnings({"unchecked", "rawtypes"})
    public static <K, N, V, S extends org.apache.flink.api.common.state.State, IS extends S> IS create(
        StateDescriptor<S, V> stateDesc, TypeSerializer<K> keySerializer, TypeSerializer<N> namespaceSerializer,
        OckDBKeyedStateBackend<K> backend) throws Exception {
        NSKeyedValueStateDescriptor<K, N, V> nsKeyedValueStateDescriptor =
            new NSKeyedValueStateDescriptor<>(stateDesc.getName(), keySerializer, namespaceSerializer,
                stateDesc.getSerializer(), InternalStateType.NSKEYED_REDUCING);
        NSKeyedValueState<K, N, V> nsKeyedValueState = backend.createNSKeyedValueState(nsKeyedValueStateDescriptor);
        LOG.info("Creating a new state, stateType: {}", stateDesc.getType());
        return (IS) new UnifiedNSReducingState<>(backend.getKeyContext(), nsKeyedValueState,
            ((ReducingStateDescriptor) stateDesc).getReduceFunction());
    }
}
