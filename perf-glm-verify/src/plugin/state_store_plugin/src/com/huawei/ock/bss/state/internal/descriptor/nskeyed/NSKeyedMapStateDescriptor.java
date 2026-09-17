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

package com.huawei.ock.bss.state.internal.descriptor.nskeyed;

import com.huawei.ock.bss.common.exception.BSSRuntimeException;
import com.huawei.ock.bss.state.internal.descriptor.InternalStateType;
import com.huawei.ock.bss.state.internal.namespace.NSKeyedMapState;

import org.apache.flink.api.common.typeutils.TypeSerializer;
import org.apache.flink.api.common.typeutils.base.MapSerializer;

import java.util.Map;

/**
 * 有namespace mapState描述符
 *
 * @param <K>  key
 * @param <N>  namespace
 * @param <UK> userKey
 * @param <UV> userValue
 * @since BeiMing 25.0.T1
 */
public class NSKeyedMapStateDescriptor<K, N, UK, UV>
    extends NSKeyedStateDescriptor<K, N, Map<UK, UV>, NSKeyedMapState<K, N, UK, UV>> {
    public NSKeyedMapStateDescriptor(String stateName, TypeSerializer<K> keySerializer,
        TypeSerializer<N> namespaceSerializer, TypeSerializer<Map<UK, UV>> valueSerializer) {
        super(stateName, InternalStateType.NSKEYED_MAP, keySerializer, namespaceSerializer, valueSerializer);
    }

    /**
     * 获取当前mapState的userKey序列化器
     *
     * @return 当前mapState的userKey序列化器
     */
    public TypeSerializer<UK> getUserKeySerializer() {
        TypeSerializer<Map<UK, UV>> mapSerializer = getValueSerializer();
        if (!(mapSerializer instanceof MapSerializer)) {
            throw new BSSRuntimeException("Failed to get userKeySerializer.");
        }
        return ((MapSerializer<UK, UV>) mapSerializer).getKeySerializer();
    }

    /**
     * 获取当前mapState的userValue序列化器
     *
     * @return 当前mapState的userValue序列化器
     */
    public TypeSerializer<UV> getUserValueSerializer() {
        TypeSerializer<Map<UK, UV>> mapSerializer = getValueSerializer();
        if (!(mapSerializer instanceof MapSerializer)) {
            throw new BSSRuntimeException("Failed to get userValueSerializer.");
        }
        return ((MapSerializer<UK, UV>) mapSerializer).getValueSerializer();
    }

    /**
     * 复制一个当前对象的副本
     *
     * @return 当前对象的副本
     */
    @Override
    public NSKeyedStateDescriptor<K, N, Map<UK, UV>, NSKeyedMapState<K, N, UK, UV>> duplicate() {
        return new NSKeyedMapStateDescriptor<>(getName(), getKeySerializer().duplicate(),
            getNamespaceSerializer().duplicate(),
            new MapSerializer<>(getUserKeySerializer().duplicate(), getUserValueSerializer().duplicate()));
    }
}
