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

import com.huawei.ock.bss.state.internal.descriptor.nskeyed.NSKeyedStateDescriptor;

import org.apache.flink.api.common.typeutils.TypeSerializer;

import java.util.Iterator;
import java.util.Map;

/**
 * NSKeyedState
 *
 * @param <K> key
 * @param <N> namespace
 * @param <V> value
 * @since 2025年1月14日15:13:02
 */
public interface NSKeyedState<K, N, V> {
    /**
     * 获取state的描述符
     *
     * @return state的描述符
     */
    NSKeyedStateDescriptor getDescriptor();

    /**
     * 判断key在table中是否存在
     *
     * @param key key
     * @param namespace namespace
     * @return “是”返回true
     */
    boolean contains(K key, N namespace);

    /**
     * 根据key+namespace获取value
     *
     * @param key key
     * @param namespace namespace
     * @return value
     */
    V get(K key, N namespace);

    /**
     * 根据key获取KV对
     *
     * @param key key
     * @return NV对
     */
    Map<N, V> getAll(K key);

    /**
     * 根据key+namespace移除数据
     *
     * @param key key
     * @param namespace namespace
     */
    void remove(K key, N namespace);

    /**
     * 移除一个key下面所有namespace的数据
     *
     * @param key key的集合
     */
    void removeAll(K key);

    /**
     * 获取key下多个namespace的迭代器
     *
     * @param key key
     * @return namespace的迭代器
     */
    Iterator<N> iterator(K key);

    /**
     * 获取序列化后的value
     *
     * @param serializedKeyAndNamespace 序列化后的key+namespace
     * @param keySerializer key序列化器
     * @param namespaceSerializer namespace序列化器
     * @param valueSerializer value序列化器
     * @return 序列化后的value
     * @throws Exception exception
     */
    byte[] getSerializedValue(byte[] serializedKeyAndNamespace, TypeSerializer<K> keySerializer,
        TypeSerializer<N> namespaceSerializer, TypeSerializer<V> valueSerializer) throws Exception;
}
