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

import org.apache.flink.api.common.functions.AggregateFunction;
import org.apache.flink.api.common.state.AggregatingStateDescriptor;
import org.apache.flink.api.common.state.AppendingState;
import org.apache.flink.api.common.state.StateDescriptor;
import org.apache.flink.api.common.typeutils.TypeSerializer;
import org.apache.flink.runtime.state.InternalKeyContext;
import org.apache.flink.runtime.state.internal.InternalAggregatingState;
import org.apache.flink.util.Preconditions;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import java.util.Arrays;
import java.util.Collection;

/**
 * UnifiedNSAggregatingState
 *
 * @since 2025年1月14日14:54:54
 */
public class UnifiedNSAggregatingState<K, N, IN, ACC, OUT> extends UnifiedNSKeyedState<K, N, ACC>
    implements InternalAggregatingState<K, N, IN, ACC, OUT>, AppendingState<IN, OUT> {
    private static final Logger LOG = LoggerFactory.getLogger(UnifiedNSAggregatingState.class);

    private final InternalKeyContext<K> keyContext;

    private final NSKeyedValueState<K, N, ACC> nsKeyedState;

    private final AggregateFunction<IN, ACC, OUT> aggregateFunction;

    private N namespace;

    public UnifiedNSAggregatingState(InternalKeyContext<K> keyContext, NSKeyedValueState<K, N, ACC> nsKeyedValueState,
        AggregateFunction<IN, ACC, OUT> aggregateFunction) {
        Preconditions.checkNotNull(keyContext);
        Preconditions.checkNotNull(nsKeyedValueState);
        Preconditions.checkNotNull(aggregateFunction);

        this.keyContext = keyContext;
        this.nsKeyedState = nsKeyedValueState;
        this.aggregateFunction = aggregateFunction;
    }

    @Override
    public void mergeNamespaces(N namespace, Collection<N> sources) throws Exception {
        if (sources == null || sources.isEmpty()) {
            return;
        }
        K key = this.keyContext.getCurrentKey();
        ACC merged = null;
        for (N source : sources) {
            ACC toMerge = this.nsKeyedState.getAndRemove(key, source);
            if (merged != null && toMerge != null) {
                merged = this.aggregateFunction.merge(merged, toMerge);
                continue;
            }
            if (merged == null) {
                merged = toMerge;
            }
        }
        // 考虑重构
        if (merged != null) {
            merged = this.aggregateFunction.merge(merged, this.nsKeyedState.get(key, namespace));
            this.nsKeyedState.put(key, namespace, merged);
        }
    }

    @Override
    public ACC getInternal() {
        return this.nsKeyedState.get(this.keyContext.getCurrentKey(), this.namespace);
    }

    @Override
    public void updateInternal(ACC value) {
        this.nsKeyedState.put(this.keyContext.getCurrentKey(), this.namespace, value);
    }

    @Override
    public OUT get() throws Exception {
        ACC acc = getInternal();
        if (acc == null) {
            return null;
        }
        return this.aggregateFunction.getResult(acc);
    }

    @Override
    public void add(IN in) throws Exception {
        if (in == null) {
            return;
        }
        this.nsKeyedState.aggregate(this.keyContext.getCurrentKey(), this.namespace, in, this.aggregateFunction);
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
    public TypeSerializer<ACC> getValueSerializer() {
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
        TypeSerializer<N> namespaceSerializer, TypeSerializer<ACC> valueSerializer) throws Exception {
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
                stateDesc.getSerializer(), InternalStateType.NSKEYED_AGGREGATING);
        NSKeyedValueState<K, N, V> nsKeyedValueState = backend.createNSKeyedValueState(nsKeyedValueStateDescriptor);
        LOG.info("Creating a new state, stateType: {}", stateDesc.getType());
        return (IS) new UnifiedNSAggregatingState<>(backend.getKeyContext(), nsKeyedValueState,
            ((AggregatingStateDescriptor) stateDesc).getAggregateFunction());
    }
}
