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

import static org.mockito.ArgumentMatchers.any;

import org.apache.flink.configuration.ReadableConfig;
import org.junit.Assert;
import org.junit.Test;
import org.powermock.api.mockito.PowerMockito;

import java.util.List;

/**
 * BoostNativeMetricOptionsTest
 *
 * @since BeiMing 25.2
 */
public class BoostNativeMetricOptionsTest {
    private ReadableConfig config;

    @Test
    public void test_metric_all() throws Exception {
        this.config = PowerMockito.mock(ReadableConfig.class);
        PowerMockito.when(config, "get", any()).thenReturn(true);
        BoostNativeMetricOptions options = BoostNativeMetricOptions.fromConfig(this.config);
    }

    @Test
    public void test_is_statistics_enabled_normal() throws Exception {
        this.config = PowerMockito.mock(ReadableConfig.class);
        PowerMockito.when(config, "get", any()).thenReturn(true);
        BoostNativeMetricOptions options = BoostNativeMetricOptions.fromConfig(this.config);
        Assert.assertTrue(options.isStatisticsEnabled());
    }

    @Test
    public void test_is_statistics_enabled_false() throws Exception {
        this.config = PowerMockito.mock(ReadableConfig.class);
        PowerMockito.when(config, "get", any()).thenReturn(false);
        BoostNativeMetricOptions options = BoostNativeMetricOptions.fromConfig(this.config);
        Assert.assertFalse(options.isStatisticsEnabled());
    }

    @Test
    public void test_is_statistics_enabled_false1() throws Exception {
        this.config = PowerMockito.mock(ReadableConfig.class);
        PowerMockito.when(config, "get", BoostNativeMetricOptions.ENABLE_METRIC).thenReturn(true);
        PowerMockito.when(config, "get", BoostNativeMetricOptions.METRIC_FRESH_TABLE).thenReturn(false);
        PowerMockito.when(config, "get", BoostNativeMetricOptions.METRIC_MEMORY).thenReturn(false);
        PowerMockito.when(config, "get", BoostNativeMetricOptions.METRIC_SLICE_TABLE).thenReturn(false);
        PowerMockito.when(config, "get", BoostNativeMetricOptions.METRIC_LSM_CACHE).thenReturn(false);
        PowerMockito.when(config, "get", BoostNativeMetricOptions.METRIC_LSM_STORE).thenReturn(false);
        PowerMockito.when(config, "get", BoostNativeMetricOptions.METRIC_SNAPSHOT).thenReturn(false);
        PowerMockito.when(config, "get", BoostNativeMetricOptions.METRIC_RESTORE).thenReturn(false);
        BoostNativeMetricOptions options = BoostNativeMetricOptions.fromConfig(this.config);
        Assert.assertFalse(options.isStatisticsEnabled());
    }

    @Test
    public void test_get_layers_statistics_normal() throws Exception {
        this.config = PowerMockito.mock(ReadableConfig.class);
        PowerMockito.when(config, "get", any()).thenReturn(true);
        BoostNativeMetricOptions options = BoostNativeMetricOptions.fromConfig(this.config);
        List<Statistics> statistics = options.getLayersStatistics(LayerType.FRESH_TABLE);
        Assert.assertNotNull(statistics);
        Assert.assertFalse(statistics.isEmpty());
    }

    @Test
    public void test_is_statistics_type_enabled_normal() throws Exception {
        this.config = PowerMockito.mock(ReadableConfig.class);
        PowerMockito.when(config, "get", any()).thenReturn(true);
        BoostNativeMetricOptions options = BoostNativeMetricOptions.fromConfig(this.config);
        Assert.assertTrue(options.isStatisticsTypeEnabled(LayerType.FRESH_TABLE));
    }

    @Test
    public void test_is_statistics_type_enabled_false() throws Exception {
        this.config = PowerMockito.mock(ReadableConfig.class);
        PowerMockito.when(config, "get", BoostNativeMetricOptions.ENABLE_METRIC).thenReturn(true);
        PowerMockito.when(config, "get", BoostNativeMetricOptions.METRIC_FRESH_TABLE).thenReturn(false);
        PowerMockito.when(config, "get", BoostNativeMetricOptions.METRIC_MEMORY).thenReturn(true);
        PowerMockito.when(config, "get", BoostNativeMetricOptions.METRIC_SLICE_TABLE).thenReturn(true);
        PowerMockito.when(config, "get", BoostNativeMetricOptions.METRIC_LSM_CACHE).thenReturn(true);
        PowerMockito.when(config, "get", BoostNativeMetricOptions.METRIC_LSM_STORE).thenReturn(true);
        PowerMockito.when(config, "get", BoostNativeMetricOptions.METRIC_SNAPSHOT).thenReturn(true);
        PowerMockito.when(config, "get", BoostNativeMetricOptions.METRIC_RESTORE).thenReturn(true);
        BoostNativeMetricOptions options = BoostNativeMetricOptions.fromConfig(this.config);
        Assert.assertFalse(options.isStatisticsTypeEnabled(LayerType.FRESH_TABLE));
    }

    @Test
    public void test_is_snapshot_metric_enabled_normal() throws Exception {
        this.config = PowerMockito.mock(ReadableConfig.class);
        PowerMockito.when(config, "get", any()).thenReturn(true);
        BoostNativeMetricOptions options = BoostNativeMetricOptions.fromConfig(this.config);
        Assert.assertTrue(options.isSnapshotMetricEnabled());
    }

    @Test
    public void test_is_restore_metric_enabled_normal() throws Exception {
        this.config = PowerMockito.mock(ReadableConfig.class);
        PowerMockito.when(config, "get", any()).thenReturn(true);
        BoostNativeMetricOptions options = BoostNativeMetricOptions.fromConfig(this.config);
        Assert.assertTrue(options.isRestoreMetricEnabled());
    }
}
