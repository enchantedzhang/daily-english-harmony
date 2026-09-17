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

import org.apache.flink.shaded.guava31.com.google.common.primitives.UnsignedBytes;
import org.apache.flink.util.Preconditions;

import java.util.Iterator;
import java.util.TreeSet;

import javax.annotation.Nonnull;
import javax.annotation.Nullable;

/**
 * TreeSetCache
 *
 * @since BeiMing 25.3.0
 */
public class TreeSetCache {
    private final int capacity;

    @Nonnull
    private final TreeSet<byte[]> treeSet;

    TreeSetCache(int capacity) {
        Preconditions.checkArgument(capacity > 0);
        this.capacity = capacity;
        this.treeSet = new TreeSet<>(UnsignedBytes.lexicographicalComparator());
    }

    /**
     * getSize
     *
     * @return int
     */
    public int getSize() {
        return treeSet.size();
    }

    /**
     * isEmpty
     *
     * @return boolean
     */
    public boolean isEmpty() {
        return treeSet.isEmpty();
    }

    /**
     * isFull
     *
     * @return boolean
     */
    public boolean isFull() {
        return treeSet.size() >= capacity;
    }

    /**
     * peekFirst 查询开头元素（不移除）
     *
     * @return byte[]
     */
    @Nullable
    public byte[] peekFirst() {
        return treeSet.isEmpty() ? null : treeSet.first();
    }

    /**
     * peekLast 查询末尾元素（不移除）
     *
     * @return byte[]
     */
    @Nullable
    public byte[] peekLast() {
        return treeSet.isEmpty() ? null : treeSet.last();
    }

    /**
     * pollFirst 获取并移除开头元素
     *
     * @return byte[]
     */
    @Nullable
    public byte[] pollFirst() {
        return treeSet.isEmpty() ? null : treeSet.pollFirst();
    }

    /**
     * pollLast 获取并移除末尾元素
     *
     * @return byte[]
     */
    @Nullable
    public byte[] pollLast() {
        return treeSet.isEmpty() ? null : treeSet.pollLast();
    }

    /**
     * add
     *
     * @param toAdd 待添加元素
     * @return boolean
     */
    public boolean add(@Nonnull byte[] toAdd) {
        return treeSet.add(toAdd);
    }

    /**
     * remove
     *
     * @param toRemove 待删除元素
     * @return boolean
     */
    public boolean remove(@Nonnull byte[] toRemove) {
        return treeSet.remove(toRemove);
    }

    /**
     * loadFromIterator 将迭代器内元素添加到cache中直到达到容量上限
     *
     * @param iterator iterator
     */
    public void loadFromIterator(@Nonnull Iterator<byte[]> iterator) {
        treeSet.clear();
        while (iterator.hasNext() && treeSet.size() < capacity) {
            treeSet.add(iterator.next());
        }
    }
}