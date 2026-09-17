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

import org.apache.flink.api.common.functions.AggregateFunction;
import org.apache.flink.api.common.functions.ReduceFunction;

import java.util.Map;

/**
 * 无namespace KeyedValueState接口
 *
 * @param <K> key
 * @param <V> value
 * @since BeiMing 25.0.T1
 */
public interface KeyedValueState<K, V> extends KeyedState<K, V> {
    /**
     * put kv
     *
     * @param key key
     * @param value value
     */
    void put(K key, V value);

    /**
     * put kv的集合
     *
     * @param kvMap kvMap
     */
    void putAll(Map<? extends K, ? extends V> kvMap);

    /**
     * UnifiedAggregatingState调用，add数据
     *
     * @param key key
     * @param value value
     * @param aggregateFunction 聚合方法
     * @param <IN> 输入类型
     * @param <OUT> 输出类型
     * @throws Exception exception
     */
    <IN, OUT> void aggregate(K key, IN value, AggregateFunction<IN, V, OUT> aggregateFunction) throws Exception;

    /**
     * UnifiedReducingState调用，add数据
     *
     * @param key key
     * @param value value
     * @param reduceFunction 聚合方法
     * @throws Exception exception
     */
    void reduce(K key, V value, ReduceFunction<V> reduceFunction) throws Exception;
}
