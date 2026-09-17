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
import static org.mockito.ArgumentMatchers.anyString;

import org.apache.flink.metrics.MetricGroup;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.powermock.api.mockito.PowerMockito;

/**
 * BoostSnapshotMetricTest
 *
 * @since BeiMing 25.2
 */
public class BoostSnapshotMetricTest {
    private BoostSnapshotMetric snapshotMetric;

    @Before
    public void setUp() {
        snapshotMetric = new BoostSnapshotMetric(true);
    }

    @Test
    public void test_total_time() {
        snapshotMetric.setTotalTime(1001L);
        long res = snapshotMetric.getTotalTime();
        Assert.assertEquals(1L, res);
    }

    @Test
    public void test_upload_time() {
        snapshotMetric.setUploadTime(1001L);
        long res = snapshotMetric.getUploadTime();
        Assert.assertEquals(1L, res);
    }

    @Test
    public void test_snapshot_file_count() {
        snapshotMetric.setSnapshotFileCount(1L);
        long res = snapshotMetric.getSnapshotFileCount();
        Assert.assertEquals(1L, res);
    }

    @Test
    public void test_snapshot_incremental_size() {
        snapshotMetric.setSnapshotIncrementalSize(1L);
        long res = snapshotMetric.getSnapshotIncrementalSize();
        Assert.assertEquals(1L, res);
    }

    @Test
    public void test_snapshot_file_size() {
        snapshotMetric.setSnapshotFileSize(1L);
        long res = snapshotMetric.getSnapshotFileSize();
        Assert.assertEquals(1L, res);
    }

    @Test
    public void test_snapshot_slice_file_count() {
        snapshotMetric.setSnapshotSliceFileCount(1L);
        long res = snapshotMetric.getSnapshotSliceFileCount();
        Assert.assertEquals(1L, res);
    }

    @Test
    public void test_snapshot_slice_incremental_size() {
        snapshotMetric.setSnapshotSliceIncrementalSize(1L);
        long res = snapshotMetric.getSnapshotSliceIncrementalSize();
        Assert.assertEquals(1L, res);
    }

    @Test
    public void test_snapshot_slice_file_size() {
        snapshotMetric.setSnapshotSliceFileSize(1L);
        long res = snapshotMetric.getSnapshotSliceFileSize();
        Assert.assertEquals(1L, res);
    }

    @Test
    public void test_snapshot_sst_file_count() {
        snapshotMetric.setSnapshotSstFileCount(1L);
        long res = snapshotMetric.getSnapshotSstFileCount();
        Assert.assertEquals(1L, res);
    }

    @Test
    public void test_snapshot_sst_incremental_size() {
        snapshotMetric.setSnapshotSstIncrementalSize(1L);
        long res = snapshotMetric.getSnapshotSstIncrementalSize();
        Assert.assertEquals(1L, res);
    }

    @Test
    public void test_snapshot_sst_file_size() {
        snapshotMetric.setSnapshotSstFileSize(1L);
        long res = snapshotMetric.getSnapshotSstFileSize();
        Assert.assertEquals(1L, res);
    }

    @Test
    public void test_add_snapshot_incremental_size() {
        snapshotMetric.addSnapshotIncrementalSize(1L);
        snapshotMetric.addSnapshotIncrementalSize(1L);
        long res = snapshotMetric.getSnapshotIncrementalSize();
        Assert.assertEquals(2L, res);
    }

    @Test
    public void test_add_snapshot_file_size() {
        snapshotMetric.addSnapshotFileSize(1L);
        snapshotMetric.addSnapshotFileSize(1L);
        long res = snapshotMetric.getSnapshotFileSize();
        Assert.assertEquals(2L, res);
    }

    @Test
    public void test_add_snapshot_slice_file_count() {
        snapshotMetric.addSnapshotSliceFileCount();
        snapshotMetric.addSnapshotSliceFileCount();
        long res = snapshotMetric.getSnapshotSliceFileCount();
        Assert.assertEquals(2L, res);
    }

    @Test
    public void test_add_snapshot_slice_incremental_size() {
        snapshotMetric.addSnapshotSliceIncrementalSize(1L);
        snapshotMetric.addSnapshotSliceIncrementalSize(1L);
        long res = snapshotMetric.getSnapshotSliceIncrementalSize();
        Assert.assertEquals(2L, res);
    }

    @Test
    public void test_add_snapshot_slice_file_size() {
        snapshotMetric.addSnapshotSliceFileSize(1L);
        snapshotMetric.addSnapshotSliceFileSize(1L);
        long res = snapshotMetric.getSnapshotSliceFileSize();
        Assert.assertEquals(2L, res);
    }

    @Test
    public void test_add_snapshot_sst_file_count() {
        snapshotMetric.addSnapshotSstFileCount();
        snapshotMetric.addSnapshotSstFileCount();
        long res = snapshotMetric.getSnapshotSstFileCount();
        Assert.assertEquals(2L, res);
    }

    @Test
    public void test_add_snapshot_sst_incremental_size() {
        snapshotMetric.addSnapshotSstIncrementalSize(1L);
        snapshotMetric.addSnapshotSstIncrementalSize(1L);
        long res = snapshotMetric.getSnapshotSstIncrementalSize();
        Assert.assertEquals(2L, res);
    }

    @Test
    public void test_add_snapshot_sst_file_size() {
        snapshotMetric.addSnapshotSstFileSize(1L);
        snapshotMetric.addSnapshotSstFileSize(1L);
        long res = snapshotMetric.getSnapshotSstFileSize();
        Assert.assertEquals(2L, res);
    }

    @Test
    public void test_clear_previous_metric() {
        snapshotMetric.clearPreviousMetric();
    }

    @Test
    public void test_register_metric() throws Exception {
        MetricGroup metricGroup = PowerMockito.mock(MetricGroup.class);
        PowerMockito.when(metricGroup, "gauge", anyString(), any()).thenReturn(null);
        snapshotMetric.registerMetric(metricGroup);
    }

    @Test
    public void test_register_metric_null() throws Exception {
        MetricGroup metricGroup = PowerMockito.mock(MetricGroup.class);
        PowerMockito.when(metricGroup, "gauge", anyString(), any()).thenReturn(null);
        snapshotMetric.registerMetric(null);
    }
}
