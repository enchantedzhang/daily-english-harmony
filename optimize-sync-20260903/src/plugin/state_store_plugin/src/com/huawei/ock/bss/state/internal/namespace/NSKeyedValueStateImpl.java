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

package com.huawei.ock.bss.state.internal.namespace;

import com.huawei.ock.bss.common.exception.BSSRuntimeException;
import com.huawei.ock.bss.state.internal.descriptor.nskeyed.NSKeyedValueStateDescriptor;
import com.huawei.ock.bss.table.api.NsKVTable;

import org.apache.flink.api.common.functions.AggregateFunction;
import org.apache.flink.api.common.functions.ReduceFunction;
import org.apache.flink.api.common.typeutils.TypeSerializer;
import org.apache.flink.api.java.tuple.Tuple2;
import org.apache.flink.queryablestate.client.state.serialization.KvStateSerializer;
import org.apache.flink.util.Preconditions;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import java.util.Collections;
import java.util.Iterator;
import java.util.Map;

/**
 * NSKeyedValueState
 *
 * @param <K> key
 * @param <N> namespace
 * @param <V> value
 * @since 2025年1月14日15:13:07
 */
public class NSKeyedValueStateImpl<K, N, V> implements NSKeyedValueState<K, N, V> {
    private static final Logger LOG = LoggerFactory.getLogger(NSKeyedValueStateImpl.class);

    private final NsKVTable<K, N, V> table;

    private final NSKeyedValueStateDescriptor<K, N, V> descriptor;

    public NSKeyedValueStateImpl(NSKeyedValueStateDescriptor<K, N, V> stateDesc, NsKVTable<K, N, V> table) {
        Preconditions.checkNotNull(stateDesc);
        Preconditions.checkNotNull(table);

        this.descriptor = stateDesc;
        this.table = table;
    }

    /**
     * 获取state的描述符
     *
     * @return state的描述符
     */
    @Override
    public NSKeyedValueStateDescriptor<K, N, V> getDescriptor() {
        return this.descriptor;
    }

    /**
     * 判断key在table中是否存在
     *
     * @param key       key
     * @param namespace namespace
     * @return “是”返回true
     */
    @Override
    public boolean contains(K key, N namespace) {
        if (key == null || namespace == null) {
            return false;
        }
        return this.table.contains(key, namespace);
    }

    /**
     * 根据key+namespace获取value
     *
     * @param key       key
     * @param namespace namespace
     * @return value
     */
    @Override
    public V get(K key, N namespace) {
        if (key == null || namespace == null) {
            return null;
        }
        return this.table.get(key, namespace);
    }

    /**
     * 根据key获取KV对
     * 实际上此方法未被调用
     *
     * @param key key
     * @return NV对
     */
    @Override
    public Map<N, V> getAll(K key) {
        if (key == null) {
            return Collections.emptyMap();
        }
        Map<N, V> res = this.table.get(key);
        if (res == null) {
            return Collections.emptyMap();
        }
        return res;
    }

    /**
     * merge namespace时调用，获取并删除
     *
     * @param key       key
     * @param namespace namespace
     * @return value
     */
    @Override
    public V getAndRemove(K key, N namespace) {
        if (key == null || namespace == null) {
            return null;
        }
        // 暂时复用get/remove接口
        V value = get(key, namespace);
        remove(key, namespace);
        return value;
    }

    /**
     * put
     *
     * @param key       key
     * @param namespace namespace
     * @param value     value
     */
    @Override
    public void put(K key, N namespace, V value) {
        if (key == null || namespace == null) {
            throw new NullPointerException("Failed to put a null key/namespace into table.");
        }
        this.table.add(key, namespace, value);
    }

    /**
     * 根据key+namespace移除数据
     *
     * @param key       key
     * @param namespace namespace
     */
    @Override
    public void remove(K key, N namespace) {
        if (key == null || namespace == null) {
            return;
        }
        this.table.remove(key, namespace);
    }

    /**
     * 移除一个key下面所有namespace的数据
     *
     * @param key key的集合
     */
    @Override
    public void removeAll(K key) {
        if (key == null) {
            return;
        }
        this.table.remove(key);
    }

    /**
     * 获取key下多个namespace的迭代器
     *
     * @param key key
     * @return namespace的迭代器
     */
    @Override
    public Iterator<N> iterator(K key) {
        Map<N, V> nsMap = this.table.get(key);
        if (nsMap == null || nsMap.isEmpty()) {
            return Collections.emptyIterator();
        }
        return new NsIterator(key, nsMap.keySet().iterator());
    }

    private class NsIterator implements Iterator<N> {
        private final K key;
        private final Iterator<N> nsIterator;
        private N currentNs;

        public NsIterator(K key, Iterator<N> nsIterator) {
            this.key = key;
            this.nsIterator = nsIterator;
        }

        @Override
        public boolean hasNext() {
            return nsIterator.hasNext();
        }

        @Override
        public N next() {
            currentNs = nsIterator.next();
            return currentNs;
        }

        @Override
        public void remove() {
            NSKeyedValueStateImpl.this.remove(key, currentNs);
        }
    }

    /**
     * UnifiedNSReducingState调用，add数据
     *
     * @param key            key
     * @param namespace      namespace
     * @param value          value
     * @param reduceFunction 聚合方法
     */
    @Override
    public void reduce(K key, N namespace, V value, ReduceFunction<V> reduceFunction) {
        V oldValue = get(key, namespace);
        try {
            if (oldValue == null) {
                put(key, namespace, value);
            } else {
                put(key, namespace, reduceFunction.reduce(oldValue, value));
            }
        } catch (Exception e) {
            throw new BSSRuntimeException("Failed to reduce value.", e);
        }
    }

    /**
     * UnifiedNSAggregatingState调用，add数据
     *
     * @param key               key
     * @param namespace         namespace
     * @param value             value
     * @param aggregateFunction 聚合方法
     * @param <IN>              输入
     * @param <OUT>             结果
     */
    @Override
    public <IN, OUT> void aggregate(K key, N namespace, IN value, AggregateFunction<IN, V, OUT> aggregateFunction) {
        V accumulator = get(key, namespace);
        try {
            if (accumulator == null) {
                accumulator = aggregateFunction.createAccumulator();
            }
            put(key, namespace, aggregateFunction.add(value, accumulator));
        } catch (Exception e) {
            throw new RuntimeException("Failed to aggregate value.");
        }
    }

    /**
     * 获取序列化后的value
     *
     * @param serializedKeyAndNamespace 序列化后的key+namespace
     * @param keySerializer             key序列化器
     * @param namespaceSerializer       namespace序列化器
     * @param valueSerializer           value序列化器
     * @return 序列化后的value
     * @throws Exception exception
     */
    @Override
    public byte[] getSerializedValue(byte[] serializedKeyAndNamespace, TypeSerializer<K> keySerializer,
        TypeSerializer<N> namespaceSerializer, TypeSerializer<V> valueSerializer) throws Exception {
        Tuple2<K, N> keyAndNamespace =
            KvStateSerializer.deserializeKeyAndNamespace(
                serializedKeyAndNamespace, keySerializer, namespaceSerializer);
        if (keyAndNamespace.f0 == null || keyAndNamespace.f1 == null) {
            LOG.error("Failed to getSerializedValue for key: {}, namespace: {}.", keyAndNamespace.f0,
                keyAndNamespace.f1);
            return null;
        }
        return this.table.getSerializedValue(keyAndNamespace.f0, keyAndNamespace.f1);
    }
}