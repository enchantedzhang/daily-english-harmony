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
import java.util.Iterator;
import java.util.Map;

/**
 * 含namespace mapState接口
 *
 * @param <K> key
 * @param <N> namespace
 * @param <UK> userKey
 * @param <UV> userValue
 * @since BeiMing 25.0.T1
 */
public interface NSKeyedMapState<K, N, UK, UV> extends NSKeyedState<K, N, Map<UK, UV>> {
    /**
     * 判断对应key、namespace和userKey下的value是否存在
     *
     * @param key key
     * @param namespace namespace
     * @param userKey userKey
     * @return “存在”返回true
     */
    boolean contains(K key, N namespace, UK userKey);

    /**
     * 根据key、namespace和userKey获取userValue
     *
     * @param key key
     * @param namespace namespace
     * @param userKey userKey
     * @return userValue
     */
    UV get(K key, N namespace, UK userKey);

    /**
     * 根据key、namespace和多个userKey获取对应的userValue的集合
     *
     * @param key key
     * @param namespace namespace
     * @param userKeySet userKeySet
     * @return userValue的集合
     */
    Map<UK, UV> getAll(K key, N namespace, Collection<? extends UK> userKeySet);

    /**
     * 向指定key、namespace中添加一个Map.Entry<userKey, userValue>
     *
     * @param key key
     * @param namespace namespace
     * @param userKey userKey
     * @param userValue userValue
     */
    void add(K key, N namespace, UK userKey, UV userValue);

    /**
     * 向指定key、namespace中添加一个Map<userKey, userValue>
     *
     * @param key key
     * @param namespace namespace
     * @param value Map<userKey, userValue>
     */
    void addAll(K key, N namespace, Map<? extends UK, ? extends UV> value);

    /**
     * 移除指定key、namespace下指定的Map.Entry<userKey, userValue>
     *
     * @param key key
     * @param namespace namespace
     * @param userKey userKey
     */
    void remove(K key, N namespace, UK userKey);

    /**
     * 移除指定key、namespace下指定的Map<userKey, userValue>
     *
     * @param key key
     * @param namespace namespace
     * @param userKeySet userKeySet
     */
    void removeAll(K key, N namespace, Collection<? extends UK> userKeySet);

    /**
     * 获取指定key、namespace下Map<userKey, userValue>的迭代器
     *
     * @param key key
     * @param namespace namespace
     * @return Map<userKey, userValue>
     */
    Iterator<Map.Entry<UK, UV>> iterator(K key, N namespace);

    /**
     * 获取指定key、namespace下Map<userKey, userValue>的可迭代对象
     *
     * @param key key
     * @param namespace namespace
     * @return Map<userKey, userValue>的可迭代对象
     */
    Iterable<Map.Entry<UK, UV>> entries(K key, N namespace);

    /**
     * 获取指定key、namespace下userKey的迭代器
     *
     * @param key key
     * @param namespace namespace
     * @return userKey的迭代器
     */
    Iterable<UK> keys(K key, N namespace);

    /**
     * 获取指定key、namespace下userValue的迭代器
     *
     * @param key key
     * @param namespace namespace
     * @return userValue的迭代器
     */
    Iterable<UV> values(K key, N namespace);
}
