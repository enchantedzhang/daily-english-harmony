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

import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.anyLong;

import org.apache.flink.metrics.MetricGroup;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.powermock.api.mockito.PowerMockito;
import org.powermock.core.classloader.annotations.PrepareForTest;
import org.powermock.modules.junit4.PowerMockRunner;

/**
 * BoostNativeMetricImplTest
 *
 * @since BeiMing 25.2
 */
@RunWith(PowerMockRunner.class)
public class BoostNativeMetricImplTest {
    private BoostNativeMetricImpl boostNativeMetric;

    private BoostNativeMetricOptions options;

    private MetricGroup metricGroup;

    @Before
    public void setUp() throws Exception {
        PowerMockito.mockStatic(BoostNativeMetricImpl.class);
        PowerMockito.when(BoostNativeMetricImpl.open(anyInt())).thenReturn(1L);
        PowerMockito.doNothing().when(BoostNativeMetricImpl.class, "close", anyLong());
        options = PowerMockito.mock(BoostNativeMetricOptions.class);
        PowerMockito.when(options.isStatisticsEnabled()).thenReturn(true);
        metricGroup = PowerMockito.mock(MetricGroup.class);
    }

    @Test
    @PrepareForTest(value = {BoostNativeMetricImpl.class})
    public void test_open() {
        boostNativeMetric = new BoostNativeMetricImpl(options, metricGroup);
    }

    @Test
    @PrepareForTest(value = {BoostNativeMetricImpl.class})
    public void test_getStatistics_failed1() {
        boostNativeMetric = new BoostNativeMetricImpl(options, metricGroup);
        boostNativeMetric.getStatistics(null);
    }

    @Test
    @PrepareForTest(value = {BoostNativeMetricImpl.class})
    public void test_getStatistics_failed2() {
        PowerMockito.when(options.isStatisticsEnabled()).thenReturn(false);
        boostNativeMetric = new BoostNativeMetricImpl(options, metricGroup);
        boostNativeMetric.getStatistics(Statistics.ABOVE_LEVEL2_HIT_COUNT);
    }

    @Test
    @PrepareForTest(value = {BoostNativeMetricImpl.class})
    public void test_getStatistics_failed3() {
        boostNativeMetric = new BoostNativeMetricImpl(options, metricGroup);
        boostNativeMetric.close();
        boostNativeMetric.getStatistics(Statistics.ABOVE_LEVEL2_HIT_COUNT);
    }
}
