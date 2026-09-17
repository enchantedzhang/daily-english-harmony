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

package com.huawei.ock.bss.common.serialize;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import java.util.ArrayList;

import com.huawei.ock.bss.common.memory.DirectDataOutputSerializer;
/**
 * ThreadLocalOutputViewPool
 *
 * @since BeiMing 25.0.T1
 */
public class ThreadLocalOutputViewPool {
    private static final Logger LOG = LoggerFactory.getLogger(ThreadLocalOutputViewPool.class);

    private static final int START_SIZE = 1024;

    private static final ThreadLocal<ThreadLocalOutputViewPool> THREAD_LOCAL_MAP = ThreadLocal.withInitial(
        ThreadLocalOutputViewPool::new);

    private final ArrayList<DirectDataOutputSerializer> reuseOutputViews = new ArrayList<>(10);

    private int reusePos = 0;

    /**
     * 获取当前线程的复用队列
     *
     * @param reuseCycleReset reuseCycleReset
     * @return ThreadLocalOutputViewPool
     */
    public static ThreadLocalOutputViewPool current(boolean reuseCycleReset) {
        ThreadLocalOutputViewPool builder = THREAD_LOCAL_MAP.get();

        if (reuseCycleReset) {
            builder.resetStart();
        }

        return builder;
    }

    /**
     * 获取可用的OutputView
     *
     * @return DirectDataOutputSerializer
     */
    public DirectDataOutputSerializer getOutputView() {
        while (this.reuseOutputViews.size() <= this.reusePos) {
            this.reuseOutputViews.add(new DirectDataOutputSerializer(START_SIZE));
        }

        DirectDataOutputSerializer view = this.reuseOutputViews.get(this.reusePos++);
        view.setPosition(0);
        return view;
    }

    /**
     * 重置复用队列循环
     */
    public void resetStart() {
        this.reusePos = 0;
    }
}