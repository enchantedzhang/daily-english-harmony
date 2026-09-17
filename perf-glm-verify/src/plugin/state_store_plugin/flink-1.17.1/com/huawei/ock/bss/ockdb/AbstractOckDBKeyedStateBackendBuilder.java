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

package com.huawei.ock.bss.ockdb;

import com.huawei.ock.bss.EmbeddedOckStateBackend;
import com.huawei.ock.bss.OckDBKeyedStateBackend;
import com.huawei.ock.bss.OckDBOptions;
import com.huawei.ock.bss.common.BoostStateDB;
import com.huawei.ock.bss.common.conf.BoostConfig;
import com.huawei.ock.bss.common.exception.BSSRuntimeException;
import com.huawei.ock.bss.metric.BoostNativeMetricOptions;
import com.huawei.ock.bss.metric.BoostSnapshotMetric;
import com.huawei.ock.bss.queue.DeepCopyHeapPriorityQueueSetFactory;
import com.huawei.ock.bss.queue.OckDBPriorityQueueSetFactory;
import com.huawei.ock.bss.resource.HeapMonitor;
import com.huawei.ock.bss.resource.ResourceContainer;
import com.huawei.ock.bss.restore.BoostIncrementalRestoreOperation;
import com.huawei.ock.bss.restore.BoostNoneRestoreOperation;
import com.huawei.ock.bss.restore.BoostRestoreOperation;
import com.huawei.ock.bss.restore.BoostRestoreResult;
import com.huawei.ock.bss.restore.BoostSavepointRestoreOperation;
import com.huawei.ock.bss.snapshot.BoostSnapshotStrategyBase;
import com.huawei.ock.bss.state.internal.descriptor.keyed.KeyedStateDescriptor;
import com.huawei.ock.bss.state.internal.descriptor.nskeyed.NSKeyedStateDescriptor;
import com.huawei.ock.bss.table.api.Table;

import org.apache.flink.api.common.ExecutionConfig;
import org.apache.flink.api.common.typeutils.TypeSerializer;
import org.apache.flink.configuration.Configuration;
import org.apache.flink.configuration.IllegalConfigurationException;
import org.apache.flink.configuration.TaskManagerOptions;
import org.apache.flink.core.fs.CloseableRegistry;
import org.apache.flink.metrics.MetricGroup;
import org.apache.flink.runtime.query.TaskKvStateRegistry;
import org.apache.flink.runtime.state.AbstractKeyedStateBackendBuilder;
import org.apache.flink.runtime.state.BackendBuildingException;
import org.apache.flink.runtime.state.HeapPriorityQueuesManager;
import org.apache.flink.runtime.state.IncrementalKeyedStateHandle;
import org.apache.flink.runtime.state.KeyGroupRange;
import org.apache.flink.runtime.state.KeyedStateHandle;
import org.apache.flink.runtime.state.LocalRecoveryConfig;
import org.apache.flink.runtime.state.PriorityQueueSetFactory;
import org.apache.flink.runtime.state.RegisteredStateMetaInfoBase;
import org.apache.flink.runtime.state.SavepointKeyedStateHandle;
import org.apache.flink.runtime.state.StreamCompressionDecorator;
import org.apache.flink.runtime.state.heap.HeapPriorityQueueSetFactory;
import org.apache.flink.runtime.state.heap.HeapPriorityQueueSnapshotRestoreWrapper;
import org.apache.flink.runtime.state.heap.InternalKeyContext;
import org.apache.flink.runtime.state.heap.InternalKeyContextImpl;
import org.apache.flink.runtime.state.metrics.LatencyTrackingStateConfig;
import org.apache.flink.runtime.state.ttl.TtlTimeProvider;
import org.apache.flink.util.FileUtils;
import org.apache.flink.util.IOUtils;
import org.apache.flink.util.UserCodeClassLoader;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import java.io.File;
import java.io.IOException;
import java.nio.file.Files;
import java.nio.file.Paths;
import java.nio.file.attribute.FileAttribute;
import java.nio.file.attribute.PosixFilePermission;
import java.nio.file.attribute.PosixFilePermissions;
import java.util.Collection;
import java.util.Map;
import java.util.Set;
import java.util.UUID;

import javax.annotation.Nonnull;
import javax.annotation.Nullable;

/**
 * builder类，通过build方法生产OckDBKeyedStateBackend实例
 *
 * @param <K> key
 * @since 2025年1月12日16:15:42
 */
public abstract class AbstractOckDBKeyedStateBackendBuilder<K> extends AbstractKeyedStateBackendBuilder<K> {
    private static final String DB_INSTANCE_DIR_STRING = "db";

    private static final Logger LOG = LoggerFactory.getLogger(AbstractOckDBKeyedStateBackendBuilder.class);

    private static final int MIN_DB_FILTER_EXPECTED_KEY_COUNT = 1000000;

    private static final int MAX_DB_FILTER_EXPECTED_KEY_COUNT = 10000000;

    private static final float MIN_DB_FILE_MEMORY_FRACTION = 0.1F;

    private static final float MAX_DB_FILE_MEMORY_FRACTION = 0.5F;

    private static final float MIN_JNI_SLICE_WATER_MASK_RATIO = 0.0F;

    private static final float MAX_JNI_SLICE_WATER_MASK_RATIO = 1.0F;

    private int taskSlotFlag;

    private long taskSlotMemoryLimit;

    private double slotManagedMemoryFraction;

    private final EmbeddedOckStateBackend.PriorityQueueStateType priorityQueueStateType;

    protected final boolean priorityQueueIsAsyncSnapshot;

    protected final String fileCompatibleIdentifier;

    protected final File instanceBasePath;

    protected final File instanceOckDBPath;

    protected final Configuration config;

    protected final LocalRecoveryConfig localRecoveryConfig;

    protected final MetricGroup metricGroup;

    protected final InternalKeyContext<K> keyContext;

    protected final ResourceContainer resourceContainer;

    protected int numberOfTransferringThreads;

    protected boolean enableIncrementalCheckpointing;

    protected String jobID;

    /**
     * BSS native metric options
     */
    protected BoostNativeMetricOptions boostNativeMetricOptions;

    /**
     * BSS snapshot metric
     */
    @Nullable
    protected BoostSnapshotMetric boostSnapshotMetric;

    public AbstractOckDBKeyedStateBackendBuilder(int numberOfKeyGroups, KeyGroupRange keyGroupRange,
        UserCodeClassLoader userCodeClassLoader, File instanceBasePath, LocalRecoveryConfig localRecoveryConfig,
        TaskKvStateRegistry kvStateRegistry, String fileCompatibleIdentifier, ExecutionConfig executionConfig,
        TypeSerializer<K> keySerializer, TtlTimeProvider ttlTimeProvider,
        LatencyTrackingStateConfig latencyTrackingStateConfig, CloseableRegistry cancelStreamRegistry,
        StreamCompressionDecorator keyGroupCompressionDecorator, @Nonnull Collection<KeyedStateHandle> stateHandles,
        boolean priorityQueueAsyncSnapshot, ResourceContainer resourceContainer,
        EmbeddedOckStateBackend.PriorityQueueStateType priorityQueueStateType, Configuration config) {
        super(kvStateRegistry, keySerializer, userCodeClassLoader.asClassLoader(), numberOfKeyGroups, keyGroupRange,
            executionConfig, ttlTimeProvider, latencyTrackingStateConfig, stateHandles, keyGroupCompressionDecorator,
            cancelStreamRegistry);
        this.instanceBasePath = instanceBasePath;
        this.instanceOckDBPath = new File(instanceBasePath, DB_INSTANCE_DIR_STRING);
        this.localRecoveryConfig = localRecoveryConfig;
        this.fileCompatibleIdentifier = fileCompatibleIdentifier;
        this.metricGroup = latencyTrackingStateConfig.getMetricGroup();
        this.keyContext = new InternalKeyContextImpl<>(keyGroupRange, numberOfKeyGroups);
        this.priorityQueueIsAsyncSnapshot = priorityQueueAsyncSnapshot;
        this.resourceContainer = resourceContainer;
        this.priorityQueueStateType = priorityQueueStateType;
        this.config = config;
        this.enableIncrementalCheckpointing = false;
    }

    /**
     * 获取OckDBKeyedStateBackend实例
     *
     * @return 返回OckDBKeyedStateBackend实例
     * @throws BackendBuildingException 异常
     */
    public abstract OckDBKeyedStateBackend<K> build() throws BackendBuildingException;

    private HeapPriorityQueueSetFactory createHeapQueueFactory() {
        return new DeepCopyHeapPriorityQueueSetFactory(keyGroupRange, numberOfKeyGroups, 128);
    }

    /**
     * initPriorityQueueFactory
     *
     * @param db db
     * @param map map
     * @return PriorityQueueSetFactory
     */
    protected PriorityQueueSetFactory initPriorityQueueFactory(BoostStateDB db,
        Map<String, RegisteredStateMetaInfoBase> map) {
        PriorityQueueSetFactory priorityQueueFactory;
        switch (priorityQueueStateType) {
            case HEAP:
                priorityQueueFactory = createHeapQueueFactory();
                break;
            case OCKDB:
                LOG.info("init native PriorityQueue Factory.");
                priorityQueueFactory = new OckDBPriorityQueueSetFactory(keyGroupRange, db, numberOfKeyGroups, map);
                break;
            default:
                throw new IllegalArgumentException("Unknown priority queue state type: " + priorityQueueStateType);
        }
        return priorityQueueFactory;
    }

    protected BoostRestoreOperation<K> initRestoreOperation(String jobID, BoostStateDB db, int keyGroupPrefixBytes,
        Map<String, RegisteredStateMetaInfoBase> registeredKvStateMetaInfos,
        Map<String, HeapPriorityQueueSnapshotRestoreWrapper<?>> registeredPQStates,
        PriorityQueueSetFactory priorityQueueSetFactory, Map<String, Table> tables,
        Map<String, KeyedStateDescriptor> keyedStateDescriptorMap,
        Map<String, NSKeyedStateDescriptor> nsKeyedStateDescriptorMap) throws BackendBuildingException {
        if (this.restoreStateHandles == null || this.restoreStateHandles.isEmpty()) {
            return new BoostNoneRestoreOperation<>();
        }
        if (this.restoreStateHandles.iterator().next() instanceof IncrementalKeyedStateHandle) {
            LOG.info("restore from BoostIncrementalRestoreOperation");
            return new BoostIncrementalRestoreOperation<>(
                jobID,
                this.fileCompatibleIdentifier,
                this.keyGroupRange,
                db,
                keyGroupPrefixBytes,
                this.numberOfTransferringThreads,
                this.cancelStreamRegistry,
                this.userCodeClassLoader,
                priorityQueueSetFactory,
                registeredKvStateMetaInfos,
                keyedStateDescriptorMap,
                nsKeyedStateDescriptorMap,
                registeredPQStates,
                tables,
                this.keySerializerProvider,
                this.instanceBasePath,
                this.restoreStateHandles);
        }
        if (this.restoreStateHandles.iterator().next() instanceof SavepointKeyedStateHandle) {
            LOG.info("restore from SavepointKeyedStateHandle");
            return new BoostSavepointRestoreOperation<>(
                keyGroupRange,
                keyGroupPrefixBytes,
                userCodeClassLoader,
                keySerializerProvider,
                db,
                restoreStateHandles,
                tables,
                registeredKvStateMetaInfos,
                keyedStateDescriptorMap,
                nsKeyedStateDescriptorMap,
                registeredPQStates,
                priorityQueueSetFactory);
        }
        throw new BackendBuildingException(
            "Only NoneRestore, IncrementalRestore and savepointRestore are supported now.");
    }

    protected HeapPriorityQueuesManager initHeapPriorityQueuesManager(PriorityQueueSetFactory priorityQueueSetFactory,
        Map<String, HeapPriorityQueueSnapshotRestoreWrapper<?>> registeredPQStates) {
        if (priorityQueueSetFactory instanceof HeapPriorityQueueSetFactory) {
            return new HeapPriorityQueuesManager(registeredPQStates,
                (HeapPriorityQueueSetFactory) priorityQueueSetFactory, keyContext.getKeyGroupRange(),
                keyContext.getNumberOfKeyGroups());
        }
        return null;
    }

    protected BoostRestoreResult restore(BoostStateDB db, BoostRestoreOperation<K> restoreOperation) throws Exception {
        if (!(restoreOperation instanceof BoostNoneRestoreOperation)) {
            db.getConfig().setEnableBloomFilter(false);
        }
        long restoreStartTime = System.currentTimeMillis();
        BoostRestoreResult restoreResult = restoreOperation.restore();
        long restoreEndTime = System.currentTimeMillis();
        LOG.info("Succeed restoring stateBackend.");
        registerRestoreMetric(restoreResult.getDownloadTime(), (restoreEndTime - restoreStartTime) / 1000);
        return restoreResult;
    }

    public AbstractOckDBKeyedStateBackendBuilder<K> setEnableIncrementalCheckpointing(
        boolean enableIncrementalCheckpointing) {
        this.enableIncrementalCheckpointing = enableIncrementalCheckpointing;
        return this;
    }

    public AbstractOckDBKeyedStateBackendBuilder<K> setNumberOfTransferringThreads(int numberOfTransferringThreads) {
        this.numberOfTransferringThreads = numberOfTransferringThreads;
        return this;
    }

    public AbstractOckDBKeyedStateBackendBuilder<K> setTaskSlotFlag(int taskSlotFlag) {
        this.taskSlotFlag = taskSlotFlag;
        return this;
    }

    public AbstractOckDBKeyedStateBackendBuilder<K> setTaskSlotMemoryLimit(long taskSlotMemoryLimit) {
        this.taskSlotMemoryLimit = taskSlotMemoryLimit;
        return this;
    }

    public AbstractOckDBKeyedStateBackendBuilder<K> setSlotManagedMemoryFraction(double slotManagedMemoryFraction) {
        this.slotManagedMemoryFraction = slotManagedMemoryFraction;
        return this;
    }

    public AbstractOckDBKeyedStateBackendBuilder<K> setJobID(String jobID) {
        this.jobID = jobID;
        return this;
    }

    /**
     * setNativeMetricOptions
     *
     * @param options BoostNativeMetricOptions
     * @return AbstractOckDBKeyedStateBackendBuilder
     */
    public AbstractOckDBKeyedStateBackendBuilder<K> setNativeMetricOptions(BoostNativeMetricOptions options) {
        this.boostNativeMetricOptions = options;
        return this;
    }

    private String getExecutionID() {
        String[] splitPath = getPathString(this.instanceBasePath).split("_uuid_", 2);
        if (splitPath.length != 2) {
            throw new BSSRuntimeException("Failed to get BackendUID.");
        }
        return splitPath[1];
    }

    protected void initInstanceBasePath() throws BackendBuildingException {
        // 1. 定义权限集合：750 (rwxr-x---)
        Set<PosixFilePermission> permissions = PosixFilePermissions.fromString("rwxr-x---");
        // 2. 将权限集合转换为FileAttribute
        FileAttribute<Set<PosixFilePermission>> attr = PosixFilePermissions.asFileAttribute(permissions);
        // 3. 创建目录（及其所有不存在的父目录）并应用权限
        try {
            Files.createDirectories(instanceBasePath.toPath(), attr);
        } catch (IOException e) {
            LOG.error("Failed to create instance base path.");
            throw new BackendBuildingException("Failed to create instance base path.", e);
        }
    }

    protected BoostConfig createBoostConfig() {
        BoostConfig boostConfig = new BoostConfig(this.keyGroupRange);
        boostConfig.setMaxParallelism(this.numberOfKeyGroups);
        boostConfig.setInstanceBasePath(getPathString(this.instanceBasePath));
        boostConfig.setBackendUID(getExecutionID());
        boostConfig.setLsmCompactionSwitch(this.config.get(OckDBOptions.OCKDB_JNI_LSM_CMPCT_SWITCH));
        boostConfig.setTtlFilterSwitch(this.config.get(OckDBOptions.OCKDB_TTL_FILTER_SWITCH));
        boostConfig.setZeroCopySwitch(this.config.get(OckDBOptions.OCKDB_ZERO_COPY_SWITCH));
        boostConfig.setCacheFilterAndIndexSwitch(this.config.get(OckDBOptions.OCKDB_CACHE_FILTER_AND_INDEX_SWITCH));
        boostConfig.setKVSeparateSwitch(this.config.get(OckDBOptions.OCKDB_KV_SEPARATE_SWITCH));
        boostConfig.setKVSeparateThreshold(this.config.get(OckDBOptions.OCKDB_KV_SEPARATE_THRESHOLD));
        boostConfig.setCacheFilterAndIndexRatio(this.config.get(OckDBOptions.OCKDB_FILTER_AND_INDEX_OWN_CACHE_RATIO));
        boostConfig.setLsmStoreCompressionPolicy(this.config.get(OckDBOptions.OCKDB_LSM_COMPRESSION_POLICY));
        boostConfig.setLsmStoreCompressionLevelPolicy(this.config.get(OckDBOptions.OCKDB_LSM_COMPRESSION_LEVEL_POLICY));
        boostConfig.setFreshTableSnapshotCompressionPolicy(
            this.config.get(OckDBOptions.OCKDB_FRESH_TABLE_SNAPSHOT_COMPRESSION_POLICY));
        boostConfig.setSliceTableSnapshotCompressionPolicy(
            this.config.get(OckDBOptions.OCKDB_SLICE_TABLE_SNAPSHOT_COMPRESSION_POLICY));
        boostConfig.setEnableBloomFilter(this.config.get(OckDBOptions.OCKDB_BLOOM_FILTER_SWITCH));
        int dbFilterExpectedKeyCount = this.config.get(OckDBOptions.OCKDB_BLOOM_FILTER_EXPECTED_KEY_COUNT);
        if (dbFilterExpectedKeyCount < MIN_DB_FILTER_EXPECTED_KEY_COUNT
            || dbFilterExpectedKeyCount > MAX_DB_FILTER_EXPECTED_KEY_COUNT) {
            LOG.error("Invalid db filter expected key count: {}, should be [1000000,10000000]",
                dbFilterExpectedKeyCount);
            throw new IllegalConfigurationException("Invalid db filter expected key count.");
        }
        boostConfig.setExpectedKeyCount(dbFilterExpectedKeyCount);
        boostConfig.setTaskSlotFlag(taskSlotFlag);
        boostConfig.setTaskSlotMemoryLimit(taskSlotMemoryLimit);
        boostConfig.setEnableLazyDownload(this.config.get(OckDBOptions.OCKDB_LAZY_DOWN_SWITCH));
        boostConfig.setPeakFilterElemNum(this.config.get(OckDBOptions.OCKDB_PEAK_FILTER_ELEM_NUM));
        String snapshotBackupDir = this.config.get(OckDBOptions.BACKUP_DIRECTORY);
        if (snapshotBackupDir != null) {
            if (snapshotBackupDir.endsWith("/")) {
                snapshotBackupDir = snapshotBackupDir.substring(0, snapshotBackupDir.length() - 1);
            }
            boostConfig.setSnapshotBackupDir(snapshotBackupDir + "/" + UUID.randomUUID());
        }
        boostConfig.setEnableLocalRecovery(this.localRecoveryConfig.isLocalRecoveryEnabled());
        setDBMemoryConfig(boostConfig);
        return boostConfig;
    }

    protected void createBackupPathForLocalRecovery(BoostConfig dbOptions) throws BackendBuildingException {
        if (localRecoveryConfig.isLocalRecoveryEnabled()) {
            String dir = this.config.get(OckDBOptions.BACKUP_DIRECTORY);
            if (dir == null) {
                throw new IllegalConfigurationException(
                    "Must config state.backend.ockdb.checkpoint.backup when localRecovery is enabled!");
            }
            try {
                EmbeddedOckStateBackend.validateFilePath(OckDBOptions.BACKUP_DIRECTORY.key(), Paths.get(dir));
                // 此处创建的是配置目录下独属于此DB的子目录
                createBackupPath(dbOptions);
            } catch (IOException e) {
                throw new BackendBuildingException(
                    "Failed to create backup directory: " + (new File(dbOptions.getSnapshotBackupDir())).getName(), e);
            }
        }
    }

    protected void createBackupPath(BoostConfig boostConfig) throws IOException {
        File snapshotBackupDir = new File(boostConfig.getSnapshotBackupDir());
        // 在第一次cp时创建，一直保留
        if (snapshotBackupDir.exists()) {
            FileUtils.deleteDirectory(snapshotBackupDir);
        }
        if (!snapshotBackupDir.mkdirs()) {
            throw new IOException("Failed to create snapshotBackupDir for checkpoint.");
        }
        logger.info("Created backup path: {}", snapshotBackupDir.getName());
    }

    protected void releaseResource(BoostStateDB db,
        Map<String, RegisteredStateMetaInfoBase> registeredKvStateMetaInfos,
        CloseableRegistry cancelStreamRegistryForBackend, BoostSnapshotStrategyBase<K> checkpointSnapshotStrategy,
        Throwable e) throws BackendBuildingException {
        IOUtils.closeQuietly(cancelStreamRegistryForBackend);
        IOUtils.closeQuietly(db);
        IOUtils.closeQuietly(resourceContainer);
        registeredKvStateMetaInfos.clear();
        IOUtils.closeQuietly(checkpointSnapshotStrategy);
        try {
            FileUtils.deleteDirectory(instanceBasePath);
        } catch (IOException ex) {
            logger.warn("Failed to delete base path for BoostStateDB: " + instanceBasePath.getName(), ex);
        }
        String message = "Failed to create Boost keyed state-backend.";
        logger.error(message, e);
        throw new BackendBuildingException(message, e);
    }

    /**
     * 注册snapshotMetric
     */
    protected void registerSnapshotMetric() {
        if (!this.boostNativeMetricOptions.isSnapshotMetricEnabled() || this.boostSnapshotMetric == null) {
            return;
        }
        MetricGroup childGroup = metricGroup.addGroup("snapshot");
        this.boostSnapshotMetric.registerMetric(childGroup);
    }

    private void setDBMemoryConfig(BoostConfig boostConfig) {
        int slotNumber = config.getInteger(TaskManagerOptions.NUM_TASK_SLOTS);
        // 控制db内存相比于slot托管内存的放大比例, 默认0.95，预留5%用于存储内部数据结构
        double dbMemoryFraction = 0.95;
        LOG.info("slot number:{}, dbMemoryFraction:{}", slotNumber, dbMemoryFraction);
        boostConfig.setTotalDBSize((long) (taskSlotMemoryLimit * dbMemoryFraction));

        float dbFileMemoryFraction = this.config.getFloat(OckDBOptions.OCKDB_FILE_MEMORY_FRACTION);
        if (dbFileMemoryFraction <= MIN_DB_FILE_MEMORY_FRACTION
            || dbFileMemoryFraction >= MAX_DB_FILE_MEMORY_FRACTION) {
            LOG.error("Invalid db file memory fraction: {}, should be (0.1,0.5)", dbFileMemoryFraction);
            throw new IllegalConfigurationException("Invalid db file memory fraction config.");
        }
        boostConfig.setFileMemoryFraction(dbFileMemoryFraction);

        float dbSliceWaterMaskRatio = this.config.get(OckDBOptions.OCKDB_JNI_SLICE_WATERMARK_RATIO);
        if (dbSliceWaterMaskRatio <= MIN_JNI_SLICE_WATER_MASK_RATIO
            || dbSliceWaterMaskRatio >= MAX_JNI_SLICE_WATER_MASK_RATIO) {
            LOG.error("Invalid db slice water mask ratio: {}, should be (0,1)", dbSliceWaterMaskRatio);
            throw new IllegalConfigurationException("Invalid db slice water mask ratio config.");
        }

        boostConfig.setSliceMemWaterMark(dbSliceWaterMaskRatio);
        LOG.info("totalDBMem:{} sliceWaterMark:{}", boostConfig.getTotalDBSize(), boostConfig.getSliceMemWaterMark());

        // 设置允许借用的heap大小（如果borrowHeap开关没开，此处获取的size固定为0）
        long borrowHeapSize = HeapMonitor.INSTANCE.getBorrowHeapSize();
        LOG.info("config borrowHpSize:{}", borrowHeapSize);
        boostConfig.setBorrowHeapSize(borrowHeapSize);
    }

    private String getPathString(File basePath) {
        try {
            return basePath.getCanonicalPath();
        } catch (IOException e) {
            throw new BSSRuntimeException("Failed to get basePath.");
        }
    }

    private void registerRestoreMetric(long downloadTime, long restoreTime) {
        if (!this.boostNativeMetricOptions.isRestoreMetricEnabled()) {
            return;
        }
        MetricGroup childGroup = metricGroup.addGroup("java_restore");
        // 时间单位: 秒
        childGroup.gauge("ockdb.restore_time", () -> restoreTime);
        childGroup.gauge("ockdb.download_time", () -> downloadTime);
    }
}
