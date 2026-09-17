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

import org.apache.flink.api.common.functions.AggregateFunction;
import org.apache.flink.api.common.functions.ReduceFunction;

/**
 * NSKeyedState实现类
 *
 * @param <K> key
 * @param <N> namespace
 * @param <V> value
 * @since 2025年1月14日15:13:02
 */
public interface NSKeyedValueState<K, N, V> extends NSKeyedState<K, N, V> {
    /**
     * merge namespace时调用，获取并删除
     *
     * @param key key
     * @param namespace namespace
     * @return value
     */
    V getAndRemove(K key, N namespace);

    /**
     * put
     *
     * @param key key
     * @param namespace namespace
     * @param value value
     */
    void put(K key, N namespace, V value);

    /**
     * UnifiedNSReducingState调用，add数据
     *
     * @param key key
     * @param namespace namespace
     * @param value value
     * @param reduceFunction 聚合方法
     */
    void reduce(K key, N namespace, V value, ReduceFunction<V> reduceFunction);

    /**
     * UnifiedNSAggregatingState调用，add数据
     *
     * @param key key
     * @param namespace namespace
     * @param value value
     * @param aggregateFunction 聚合方法
     * @param <IN> 输入
     * @param <OUT> 结果
     */
    <IN, OUT> void aggregate(K key, N namespace, IN value, AggregateFunction<IN, V, OUT> aggregateFunction);
}
