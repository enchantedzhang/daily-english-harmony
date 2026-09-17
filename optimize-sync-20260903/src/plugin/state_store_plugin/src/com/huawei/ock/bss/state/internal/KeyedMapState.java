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
import java.util.Iterator;
import java.util.Map;

/**
 * 无namespace mapState接口
 *
 * @param <K> key
 * @param <UK> userKey
 * @param <UV> userValue
 * @since BeiMing 25.0.T1
 */
public interface KeyedMapState<K, UK, UV> extends KeyedState<K, Map<UK, UV>> {
    /**
     * 判断对应key和userKey下的value是否存在
     *
     * @param key key
     * @param userKey userKey
     * @return “存在”返回true
     */
    boolean contains(K key, UK userKey);

    /**
     * 根据key和userKey获取userValue
     *
     * @param key key
     * @param userKey userKey
     * @return userValue
     */
    UV get(K key, UK userKey);

    /**
     * 根据key和多个userKey获取对应的userValue的集合
     *
     * @param key key
     * @param userKeys userKeys
     * @return userValue的集合
     */
    Map<UK, UV> getAll(K key, Collection<? extends UK> userKeys);

    /**
     * 根据多个key和对应的多个userKey的集合获取对应的userValue，并组合为Map<K, Map<UK, UV>>返回
     *
     * @param keySet keySet
     * @return Map<K, Map<UK, UV>>
     */
    Map<K, Map<UK, UV>> getAll(Map<K, ? extends Collection<? extends UK>> keySet);

    /**
     * 向指定key中添加一个Map.Entry<userKey, userValue>
     *
     * @param key key
     * @param userKey userKey
     * @param userValue userValue
     */
    void add(K key, UK userKey, UV userValue);

    /**
     * 向指定key中添加一个Map<userKey, userValue>
     *
     * @param key key
     * @param value Map<userKey, userValue>
     */
    void addAll(K key, Map<? extends UK, ? extends UV> value);

    /**
     * 向多个指定key中添加多个Map<userKey, userValue>
     *
     * @param kMaps kMaps
     */
    void addAll(Map<? extends K, ? extends Map<? extends UK, ? extends UV>> kMaps);

    /**
     * 移除指定key下指定的Map.Entry<userKey, userValue>
     *
     * @param key key
     * @param userKey userKey
     */
    void remove(K key, UK userKey);

    /**
     * 移除指定key下指定的Map<userKey, userValue>
     *
     * @param key key
     * @param userKeys userKeys
     */
    void removeAll(K key, Collection<? extends UK> userKeys);

    /**
     * 移除多个指定key下多个指定的Map<userKey, userValue>
     *
     * @param kMaps kMaps
     */
    void removeAll(Map<? extends K, ? extends Collection<? extends UK>> kMaps);

    /**
     * 获取指定key下Map<userKey, userValue>的迭代器
     *
     * @param key key
     * @return Map<userKey, userValue>
     */
    Iterator<Map.Entry<UK, UV>> iterator(K key);

    /**
     * 获取指定key下Map<userKey, userValue>的可迭代对象
     *
     * @param key key
     * @return Map<userKey, userValue>的可迭代对象
     */
    Iterable<Map.Entry<UK, UV>> entries(K key);

    /**
     * 获取指定key下userKey的迭代器
     *
     * @param key key
     * @return userKey的迭代器
     */
    Iterable<UK> mapKeys(K key);

    /**
     * 获取指定key下userValue的迭代器
     *
     * @param key key
     * @return userValue的迭代器
     */
    Iterable<UV> mapValues(K key);
}
