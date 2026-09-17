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

package com.huawei.ock.bss;

import com.huawei.ock.bss.common.BoostStateDB;
import com.huawei.ock.bss.common.conf.BoostConfig;
import com.huawei.ock.bss.metric.BoostSnapshotMetric;
import com.huawei.ock.bss.ockdb.AbstractOckDBKeyedStateBackendBuilder;
import com.huawei.ock.bss.resource.ResourceContainer;
import com.huawei.ock.bss.restore.BoostIncrementalRestoreOperation;
import com.huawei.ock.bss.restore.BoostRestoreOperation;
import com.huawei.ock.bss.restore.BoostRestoreResult;
import com.huawei.ock.bss.snapshot.BoostIncrementalSnapshotStrategy;
import com.huawei.ock.bss.snapshot.BoostSnapshotStrategyBase;
import com.huawei.ock.bss.snapshot.BoostStateUploader;
import com.huawei.ock.bss.snapshot.NativeBoostFullSnapshotStrategy;
import com.huawei.ock.bss.state.internal.descriptor.keyed.KeyedStateDescriptor;
import com.huawei.ock.bss.state.internal.descriptor.nskeyed.NSKeyedStateDescriptor;
import com.huawei.ock.bss.table.api.Table;

import org.apache.flink.api.common.ExecutionConfig;
import org.apache.flink.api.common.typeutils.TypeSerializer;
import org.apache.flink.configuration.Configuration;
import org.apache.flink.core.fs.CloseableRegistry;
import org.apache.flink.runtime.query.TaskKvStateRegistry;
import org.apache.flink.runtime.state.BackendBuildingException;
import org.apache.flink.runtime.state.CompositeKeySerializationUtils;
import org.apache.flink.runtime.state.HeapPriorityQueuesManager;
import org.apache.flink.runtime.state.KeyGroupRange;
import org.apache.flink.runtime.state.KeyedStateHandle;
import org.apache.flink.runtime.state.LocalRecoveryConfig;
import org.apache.flink.runtime.state.PriorityQueueSetFactory;
import org.apache.flink.runtime.state.RegisteredStateMetaInfoBase;
import org.apache.flink.runtime.state.StateHandleID;
import org.apache.flink.runtime.state.StreamCompressionDecorator;
import org.apache.flink.runtime.state.StreamStateHandle;
import org.apache.flink.runtime.state.heap.HeapPriorityQueueSnapshotRestoreWrapper;
import org.apache.flink.runtime.state.metrics.LatencyTrackingStateConfig;
import org.apache.flink.runtime.state.ttl.TtlTimeProvider;
import org.apache.flink.util.ResourceGuard;
import org.apache.flink.util.UserCodeClassLoader;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import java.io.File;
import java.util.Collection;
import java.util.HashMap;
import java.util.Map;
import java.util.SortedMap;
import java.util.TreeMap;
import java.util.UUID;

import javax.annotation.Nonnull;

/**
 * builder类，通过build方法生产OckDBKeyedStateBackend实例
 *
 * @param <K> key
 * @since 2025年1月12日16:15:42
 */
public class OckDBKeyedStateBackendBuilder<K> extends AbstractOckDBKeyedStateBackendBuilder<K> {
    private static final Logger LOG = LoggerFactory.getLogger(OckDBKeyedStateBackendBuilder.class);

    public OckDBKeyedStateBackendBuilder(int numberOfKeyGroups, KeyGroupRange keyGroupRange,
        UserCodeClassLoader userCodeClassLoader, File instanceBasePath,
        LocalRecoveryConfig localRecoveryConfig,
        TaskKvStateRegistry kvStateRegistry, String fileCompatibleIdentifier,
        ExecutionConfig executionConfig,
        TypeSerializer<K> keySerializer, TtlTimeProvider ttlTimeProvider,
        LatencyTrackingStateConfig latencyTrackingStateConfig,
        CloseableRegistry cancelStreamRegistry,
        StreamCompressionDecorator keyGroupCompressionDecorator,
        @Nonnull Collection<KeyedStateHandle> stateHandles,
        boolean priorityQueueAsyncSnapshot, ResourceContainer resourceContainer,
        EmbeddedOckStateBackend.PriorityQueueStateType priorityQueueStateType,
        Configuration config) {
        super(numberOfKeyGroups, keyGroupRange, userCodeClassLoader, instanceBasePath, localRecoveryConfig,
            kvStateRegistry, fileCompatibleIdentifier, executionConfig, keySerializer, ttlTimeProvider,
            latencyTrackingStateConfig, cancelStreamRegistry, keyGroupCompressionDecorator, stateHandles,
            priorityQueueAsyncSnapshot, resourceContainer, priorityQueueStateType, config);
    }

    /**
     * 获取OckDBKeyedStateBackend实例
     *
     * @return 返回OckDBKeyedStateBackend实例
     * @throws BackendBuildingException 异常
     */
    @Override
    public OckDBKeyedStateBackend<K> build() throws BackendBuildingException {
        initInstanceBasePath();

        BoostConfig dbOptions = createBoostConfig();
        createBackupPathForLocalRecovery(dbOptions);
        BoostStateDB db = new BoostStateDB(dbOptions);
        if (this.boostNativeMetricOptions.isStatisticsEnabled()) {
            db.createBoostNativeMetric(this.boostNativeMetricOptions, this.metricGroup);
        }
        Map<String, RegisteredStateMetaInfoBase> registeredKvStateMetaInfos = new HashMap<>();
        PriorityQueueSetFactory priorityQueueSetFactory = initPriorityQueueFactory(db, registeredKvStateMetaInfos);
        Map<String, Table> tables = new HashMap<>();
        Map<String, KeyedStateDescriptor> keyedStateDescriptorMap = new HashMap<>();
        Map<String, NSKeyedStateDescriptor> nsKeyedStateDescriptorMap = new HashMap<>();
        // registeredPQStates要放入heapPriorityQueuesManager
        Map<String, HeapPriorityQueueSnapshotRestoreWrapper<?>> registeredPQStates = new HashMap<>();
        // cancelStreamRegistryForBackend在创建checkpointSnapshotStrategy和创建OckDBKeyedStateBackend时使用
        // this.cancelStreamRegistry在restore时使用
        CloseableRegistry cancelStreamRegistryForBackend = new CloseableRegistry();
        BoostSnapshotStrategyBase<K> checkpointSnapshotStrategy = null;
        HeapPriorityQueuesManager heapPriorityQueuesManager = null;

        try {
            int keyGroupPrefixBytes =
                CompositeKeySerializationUtils.computeRequiredBytesInKeyGroupPrefix(numberOfKeyGroups);
            ResourceGuard resourceGuard = new ResourceGuard();
            UUID backendUID = UUID.randomUUID();
            SortedMap<Long, Map<StateHandleID, StreamStateHandle>> materializedSstFiles = new TreeMap<>();
            long lastCompletedCheckpointId = -1L;
            // restore
            BoostRestoreOperation<K> restoreOperation =
                initRestoreOperation(jobID, db, keyGroupPrefixBytes, registeredKvStateMetaInfos, registeredPQStates,
                    priorityQueueSetFactory, tables, keyedStateDescriptorMap, nsKeyedStateDescriptorMap);
            BoostRestoreResult restoreResult = restore(db, restoreOperation);
            if (restoreOperation instanceof BoostIncrementalRestoreOperation) {
                backendUID = restoreResult.getBackendUID();
                materializedSstFiles = restoreResult.getRestoredSstFiles();
                lastCompletedCheckpointId = restoreResult.getLastCompletedCheckpointId();
            }
            heapPriorityQueuesManager = initHeapPriorityQueuesManager(priorityQueueSetFactory, registeredPQStates);

            // snapshot资源封装
            // prepareDirectories()用于restore时
            checkpointSnapshotStrategy = initSavepointAndCheckpointStrategies(jobID, resourceGuard, db,
                keyGroupPrefixBytes, backendUID, registeredKvStateMetaInfos, lastCompletedCheckpointId,
                materializedSstFiles, keyedStateDescriptorMap, nsKeyedStateDescriptorMap, heapPriorityQueuesManager,
                priorityQueueIsAsyncSnapshot);
        } catch (Throwable e) {
            releaseResource(db, registeredKvStateMetaInfos, cancelStreamRegistryForBackend, checkpointSnapshotStrategy,
                e);
        }
        logger.info("Finished building Boost keyed state-backend at {}.", instanceBasePath.getName());
        return new OckDBKeyedStateBackend<>(db, tables, this.instanceBasePath, this.instanceOckDBPath,
            keyedStateDescriptorMap, nsKeyedStateDescriptorMap, registeredKvStateMetaInfos, registeredPQStates,
            this.priorityQueueIsAsyncSnapshot, this.keyGroupRange, this.userCodeClassLoader, this.localRecoveryConfig,
            this.kvStateRegistry, this.fileCompatibleIdentifier, this.executionConfig, this.config, this.metricGroup,
            this.keySerializerProvider.currentSchemaSerializer(), this.ttlTimeProvider, this.latencyTrackingStateConfig,
            cancelStreamRegistryForBackend, this.keyGroupCompressionDecorator, this.keyContext, priorityQueueSetFactory,
            this.resourceContainer, heapPriorityQueuesManager, checkpointSnapshotStrategy);
    }

    private BoostSnapshotStrategyBase<K> initSavepointAndCheckpointStrategies(
        String jobID, ResourceGuard resourceGuard, BoostStateDB db, int keyGroupPrefixBytes, UUID backendID,
        Map<String, RegisteredStateMetaInfoBase> registeredKvStateMetaInfos,
        long lastCompletedCheckpointId,
        SortedMap<Long, Map<StateHandleID, StreamStateHandle>> materializedSstFiles,
        Map<String, KeyedStateDescriptor> keyedStateDescriptorMap,
        Map<String, NSKeyedStateDescriptor> nsKeyedStateDescriptorMap,
        HeapPriorityQueuesManager heapPriorityQueuesManager,
        boolean priorityQueueIsAsyncSnapshot) {
        if (this.boostNativeMetricOptions.isSnapshotMetricEnabled()) {
            this.boostSnapshotMetric = new BoostSnapshotMetric(true);
        }
        BoostSnapshotStrategyBase<K> checkpointSnapshotStrategy;
        OckDBKeyedStateBackend.BoostKvStateInfo kvStateInfo =
            new OckDBKeyedStateBackend.BoostKvStateInfo(
                registeredKvStateMetaInfos,
                keyedStateDescriptorMap,
                nsKeyedStateDescriptorMap,
                heapPriorityQueuesManager,
                priorityQueueIsAsyncSnapshot);
        BoostStateUploader stateUploader = new BoostStateUploader(numberOfTransferringThreads);
        if (enableIncrementalCheckpointing) {
            checkpointSnapshotStrategy = new BoostIncrementalSnapshotStrategy<>(
                jobID,
                db,
                resourceGuard,
                keySerializerProvider.currentSchemaSerializer(),
                kvStateInfo,
                keyGroupRange,
                keyGroupPrefixBytes,
                localRecoveryConfig,
                instanceBasePath,
                backendID,
                materializedSstFiles,
                stateUploader,
                lastCompletedCheckpointId);
            LOG.info("enableIncrementalCheckpointing");
        } else {
            checkpointSnapshotStrategy = new NativeBoostFullSnapshotStrategy<>(
                jobID,
                db,
                resourceGuard,
                keySerializerProvider.currentSchemaSerializer(),
                kvStateInfo,
                keyGroupRange,
                keyGroupPrefixBytes,
                localRecoveryConfig,
                instanceBasePath,
                backendID,
                stateUploader);
            LOG.info("enableFullCheckpointing");
        }
        checkpointSnapshotStrategy.setSnapshotMetric(this.boostSnapshotMetric);
        registerSnapshotMetric();
        return checkpointSnapshotStrategy;
    }
}