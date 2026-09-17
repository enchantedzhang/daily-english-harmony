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

import org.apache.flink.api.common.functions.AggregateFunction;
import org.apache.flink.api.common.state.AggregatingStateDescriptor;
import org.apache.flink.api.common.state.StateDescriptor;
import org.apache.flink.api.common.typeutils.TypeSerializer;
import org.apache.flink.runtime.state.VoidNamespace;
import org.apache.flink.runtime.state.InternalKeyContext;
import org.apache.flink.runtime.state.internal.InternalAggregatingState;
import org.apache.flink.util.Preconditions;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import java.util.Arrays;
import java.util.Collection;

/**
 * UnifiedAggregatingState
 *
 * @param <K> key
 * @param <IN> 每次添加的元素
 * @param <ACC> 中间结果
 * @param <OUT> 最终输出的元素
 * @since 2025年1月14日14:54:26
 */
public class UnifiedAggregatingState<K, IN, ACC, OUT>
    extends UnifiedKeyedState<K, ACC> implements InternalAggregatingState<K, VoidNamespace, IN, ACC, OUT> {
    private static final Logger LOG = LoggerFactory.getLogger(UnifiedAggregatingState.class);

    private final InternalKeyContext<K> keyContext;

    // kvTable存储中间值，中间值作为底层三合一state的V
    private final KeyedValueState<K, ACC> keyedState;

    private final AggregateFunction<IN, ACC, OUT> aggregateFunction;

    public UnifiedAggregatingState(InternalKeyContext<K> keyContext, KeyedValueState<K, ACC> keyedState,
        AggregateFunction<IN, ACC, OUT> aggregateFunction) {
        Preconditions.checkNotNull(keyContext);
        Preconditions.checkNotNull(keyedState);
        Preconditions.checkNotNull(aggregateFunction);

        this.keyContext = keyContext;
        this.keyedState = keyedState;
        this.aggregateFunction = aggregateFunction;
    }

    @Override
    public void mergeNamespaces(VoidNamespace voidNamespace, Collection<VoidNamespace> collection) {
        throw new UnsupportedOperationException("mergeNamespaces not supported in keyedState.");
    }

    @Override
    public ACC getInternal() {
        return this.keyedState.get(this.keyContext.getCurrentKey());
    }

    // 直接替换ACC中间值
    @Override
    public void updateInternal(ACC acc) {
        this.keyedState.put(this.keyContext.getCurrentKey(), acc);
    }

    @Override
    public OUT get() throws Exception {
        ACC accumulator = getInternal();
        if (accumulator == null) {
            return null;
        }
        // 对于reducing，中间值就是结果
        // 对于aggregating，中间值需要通过aggregateFunction.getResult()计算出结果
        return this.aggregateFunction.getResult(accumulator);
    }

    @Override
    public void add(IN value) throws Exception {
        if (value == null) {
            return;
        }
        this.keyedState.aggregate(this.keyContext.getCurrentKey(), value, this.aggregateFunction);
    }

    @Override
    @SuppressWarnings("unchecked")
    public TypeSerializer<K> getKeySerializer() {
        return this.keyedState.getDescriptor().getKeySerializer();
    }

    @Override
    @SuppressWarnings("unchecked")
    public TypeSerializer<ACC> getValueSerializer() {
        return this.keyedState.getDescriptor().getValueSerializer();
    }

    @Override
    public byte[] getSerializedValue(byte[] serializedKeyAndNamespace, TypeSerializer<K> keySerializer,
        TypeSerializer<VoidNamespace> namespaceSerializer, TypeSerializer<ACC> valueSerializer)
        throws Exception {
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
                InternalStateType.KEYED_AGGREGATING);
        keyedValueStateDescriptor.setStateTtlConfig(stateDesc.getTtlConfig());
        KeyedValueState<K, V> keyedValueState = backend.createKeyedValueState(keyedValueStateDescriptor);
        LOG.info("Creating a new state, stateType: {}", stateDesc.getType());
        return (IS) new UnifiedAggregatingState<>(backend.getKeyContext(), keyedValueState,
            ((AggregatingStateDescriptor) stateDesc).getAggregateFunction());
    }
}
