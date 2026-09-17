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
import com.huawei.ock.bss.state.internal.KeyedMapState;
import com.huawei.ock.bss.state.internal.descriptor.keyed.KeyedMapStateDescriptor;

import org.apache.flink.api.common.state.MapStateDescriptor;
import org.apache.flink.api.common.state.StateDescriptor;
import org.apache.flink.api.common.typeutils.TypeSerializer;
import org.apache.flink.runtime.state.VoidNamespace;
import org.apache.flink.runtime.state.InternalKeyContext;
import org.apache.flink.runtime.state.internal.InternalMapState;
import org.apache.flink.util.Preconditions;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import java.util.Arrays;
import java.util.Iterator;
import java.util.Map;

/**
 * UnifiedMapState
 *
 * @param <K> key
 * @param <UK> userKey
 * @param <UV> userValue
 * @since 2025年1月14日14:53:57
 */
public class UnifiedMapState<K, UK, UV>
    extends UnifiedKeyedState<K, Map<UK, UV>> implements InternalMapState<K, VoidNamespace, UK, UV> {
    private static final Logger LOG = LoggerFactory.getLogger(UnifiedMapState.class);

    private final InternalKeyContext<K> keyContext;

    private final KeyedMapState<K, UK, UV> keyedState;

    public UnifiedMapState(InternalKeyContext<K> keyContext, KeyedMapState<K, UK, UV> keyedState) {
        Preconditions.checkNotNull(keyContext);
        Preconditions.checkNotNull(keyedState);

        this.keyContext = keyContext;
        this.keyedState = keyedState;
    }

    @Override
    public boolean contains(UK userKey) throws Exception {
        if (userKey == null) {
            return false;
        }
        return this.keyedState.contains(this.keyContext.getCurrentKey(), userKey);
    }

    @Override
    public UV get(UK userKey) throws Exception {
        if (userKey == null) {
            return null;
        }
        return this.keyedState.get(this.keyContext.getCurrentKey(), userKey);
    }

    @Override
    public void put(UK userKey, UV userValue) throws Exception {
        if (userKey == null || userValue == null) {
            return;
        }
        this.keyedState.add(this.keyContext.getCurrentKey(), userKey, userValue);
    }

    @Override
    public void putAll(Map<UK, UV> map) throws Exception {
        if (map == null || map.isEmpty()) {
            return;
        }
        this.keyedState.addAll(this.keyContext.getCurrentKey(), map);
    }

    @Override
    public void remove(UK userKey) throws Exception {
        if (userKey == null) {
            return;
        }
        this.keyedState.remove(this.keyContext.getCurrentKey(), userKey);
    }

    @Override
    public Iterable<Map.Entry<UK, UV>> entries() {
        Iterator<Map.Entry<UK, UV>> iterator = this.keyedState.iterator(this.keyContext.getCurrentKey());
        return () -> iterator;
    }

    @Override
    public Iterable<UK> keys() {
        final Iterator<Map.Entry<UK, UV>> iterator = this.keyedState.iterator(this.keyContext.getCurrentKey());
        Iterator<UK> keyIterator = new Iterator<UK>() {
            @Override
            public boolean hasNext() {
                return iterator.hasNext();
            }

            @Override
            public UK next() {
                return iterator.next().getKey();
            }

            @Override
            public void remove() {
                iterator.remove();
            }
        };
        return () -> keyIterator;
    }

    @Override
    public Iterable<UV> values() throws Exception {
        final Iterator<Map.Entry<UK, UV>> iterator = this.keyedState.iterator(this.keyContext.getCurrentKey());
        Iterator<UV> valueIterator = new Iterator<UV>() {
            @Override
            public boolean hasNext() {
                return iterator.hasNext();
            }

            @Override
            public UV next() {
                return iterator.next().getValue();
            }

            @Override
            public void remove() {
                iterator.remove();
            }
        };
        return () -> valueIterator;
    }

    @Override
    public Iterator<Map.Entry<UK, UV>> iterator() throws Exception {
        return this.keyedState.iterator(this.keyContext.getCurrentKey());
    }

    @Override
    public boolean isEmpty() throws Exception {
        return !this.keyedState.contains(this.keyContext.getCurrentKey());
    }

    @Override
    @SuppressWarnings("unchecked")
    public TypeSerializer<K> getKeySerializer() {
        return this.keyedState.getDescriptor().getKeySerializer();
    }

    @Override
    @SuppressWarnings("unchecked")
    public TypeSerializer<Map<UK, UV>> getValueSerializer() {
        return this.keyedState.getDescriptor().getValueSerializer();
    }

    @Override
    public byte[] getSerializedValue(byte[] serializedKeyAndNamespace, TypeSerializer<K> keySerializer,
        TypeSerializer<VoidNamespace> namespaceSerializer, TypeSerializer<Map<UK, UV>> valueSerializer)
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
     * @param <UK> userKey
     * @param <UV> userValue
     * @param <SV> state的value
     * @param <S> state
     * @param <IS> internalState
     * @return 实例
     * @throws Exception exception
     */
    @SuppressWarnings({"unchecked", "rawtypes"})
    public static <K, UK, UV, SV, S extends org.apache.flink.api.common.state.State, IS extends S> IS create(
        StateDescriptor<S, SV> stateDesc, TypeSerializer<K> keySerializer, OckDBKeyedStateBackend<K> backend)
        throws Exception {
        KeyedMapStateDescriptor<K, UK, UV> keyedMapStateDescriptor =
            new KeyedMapStateDescriptor<>(stateDesc.getName(), keySerializer,
                ((MapStateDescriptor) stateDesc).getSerializer());
        keyedMapStateDescriptor.setStateTtlConfig(stateDesc.getTtlConfig());
        KeyedMapState<K, UK, UV> keyedMapState = backend.createKeyedMapState(keyedMapStateDescriptor);
        LOG.info("Creating a new state, stateType: {}", stateDesc.getType());
        return (IS) new UnifiedMapState<K, UK, UV>(backend.getKeyContext(), keyedMapState);
    }
}
