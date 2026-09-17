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

import com.huawei.ock.bss.state.internal.descriptor.keyed.KeyedStateDescriptor;

import org.apache.flink.api.common.typeutils.TypeSerializer;

import java.util.Collection;
import java.util.Map;

/**
 * KeyedState接口
 *
 * @param <K> key
 * @param <V> value
 * @since 2025年1月14日15:07:52
 */
public interface KeyedState<K, V> {
    /**
     * 获取state的描述符
     *
     * @return state的描述符
     */
    KeyedStateDescriptor getDescriptor();

    /**
     * 判断key在table中是否存在
     *
     * @param key key
     * @return “存在”返回true
     */
    boolean contains(K key);

    /**
     * 根据key获取value
     *
     * @param key key
     * @return value
     */
    V get(K key);

    /**
     * 根据key的集合获取KV对
     *
     * @param keySet key的集合
     * @return KV对
     */
    Map<K, V> getAll(Collection<? extends K> keySet);

    /**
     * 根据key移除数据
     *
     * @param key key
     */
    void remove(K key);

    /**
     * 根据key的集合移除数据
     *
     * @param keySet key的集合
     */
    void removeAll(Collection<? extends K> keySet);

    /**
     * 获取序列化后的value
     *
     * @param serializedKey 序列化后的key
     * @param keySerializer key序列化器
     * @param valueSerializer value序列化器
     * @return 序列化后的value
     * @throws Exception exception
     */
    byte[] getSerializedValue(byte[] serializedKey, TypeSerializer<K> keySerializer, TypeSerializer<V> valueSerializer)
        throws Exception;
}
