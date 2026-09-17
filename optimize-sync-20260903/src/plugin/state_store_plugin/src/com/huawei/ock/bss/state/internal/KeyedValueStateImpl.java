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

package com.huawei.ock.bss.state.internal;

import com.huawei.ock.bss.state.internal.descriptor.keyed.KeyedValueStateDescriptor;
import com.huawei.ock.bss.table.api.KVTable;

import org.apache.flink.api.common.functions.AggregateFunction;
import org.apache.flink.api.common.functions.ReduceFunction;
import org.apache.flink.api.common.typeutils.TypeSerializer;
import org.apache.flink.queryablestate.client.VoidNamespaceSerializer;
import org.apache.flink.queryablestate.client.state.serialization.KvStateSerializer;
import org.apache.flink.util.Preconditions;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import java.util.AbstractMap;
import java.util.Collection;
import java.util.Collections;
import java.util.Map;
import java.util.Objects;
import java.util.stream.Collectors;

/**
 * 无namespace KeyedValueState实现类
 *
 * @param <K> key
 * @param <V> value
 * @since 2025年1月14日15:08:02
 */
public class KeyedValueStateImpl<K, V> implements KeyedValueState<K, V> {
    private static final Logger LOG = LoggerFactory.getLogger(KeyedValueStateImpl.class);

    private final KeyedValueStateDescriptor<K, V> descriptor;

    private final KVTable<K, V> table;

    public KeyedValueStateImpl(KeyedValueStateDescriptor<K, V> descriptor, KVTable<K, V> table) {
        this.descriptor = descriptor;
        this.table = table;
    }

    /**
     * put kv
     *
     * @param key   key
     * @param value value
     */
    public void put(K key, V value) {
        Preconditions.checkNotNull(key);
        this.table.put(key, value);
    }

    /**
     * put kv的集合
     *
     * @param kvMap kvMap
     */
    public void putAll(Map<? extends K, ? extends V> kvMap) {
        if (kvMap == null || kvMap.isEmpty()) {
            return;
        }
        kvMap.forEach(this::put);
    }

    /**
     * 获取state的描述符
     *
     * @return state的描述符
     */
    @Override
    public KeyedValueStateDescriptor<K, V> getDescriptor() {
        return this.descriptor;
    }

    /**
     * 判断key在table中是否存在
     *
     * @param key key
     * @return “存在”返回true
     */
    @Override
    public boolean contains(K key) {
        if (key == null) {
            return false;
        }
        return this.table.contains(key);
    }

    /**
     * 根据key获取value
     *
     * @param key key
     * @return value
     */
    @Override
    public V get(K key) {
        if (key == null) {
            return null;
        }
        return this.table.get(key);
    }

    /**
     * 根据key的集合获取KV对
     *
     * @param keySet key的集合
     * @return KV对
     */
    @Override
    public Map<K, V> getAll(Collection<? extends K> keySet) {
        if (keySet == null || keySet.isEmpty()) {
            return Collections.emptyMap();
        }

        return keySet.stream()
            .filter(Objects::nonNull)
            .map(key -> new AbstractMap.SimpleEntry<>(key, get(key)))
            .filter(entry -> entry.getValue() != null)
            .collect(Collectors.toMap(
                Map.Entry::getKey,
                Map.Entry::getValue
            ));
    }

    /**
     * 根据key移除数据
     *
     * @param key key
     */
    @Override
    public void remove(K key) {
        if (key == null) {
            return;
        }
        this.table.remove(key);
    }

    /**
     * 根据key的集合移除数据
     *
     * @param keySet key的集合
     */
    @Override
    public void removeAll(Collection<? extends K> keySet) {
        if (keySet == null || keySet.isEmpty()) {
            return;
        }
        keySet.forEach(this::remove);
    }

    /**
     * UnifiedReducingState调用，add数据
     *
     * @param key            key
     * @param value          value
     * @param reduceFunction 聚合方法
     * @throws Exception exception
     */
    @Override
    public void reduce(K key, V value, ReduceFunction<V> reduceFunction) throws Exception {
        V oldValue = get(key);
        try {
            if (oldValue == null) {
                put(key, value);
            } else {
                put(key, reduceFunction.reduce(oldValue, value));
            }
        } catch (Exception e) {
            throw new Exception("Failed to reduce value.");
        }
    }

    /**
     * UnifiedAggregatingState调用，add数据
     *
     * @param key               key
     * @param value             value
     * @param aggregateFunction 聚合方法
     * @param <IN>              输入类型
     * @param <OUT>             输出类型
     * @throws Exception exception
     */
    @Override
    public <IN, OUT> void aggregate(K key, IN value, AggregateFunction<IN, V, OUT> aggregateFunction)
        throws Exception {
        try {
            V accumulator = get(key);
            if (accumulator == null) {
                accumulator = aggregateFunction.createAccumulator();
            }
            put(key, aggregateFunction.add(value, accumulator));
        } catch (Exception e) {
            throw new Exception("Failed to aggregate value.");
        }
    }

    /**
     * 获取序列化后的value
     *
     * @param serializedKey   序列化后的key
     * @param keySerializer   key序列化器
     * @param valueSerializer value序列化器
     * @return 序列化后的value
     * @throws Exception exception
     */
    @Override
    public byte[] getSerializedValue(byte[] serializedKey, TypeSerializer<K> keySerializer,
        TypeSerializer<V> valueSerializer) throws Exception {
        K key = KvStateSerializer.deserializeKeyAndNamespace(serializedKey, keySerializer,
            VoidNamespaceSerializer.INSTANCE).f0;
        if (key == null) {
            LOG.error("Failed to getSerializedValue for key: null.");
            return null;
        }
        return this.table.getSerializedValue(key);
    }
}
