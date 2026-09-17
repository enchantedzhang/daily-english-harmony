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

package com.huawei.ock.bss.snapshot;

import com.huawei.ock.bss.common.BinaryKeyValueItem;
import com.huawei.ock.bss.common.exception.BSSRuntimeException;
import com.huawei.ock.bss.iterator.CloseableIterator;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

/**
 * 封装从db获取的KV迭代器资源
 *
 * @since BeiMing 25.0.T1
 */
public class SavepointDBResult {
    private static final Logger LOG = LoggerFactory.getLogger(SavepointDBResult.class);

    private final long snapshotId;

    private final long startTime;

    private BinaryKeyValueItem current;

    public SavepointDBResult(long snapshotId) {
        if (snapshotId == 0) {
            throw new BSSRuntimeException("Failed to trigger savepoint");
        }
        this.snapshotId = snapshotId;
        startTime = System.currentTimeMillis();
        current = new BinaryKeyValueItem();
    }

    /**
     * getSnapshotId
     *
     * @return long
     */
    public long getSnapshotId() {
        return this.snapshotId;
    }

    /**
     * iterator
     *
     * @return CloseableIterator<BinaryKeyValueItem>
     */
    public CloseableIterator<BinaryKeyValueItem> iterator() {
        return new CloseableIterator<BinaryKeyValueItem>() {
            private int closeNum = 0;

            private boolean closed = false;

            /**
             * close
             */
            @Override
            public synchronized void close() {
                if (closed) {
                    SavepointDBResult.LOG.error("Iterator already closed, closeNum: " + closeNum);
                    return;
                }
                closed = true;
                closeNum++;
                SavepointDBResult.LOG.info("snapshotId: {}, Finish SavepointDBResult iterator time cost: {}",
                    snapshotId, (System.currentTimeMillis() - startTime));
                SavepointDBResult.this.close(snapshotId);
            }

            /**
             * hasNext
             *
             * @return boolean
             */
            @Override
            public synchronized boolean hasNext() {
                if (closed) {
                    return false;
                }
                return SavepointDBResult.this.hasNext(snapshotId);
            }

            /**
             * next
             *
             * @return BinaryKeyValueItem
             */
            @Override
            public synchronized BinaryKeyValueItem next() {
                if (closed) {
                    throw new BSSRuntimeException("Iterator already closed.");
                }
                return SavepointDBResult.this.next(snapshotId);
            }
        };
    }

    /**
     * close
     */
    public void close() {
        LOG.info("Closing SavepointDBResult after {}", (System.currentTimeMillis() - startTime));
    }

    private native boolean hasNext(long snapshotId);

    private native BinaryKeyValueItem next(long snapshotId);

    private native void close(long snapshotId);
}
