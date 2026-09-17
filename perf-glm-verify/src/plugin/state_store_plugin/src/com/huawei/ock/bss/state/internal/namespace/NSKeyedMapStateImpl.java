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
import com.huawei.ock.bss.state.internal.descriptor.nskeyed.NSKeyedMapStateDescriptor;
import com.huawei.ock.bss.state.internal.descriptor.nskeyed.NSKeyedStateDescriptor;
import com.huawei.ock.bss.table.api.NsKMapTable;

import org.apache.flink.api.common.typeutils.TypeSerializer;
import org.apache.flink.api.common.typeutils.base.MapSerializer;
import org.apache.flink.api.java.tuple.Tuple2;
import org.apache.flink.queryablestate.client.state.serialization.KvStateSerializer;
import org.apache.flink.util.Preconditions;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import java.util.Collection;
import java.util.HashMap;
import java.util.Iterator;
import java.util.Map;

/**
 * 含namespace mapState实现类
 *
 * @param <K>  key
 * @param <N>  namespace
 * @param <UK> userKey
 * @param <UV> userValue
 * @since BeiMing 25.0.T1
 */
public class NSKeyedMapStateImpl<K, N, UK, UV> implements NSKeyedMapState<K, N, UK, UV> {
    private static final Logger LOG = LoggerFactory.getLogger(NSKeyedMapStateImpl.class);

    private final NSKeyedMapStateDescriptor<K, N, UK, UV> descriptor;

    private final NsKMapTable<K, N, UK, UV, Map<UK, UV>> table;

    public NSKeyedMapStateImpl(NSKeyedMapStateDescriptor<K, N, UK, UV> descriptor,
        NsKMapTable<K, N, UK, UV, Map<UK, UV>> table) {
        Preconditions.checkNotNull(descriptor);
        Preconditions.checkNotNull(table);

        this.descriptor = descriptor;
        this.table = table;
    }

    /**
     * 判断对应key、namespace和userKey下的value是否存在
     *
     * @param key       key
     * @param namespace namespace
     * @param userKey   userKey
     * @return “存在”返回true
     */
    @Override
    public boolean contains(K key, N namespace, UK userKey) {
        if (key == null || namespace == null || userKey == null) {
            return false;
        }
        return this.table.contains(this.table.getKeyPair(key, namespace), userKey);
    }

    @Override
    public boolean contains(K key, N namespace) {
        if (key == null || namespace == null) {
            return false;
        }
        return this.table.contains(this.table.getKeyPair(key, namespace));
    }

    /**
     * 根据key、namespace和userKey获取userValue
     *
     * @param key       key
     * @param namespace namespace
     * @param userKey   userKey
     * @return userValue
     */
    @Override
    public UV get(K key, N namespace, UK userKey) {
        if (key == null || namespace == null || userKey == null) {
            return null;
        }
        // 默认值为null因此不做检查分支
        return this.table.get(this.table.getKeyPair(key, namespace), userKey);
    }

    /**
     * 根据key、namespace和多个userKey获取对应的userValue的集合
     *
     * @param key        key
     * @param namespace  namespace
     * @param userKeySet userKeySet
     * @return userValue的集合
     */
    @Override
    public Map<UK, UV> getAll(K key, N namespace, Collection<? extends UK> userKeySet) {
        if (key == null || namespace == null || userKeySet == null || userKeySet.isEmpty()) {
            return new HashMap<>();
        }
        Map<UK, UV> res = new HashMap<>();
        for (UK userKey : userKeySet) {
            if (userKey == null) {
                continue;
            }
            UV userValue = get(key, namespace, userKey);
            if (userValue == null) {
                continue;
            }
            res.put(userKey, userValue);
        }
        return res;
    }

    /**
     * 根据key+namespace获取Map<UK, UV>
     *
     * @param key       key
     * @param namespace namespace
     * @return Map<UK, UV>
     */
    @Override
    public Map<UK, UV> get(K key, N namespace) {
        if (key == null || namespace == null) {
            return null;
        }
        return this.table.get(this.table.getKeyPair(key, namespace));
    }

    /**
     * 根据key获取KV对
     *
     * @param key key
     * @return NV对
     */
    @Override
    public Map<N, Map<UK, UV>> getAll(K key) {
        throw new UnsupportedOperationException();
    }

    /**
     * 向指定key、namespace中添加一个Map.Entry<userKey, userValue>
     *
     * @param key       key
     * @param namespace namespace
     * @param userKey   userKey
     * @param userValue userValue
     */
    @Override
    public void add(K key, N namespace, UK userKey, UV userValue) {
        if (key == null || namespace == null || userKey == null || userValue == null) {
            return;
        }
        this.table.add(this.table.getKeyPair(key, namespace), userKey, userValue);
    }

    /**
     * 向指定key、namespace中添加一个Map<userKey, userValue>
     *
     * @param key       key
     * @param namespace namespace
     * @param value     Map<userKey, userValue>
     */
    @Override
    public void addAll(K key, N namespace, Map<? extends UK, ? extends UV> value) {
        if (key == null || namespace == null || value == null || value.isEmpty()) {
            return;
        }
        this.table.addAll(this.table.getKeyPair(key, namespace), value);
    }

    /**
     * 移除指定key、namespace下指定的Map.Entry<userKey, userValue>
     *
     * @param key       key
     * @param namespace namespace
     * @param userKey   userKey
     */
    @Override
    public void remove(K key, N namespace, UK userKey) {
        if (key == null || namespace == null || userKey == null) {
            return;
        }
        this.table.remove(this.table.getKeyPair(key, namespace), userKey);
    }

    /**
     * 移除指定key、namespace下指定的Map<userKey, userValue>
     *
     * @param key        key
     * @param namespace  namespace
     * @param userKeySet userKeySet
     */
    @Override
    public void removeAll(K key, N namespace, Collection<? extends UK> userKeySet) {
        if (key == null || namespace == null || userKeySet == null || userKeySet.isEmpty()) {
            return;
        }
        for (UK userKey : userKeySet) {
            if (userKey == null) {
                continue;
            }
            remove(key, namespace, userKey);
        }
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
        this.table.remove(this.table.getKeyPair(key, namespace));
    }

    /**
     * 移除一个key下面所有namespace的数据
     *
     * @param key key的集合
     */
    @Override
    public void removeAll(K key) {
        throw new UnsupportedOperationException();
    }

    /**
     * 获取指定key、namespace下Map<userKey, userValue>的迭代器
     *
     * @param key       key
     * @param namespace namespace
     * @return Map<userKey, userValue>
     */
    @Override
    public Iterator<Map.Entry<UK, UV>> iterator(K key, N namespace) {
        if (key == null || namespace == null) {
            return null;
        }
        return new BoostNSIterator(key, namespace, this.table.iterator(this.table.getKeyPair(key, namespace)));
    }

    /**
     * 获取指定key、namespace下Map<userKey, userValue>的可迭代对象
     *
     * @param key       key
     * @param namespace namespace
     * @return Map<userKey, userValue>的可迭代对象
     */
    @Override
    public Iterable<Map.Entry<UK, UV>> entries(K key, N namespace) {
        if (key == null || namespace == null) {
            return null;
        }
        return () -> iterator(key, namespace);
    }

    /**
     * 获取指定key、namespace下userKey的迭代器
     *
     * @param key       key
     * @param namespace namespace
     * @return userKey的迭代器
     */
    @Override
    public Iterable<UK> keys(K key, N namespace) {
        if (key == null || namespace == null) {
            return null;
        }
        return () -> new Iterator<UK>() {
            final Iterator<Map.Entry<UK, UV>> iterator = NSKeyedMapStateImpl.this.iterator(key, namespace);

            @Override
            public boolean hasNext() {
                return this.iterator.hasNext();
            }

            @Override
            public UK next() {
                return this.iterator.next().getKey();
            }

            @Override
            public void remove() {
                this.iterator.remove();
            }
        };
    }

    /**
     * 获取指定key、namespace下userValue的迭代器
     *
     * @param key       key
     * @param namespace namespace
     * @return userValue的迭代器
     */
    @Override
    public Iterable<UV> values(K key, N namespace) {
        if (key == null || namespace == null) {
            return null;
        }
        return () -> new Iterator<UV>() {
            final Iterator<Map.Entry<UK, UV>> iterator = NSKeyedMapStateImpl.this.iterator(key, namespace);

            @Override
            public boolean hasNext() {
                return this.iterator.hasNext();
            }

            @Override
            public UV next() {
                return this.iterator.next().getValue();
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
    public NSKeyedStateDescriptor getDescriptor() {
        return this.descriptor;
    }

    /**
     * 获取key下多个namespace的迭代器
     *
     * @param key key
     * @return namespace的迭代器
     */
    @Override
    public Iterator<N> iterator(K key) {
        throw new UnsupportedOperationException();
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
        TypeSerializer<N> namespaceSerializer, TypeSerializer<Map<UK, UV>> valueSerializer) throws Exception {
        if (!(valueSerializer instanceof MapSerializer)) {
            LOG.error("Failed to getSerializedValue for mapState, expected MapSerializer, but got: {}",
                valueSerializer.getClass());
            return null;
        }
        Tuple2<K, N> keyAndNamespace =
            KvStateSerializer.deserializeKeyAndNamespace(
                serializedKeyAndNamespace, keySerializer, namespaceSerializer);
        if (keyAndNamespace.f0 == null || keyAndNamespace.f1 == null) {
            LOG.error("Failed to getSerializedValue for key: {}, namespace: {}", keyAndNamespace.f0,
                keyAndNamespace.f1);
            return null;
        }
        return this.table.getSerializedValue(this.table.getKeyPair(keyAndNamespace.f0, keyAndNamespace.f1));
    }

    /**
     * 用于获取指定key、namespace下 “UK/UV Map” 的迭代器
     */
    class BoostNSIterator implements Iterator<Map.Entry<UK, UV>> {
        Iterator<Map.Entry<UK, UV>> iterator;

        BoostNSMapEntry current;

        private final K key;

        private final N namespace;

        BoostNSIterator(K key, N namespace, Iterator<Map.Entry<UK, UV>> iterator) {
            this.key = key;
            this.namespace = namespace;
            this.iterator = iterator;
        }

        @Override
        public boolean hasNext() {
            return this.iterator != null && this.iterator.hasNext();
        }

        @Override
        public Map.Entry<UK, UV> next() {
            Map.Entry<UK, UV> entry = this.iterator.next();
            this.current = new BoostNSMapEntry(this.key, this.namespace, entry.getKey(), entry.getValue());
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
    class BoostNSMapEntry implements Map.Entry<UK, UV> {
        private boolean deleted;

        private final K key;

        private final N namespace;

        private final UK userKey;

        private UV userValue;

        BoostNSMapEntry(K key, N namespace, UK userKey, UV userValue) {
            this.key = key;
            this.namespace = namespace;
            this.userKey = userKey;
            this.userValue = userValue;
            this.deleted = false;
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
            NSKeyedMapStateImpl.this.table.addForIterator(
                NSKeyedMapStateImpl.this.table.getKeyPair(this.key, this.namespace), this.userKey, this.userValue);
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
            NSKeyedMapStateImpl.this.table.removeForIterator(
                NSKeyedMapStateImpl.this.table.getKeyPair(this.key, this.namespace), this.userKey);
        }
    }
}
