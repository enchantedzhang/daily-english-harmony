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

import java.util.Collection;
import java.util.List;

/**
 * 含namespace的listState接口
 *
 * @param <K> key
 * @param <N> namespace
 * @param <E> element
 * @since BeiMing 25.0.T1
 */
public interface NSKeyedListState<K, N, E> extends NSKeyedState<K, N, List<E>> {
    /**
     * 指定key和namespace，add一个元素
     *
     * @param key key
     * @param namespace namespace
     * @param element element
     */
    void add(K key, N namespace, E element);

    /**
     * 指定key和namespace，add多个元素
     *
     * @param key key
     * @param namespace namespace
     * @param elements 元素集合
     */
    void addAll(K key, N namespace, Collection<? extends E> elements);

    /**
     * 指定key和namespace，将element作为单元素list，新增/覆盖写
     *
     * @param key key
     * @param namespace namespace
     * @param element element
     */
    void put(K key, N namespace, E element);

    /**
     * 指定key和namespace，将elements作为list，新增/覆盖写
     *
     * @param key key
     * @param namespace namespace
     * @param elements elements
     */
    void putAll(K key, N namespace, Collection<? extends E> elements);

    /**
     * 指定key和namespace下移除指定元素
     *
     * @param key key
     * @param namespace namespace
     * @param element element
     * @return 成功返回true
     */
    boolean remove(K key, N namespace, E element);

    /**
     * 移除指定key和namespace下多个指定元素
     *
     * @param key key
     * @param namespace namespace
     * @param elements 多个元素
     * @return 成功返回true
     */
    boolean removeAll(K key, N namespace, Collection<? extends E> elements);

    /**
     * 待定
     *
     * @param key key
     * @param namespace namespace
     * @return element
     */
    E poll(K key, N namespace);

    /**
     * 待定
     *
     * @param key key
     * @param namespace namespace
     * @return element
     */
    E peek(K key, N namespace);
}
