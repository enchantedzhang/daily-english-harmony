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

import org.apache.flink.configuration.ConfigOption;
import org.apache.flink.configuration.ConfigOptions;
import org.apache.flink.configuration.ReadableConfig;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import java.io.Serializable;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/**
 * BoostNativeMetricOptions
 *
 * @since BeiMing 25.2
 */
public class BoostNativeMetricOptions implements Serializable {
    /**
     * config to enable metric
     */
    public static final ConfigOption<Boolean> ENABLE_METRIC =
        ConfigOptions.key("state.backend.ockdb.metric.enable")
            .booleanType()
            .defaultValue(false)
            .withDescription("Whether enable metric, disable this will disable all native metrics.");

    /**
     * config to enable metric MemoryManager
     */
    public static final ConfigOption<Boolean> METRIC_MEMORY =
        ConfigOptions.key("state.backend.ockdb.metric.memory")
            .booleanType()
            .defaultValue(false)
            .withDescription("Whether enable metric of memory, enable this may slightly decrease IO performance.");

    /**
     * config to enable metric FreshTable
     */
    public static final ConfigOption<Boolean> METRIC_FRESH_TABLE =
        ConfigOptions.key("state.backend.ockdb.metric.fresh.table")
            .booleanType()
            .defaultValue(false)
            .withDescription("Whether enable metric of FreshTable, enable this may slightly decrease IO performance.");

    /**
     * config to enable metric SliceTable
     */
    public static final ConfigOption<Boolean> METRIC_SLICE_TABLE =
        ConfigOptions.key("state.backend.ockdb.metric.slice.table")
            .booleanType()
            .defaultValue(false)
            .withDescription("Whether enable metric of SliceTable, enable this may slightly decrease IO performance.");

    /**
     * config to enable metric LsmStore
     */
    public static final ConfigOption<Boolean> METRIC_LSM_STORE =
        ConfigOptions.key("state.backend.ockdb.metric.lsm.store")
            .booleanType()
            .defaultValue(false)
            .withDescription("Whether enable metric of LsmStore, enable this may slightly decrease IO performance.");

    /**
     * config to enable metric LsmCache
     */
    public static final ConfigOption<Boolean> METRIC_LSM_CACHE =
        ConfigOptions.key("state.backend.ockdb.metric.lsm.cache")
            .booleanType()
            .defaultValue(false)
            .withDescription("Whether enable metric of lsm cache, enable this may slightly decrease IO performance.");

    /**
     * config to enable metric Snapshot
     */
    public static final ConfigOption<Boolean> METRIC_SNAPSHOT =
        ConfigOptions.key("state.backend.ockdb.metric.snapshot")
            .booleanType()
            .defaultValue(false)
            .withDescription("Whether enable metric of snapshot, enable this may slightly decrease IO performance.");

    /**
     * config to enable metric Restore
     */
    public static final ConfigOption<Boolean> METRIC_RESTORE =
        ConfigOptions.key("state.backend.ockdb.metric.restore")
            .booleanType()
            .defaultValue(false)
            .withDescription("Whether enable metric of restore, enable this may slightly decrease IO performance.");

    private static final Logger LOG = LoggerFactory.getLogger(BoostNativeMetricOptions.class);

    private static final long serialVersionUID = 1L;

    private final Map<LayerType, Boolean> statistics;

    private final Map<LayerType, List<Statistics>> layersStatistics;

    private boolean enableMetricSnapshot;

    private boolean enableMetricRestore;

    public BoostNativeMetricOptions() {
        this.statistics = new HashMap<>();
        this.layersStatistics = new HashMap<>();
        for (LayerType layerType : LayerType.values()) {
            statistics.put(layerType, false);
            layersStatistics.put(layerType, new ArrayList<>());
        }
        this.enableMetricSnapshot = false;
        this.enableMetricRestore = false;
    }

    /**
     * create BoostNativeMetricOptions from config
     *
     * @param config ReadableConfig
     * @return BoostNativeMetricOptions
     */
    public static BoostNativeMetricOptions fromConfig(ReadableConfig config) {
        BoostNativeMetricOptions options = new BoostNativeMetricOptions();
        configureMetrics(options, config);
        return options;
    }

    /**
     * isSnapshotMetricEnabled
     *
     * @return boolean
     */
    public boolean isSnapshotMetricEnabled() {
        return this.enableMetricSnapshot;
    }

    /**
     * isRestoreMetricEnabled
     *
     * @return boolean
     */
    public boolean isRestoreMetricEnabled() {
        return this.enableMetricRestore;
    }

    /**
     * isStatisticsEnabled
     *
     * @return boolean
     */
    public boolean isStatisticsEnabled() {
        if (this.statistics == null) {
            return false;
        }
        for (LayerType layerType : this.statistics.keySet()) {
            if (this.statistics.get(layerType)) {
                return true;
            }
        }
        return false;
    }

    /**
     * isStatisticsTypeEnabled
     *
     * @param layerType LayerType
     * @return boolean
     */
    public boolean isStatisticsTypeEnabled(LayerType layerType) {
        return this.statistics.get(layerType);
    }

    /**
     * getLayersStatistics
     *
     * @param layerType LayerType
     * @return List<Statistics>
     */
    public List<Statistics> getLayersStatistics(LayerType layerType) {
        return this.layersStatistics.get(layerType);
    }

    /**
     * get layer-enableMetric bit map for all layers
     *
     * @return int
     */
    public int getSwitchMap() {
        int flags = 0;
        int pos = 0;
        for (LayerType layerType : LayerType.values()) {
            if (pos == Integer.SIZE - 1) {
                // 已经不适合使用32位int，应当使用更长的类型
                LOG.error("Failed to get bitmap for LayerType:{}, pos: {}", layerType, pos);
                break;
            }
            if (this.statistics.get(layerType)) {
                flags |= (1 << pos);
            } else {
                flags &= ~(1 << pos);
            }
            pos++;
        }
        return flags;
    }

    private static void configureMetrics(BoostNativeMetricOptions options, ReadableConfig config) {
        if (!config.get(ENABLE_METRIC)) {
            return;
        }
        if (config.get(METRIC_MEMORY)) {
            options.enableMemoryMetrics();
        }
        if (config.get(METRIC_FRESH_TABLE)) {
            options.enableFreshTableMetrics();
        }
        if (config.get(METRIC_SLICE_TABLE)) {
            options.enableSliceTableMetrics();
        }
        if (config.get(METRIC_LSM_STORE)) {
            options.enableLsmStoreMetrics();
        }
        if (config.get(METRIC_LSM_CACHE)) {
            options.enableLsmCacheMetrics();
        }
        if (config.get(METRIC_SNAPSHOT)) {
            options.enableSnapshotMetrics();
        }
        if (config.get(METRIC_RESTORE)) {
            options.enableRestoreMetrics();
        }
    }

    private void enableMemoryMetrics() {
        this.statistics.put(LayerType.MEMORY_MANAGER, true);
        List<Statistics> memoryStatistics = this.layersStatistics.get(LayerType.MEMORY_MANAGER);
        for (Statistics type : Statistics.values()) {
            if (type.getValue() >= 0 && type.getValue() < 100) {
                memoryStatistics.add(type);
            }
        }
    }

    private void enableFreshTableMetrics() {
        this.statistics.put(LayerType.FRESH_TABLE, true);
        List<Statistics> freshStatistics = this.layersStatistics.get(LayerType.FRESH_TABLE);
        freshStatistics.add(Statistics.FRESH_HIT_COUNT);
        freshStatistics.add(Statistics.FRESH_MISS_COUNT);
        freshStatistics.add(Statistics.FRESH_RECORD_COUNT);
        freshStatistics.add(Statistics.FRESH_FLUSHING_RECORD_COUNT);
        freshStatistics.add(Statistics.FRESH_FLUSHING_SEGMENT_COUNT);
        freshStatistics.add(Statistics.FRESH_FLUSHED_RECORD_COUNT);
        freshStatistics.add(Statistics.FRESH_FLUSHED_SEGMENT_COUNT);
        freshStatistics.add(Statistics.FRESH_SEGMENT_CREATE_FAIL_COUNT);
        freshStatistics.add(Statistics.FRESH_FLUSH_COUNT);
        freshStatistics.add(Statistics.FRESH_BINARY_KEY_SIZE);
        freshStatistics.add(Statistics.FRESH_BINARY_VALUE_SIZE);
        freshStatistics.add(Statistics.FRESH_BINARY_MAP_NODE_SIZE);
        freshStatistics.add(Statistics.FRESH_WASTED_SIZE);
    }

    private void enableSliceTableMetrics() {
        this.statistics.put(LayerType.SLICE_TABLE, true);
        List<Statistics> sliceStatistics = this.layersStatistics.get(LayerType.SLICE_TABLE);
        sliceStatistics.add(Statistics.SLICE_HIT_COUNT);
        sliceStatistics.add(Statistics.SLICE_MISS_COUNT);
        sliceStatistics.add(Statistics.SLICE_READ_COUNT);
        sliceStatistics.add(Statistics.SLICE_READ_AVG_SIZE);
        sliceStatistics.add(Statistics.SLICE_EVICT_SIZE);
        sliceStatistics.add(Statistics.SLICE_EVICT_WAITING_COUNT);
        sliceStatistics.add(Statistics.SLICE_COMPACTION_COUNT);
        sliceStatistics.add(Statistics.SLICE_COMPACTION_SLICE_COUNT);
        sliceStatistics.add(Statistics.SLICE_COMPACTION_AVG_SLICE_COUNT);
        sliceStatistics.add(Statistics.SLICE_CHAIN_AVG_SIZE);
        sliceStatistics.add(Statistics.SLICE_AVG_SIZE);
    }

    private void enableLsmStoreMetrics() {
        this.statistics.put(LayerType.LSM_STORE, true);
        int lsmLowRange = 400;
        int lsmHighRange = 600;
        List<Statistics> lsmStoreStatistics = this.layersStatistics.get(LayerType.LSM_STORE);
        for (Statistics value : Statistics.values()) {
            if (value.getValue() >= lsmLowRange && value.getValue() < lsmHighRange) {
                lsmStoreStatistics.add(value);
            }
        }
    }

    private void enableLsmCacheMetrics() {
        this.statistics.put(LayerType.LSM_CACHE, true);
        List<Statistics> lsmCacheStatistics = this.layersStatistics.get(LayerType.LSM_CACHE);
        for (Statistics type : Statistics.values()) {
            if (type.getValue() >= 300 && type.getValue() < 400) {
                lsmCacheStatistics.add(type);
            }
        }
    }

    private void enableSnapshotMetrics() {
        this.enableMetricSnapshot = true;
    }

    private void enableRestoreMetrics() {
        this.enableMetricRestore = true;
        this.statistics.put(LayerType.RESTORE, true);
        List<Statistics> restoreStatistics = this.layersStatistics.get(LayerType.RESTORE);
        restoreStatistics.add(Statistics.RESTORE_LAZY_DOWNLOAD_TIME);
    }
}