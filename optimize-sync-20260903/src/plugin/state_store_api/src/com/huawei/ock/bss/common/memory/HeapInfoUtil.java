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

package com.huawei.ock.bss.common.memory;

import java.lang.management.ManagementFactory;
import java.lang.management.MemoryMXBean;
import java.lang.management.MemoryUsage;

/**
 * HeapInfoUtil
 *
 * @since BeiMing 25.0.T1
 */
public class HeapInfoUtil {
    private static final MemoryMXBean MEMORY_MX_BEAN = ManagementFactory.getMemoryMXBean();

    private static final double ONE_HUNDRED = 100.0D;

    private static final int ALIGN_STEP = 10;

    /**
     * getHeapUsage
     *
     * @return double
     */
    public static double getHeapUsage() {
        MemoryUsage heapMemoryUsage = MEMORY_MX_BEAN.getHeapMemoryUsage();
        long usedHeapMemory = heapMemoryUsage.getUsed();
        long maxHeapMemory = heapMemoryUsage.getMax();
        return ((double) usedHeapMemory / maxHeapMemory) * ONE_HUNDRED;
    }

    /**
     * getAlignHeapUsage
     *
     * @param realHeapUsage realHeapUsage
     * @return double
     */
    public static double getAlignHeapUsage(double realHeapUsage) {
        // 按10个点对齐的利用率，eg: 73->80
        return (int) (realHeapUsage / ALIGN_STEP) * ALIGN_STEP + ALIGN_STEP;
    }

    /**
     * getBorrowHeapSize
     *
     * @param heapBorrowRatio heapBorrowRatio
     * @return long
     */
    public static long getBorrowHeapSize(double heapBorrowRatio) {
        if (heapBorrowRatio <= 0) {
            return 0;
        }
        MemoryUsage heapMemoryUsage = MEMORY_MX_BEAN.getHeapMemoryUsage();
        long maxHeapMemory = heapMemoryUsage.getMax();
        return (long) ((double) maxHeapMemory * heapBorrowRatio);
    }
}