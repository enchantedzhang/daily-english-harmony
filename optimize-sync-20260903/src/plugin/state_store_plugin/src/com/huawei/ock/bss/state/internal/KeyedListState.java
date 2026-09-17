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

import java.util.Collection;
import java.util.List;
import java.util.Map;

/**
 * 无namespace listState接口
 *
 * @param <K> key
 * @param <E> element
 * @since BeiMing 25.0.T1
 */
public interface KeyedListState<K, E> extends KeyedState<K, List<E>> {
    /**
     * 指定key，add一个元素
     *
     * @param key     key
     * @param element element
     */
    void add(K key, E element);

    /**
     * 指定key，add多个元素
     *
     * @param key      key
     * @param elements 元素集合
     */
    void addAll(K key, Collection<? extends E> elements);

    /**
     * 将多个Key/List批量add
     *
     * @param keyLists 多个Key/List
     */
    void addAll(Map<? extends K, ? extends Collection<? extends E>> keyLists);

    /**
     * 指定key，将element作为单元素list，新增/覆盖写
     *
     * @param key     key
     * @param element element
     */
    void put(K key, E element);

    /**
     * 指定key，将elements作为list，新增/覆盖写
     *
     * @param key      key
     * @param elements elements
     */
    void putAll(K key, Collection<? extends E> elements);

    /**
     * 批量 putAll(K key, Collection<? extends E> elements)
     *
     * @param keyLists keyLists
     */
    void putAll(Map<? extends K, ? extends Collection<? extends E>> keyLists);

    /**
     * 指定key下移除指定元素
     *
     * @param key     key
     * @param element element
     * @return 成功返回true
     */
    boolean remove(K key, E element);

    /**
     * 移除指定key下多个指定元素
     *
     * @param key      key
     * @param elements 多个元素
     * @return 成功返回true
     */
    boolean removeAll(K key, Collection<? extends E> elements);

    /**
     * 批量 removeAll(K key, Collection<? extends E> elements)
     *
     * @param keyLists keyLists
     * @return 成功返回true
     */
    boolean removeAll(Map<? extends K, ? extends Collection<? extends E>> keyLists);
}
