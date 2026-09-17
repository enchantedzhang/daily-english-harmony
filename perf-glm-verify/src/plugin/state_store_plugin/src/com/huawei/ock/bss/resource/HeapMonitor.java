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

package com.huawei.ock.bss.resource;

import com.huawei.ock.bss.common.BoostStateDB;
import com.huawei.ock.bss.common.memory.HeapInfoUtil;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import java.util.concurrent.Executors;
import java.util.concurrent.ScheduledExecutorService;
import java.util.concurrent.TimeUnit;

/**
 * HeapMonitor
 *
 * @since BeiMing 25.0.T1
 */
public class HeapMonitor {
    public static final HeapMonitor INSTANCE = new HeapMonitor();

    private static final Logger LOG = LoggerFactory.getLogger(HeapMonitor.class);

    private static final double FULL_RATIO = 100.0D;

    // 预留20% heap用于突发流量
    private static final double BURST_HEAP_RATIO = 20.0D;

    private static final int MONITOR_PERIOD = 10;

    private boolean isStarted = false;

    // 给flink算子预留的heap占比
    private double retainHeapRatio;

    private long borrowHeapSize;

    private HeapMonitor() {
    }

    public long getBorrowHeapSize() {
        return borrowHeapSize;
    }

    /**
     * start
     *
     * @param borrowHeapRatio borrowHeapRatio
     */
    public void start(double borrowHeapRatio) {
        if (isStarted) {
            return;
        }

        synchronized (this) {
            if (!isStarted) {
                this.retainHeapRatio = FULL_RATIO - Math.abs(borrowHeapRatio) * FULL_RATIO;
                borrowHeapSize = HeapInfoUtil.getBorrowHeapSize(borrowHeapRatio);
                ScheduledExecutorService scheduler = Executors.newScheduledThreadPool(1);
                scheduler.scheduleAtFixedRate(new HeapMonitorTask(), MONITOR_PERIOD, MONITOR_PERIOD,
                    TimeUnit.MILLISECONDS);
                isStarted = true;
            }
        }
    }

    private class HeapMonitorTask implements Runnable {
        @Override
        public void run() {
            try {
                double currentHeapUsage = HeapInfoUtil.getHeapUsage();
                double alignHeapUsage = HeapInfoUtil.getAlignHeapUsage(currentHeapUsage);
                if (alignHeapUsage >= retainHeapRatio) {
                    LOG.info("currentHpUsage:{}, alignHpUsage:{}, retainHpRatio:{}", currentHeapUsage, alignHeapUsage,
                        retainHeapRatio);

                    // heap利用率增加后，需要更新预留heap占比，同时缩减允许借用的heap占比
                    retainHeapRatio = retainHeapRatio + BURST_HEAP_RATIO;
                    double borrowHeapRatio = (FULL_RATIO - retainHeapRatio) / FULL_RATIO;
                    if (borrowHeapRatio <= 0) {
                        LOG.info("update borrowHpSize to zero");
                        BoostStateDB.updateBorrowHeapSize(0);
                        borrowHeapSize = 0;
                        return;
                    }

                    long newBorrowHeapSize = HeapInfoUtil.getBorrowHeapSize(borrowHeapRatio);
                    // 允许借用heap大小只能逐步缩小，因为JVM Heap扩展后不再归还操作系统
                    if (newBorrowHeapSize < borrowHeapSize) {
                        LOG.info("update borrowHpRatio:{}, oriBHPSize:{}, newBHPSize:{}", borrowHeapRatio,
                            borrowHeapSize, newBorrowHeapSize);
                        borrowHeapSize = BoostStateDB.updateBorrowHeapSize(newBorrowHeapSize);
                    }
                }
            } catch (Exception e) {
                // 捕获全量异常，避免异步线程退出
                LOG.warn("Unexpected exception:{}", e.getMessage(), e);
            }
        }
    }
}