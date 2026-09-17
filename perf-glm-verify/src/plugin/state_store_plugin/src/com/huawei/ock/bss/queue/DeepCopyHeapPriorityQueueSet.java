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

import javax.annotation.Nonnegative;
import javax.annotation.Nonnull;

/**
 * DeepCopyHeapPriorityQueueSet
 *
 * @param <E> element
 * @since BeiMing 25.3.0
 */
public class DeepCopyHeapPriorityQueueSet<E extends HeapPriorityQueueElement> extends HeapPriorityQueueSet<E> {
    private final TypeSerializer<E> serializer;

    public DeepCopyHeapPriorityQueueSet(@Nonnull PriorityComparator<E> elementPriorityComparator,
        @Nonnull TypeSerializer<E> serializer, @Nonnull KeyExtractorFunction<E> keyExtractor,
        @Nonnegative int minimumCapacity, @Nonnull KeyGroupRange keyGroupRange,
        @Nonnegative int totalNumberOfKeyGroups) {
        super(elementPriorityComparator, keyExtractor, minimumCapacity, keyGroupRange, totalNumberOfKeyGroups);
        this.serializer = serializer;
    }

    /**
     * 添加元素
     *
     * @param element 元素
     * @return 返回是否添加成功
     */
    public boolean add(@Nonnull E element) {
        return super.add(this.serializer.copy(element));
    }
}