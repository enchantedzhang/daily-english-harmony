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
import com.huawei.ock.bss.state.internal.descriptor.nskeyed.NSKeyedMapStateDescriptor;
import com.huawei.ock.bss.state.internal.namespace.NSKeyedMapState;

import org.apache.flink.api.common.state.MapStateDescriptor;
import org.apache.flink.api.common.state.StateDescriptor;
import org.apache.flink.api.common.typeutils.TypeSerializer;
import org.apache.flink.runtime.state.heap.InternalKeyContext;
import org.apache.flink.runtime.state.internal.InternalMapState;
import org.apache.flink.util.Preconditions;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import java.util.Arrays;
import java.util.Iterator;
import java.util.Map;

/**
 * 含namespace mapState
 *
 * @param <K> key
 * @param <N> namespace
 * @param <UK> userK
 * @param <UV> userV
 * @since BeiMing 25.0.T1
 */
public class UnifiedNSMapState<K, N, UK, UV> extends UnifiedNSKeyedState<K, N, Map<UK, UV>>
    implements InternalMapState<K, N, UK, UV> {
    private static final Logger LOG = LoggerFactory.getLogger(UnifiedNSMapState.class);

    private N namespace;

    private final InternalKeyContext<K> keyContext;

    private final NSKeyedMapState<K, N, UK, UV> keyedState;

    public UnifiedNSMapState(InternalKeyContext<K> keyContext, NSKeyedMapState<K, N, UK, UV> nsKeyedMapState) {
        Preconditions.checkNotNull(keyContext);
        Preconditions.checkNotNull(nsKeyedMapState);
        this.keyContext = keyContext;
        this.keyedState = nsKeyedMapState;
    }

    @Override
    public boolean contains(UK userKey) throws Exception {
        if (userKey == null) {
            return false;
        }
        return this.keyedState.contains(this.keyContext.getCurrentKey(), this.namespace, userKey);
    }

    @Override
    public UV get(UK userKey) throws Exception {
        if (userKey == null) {
            return null;
        }
        return this.keyedState.get(this.keyContext.getCurrentKey(), this.namespace, userKey);
    }

    @Override
    public void put(UK userKey, UV userValue) throws Exception {
        if (userKey == null || userValue == null) {
            return;
        }
        this.keyedState.add(this.keyContext.getCurrentKey(), this.namespace, userKey, userValue);
    }

    @Override
    public void putAll(Map<UK, UV> value) throws Exception {
        if (value == null || value.isEmpty()) {
            return;
        }
        this.keyedState.addAll(this.keyContext.getCurrentKey(), this.namespace, value);
    }

    @Override
    public void remove(UK userKey) throws Exception {
        if (userKey == null) {
            return;
        }
        this.keyedState.remove(this.keyContext.getCurrentKey(), this.namespace, userKey);
    }

    @Override
    public Iterable<Map.Entry<UK, UV>> entries() {
        Iterator<Map.Entry<UK, UV>> iterator =
            this.keyedState.iterator(this.keyContext.getCurrentKey(), this.namespace);
        return () -> iterator;
    }

    @Override
    public Iterable<UK> keys() {
        final Iterator<Map.Entry<UK, UV>> iterator =
            this.keyedState.iterator(this.keyContext.getCurrentKey(), this.namespace);
        Iterator<UK> keyIterator = new Iterator<UK>() {
            @Override
            public boolean hasNext() {
                return iterator.hasNext();
            }

            @Override
            public UK next() {
                return iterator.next().getKey();
            }
        };
        return () -> keyIterator;
    }

    @Override
    public Iterable<UV> values() throws Exception {
        final Iterator<Map.Entry<UK, UV>> iterator =
            this.keyedState.iterator(this.keyContext.getCurrentKey(), this.namespace);
        Iterator<UV> valueIterator = new Iterator<UV>() {
            @Override
            public boolean hasNext() {
                return iterator.hasNext();
            }

            @Override
            public UV next() {
                return iterator.next().getValue();
            }
        };
        return () -> valueIterator;
    }

    @Override
    public Iterator<Map.Entry<UK, UV>> iterator() throws Exception {
        return this.keyedState.iterator(this.keyContext.getCurrentKey(), this.namespace);
    }

    @Override
    public boolean isEmpty() throws Exception {
        return !this.keyedState.contains(this.keyContext.getCurrentKey(), this.namespace);
    }

    @Override
    @SuppressWarnings({"unchecked"})
    public TypeSerializer<K> getKeySerializer() {
        return this.keyedState.getDescriptor().getKeySerializer();
    }

    @Override
    @SuppressWarnings({"unchecked"})
    public TypeSerializer<N> getNamespaceSerializer() {
        return this.keyedState.getDescriptor().getNamespaceSerializer();
    }

    @Override
    @SuppressWarnings({"unchecked"})
    public TypeSerializer<Map<UK, UV>> getValueSerializer() {
        return this.keyedState.getDescriptor().getValueSerializer();
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
        TypeSerializer<N> namespaceSerializer, TypeSerializer<Map<UK, UV>> valueSerializer) throws Exception {
        if (serializedKeyAndNamespace == null || serializedKeyAndNamespace.length == 0 || keySerializer == null
            || namespaceSerializer == null || valueSerializer == null) {
            LOG.error("Failed to getSerializedValue for serializedKeyAndNamespace: {}, "
                    + "keySerializer: {}, namespaceSerializer: {}, valueSerializer: {}.",
                Arrays.toString(serializedKeyAndNamespace), keySerializer, namespaceSerializer, valueSerializer);
            return null;
        }
        return this.keyedState.getSerializedValue(serializedKeyAndNamespace, keySerializer, namespaceSerializer,
            valueSerializer);
    }

    @Override
    public void clear() {
        this.keyedState.remove(this.keyContext.getCurrentKey(), this.namespace);
    }

    /**
     * 创建对象
     *
     * @param stateDesc 状态描述符
     * @param keySerializer key序列化器
     * @param namespaceSerializer namespace序列化器
     * @param backend stateBackend
     * @param <K> key
     * @param <UK> userKey
     * @param <UV> userValue
     * @param <SV> state的value
     * @param <S> state
     * @param <IS> internalState
     * @return 实例
     * @throws Exception exception
     */
    @SuppressWarnings({"unchecked"})
    public static <K, N, UK, UV, SV, S extends org.apache.flink.api.common.state.State, IS extends S> IS create(
        StateDescriptor<S, SV> stateDesc, TypeSerializer<K> keySerializer, TypeSerializer<N> namespaceSerializer,
        OckDBKeyedStateBackend<K> backend) throws Exception {
        if (!(stateDesc instanceof MapStateDescriptor)) {
            throw new BSSRuntimeException("Failed to cast StateDescriptor to MapStateDescriptor.");
        }
        NSKeyedMapStateDescriptor<K, N, UK, UV> keyedMapStateDescriptor =
            new NSKeyedMapStateDescriptor<>(stateDesc.getName(), keySerializer, namespaceSerializer,
                ((MapStateDescriptor<UK, UV>) stateDesc).getSerializer());
        NSKeyedMapState<K, N, UK, UV> nsKeyedMapState = backend.createNSKeyedMapState(keyedMapStateDescriptor);
        LOG.info("Creating a new state, stateType: {}", stateDesc.getType());
        return (IS) new UnifiedNSMapState<>(backend.getKeyContext(), nsKeyedMapState);
    }
}
