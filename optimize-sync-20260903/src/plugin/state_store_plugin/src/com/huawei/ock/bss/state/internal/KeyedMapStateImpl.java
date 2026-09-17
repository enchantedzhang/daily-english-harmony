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

import com.huawei.ock.bss.common.exception.BSSRuntimeException;
import com.huawei.ock.bss.state.internal.descriptor.keyed.KeyedMapStateDescriptor;
import com.huawei.ock.bss.state.internal.descriptor.keyed.KeyedStateDescriptor;
import com.huawei.ock.bss.table.api.KMapTable;

import org.apache.flink.api.common.typeutils.TypeSerializer;
import org.apache.flink.api.common.typeutils.base.MapSerializer;
import org.apache.flink.queryablestate.client.VoidNamespaceSerializer;
import org.apache.flink.queryablestate.client.state.serialization.KvStateSerializer;
import org.apache.flink.util.Preconditions;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import java.util.Collection;
import java.util.HashMap;
import java.util.Iterator;
import java.util.Map;

/**
 * 无namespace mapState实现类
 *
 * @param <K> key
 * @param <UK> userKey
 * @param <UV> userValue
 * @since BeiMing 25.0.T1
 */
public class KeyedMapStateImpl<K, UK, UV> implements KeyedMapState<K, UK, UV> {
    private static final Logger LOG = LoggerFactory.getLogger(KeyedMapStateImpl.class);

    private final KeyedMapStateDescriptor<K, UK, UV> descriptor;

    private final KMapTable<K, UK, UV, Map<UK, UV>> table;

    public KeyedMapStateImpl(KeyedMapStateDescriptor<K, UK, UV> descriptor,
        KMapTable<K, UK, UV, Map<UK, UV>> table) {
        Preconditions.checkNotNull(table);
        Preconditions.checkNotNull(descriptor);

        this.table = table;
        this.descriptor = descriptor;
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
     * 判断对应key和userKey下的userValue是否存在
     *
     * @param key key
     * @param userKey userKey
     * @return “存在”返回true
     */
    @Override
    public boolean contains(K key, UK userKey) {
        if (key == null || userKey == null) {
            return false;
        }
        return this.table.contains(key, userKey);
    }

    /**
     * 根据key获取Map<UK, UV>
     *
     * @param key key
     * @return Map<UK, UV>
     */
    @Override
    public Map<UK, UV> get(K key) {
        if (key == null) {
            return new HashMap<>();
        }
        return this.table.get(key);
    }

    /**
     * 根据key的集合获取Map<K, Map<UK, UV>>对
     *
     * @param keySet key的集合
     * @return Map<K, Map<UK, UV>>
     */
    @Override
    public Map<K, Map<UK, UV>> getAll(Collection<? extends K> keySet) {
        if (keySet == null || keySet.isEmpty()) {
            return new HashMap<>();
        }
        Map<K, Map<UK, UV>> map = new HashMap<>();
        for (K key : keySet) {
            if (key == null) {
                continue;
            }
            Map<UK, UV> value = this.table.get(key);
            if (value == null || value.isEmpty()) {
                continue;
            }
            map.put(key, value);
        }
        return map;
    }

    /**
     * 根据key和userKey获取userValue
     *
     * @param key key
     * @param userKey userKey
     * @return userValue
     */
    @Override
    public UV get(K key, UK userKey) {
        if (key == null || userKey == null) {
            return null;
        }
        return this.table.get(key, userKey);
    }

    /**
     * 根据key和多个userKey获取对应的userValue的集合
     *
     * @param key key
     * @param userKeys userKeys
     * @return userValue的集合
     */
    @Override
    public Map<UK, UV> getAll(K key, Collection<? extends UK> userKeys) {
        if (key == null || userKeys == null || userKeys.isEmpty()) {
            return new HashMap<>();
        }
        Map<UK, UV> res = new HashMap<>();
        for (UK userKey : userKeys) {
            if (userKey == null) {
                continue;
            }
            UV value = this.table.get(key, userKey);
            if (value == null) {
                continue;
            }
            res.put(userKey, value);
        }
        return res;
    }

    /**
     * 根据多个key和对应的多个userKey的集合获取对应的userValue，并组合为Map<K, Map<UK, UV>>返回
     *
     * @param keySet keySet
     * @return Map<K, Map<UK, UV>>
     */
    @Override
    public Map<K, Map<UK, UV>> getAll(Map<K, ? extends Collection<? extends UK>> keySet) {
        Map<K, Map<UK, UV>> res = new HashMap<>();
        if (keySet == null || keySet.isEmpty()) {
            return res;
        }
        for (Map.Entry<K, ? extends Collection<? extends UK>> entry : keySet.entrySet()) {
            if (entry == null || entry.getKey() == null || entry.getValue() == null) {
                continue;
            }
            K key = entry.getKey();
            Collection<? extends UK> userKeys = entry.getValue();
            Map<UK, UV> value = getAll(key, userKeys);
            if (value == null || value.isEmpty()) {
                continue;
            }
            res.put(key, value);
        }
        return res;
    }

    /**
     * 向指定key中添加一个Map.Entry<userKey, userValue>
     *
     * @param key key
     * @param userKey userKey
     * @param userValue userValue
     */
    @Override
    public void add(K key, UK userKey, UV userValue) {
        if (key == null || userKey == null || userValue == null) {
            return;
        }
        this.table.add(key, userKey, userValue);
    }

    /**
     * 向指定key中添加一个Map<userKey, userValue>
     *
     * @param key key
     * @param value Map<userKey, userValue>
     */
    @Override
    public void addAll(K key, Map<? extends UK, ? extends UV> value) {
        if (key == null || value == null || value.isEmpty()) {
            return;
        }
        this.table.addAll(key, value);
    }

    /**
     * 向多个指定key中添加多个Map<userKey, userValue>
     *
     * @param kMaps kMaps
     */
    @Override
    public void addAll(Map<? extends K, ? extends Map<? extends UK, ? extends UV>> kMaps) {
        if (kMaps == null || kMaps.isEmpty()) {
            return;
        }
        for (Map.Entry<? extends K, ? extends Map<? extends UK, ? extends UV>> entry : kMaps.entrySet()) {
            if (entry == null || entry.getKey() == null || entry.getValue() == null) {
                continue;
            }
            addAll(entry.getKey(), entry.getValue());
        }
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
        for (K key : keySet) {
            if (key == null) {
                continue;
            }
            this.table.remove(key);
        }
    }

    /**
     * 移除指定key下指定的Map.Entry<userKey, userValue>
     *
     * @param key key
     * @param userKey userKey
     */
    @Override
    public void remove(K key, UK userKey) {
        if (key == null || userKey == null) {
            return;
        }
        this.table.remove(key, userKey);
    }

    /**
     * 移除指定key下指定的Map<userKey, userValue>
     *
     * @param key key
     * @param userKeys userKeys
     */
    @Override
    public void removeAll(K key, Collection<? extends UK> userKeys) {
        if (key == null || userKeys == null || userKeys.isEmpty()) {
            return;
        }
        for (UK userKey : userKeys) {
            if (userKey == null) {
                continue;
            }
            remove(key, userKey);
        }
    }

    /**
     * 移除多个指定key下多个指定的Map<userKey, userValue>
     *
     * @param kMaps kMaps
     */
    @Override
    public void removeAll(Map<? extends K, ? extends Collection<? extends UK>> kMaps) {
        if (kMaps == null || kMaps.isEmpty()) {
            return;
        }
        for (Map.Entry<? extends K, ? extends Collection<? extends UK>> entry : kMaps.entrySet()) {
            if (entry == null || entry.getKey() == null || entry.getValue() == null) {
                continue;
            }
            removeAll(entry.getKey(), entry.getValue());
        }
    }

    /**
     * 获取指定key下Map<userKey, userValue>的迭代器
     *
     * @param key key
     * @return Map<userKey, userValue>
     */
    @Override
    public Iterator<Map.Entry<UK, UV>> iterator(K key) {
        if (key == null) {
            return null;
        }
        return new BoostIterator(key, this.table.iterator(key));
    }

    /**
     * 获取指定key下Map<userKey, userValue>的可迭代对象
     *
     * @param key key
     * @return Map<userKey, userValue>的可迭代对象
     */
    @Override
    public Iterable<Map.Entry<UK, UV>> entries(K key) {
        if (key == null) {
            return null;
        }
        return () -> iterator(key);
    }

    /**
     * 获取指定key下userKey的迭代器
     *
     * @param key key
     * @return userKey的迭代器
     */
    @Override
    public Iterable<UK> mapKeys(K key) {
        if (key == null) {
            return null;
        }
        return () -> new Iterator<UK>() {
            final Iterator iterator = KeyedMapStateImpl.this.iterator(key);

            @Override
            public boolean hasNext() {
                return this.iterator.hasNext();
            }

            @Override
            public UK next() {
                return ((Map.Entry<UK, UV>) this.iterator.next()).getKey();
            }

            @Override
            public void remove() {
                this.iterator.remove();
            }
        };
    }

    /**
     * 获取指定key下userValue的迭代器
     *
     * @param key key
     * @return userValue的迭代器
     */
    @Override
    public Iterable<UV> mapValues(K key) {
        if (key == null) {
            return null;
        }
        return () -> new Iterator<UV>() {
            final Iterator iterator = KeyedMapStateImpl.this.iterator(key);

            @Override
            public boolean hasNext() {
                return this.iterator.hasNext();
            }

            @Override
            public UV next() {
                return ((Map.Entry<UK, UV>) this.iterator.next()).getValue();
            }

            @Override
            public void remove() {
                this.iterator.remove();
            }
        };
    }

    /**
     * 获取state的描述符
     *
     * @return state的描述符
     */
    @Override
    public KeyedStateDescriptor getDescriptor() {
        return this.descriptor;
    }

    /**
     * 获取序列化后的value
     *
     * @param serializedKeyAndNamespace 序列化后的key
     * @param keySerializer key序列化器
     * @param valueSerializer value序列化器
     * @return 序列化后的value
     * @throws Exception exception
     */
    @Override
    public byte[] getSerializedValue(byte[] serializedKeyAndNamespace, TypeSerializer<K> keySerializer,
        TypeSerializer<Map<UK, UV>> valueSerializer) throws Exception {
        if (!(valueSerializer instanceof MapSerializer)) {
            LOG.error("Failed to getSerializedValue for mapState, expected MapSerializer, but got: {}",
                valueSerializer.getClass());
            return null;
        }
        K key = KvStateSerializer.deserializeKeyAndNamespace(serializedKeyAndNamespace, keySerializer,
            VoidNamespaceSerializer.INSTANCE).f0;
        if (key == null) {
            LOG.error("Failed to getSerializedValue for key: null.");
            return null;
        }
        return this.table.getSerializedValue(key);
    }

    /**
     * 用于获取指定key下 “UK/UV Map” 的迭代器
     */
    class BoostIterator implements Iterator<Map.Entry<UK, UV>> {
        Iterator<Map.Entry<UK, UV>> iterator;

        BoostMapEntry current;

        private final K key;

        BoostIterator(K key, Iterator<Map.Entry<UK, UV>> iterator) {
            this.key = key;
            this.iterator = iterator;
        }

        @Override
        public boolean hasNext() {
            return this.iterator != null && this.iterator.hasNext();
        }

        @Override
        public Map.Entry<UK, UV> next() {
            Map.Entry<UK, UV> entry = this.iterator.next();
            this.current = new BoostMapEntry(this.key, entry.getKey(), entry.getValue());
            return this.current;
        }

        @Override
        public void remove() {
            if (this.current == null) {
                throw new BSSRuntimeException("Failed to remove, currentEntry is null.");
            }
            this.current.remove();
        }
    }

    /**
     * 用于封装某个“UK/UV Map”的Entry
     */
    class BoostMapEntry implements Map.Entry<UK, UV> {
        private boolean deleted;

        private final K key;

        private final UK userKey;

        private UV userValue;

        BoostMapEntry(K key, UK userKey, UV userValue) {
            Preconditions.checkNotNull(key);
            Preconditions.checkNotNull(userKey);
            Preconditions.checkNotNull(userValue);
            this.deleted = false;
            this.key = key;
            this.userKey = userKey;
            this.userValue = userValue;
        }

        @Override
        public UK getKey() {
            return this.userKey;
        }

        @Override
        public UV getValue() {
            return this.userValue;
        }

        @Override
        public UV setValue(UV value) {
            if (value == null) {
                return null;
            }
            if (this.deleted) {
                throw new BSSRuntimeException("Failed to set value, state has been deleted");
            }
            UV oldValue = this.userValue;
            this.userValue = value;
            KeyedMapStateImpl.this.table.addForIterator(this.key, this.userKey, this.userValue);
            return oldValue;
        }

        /**
         * 删除当前Map.Entry<UK, UV>
         */
        public void remove() {
            if (this.deleted) {
                throw new BSSRuntimeException("Failed to remove entry, already deleted");
            }
            this.deleted = true;
            this.userValue = null;
            KeyedMapStateImpl.this.table.remove(this.key, this.userKey);
        }
    }
}