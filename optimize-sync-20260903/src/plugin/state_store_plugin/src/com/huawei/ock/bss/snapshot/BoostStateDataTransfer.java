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

import org.apache.flink.util.concurrent.ExecutorThreadFactory;

import java.io.Closeable;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

/**
 * checkpoint时和jobmanager进行数据通信基类
 *
 * @since BeiMing 25.0.T1
 */
public class BoostStateDataTransfer implements Closeable {
    /**
     * executorService
     */
    protected final ExecutorService executorService;

    BoostStateDataTransfer(int threadNum) {
        // 应当给予线程数限制，具体数量待定
        if (threadNum > 1) {
            executorService = Executors.newFixedThreadPool(Math.min(threadNum, 4),
                new ExecutorThreadFactory("Flink-BoostStateStoreDataTransfer"));
        } else {
            executorService = org.apache.flink.util.concurrent.Executors.newDirectExecutorService();
        }
    }

    @Override
    public void close() {
        executorService.shutdown();
    }
}
