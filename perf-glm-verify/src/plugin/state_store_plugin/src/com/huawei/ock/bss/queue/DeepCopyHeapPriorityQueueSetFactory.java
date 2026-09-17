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

package com.huawei.ock.bss.queue;

import org.apache.flink.api.common.typeutils.TypeSerializer;
import org.apache.flink.runtime.state.KeyExtractorFunction;
import org.apache.flink.runtime.state.KeyGroupRange;
import org.apache.flink.runtime.state.PriorityComparator;
import org.apache.flink.runtime.state.heap.HeapPriorityQueueElement;
import org.apache.flink.runtime.state.heap.HeapPriorityQueueSet;
import org.apache.flink.runtime.state.heap.HeapPriorityQueueSetFactory;

import javax.annotation.Nonnegative;
import javax.annotation.Nonnull;

/**
 * DeepCopyHeapPriorityQueueSetFactory
 *
 * @since BeiMing 25.3.0
 */
public class DeepCopyHeapPriorityQueueSetFactory extends HeapPriorityQueueSetFactory {
    @Nonnull
    private final KeyGroupRange keyGroupRange;

    @Nonnegative
    private final int totalKeyGroups;

    @Nonnegative
    private final int minimumCapacity;

    public DeepCopyHeapPriorityQueueSetFactory(@Nonnull KeyGroupRange keyGroupRange, @Nonnegative int totalKeyGroups,
        @Nonnegative int minimumCapacity) {
        super(keyGroupRange, totalKeyGroups, minimumCapacity);
        this.keyGroupRange = keyGroupRange;
        this.totalKeyGroups = totalKeyGroups;
        this.minimumCapacity = minimumCapacity;
    }

    /**
     * 创建pq
     *
     * @param stateName state的名称
     * @param byteOrderedElementSerializer 序列化器
     * @param <E> pq元素
     * @return 返回pq实例
     */
    @Nonnull
    @Override
    public <E extends HeapPriorityQueueElement & org.apache.flink.runtime.state.PriorityComparable<? super E>
        & org.apache.flink.runtime.state.Keyed<?>> HeapPriorityQueueSet<E> create(
        @Nonnull String stateName, @Nonnull TypeSerializer<E> byteOrderedElementSerializer) {
        return new DeepCopyHeapPriorityQueueSet<>(PriorityComparator.forPriorityComparableObjects(),
            byteOrderedElementSerializer, KeyExtractorFunction.forKeyedObjects(), this.minimumCapacity,
            this.keyGroupRange, this.totalKeyGroups);
    }
}