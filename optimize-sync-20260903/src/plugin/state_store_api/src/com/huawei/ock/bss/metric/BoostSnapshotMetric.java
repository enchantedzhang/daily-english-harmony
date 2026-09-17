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

import org.apache.flink.metrics.MetricGroup;

/**
 * 收集snapshot监控信息
 *
 * @since BeiMing 25.2
 */
public class BoostSnapshotMetric implements AutoCloseable {
    private boolean enabled;

    private long snapshotId;

    private long totalTime = 0L;

    private long uploadTime = 0L;

    private long snapshotFileCount = 0L;

    private long snapshotIncrementalSize = 0L;

    private long snapshotFileSize = 0L;

    private long snapshotSliceFileCount = 0L;

    private long snapshotSliceIncrementalSize = 0L;

    private long snapshotSliceFileSize = 0L;

    private long snapshotSstFileCount = 0L;

    private long snapshotSstIncrementalSize = 0L;

    private long snapshotSstFileSize = 0L;

    public BoostSnapshotMetric(boolean enabled) {
        this.enabled = enabled;
    }

    public void setSnapshotId(long snapshotId) {
        this.snapshotId = snapshotId;
    }

    public void setTotalTime(long totalTime) {
        this.totalTime = totalTime;
    }

    public void setUploadTime(long uploadTime) {
        this.uploadTime = uploadTime;
    }

    public void setSnapshotFileCount(long snapshotFileCount) {
        this.snapshotFileCount = snapshotFileCount;
    }

    public void setSnapshotIncrementalSize(long snapshotIncrementalSize) {
        this.snapshotIncrementalSize = snapshotIncrementalSize;
    }

    public void setSnapshotFileSize(long snapshotFileSize) {
        this.snapshotFileSize = snapshotFileSize;
    }

    public void setSnapshotSliceFileCount(long snapshotSliceFileCount) {
        this.snapshotSliceFileCount = snapshotSliceFileCount;
    }

    public void setSnapshotSliceIncrementalSize(long snapshotSliceIncrementalSize) {
        this.snapshotSliceIncrementalSize = snapshotSliceIncrementalSize;
    }

    public void setSnapshotSliceFileSize(long snapshotSliceFileSize) {
        this.snapshotSliceFileSize = snapshotSliceFileSize;
    }

    public void setSnapshotSstFileCount(long snapshotSstFileCount) {
        this.snapshotSstFileCount = snapshotSstFileCount;
    }

    public void setSnapshotSstIncrementalSize(long snapshotSstIncrementalSize) {
        this.snapshotSstIncrementalSize = snapshotSstIncrementalSize;
    }

    public void setSnapshotSstFileSize(long snapshotSstFileSize) {
        this.snapshotSstFileSize = snapshotSstFileSize;
    }

    public void addSnapshotIncrementalSize(long size) {
        this.snapshotIncrementalSize += size;
    }

    public void addSnapshotFileSize(long size) {
        this.snapshotFileSize += size;
    }

    public void addSnapshotSliceFileCount() {
        this.snapshotSliceFileCount++;
    }

    public void addSnapshotSliceIncrementalSize(long size) {
        this.snapshotSliceIncrementalSize += size;
    }

    public void addSnapshotSliceFileSize(long size) {
        this.snapshotSliceFileSize += size;
    }

    public void addSnapshotSstFileCount() {
        this.snapshotSstFileCount++;
    }

    public void addSnapshotSstIncrementalSize(long size) {
        this.snapshotSstIncrementalSize += size;
    }

    public void addSnapshotSstFileSize(long size) {
        this.snapshotSstFileSize += size;
    }

    public long getTotalTime() {
        return totalTime / 1000;
    }

    public long getUploadTime() {
        return uploadTime / 1000;
    }

    public long getSnapshotFileCount() {
        return snapshotFileCount;
    }

    public long getSnapshotIncrementalSize() {
        return snapshotIncrementalSize;
    }

    public long getSnapshotFileSize() {
        return snapshotFileSize;
    }

    public long getSnapshotSliceFileCount() {
        return snapshotSliceFileCount;
    }

    public long getSnapshotSliceIncrementalSize() {
        return snapshotSliceIncrementalSize;
    }

    public long getSnapshotSliceFileSize() {
        return snapshotSliceFileSize;
    }

    public long getSnapshotSstFileCount() {
        return snapshotSstFileCount;
    }

    public long getSnapshotSstFileSize() {
        return snapshotSstFileSize;
    }

    public long getSnapshotSstIncrementalSize() {
        return snapshotSstIncrementalSize;
    }

    /**
     * 为snapshot注册Metric
     *
     * @param metricGroup MetricGroup
     */
    public void registerMetric(MetricGroup metricGroup) {
        if (metricGroup == null) {
            return;
        }
        metricGroup.gauge("ockdb.snapshot_total_time", this::getTotalTime);
        metricGroup.gauge("ockdb.snapshot_upload_time", this::getUploadTime);
        metricGroup.gauge("ockdb.snapshot_file_count", this::getSnapshotFileCount);
        metricGroup.gauge("ockdb.snapshot_incremental_size", this::getSnapshotIncrementalSize);
        metricGroup.gauge("ockdb.snapshot_file_size", this::getSnapshotFileSize);
        metricGroup.gauge("ockdb.snapshot_slice_file_count", this::getSnapshotSliceFileCount);
        metricGroup.gauge("ockdb.snapshot_slice_incremental_file_size", this::getSnapshotSliceIncrementalSize);
        metricGroup.gauge("ockdb.snapshot_slice_file_size", this::getSnapshotSliceFileSize);
        metricGroup.gauge("ockdb.snapshot_sst_file_count", this::getSnapshotSstFileCount);
        metricGroup.gauge("ockdb.snapshot_sst_incremental_file_size", this::getSnapshotSstIncrementalSize);
        metricGroup.gauge("ockdb.snapshot_sst_file_size", this::getSnapshotSstFileSize);
    }

    /**
     * 清理上次snapshot的文件监控信息
     */
    public void clearPreviousMetric() {
        setSnapshotIncrementalSize(0L);
        setSnapshotFileSize(0L);
        setSnapshotSliceFileCount(0L);
        setSnapshotSliceIncrementalSize(0L);
        setSnapshotSliceFileSize(0L);
        setSnapshotSstFileCount(0L);
        setSnapshotSstIncrementalSize(0L);
        setSnapshotSstFileSize(0L);
    }

    @Override
    public void close() throws Exception {
    }
}
