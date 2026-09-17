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

import com.huawei.ock.bss.state.internal.descriptor.keyed.KeyedListStateDescriptor;
import com.huawei.ock.bss.state.internal.descriptor.keyed.KeyedStateDescriptor;
import com.huawei.ock.bss.table.api.KListTable;

import org.apache.flink.api.common.typeutils.TypeSerializer;
import org.apache.flink.api.common.typeutils.base.ListSerializer;
import org.apache.flink.queryablestate.client.state.serialization.KvStateSerializer;
import org.apache.flink.runtime.state.ListDelimitedSerializer;
import org.apache.flink.runtime.state.VoidNamespaceSerializer;
import org.apache.flink.util.Preconditions;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import java.io.IOException;
import java.util.ArrayList;
import java.util.Collection;
import java.util.Collections;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/**
 * 无namespace listState实现类
 *
 * @param <K> key
 * @param <E> element
 * @since BeiMing 25.0.T1
 */
public class KeyedListStateImpl<K, E> implements KeyedListState<K, E> {
    private static final Logger LOG = LoggerFactory.getLogger(KeyedListStateImpl.class);

    private final KeyedListStateDescriptor<K, E> descriptor;

    private final KListTable<K, E> table;

    private final ListDelimitedSerializer listDelimitedSerializer;

    public KeyedListStateImpl(KeyedListStateDescriptor<K, E> descriptor, KListTable<K, E> table) {
        Preconditions.checkNotNull(descriptor);
        Preconditions.checkNotNull(table);

        this.descriptor = descriptor;
        this.table = table;
        this.listDelimitedSerializer = new ListDelimitedSerializer();
    }

    /**
     * 指定key添加一个元素
     *
     * @param key     key
     * @param element element
     */
    @Override
    public void add(K key, E element) {
        if (key == null || element == null) {
            LOG.debug("Trying to add 'key: {}, element: {}' to listState.", key, element);
            return;
        }
        this.table.add(key, element);
    }

    /**
     * 指定key添加多个元素
     *
     * @param key      key
     * @param elements 元素集合
     */
    @Override
    public void addAll(K key, Collection<? extends E> elements) {
        if (key == null) {
            LOG.debug("Key is null while addAll in listState.");
            return;
        }
        if (elements == null || elements.isEmpty()) {
            LOG.debug("Element collection is null or empty while addAll in listState");
            return;
        }
        this.table.addAll(key, elements);
    }

    /**
     * 为多个key各添加不同的元素集合
     *
     * @param kListMap value为elements集合
     */
    @Override
    public void addAll(Map<? extends K, ? extends Collection<? extends E>> kListMap) {
        if (kListMap == null || kListMap.isEmpty()) {
            LOG.debug("kListMap collection is null or empty while addAll in listState");
            return;
        }
        for (Map.Entry<? extends K, ? extends Collection<? extends E>> entry : kListMap.entrySet()) {
            addAll(entry.getKey(), entry.getValue());
        }
    }

    /**
     * 判断"Key/list对"在table中是否存在
     *
     * @param key key
     * @return “是”返回true
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
    public List<E> get(K key) {
        if (key == null) {
            LOG.debug("key is null while get in listState.");
            return null;
        }
        return this.table.get(key);
    }

    /**
     * 未被上层调用
     *
     * @param keySet key的集合
     * @return KList集合
     */
    @Override
    public Map<K, List<E>> getAll(Collection<? extends K> keySet) {
        if (keySet == null || keySet.isEmpty()) {
            LOG.debug("keySet is null or empty while getAll in listState.");
            return Collections.emptyMap();
        }
        Map<K, List<E>> res = new HashMap<>();
        for (K key : keySet) {
            if (key == null) {
                continue;
            }
            List<E> elements = get(key);
            if (elements == null || elements.isEmpty()) {
                continue;
            }
            res.put(key, elements);
        }
        return res;
    }

    /**
     * 指定key，将element作为单元素list，新增/覆盖写
     *
     * @param key     key
     * @param element element
     */
    @Override
    public void put(K key, E element) {
        if (key == null || element == null) {
            LOG.debug("Trying to put 'key: {}, element: {}' to listState.", key, element);
            return;
        }
        this.table.put(key, Collections.singletonList(element));
    }

    /**
     * 指定key，将elements作为list，新增/覆盖写
     *
     * @param key      key
     * @param elements elements
     */
    @Override
    public void putAll(K key, Collection<? extends E> elements) {
        if (key == null) {
            LOG.debug("Key is null while pullAll in listState.");
            return;
        }
        if (elements == null || elements.isEmpty()) {
            LOG.debug("Element collection is null or empty while putAll in listState.");
            return;
        }
        List<E> value = new ArrayList<>();
        for (E element : elements) {
            if (element == null) {
                LOG.debug("Element is null while putAll in listState.");
                return;
            }
            value.add(element);
        }
        this.table.put(key, value);
    }

    /**
     * 批量 putAll(K key, Collection<? extends E> elements)
     *
     * @param kListMap kListMap
     */
    @Override
    public void putAll(Map<? extends K, ? extends Collection<? extends E>> kListMap) {
        if (kListMap == null || kListMap.isEmpty()) {
            LOG.debug("kListMap collection is null or empty while putAll in listState.");
            return;
        }
        for (Map.Entry<? extends K, ? extends Collection<? extends E>> entry : kListMap.entrySet()) {
            putAll(entry.getKey(), entry.getValue());
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
            LOG.debug("Trying to remove null key in listState.");
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
            LOG.debug("keySet is null while removeAll in listState");
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
     * 指定key下移除指定元素
     *
     * @param key     key
     * @param element element
     * @return 成功返回true
     */
    @Override
    public boolean remove(K key, E element) {
        if (key == null) {
            LOG.debug("key is null while remove in listState");
            return false;
        }
        if (element == null) {
            return true;
        }
        removeElement(key, element);
        return true;
    }

    private void removeElement(K key, E element) {
        List<E> value = get(key);
        if (value == null || value.isEmpty()) {
            LOG.debug("value is null or empty while removeElement in listState");
            return;
        }
        value.remove(element);
        if (value.isEmpty()) {
            remove(key);
        } else {
            putAll(key, value);
        }
    }

    /**
     * 移除指定key下多个指定元素
     *
     * @param key      key
     * @param elements 多个元素
     * @return 成功返回true
     */
    @Override
    public boolean removeAll(K key, Collection<? extends E> elements) {
        if (key == null) {
            LOG.debug("key is null while removeAll in listState");
            return false;
        }
        if (elements == null || elements.isEmpty()) {
            return true;
        }
        removeElements(key, elements);
        return true;
    }

    private void removeElements(K key, Collection<? extends E> elements) {
        List<E> value = get(key);
        if (value == null || value.isEmpty()) {
            LOG.debug("value is null or empty while removeElements in listState");
            return;
        }
        value.removeAll(elements);
        if (value.isEmpty()) {
            remove(key);
        } else {
            putAll(key, value);
        }
    }

    /**
     * 批量 removeAll(K key, Collection<? extends E> elements)
     *
     * @param keyMap keyMap
     * @return 成功返回true
     */
    @Override
    public boolean removeAll(Map<? extends K, ? extends Collection<? extends E>> keyMap) {
        if (keyMap == null || keyMap.isEmpty()) {
            LOG.debug("keyMap is null or empty while removeAll in listState");
            return false;
        }
        for (Map.Entry<? extends K, ? extends Collection<? extends E>> entry : keyMap.entrySet()) {
            if (entry == null || entry.getKey() == null || entry.getValue() == null) {
                continue;
            }
            K key = entry.getKey();
            Collection<? extends E> value = entry.getValue();
            removeElements(key, value);
        }
        return true;
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
     * @param keySerializer             key序列化器
     * @param valueSerializer           value序列化器
     * @return 序列化后的value
     * @throws IOException ioException
     */
    @Override
    public byte[] getSerializedValue(byte[] serializedKeyAndNamespace, TypeSerializer<K> keySerializer,
        TypeSerializer<List<E>> valueSerializer) throws IOException {
        if (!(valueSerializer instanceof ListSerializer)) {
            LOG.error("Failed to getSerializedValue for listState, expected ListSerializer, but got: {}",
                valueSerializer.getClass());
            return null;
        }
        K key = KvStateSerializer.deserializeKeyAndNamespace(serializedKeyAndNamespace, keySerializer,
            VoidNamespaceSerializer.INSTANCE).f0;
        if (key == null) {
            LOG.error("Failed to getSerializedValue for key: null.");
            return null;
        }
        List<E> value = get(key);
        if (value == null) {
            LOG.error("value is null while getSerializedValue in listState");
            return null;
        }
        return listDelimitedSerializer.serializeList(value,
            ((ListSerializer<E>) valueSerializer).getElementSerializer());
    }
}
