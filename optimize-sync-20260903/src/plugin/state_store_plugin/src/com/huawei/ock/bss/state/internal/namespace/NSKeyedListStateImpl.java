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

import com.huawei.ock.bss.state.internal.descriptor.nskeyed.NSKeyedListStateDescriptor;
import com.huawei.ock.bss.state.internal.descriptor.nskeyed.NSKeyedStateDescriptor;
import com.huawei.ock.bss.table.api.NsKListTable;

import org.apache.flink.api.common.typeutils.TypeSerializer;
import org.apache.flink.api.common.typeutils.base.ListSerializer;
import org.apache.flink.api.java.tuple.Tuple2;
import org.apache.flink.queryablestate.client.state.serialization.KvStateSerializer;
import org.apache.flink.runtime.state.ListDelimitedSerializer;
import org.apache.flink.util.Preconditions;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import java.io.IOException;
import java.util.ArrayList;
import java.util.Collection;
import java.util.Collections;
import java.util.Iterator;
import java.util.List;
import java.util.Map;

/**
 * 含namespace的listState实现类
 *
 * @param <K> key
 * @param <N> namespace
 * @param <E> element
 * @since BeiMing 25.0.T1
 */
public class NSKeyedListStateImpl<K, N, E> implements NSKeyedListState<K, N, E> {
    private static final Logger LOG = LoggerFactory.getLogger(NSKeyedListStateImpl.class);

    private final NSKeyedListStateDescriptor<K, N, E> descriptor;

    private final NsKListTable<K, N, E> table;

    private final ListDelimitedSerializer listDelimitedSerializer;

    public NSKeyedListStateImpl(NSKeyedListStateDescriptor<K, N, E> descriptor, NsKListTable<K, N, E> table) {
        Preconditions.checkNotNull(descriptor);
        Preconditions.checkNotNull(table);

        this.descriptor = descriptor;
        this.table = table;
        this.listDelimitedSerializer = new ListDelimitedSerializer();
    }

    /**
     * 指定key和namespace，add一个元素
     *
     * @param key       key
     * @param namespace namespace
     * @param element   element
     */
    @Override
    public void add(K key, N namespace, E element) {
        if (key == null || namespace == null || element == null) {
            return;
        }
        this.table.add(this.table.getKeyPair(key, namespace), element);
    }

    /**
     * 指定key和namespace，add多个元素
     *
     * @param key       key
     * @param namespace namespace
     * @param elements  元素集合
     */
    @Override
    public void addAll(K key, N namespace, Collection<? extends E> elements) {
        if (key == null || namespace == null || elements == null) {
            return;
        }
        if (elements.isEmpty()) {
            return;
        }
        this.table.addAll(this.table.getKeyPair(key, namespace), elements);
    }

    /**
     * 根据key+namespace获取value
     *
     * @param key       key
     * @param namespace namespace
     * @return value
     */
    @Override
    public List<E> get(K key, N namespace) {
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
    public Map<N, List<E>> getAll(K key) {
        throw new UnsupportedOperationException("getAll not supported in listState now.");
    }

    /**
     * 指定key和namespace，将element作为单元素list，新增/覆盖写
     *
     * @param key       key
     * @param namespace namespace
     * @param element   element
     */
    @Override
    public void put(K key, N namespace, E element) {
        if (key == null || namespace == null || element == null) {
            return;
        }
        this.table.put(this.table.getKeyPair(key, namespace), Collections.singletonList(element));
    }

    /**
     * 指定key和namespace，将elements作为list，新增/覆盖写
     *
     * @param key       key
     * @param namespace namespace
     * @param elements  elements
     */
    @Override
    public void putAll(K key, N namespace, Collection<? extends E> elements) {
        if (key == null || namespace == null || elements == null) {
            return;
        }
        if (elements.isEmpty()) {
            return;
        }
        this.table.put(this.table.getKeyPair(key, namespace), new ArrayList<>(elements));
    }

    /**
     * 删除List中指定的一个element
     *
     * @param key       key
     * @param namespace namespace
     * @param element   待删除element
     * @return 执行结果
     */
    @Override
    public boolean remove(K key, N namespace, E element) {
        if (key == null || namespace == null) {
            return false;
        }
        if (element == null) {
            return true;
        }
        List<E> value = get(key, namespace);
        if (value == null) {
            return true;
        }
        value.remove(element);
        if (value.isEmpty()) {
            remove(key, namespace);
            return true;
        }
        putAll(key, namespace, value);
        return true;
    }

    /**
     * 移除指定key和namespace下多个指定元素
     *
     * @param key       key
     * @param namespace namespace
     * @param elements  多个元素
     * @return 成功返回true
     */
    @Override
    public boolean removeAll(K key, N namespace, Collection<? extends E> elements) {
        if (key == null || namespace == null || elements == null || elements.isEmpty()) {
            return false;
        }
        List<E> value = get(key, namespace);
        if (value == null) {
            return true;
        }
        value.removeAll(elements);
        if (value.isEmpty()) {
            remove(key, namespace);
            return true;
        }
        putAll(key, namespace, value);
        return true;
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
        throw new UnsupportedOperationException("remove all in a key not supported now.");
    }

    /**
     * 待定
     *
     * @param key       key
     * @param namespace namespace
     * @return element
     */
    @Override
    public E poll(K key, N namespace) {
        throw new UnsupportedOperationException();
    }

    /**
     * 待定
     *
     * @param key       key
     * @param namespace namespace
     * @return element
     */
    @Override
    public E peek(K key, N namespace) {
        throw new UnsupportedOperationException();
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
        return this.table.contains(this.table.getKeyPair(key, namespace));
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
     * @throws IOException ioException
     */
    @Override
    public byte[] getSerializedValue(byte[] serializedKeyAndNamespace, TypeSerializer<K> keySerializer,
        TypeSerializer<N> namespaceSerializer, TypeSerializer<List<E>> valueSerializer) throws IOException {
        if (!(valueSerializer instanceof ListSerializer)) {
            LOG.error("Failed to getSerializedValue for listState, expected ListSerializer, but got: {}",
                valueSerializer.getClass());
            return null;
        }
        Tuple2<K, N> keyAndNamespace = KvStateSerializer.deserializeKeyAndNamespace(
            serializedKeyAndNamespace, keySerializer, namespaceSerializer);
        if (keyAndNamespace.f0 == null || keyAndNamespace.f1 == null) {
            LOG.error("Failed to getSerializedValue for key: {}, namespace: {}.", keyAndNamespace.f0,
                keyAndNamespace.f1);
            return null;
        }
        List<E> value = get(keyAndNamespace.f0, keyAndNamespace.f1);
        if (value == null) {
            LOG.error("Failed to getSerializedValue for key: {}, namespace: {}, key/namespace do not exist.",
                keyAndNamespace.f0, keyAndNamespace.f1);
            return null;
        }
        return listDelimitedSerializer.serializeList(value,
            ((ListSerializer<E>) valueSerializer).getElementSerializer());
    }
}
