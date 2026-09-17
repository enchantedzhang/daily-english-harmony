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

package com.huawei.ock.bss.iterator;

import org.apache.flink.contrib.streaming.state.iterator.SingleStateIterator;
import org.apache.flink.runtime.state.KeyValueStateIterator;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import java.io.IOException;
import java.util.Comparator;
import java.util.List;
import java.util.PriorityQueue;

/**
 * 实现KeyValueStateIterator接口，由kvState迭代器和pqState迭代器共同组成，供flink侧调用
 *
 * @since BeiMing 25.0.T1
 */
public class BoostKeyValueStateIterator implements KeyValueStateIterator {
    private static final Logger LOG = LoggerFactory.getLogger(BoostKeyValueStateIterator.class);

    private Comparator<SingleStateIterator> keyGroupComparator;

    private final PriorityQueue<SingleStateIterator> iteratorHeap;

    private boolean newKeyGroup;

    private boolean newState;

    private SingleStateIterator currentIterator;

    private final int keyGroupPrefixBytes;

    public BoostKeyValueStateIterator(List<SingleStateIterator> keyGroupStateIterators, int keyGroupPrefixBytes) {
        this.keyGroupPrefixBytes = keyGroupPrefixBytes;
        this.keyGroupComparator = (o1, o2) -> compareKeyGroupsForByteArrays(o1.key(), o2.key(), keyGroupPrefixBytes);

        // 初始化iteratorHeap
        this.iteratorHeap = new PriorityQueue<>(keyGroupStateIterators.size(), keyGroupComparator);
        for (SingleStateIterator it : keyGroupStateIterators) {
            if (!it.isValid()) {
                it.close();
                continue;
            }
            iteratorHeap.offer(it);
        }

        // 根据iteratorHeap初始化currentIterator
        if (iteratorHeap.isEmpty()) {
            currentIterator = null;
        } else {
            currentIterator = iteratorHeap.remove();
        }

        LOG.info("Succeed to build BoostKeyValueStateIterator, iterator num:{}",
            currentIterator == null ? 0 : iteratorHeap.size() + 1);
    }

    @Override
    public void next() throws IOException {
        newKeyGroup = false;
        newState = false;
        int previousKeyGroup = computeKeyGroup(currentIterator.key());
        int previousStateId = currentIterator.getKvStateId();
        currentIterator.next();
        if (currentIterator.isValid()) {
            if (previousKeyGroup != computeKeyGroup(currentIterator.key())) {
                iteratorHeap.offer(currentIterator);
                currentIterator = iteratorHeap.remove();
                newState = previousStateId != currentIterator.getKvStateId();
                newKeyGroup = previousKeyGroup != computeKeyGroup(currentIterator.key());
            } else {
                newState = previousStateId != currentIterator.getKvStateId();
            }
        } else {
            currentIterator.close();
            if (iteratorHeap.isEmpty()) {
                currentIterator = null;
            } else {
                currentIterator = iteratorHeap.remove();
                newState = true;
                newKeyGroup = previousKeyGroup != computeKeyGroup(currentIterator.key());
            }
        }
    }

    @Override
    public int keyGroup() {
        return computeKeyGroup(currentIterator.key());
    }

    @Override
    public byte[] key() {
        return this.currentIterator.key();
    }

    @Override
    public byte[] value() {
        return this.currentIterator.value();
    }

    @Override
    public int kvStateId() {
        return this.currentIterator.getKvStateId();
    }

    @Override
    public boolean isNewKeyValueState() {
        return this.newState;
    }

    @Override
    public boolean isNewKeyGroup() {
        return this.newKeyGroup;
    }

    @Override
    public boolean isValid() {
        return this.currentIterator != null && this.currentIterator.isValid();
    }

    @Override
    public void close() {
        LOG.info("Closing savepoint iterator: BoostKeyValueStateIterator. Current iterator size: {}",
            this.iteratorHeap.size());
    }

    private static int compareKeyGroupsForByteArrays(byte[] a, byte[] b, int len) {
        for (int i = 0; i < len; ++i) {
            int diff = (a[i] & 0xFF) - (b[i] & 0xFF);
            if (diff != 0) {
                return diff;
            }
        }
        return 0;
    }

    private int computeKeyGroup(byte[] key) {
        int keyGroup = 0;
        for (int i = 0; i < keyGroupPrefixBytes; i++) {
            keyGroup <<= 8;
            keyGroup |= (key[i] & 0xFF);
        }
        return keyGroup;
    }
}
