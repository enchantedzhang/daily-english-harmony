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

import com.huawei.ock.bss.state.internal.KeyedState;
import com.huawei.ock.bss.state.internal.descriptor.InternalStateType;

import org.apache.flink.api.common.state.StateTtlConfig;
import org.apache.flink.api.common.typeutils.TypeSerializer;
import org.apache.flink.util.Preconditions;

import java.io.Serializable;
import java.util.Objects;

import javax.annotation.Nullable;

/**
 * 无namespace的描述符
 *
 * @param <K> key
 * @param <S> state
 * @param <V> value
 * @since 2025年1月14日14:20:42
 */
public abstract class KeyedStateDescriptor<K, V, S extends KeyedState<K, V>> implements Serializable {
    private static final long serialVersionUID = 1L;

    /**
     * 状态的ttl
     */
    @Nullable
    protected StateTtlConfig ttlConfig;

    private final String name;

    private final InternalStateType stateType;

    private final TypeSerializer<K> keySerializer;

    private final TypeSerializer<V> valueSerializer;

    KeyedStateDescriptor(String name, InternalStateType stateType, TypeSerializer<K> keySerializer,
        TypeSerializer<V> valueSerializer) {
        Preconditions.checkNotNull(name);
        Preconditions.checkNotNull(valueSerializer);
        Preconditions.checkNotNull(stateType);
        Preconditions.checkNotNull(keySerializer);

        this.name = name;
        this.valueSerializer = valueSerializer;
        this.stateType = stateType;
        this.keySerializer = keySerializer;
    }

    /**
     * 返回一个新的相同descriptor对象
     *
     * @return 复制对象
     */
    public abstract KeyedStateDescriptor<K, V, S> duplicate();

    public String getName() {
        return this.name;
    }

    public InternalStateType getStateType() {
        return this.stateType;
    }

    public TypeSerializer<K> getKeySerializer() {
        return this.keySerializer;
    }

    public TypeSerializer<V> getValueSerializer() {
        return this.valueSerializer;
    }

    public void setStateTtlConfig(StateTtlConfig ttlConfig) {
        this.ttlConfig = ttlConfig;
    }

    /**
     * 当前状态是否支持ttl
     *
     * @return “是”返回true
     */
    public boolean isEnableStateTTL() {
        return (this.ttlConfig != null) && this.ttlConfig.isEnabled();
    }

    /**
     * 获取状态的ttl
     *
     * @return 状态的ttl
     */
    public long getStateTTL() {
        if (this.ttlConfig == null) {
            return -1L;
        }
        return this.ttlConfig.getTtl().toMilliseconds();
    }

    /**
     * 重写hash方法
     *
     * @return hash值
     */
    public int hashCode() {
        int result = Objects.hashCode(this.name);
        result = 31 * result + Objects.hashCode(this.keySerializer);
        result = 31 * result + Objects.hashCode(this.valueSerializer);
        return result;
    }
}
