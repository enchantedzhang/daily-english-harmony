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

package com.huawei.ock.bss.metric;

import org.apache.flink.metrics.Gauge;
import org.apache.flink.metrics.MetricGroup;
import org.apache.flink.metrics.View;
import org.apache.flink.util.Preconditions;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import java.util.List;

/**
 * BoostNativeMetricImpl
 *
 * @since BeiMing 25.2
 */
public class BoostNativeMetricImpl implements BoostNativeMetric {
    private static final Logger LOG = LoggerFactory.getLogger(BoostNativeMetricImpl.class);

    private final boolean metricStatistics;

    private long nativeHandle;

    private final BoostNativeMetricOptions options;

    private MetricGroup metricGroup;

    private final Object lock;

    public BoostNativeMetricImpl(BoostNativeMetricOptions options, MetricGroup metricGroup) {
        this.nativeHandle = open(options.getSwitchMap());
        Preconditions.checkState(this.nativeHandle != 0, "Failed to create BoostNativeMetric.");
        this.metricStatistics = options.isStatisticsEnabled();
        this.options = options;
        this.metricGroup = metricGroup;
        this.lock = new Object();
        registerStatistics();
    }

    @Override
    public long getStatistics(Statistics statistics) {
        if (statistics == null) {
            LOG.error("Statistics is null.");
            return 0L;
        }
        if (!metricStatistics) {
            LOG.error("Statistics {} metric not enabled.", statistics.name());
            return 0L;
        }
        synchronized (lock) {
            if (this.nativeHandle != 0) {
                long res = getMetric(this.nativeHandle, statistics.getValue());
                if (res == Long.MAX_VALUE) {
                    LOG.error("Failed to get statistics {} metric, res value is invalid.", statistics.name());
                    return 0L;
                }
                return res;
            }
        }
        LOG.debug("BoostNativeMetricImpl already closed.");
        return 0L;
    }

    @Override
    public void close() {
        long handleToClose = 0L;
        synchronized (lock) {
            handleToClose = this.nativeHandle;
            this.nativeHandle = 0L;
        }
        close(handleToClose);
    }

    @Override
    public long getNativeHandle() {
        return this.nativeHandle;
    }

    private void registerStatistics() {
        if (!metricStatistics) {
            LOG.info("Statistics metric not enabled.");
            return;
        }
        for (LayerType layerType : LayerType.values()) {
            List<Statistics> layersStatistics = this.options.getLayersStatistics(layerType);
            if (layersStatistics.isEmpty()) {
                continue;
            }
            MetricGroup childGroup = metricGroup.addGroup(layerType.name().toLowerCase());
            for (Statistics statistics : layersStatistics) {
                childGroup.gauge(
                    String.format("ockdb.%s", statistics.name().toLowerCase()),
                    new BoostNativeStatisticsMetricView(statistics));
            }
        }
    }

    abstract static class BoostNativeView implements View {
        private boolean closed;

        BoostNativeView() {
            this.closed = false;
        }

        void close() {
            closed = true;
        }

        boolean isClosed() {
            return closed;
        }
    }

    class BoostNativeStatisticsMetricView extends BoostNativeView implements Gauge<Long> {
        private final Statistics statistics;

        private long value = 0L;

        private BoostNativeStatisticsMetricView(final Statistics statistics) {
            this.statistics = statistics;
        }

        @Override
        public Long getValue() {
            return value;
        }

        @Override
        public void update() {
            this.value = getStatistics(this.statistics);
        }
    }

    /**
     * cpp侧创建BoostNativeMetric实例
     *
     * @param switchMap bitmap
     * @return cpp侧handle
     */
    public static native long open(int switchMap);

    /**
     * 获取cpp监控项的值
     *
     * @param nativeHandle cpp侧BoostNativeMetric实例句柄
     * @param type Statistics枚举的value
     * @return 监控值
     */
    public static native long getMetric(long nativeHandle, int type);

    /**
     * 关闭cpp监控组件
     *
     * @param nativeHandle cpp侧handle
     */
    public static native void close(long nativeHandle);
}
