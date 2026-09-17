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
import com.huawei.ock.bss.common.exception.BSSRuntimeException;
import com.huawei.ock.bss.state.internal.descriptor.nskeyed.NSKeyedListStateDescriptor;
import com.huawei.ock.bss.state.internal.namespace.NSKeyedListState;

import org.apache.flink.api.common.state.ListStateDescriptor;
import org.apache.flink.api.common.state.StateDescriptor;
import org.apache.flink.api.common.typeutils.TypeSerializer;
import org.apache.flink.runtime.state.InternalKeyContext;
import org.apache.flink.runtime.state.internal.InternalListState;
import org.apache.flink.util.Preconditions;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import java.util.Arrays;
import java.util.Collection;
import java.util.List;

/**
 * 含namespace listState
 *
 * @param <K> key
 * @param <N> namespace
 * @param <E> element
 * @since BeiMing 25.0.T1
 */
public class UnifiedNSListState<K, N, E> extends UnifiedNSKeyedState<K, N, List<E>>
    implements InternalListState<K, N, E> {
    private static final Logger LOG = LoggerFactory.getLogger(UnifiedNSListState.class);

    private final InternalKeyContext<K> keyContext;

    private final NSKeyedListState<K, N, E> nsKeyedState;

    private N namespace;

    public UnifiedNSListState(InternalKeyContext<K> keyContext, NSKeyedListState<K, N, E> nsKeyedState) {
        Preconditions.checkNotNull(keyContext);
        Preconditions.checkNotNull(keyContext);

        this.keyContext = keyContext;
        this.nsKeyedState = nsKeyedState;
    }

    @Override
    public void add(E element) throws Exception {
        if (element == null) {
            return;
        }
        this.nsKeyedState.add(this.keyContext.getCurrentKey(), this.namespace, element);
    }

    @Override
    public void addAll(List<E> elements) throws Exception {
        if (elements == null || elements.isEmpty()) {
            return;
        }
        this.nsKeyedState.addAll(this.keyContext.getCurrentKey(), namespace, elements);
    }

    @Override
    public void update(List<E> value) throws Exception {
        updateInternal(value);
    }

    @Override
    public void updateInternal(List<E> value) throws Exception {
        if (value == null || value.isEmpty()) {
            this.nsKeyedState.remove(this.keyContext.getCurrentKey(), this.namespace);
            return;
        }
        this.nsKeyedState.putAll(this.keyContext.getCurrentKey(), this.namespace, value);
    }

    @Override
    public Iterable<E> get() throws Exception {
        return getInternal();
    }

    @Override
    public List<E> getInternal() {
        return this.nsKeyedState.get(this.keyContext.getCurrentKey(), this.namespace);
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
    public TypeSerializer<List<E>> getValueSerializer() {
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
    public void mergeNamespaces(N target, Collection<N> sources) {
        if (target == null || sources == null) {
            return;
        }
        for (N source : sources) {
            if (source == null) {
                continue;
            }
            List<E> value = this.nsKeyedState.get(this.keyContext.getCurrentKey(), source);
            if (value == null || value.isEmpty()) {
                continue;
            }
            this.nsKeyedState.addAll(this.keyContext.getCurrentKey(), target, value);
            this.nsKeyedState.remove(this.keyContext.getCurrentKey(), source);
        }
    }

    @Override
    public byte[] getSerializedValue(byte[] serializedKeyAndNamespace, TypeSerializer<K> keySerializer,
        TypeSerializer<N> namespaceSerializer, TypeSerializer<List<E>> valueSerializer) throws Exception {
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
     * 创建对象
     *
     * @param stateDesc 状态描述符
     * @param keySerializer key序列化器
     * @param namespaceSerializer namespace序列化器
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
    public static <K, N, E, SV, S extends org.apache.flink.api.common.state.State, IS extends S> IS create(
        StateDescriptor<S, SV> stateDesc, TypeSerializer<K> keySerializer, TypeSerializer<N> namespaceSerializer,
        OckDBKeyedStateBackend<K> backend) throws Exception {
        if (!(stateDesc instanceof ListStateDescriptor)) {
            throw new BSSRuntimeException(
                "Failed to create UnifiedListState, StateDescriptor not instance of ListStateDescriptor.");
        }
        NSKeyedListStateDescriptor<K, N, E> nsKeyedListStateDescriptor =
            new NSKeyedListStateDescriptor<>(stateDesc.getName(), keySerializer, namespaceSerializer,
                ((ListStateDescriptor) stateDesc).getElementSerializer());
        NSKeyedListState<K, N, E> nsKeyedListState = backend.createNSKeyedListState(nsKeyedListStateDescriptor);
        LOG.info("Creating a new state, stateType: {}", stateDesc.getType());
        return (IS) new UnifiedNSListState<>(backend.getKeyContext(), nsKeyedListState);
    }
}
