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

package com.huawei.ock.bss.common;

import com.huawei.ock.bss.common.conf.BoostConfig;
import com.huawei.ock.bss.common.exception.BSSRuntimeException;
import com.huawei.ock.bss.jni.AbstractNativeHandleReference;
import com.huawei.ock.bss.metric.BoostNativeMetric;
import com.huawei.ock.bss.metric.BoostNativeMetricImpl;
import com.huawei.ock.bss.metric.BoostNativeMetricOptions;
import com.huawei.ock.bss.table.AbstractTable;
import com.huawei.ock.bss.table.BoostPQTable;
import com.huawei.ock.bss.table.api.Table;
import com.huawei.ock.bss.table.api.TableDescription;
import com.huawei.ock.bss.table.description.PQTableDescription;

import org.apache.flink.metrics.MetricGroup;
import org.apache.flink.util.IOUtils;
import org.apache.flink.util.Preconditions;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import java.nio.file.Path;
import java.util.List;
import java.util.Map;
import java.util.concurrent.ConcurrentHashMap;
import java.util.stream.Collectors;

public class BoostStateDB extends AbstractNativeHandleReference {
    private static final Logger LOG = LoggerFactory.getLogger(BoostStateDB.class);

    private final BoostConfig config;

    @SuppressWarnings("rawtypes")
    private final Map<String, Table> tables;
    private Map<String, BoostPQTable> pqTables;

    private BoostNativeMetric boostNativeMetric;

    public BoostStateDB(BoostConfig config) {
        Preconditions.checkNotNull(config);
        this.nativeHandle = open(config);
        Preconditions.checkArgument(nativeHandle != 0);
        this.config = config;
        this.tables = new ConcurrentHashMap<>();
        this.pqTables = new ConcurrentHashMap<>();
    }

    public BoostConfig getConfig() {
        return config;
    }

    /**
     * getTableOrCreate
     *
     * @param tableDescription tableDescription
     * @return com.huawei.ock.bss.table.api.Table
     * @throws BSSRuntimeException BSSRuntimeException
     */
    @SuppressWarnings("rawtypes")
    public Table getTableOrCreate(TableDescription tableDescription) throws BSSRuntimeException {
        checkNativeHandleValid();
        Table gTable = this.tables.get(tableDescription.getTableName());
        if (gTable == null) {
            gTable = createTable(tableDescription);
        } else if (!tableDescription.getTableSerializer().equals(gTable.getTableDescription().getTableSerializer())) {
            gTable.getTableDescription().updateTableSerializer(tableDescription.getTableSerializer());
            gTable = createTable(gTable.getTableDescription());
        }
        return gTable;
    }

    /**
     * getTable
     *
     * @param stateName stateName
     * @return com.huawei.ock.bss.table.api.Table
     */
    @SuppressWarnings("rawtypes")
    public Table getTable(String stateName) {
        checkNativeHandleValid();
        return this.tables.get(stateName);
    }

    @SuppressWarnings("rawtypes")
    private Table createTable(TableDescription tableDescription) {
        Table gTable = tableDescription.createTable(this);
        this.tables.put(tableDescription.getTableName(), gTable);
        return gTable;
    }

    /**
     * 恢复文件
     *
     * @param tmpSstPath  恢复文件路径
     * @param remotePaths 远程文件列表
     * @param localPaths  本地文件列表
     * @param isNewjob  新任务标志
     */
    public void restore(List<Path> tmpSstPath, List<String> remotePaths, List<String> localPaths, boolean isNewjob) {
        checkNativeHandleValid();
        List<String> metaPaths = tmpSstPath.stream().map(p -> p.toAbsolutePath().toString())
            .collect(Collectors.toList());
        boolean isLazyDownload = getConfig().isEnableLazyDownload();
        boolean res = restore(this.nativeHandle, metaPaths, remotePaths, localPaths, isLazyDownload, isNewjob);
        if (!res) {
            throw new BSSRuntimeException("Failed to restore DB!");
        }
    }

    /**
     * 通知下层DB 当前checkpoint取消，释放资源
     *
     * @param checkpointId 当前要取消的checkpointId
     */
    public void notifyCheckpointAborted(long checkpointId) {
        checkNativeHandleValid();
        if (checkpointId < 0) {
            LOG.error("checkpointId is negative: {}", checkpointId);
            return;
        }
        notifyDBSnapshotAbort(this.nativeHandle, checkpointId);
    }

    /**
     * 通知下层DB 当前checkpoint已完成，可以清理backup目录的过期文件
     *
     * @param checkpointId 当前要完成的checkpointId
     */
    public void notifyDBSnapshotCompleted(long checkpointId) {
        checkNativeHandleValid();
        if (checkpointId < 0) {
            LOG.error("checkpointId is negative: {}", checkpointId);
            return;
        }
        notifyDBSnapshotComplete(this.nativeHandle, checkpointId);
    }

    /**
     * updateBorrowHeapSize
     *
     * @param borrowHeapSize borrowHeapSize
     * @return int
     */
    public static long updateBorrowHeapSize(long borrowHeapSize) {
        return changeHeapAvailableSize(borrowHeapSize);
    }

    /**
     * 获取下层KV数据的迭代器用于savepoint
     *
     * @return CloseableIterator<BinaryKeyValueItem>
     */
    public long getSavepointID() {
        checkNativeHandleValid();
        return newIterator(this.nativeHandle);
    }

    /**
     * putOrReplaceTable
     *
     * @param newTable newTable
     */
    @SuppressWarnings("rawtypes")
    public void putOrReplaceTable(Table newTable) {
        checkNativeHandleValid();
        if (newTable == null) {
            return;
        }
        String tableName = newTable.getTableDescription().getTableName();
        Table oldTable = tables.get(tableName);
        if (oldTable instanceof AbstractTable) {
            ((AbstractTable) oldTable).close();
        }
        tables.put(tableName, newTable);
    }

    /**
     * createBoostNativeMetric
     *
     * @param options BoostNativeMetricOptions
     * @param metricGroup MetricGroup
     */
    public void createBoostNativeMetric(BoostNativeMetricOptions options, MetricGroup metricGroup) {
        this.boostNativeMetric = new BoostNativeMetricImpl(options, metricGroup);
        registerBoostNativeMetric(this.nativeHandle, this.boostNativeMetric.getNativeHandle());
    }

    public BoostNativeMetric getBoostNativeMetric() {
        return this.boostNativeMetric;
    }

    /**
     * 创建一个C++层的实例，并返回实例地址
     *
     * @param config DB配置
     * @return long
     */
    public static native long open(BoostConfig config);

    private native void registerBoostNativeMetric(long nativeHandle, long metricNativeHandle);

    private native long newIterator(long nativeHandle);

    private native boolean restore(long nativeHandle, List<String> metaPaths, List<String> remotePaths,
        List<String> localPaths, boolean isLazyDownload, boolean isNewjob);

    private native void notifyDBSnapshotAbort(long nativeHandle, long checkpointId);

    private native void notifyDBSnapshotComplete(long nativeHandle, long checkpointId);

    private static native long changeHeapAvailableSize(long borrowHeapSize);

    /**
     * closeInternal 关闭实例，释放资源
     */
    protected void closeInternal() {
        super.closeInternal();
        IOUtils.closeQuietly(this.boostNativeMetric);
    }

    /**
     * create Pq table.
     *
     * @param kpqTableDescription kpqTableDescription
     * @return BoostPQTable
     */
    public <K> BoostPQTable createPQTable(PQTableDescription<K> kpqTableDescription) {
        BoostPQTable pqTable = pqTables.get(kpqTableDescription.getStateName());
        if (pqTable == null) {
            pqTable = new BoostPQTable(this, kpqTableDescription);
            pqTables.put(kpqTableDescription.getStateName(), pqTable);
        }
        return pqTable;
    }
}