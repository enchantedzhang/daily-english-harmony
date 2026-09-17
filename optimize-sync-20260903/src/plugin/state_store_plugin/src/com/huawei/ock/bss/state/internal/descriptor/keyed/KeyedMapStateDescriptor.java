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

package com.huawei.ock.bss.state.internal.descriptor.keyed;

import com.huawei.ock.bss.common.exception.BSSRuntimeException;
import com.huawei.ock.bss.state.internal.KeyedMapState;
import com.huawei.ock.bss.state.internal.descriptor.InternalStateType;

import org.apache.flink.api.common.typeutils.TypeSerializer;
import org.apache.flink.api.common.typeutils.base.MapSerializer;

import java.util.Map;

/**
 * 无namespace mapState描述符
 *
 * @param <K> key
 * @param <UK> userKey
 * @param <UV> userValue
 * @since BeiMing 25.0.T1
 */
public class KeyedMapStateDescriptor<K, UK, UV> extends KeyedStateDescriptor<K, Map<UK, UV>, KeyedMapState<K, UK, UV>> {
    public KeyedMapStateDescriptor(String name, TypeSerializer<K> keySerializer,
        TypeSerializer<Map<UK, UV>> valueSerializer) {
        super(name, InternalStateType.KEYED_MAP, keySerializer, valueSerializer);
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
    public KeyedStateDescriptor<K, Map<UK, UV>, KeyedMapState<K, UK, UV>> duplicate() {
        KeyedStateDescriptor<K, Map<UK, UV>, KeyedMapState<K, UK, UV>> descriptor =
            new KeyedMapStateDescriptor<>(getName(), getKeySerializer().duplicate(), getValueSerializer().duplicate());
        descriptor.setStateTtlConfig(this.ttlConfig);
        return descriptor;
    }
}
