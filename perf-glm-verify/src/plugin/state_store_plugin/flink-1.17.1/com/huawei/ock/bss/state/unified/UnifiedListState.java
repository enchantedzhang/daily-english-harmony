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
import com.huawei.ock.bss.common.exception.BSSRuntimeException;
import com.huawei.ock.bss.state.internal.KeyedListState;
import com.huawei.ock.bss.state.internal.descriptor.keyed.KeyedListStateDescriptor;

import org.apache.flink.api.common.state.ListStateDescriptor;
import org.apache.flink.api.common.state.StateDescriptor;
import org.apache.flink.api.common.typeutils.TypeSerializer;
import org.apache.flink.runtime.state.VoidNamespace;
import org.apache.flink.runtime.state.heap.InternalKeyContext;
import org.apache.flink.runtime.state.internal.InternalListState;
import org.apache.flink.util.Preconditions;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import java.util.Arrays;
import java.util.Collection;
import java.util.List;

/**
 * UnifiedListState
 *
 * @param <K> key
 * @param <E> element
 * @since 2025年1月14日14:54:08
 */
public class UnifiedListState<K, E> extends UnifiedKeyedState<K, List<E>>
    implements InternalListState<K, VoidNamespace, E> {
    private static final Logger LOG = LoggerFactory.getLogger(UnifiedListState.class);

    private final InternalKeyContext<K> keyContext;

    private final KeyedListState<K, E> keyedState;

    public UnifiedListState(InternalKeyContext<K> keyContext, KeyedListState<K, E> keyedState) {
        Preconditions.checkNotNull(keyContext);
        Preconditions.checkNotNull(keyedState);

        this.keyContext = keyContext;
        this.keyedState = keyedState;
    }

    @Override
    public Iterable<E> get() throws Exception {
        return getInternal();
    }

    @Override
    public List<E> getInternal() throws Exception {
        return this.keyedState.get(this.keyContext.getCurrentKey());
    }

    @Override
    public void add(E element) throws Exception {
        if (element == null) {
            return;
        }
        this.keyedState.add(this.keyContext.getCurrentKey(), element);
    }

    @Override
    public void update(List<E> elements) throws Exception {
        updateInternal(elements);
    }

    @Override
    public void updateInternal(List<E> elements) throws Exception {
        if (elements == null) {
            return;
        }
        if (elements.isEmpty()) {
            this.keyedState.remove(this.keyContext.getCurrentKey());
            return;
        }
        this.keyedState.putAll(this.keyContext.getCurrentKey(), elements);
    }

    @Override
    public void addAll(List<E> elements) throws Exception {
        if (elements == null || elements.isEmpty()) {
            return;
        }
        this.keyedState.addAll(this.keyContext.getCurrentKey(), elements);
    }

    @Override
    public void mergeNamespaces(VoidNamespace voidNamespace, Collection<VoidNamespace> collection) throws Exception {
        throw new UnsupportedOperationException(
            "mergeNamespaces not supported because this state contains no namespace");
    }

    @Override
    @SuppressWarnings("unchecked")
    public TypeSerializer<K> getKeySerializer() {
        return this.keyedState.getDescriptor().getKeySerializer();
    }

    @Override
    @SuppressWarnings("unchecked")
    public TypeSerializer<List<E>> getValueSerializer() {
        return this.keyedState.getDescriptor().getValueSerializer();
    }

    @Override
    public byte[] getSerializedValue(byte[] serializedKeyAndNamespace, TypeSerializer<K> keySerializer,
        TypeSerializer<VoidNamespace> namespaceSerializer, TypeSerializer<List<E>> valueSerializer)
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
     * 创建对象
     *
     * @param stateDesc 状态描述符
     * @param keySerializer key序列化器
     * @param backend stateBackend
     * @param <K> key
     * @param <E> element
     * @param <SV> state的value
     * @param <S> state
     * @param <IS> internalState
     * @return UnifiedListState
     * @throws Exception exception
     */
    @SuppressWarnings({"unchecked", "rawtypes"})
    public static <K, E, SV, S extends org.apache.flink.api.common.state.State, IS extends S> IS create(
        StateDescriptor<S, SV> stateDesc, TypeSerializer<K> keySerializer, OckDBKeyedStateBackend<K> backend)
        throws Exception {
        if (!(stateDesc instanceof ListStateDescriptor)) {
            throw new BSSRuntimeException(
                "Failed to create UnifiedListState, StateDescriptor not instance of ListStateDescriptor.");
        }
        KeyedListStateDescriptor<K, E> keyedListStateDescriptor =
            new KeyedListStateDescriptor<>(stateDesc.getName(), keySerializer,
                ((ListStateDescriptor) stateDesc).getElementSerializer());
        keyedListStateDescriptor.setStateTtlConfig(stateDesc.getTtlConfig());
        KeyedListState<K, E> keyedListState = backend.createKeyedListState(keyedListStateDescriptor);
        LOG.info("Creating a new state, stateType: {}", stateDesc.getType());
        return (IS) new UnifiedListState<>(backend.getKeyContext(), keyedListState);
    }
}
