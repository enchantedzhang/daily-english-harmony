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
import com.huawei.ock.bss.common.conf.TableConfig;
import com.huawei.ock.bss.common.exception.BSSRuntimeException;
import com.huawei.ock.bss.resource.ResourceContainer;
import com.huawei.ock.bss.snapshot.BoostSnapshotStrategyBase;
import com.huawei.ock.bss.snapshot.FullBoostSnapshotResources;
import com.huawei.ock.bss.snapshot.SavepointDBResult;
import com.huawei.ock.bss.state.internal.KeyedListState;
import com.huawei.ock.bss.state.internal.KeyedListStateImpl;
import com.huawei.ock.bss.state.internal.KeyedMapState;
import com.huawei.ock.bss.state.internal.KeyedMapStateImpl;
import com.huawei.ock.bss.state.internal.KeyedValueState;
import com.huawei.ock.bss.state.internal.KeyedValueStateImpl;
import com.huawei.ock.bss.state.internal.descriptor.keyed.KeyedListStateDescriptor;
import com.huawei.ock.bss.state.internal.descriptor.keyed.KeyedMapStateDescriptor;
import com.huawei.ock.bss.state.internal.descriptor.keyed.KeyedStateDescriptor;
import com.huawei.ock.bss.state.internal.descriptor.keyed.KeyedValueStateDescriptor;
import com.huawei.ock.bss.state.internal.descriptor.nskeyed.NSKeyedListStateDescriptor;
import com.huawei.ock.bss.state.internal.descriptor.nskeyed.NSKeyedMapStateDescriptor;
import com.huawei.ock.bss.state.internal.descriptor.nskeyed.NSKeyedStateDescriptor;
import com.huawei.ock.bss.state.internal.descriptor.nskeyed.NSKeyedValueStateDescriptor;
import com.huawei.ock.bss.state.internal.namespace.NSKeyedListState;
import com.huawei.ock.bss.state.internal.namespace.NSKeyedListStateImpl;
import com.huawei.ock.bss.state.internal.namespace.NSKeyedMapState;
import com.huawei.ock.bss.state.internal.namespace.NSKeyedMapStateImpl;
import com.huawei.ock.bss.state.internal.namespace.NSKeyedValueState;
import com.huawei.ock.bss.state.internal.namespace.NSKeyedValueStateImpl;
import com.huawei.ock.bss.state.unified.UnifiedAggregatingState;
import com.huawei.ock.bss.state.unified.UnifiedListState;
import com.huawei.ock.bss.state.unified.UnifiedMapState;
import com.huawei.ock.bss.state.unified.UnifiedReducingState;
import com.huawei.ock.bss.state.unified.UnifiedValueState;
import com.huawei.ock.bss.state.unified.namespace.UnifiedNSAggregatingState;
import com.huawei.ock.bss.state.unified.namespace.UnifiedNSListState;
import com.huawei.ock.bss.state.unified.namespace.UnifiedNSMapState;
import com.huawei.ock.bss.state.unified.namespace.UnifiedNSReducingState;
import com.huawei.ock.bss.state.unified.namespace.UnifiedNSValueState;
import com.huawei.ock.bss.table.api.KListTable;
import com.huawei.ock.bss.table.api.KMapTable;
import com.huawei.ock.bss.table.api.KVTable;
import com.huawei.ock.bss.table.api.NsKListTable;
import com.huawei.ock.bss.table.api.NsKMapTable;
import com.huawei.ock.bss.table.api.NsKVTable;
import com.huawei.ock.bss.table.api.Table;
import com.huawei.ock.bss.table.api.TableDescription;
import com.huawei.ock.bss.transformer.OckDBSnapshotTransformFactoryAdaptor;
import com.huawei.ock.bss.util.DescriptionUtils;
import com.huawei.ock.bss.util.KeyedStateBackendUtils;
import com.huawei.ock.bss.util.LogSanitizer;

import org.apache.flink.api.common.ExecutionConfig;
import org.apache.flink.api.common.state.State;
import org.apache.flink.api.common.state.StateDescriptor;
import org.apache.flink.api.common.state.StateTtlConfig;
import org.apache.flink.api.common.typeutils.TypeSerializer;
import org.apache.flink.api.common.typeutils.TypeSerializerSchemaCompatibility;
import org.apache.flink.api.common.typeutils.TypeSerializerSnapshot;
import org.apache.flink.api.common.typeutils.base.MapSerializer;
import org.apache.flink.api.common.typeutils.base.MapSerializerSnapshot;
import org.apache.flink.api.java.tuple.Tuple2;
import org.apache.flink.configuration.Configuration;
import org.apache.flink.core.fs.CloseableRegistry;
import org.apache.flink.metrics.MetricGroup;
import org.apache.flink.runtime.checkpoint.CheckpointOptions;
import org.apache.flink.runtime.checkpoint.SnapshotType;
import org.apache.flink.runtime.query.TaskKvStateRegistry;
import org.apache.flink.runtime.state.AbstractKeyedStateBackend;
import org.apache.flink.runtime.state.CheckpointStreamFactory;
import org.apache.flink.runtime.state.CompositeKeySerializationUtils;
import org.apache.flink.runtime.state.HeapPriorityQueuesManager;
import org.apache.flink.runtime.state.KeyGroupRange;
import org.apache.flink.runtime.state.KeyGroupedInternalPriorityQueue;
import org.apache.flink.runtime.state.Keyed;
import org.apache.flink.runtime.state.KeyedStateHandle;
import org.apache.flink.runtime.state.LocalRecoveryConfig;
import org.apache.flink.runtime.state.PriorityComparable;
import org.apache.flink.runtime.state.PriorityQueueSetFactory;
import org.apache.flink.runtime.state.RegisteredKeyValueStateBackendMetaInfo;
import org.apache.flink.runtime.state.RegisteredStateMetaInfoBase;
import org.apache.flink.runtime.state.SavepointResources;
import org.apache.flink.runtime.state.SnapshotExecutionType;
import org.apache.flink.runtime.state.SnapshotResult;
import org.apache.flink.runtime.state.SnapshotStrategyRunner;
import org.apache.flink.runtime.state.StateSnapshotTransformer;
import org.apache.flink.runtime.state.StreamCompressionDecorator;
import org.apache.flink.runtime.state.heap.HeapPriorityQueueElement;
import org.apache.flink.runtime.state.heap.HeapPriorityQueueSnapshotRestoreWrapper;
import org.apache.flink.runtime.state.InternalKeyContext;
import org.apache.flink.runtime.state.metainfo.StateMetaInfoSnapshot;
import org.apache.flink.runtime.state.metrics.LatencyTrackingStateConfig;
import org.apache.flink.runtime.state.ttl.TtlTimeProvider;
import org.apache.flink.util.FileUtils;
import org.apache.flink.util.FlinkRuntimeException;
import org.apache.flink.util.IOUtils;
import org.apache.flink.util.Preconditions;
import org.apache.flink.util.ResourceGuard;
import org.apache.flink.util.StateMigrationException;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import java.io.File;
import java.io.IOException;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.concurrent.RunnableFuture;
import java.util.stream.Collectors;
import java.util.stream.Stream;

import javax.annotation.Nonnull;
import javax.annotation.Nullable;

/**
 * OckDBKeyedStateBackend
 *
 * @param <K> key
 * @since 2025年1月12日16:15:42
 */
@SuppressWarnings({"rawtypes", "unchecked"})
public class OckDBKeyedStateBackend<K> extends AbstractKeyedStateBackend<K> {
    private static final Logger LOG = LoggerFactory.getLogger(EmbeddedOckStateBackend.class);

    private static final Map<StateDescriptor.Type, OckDBKeyedStateBackend.KeyedStateFactory> KEYED_STATE_FACTORIES =
        Stream.of(Tuple2.of(StateDescriptor.Type.VALUE, (KeyedStateFactory) UnifiedValueState::create),
                Tuple2.of(StateDescriptor.Type.AGGREGATING, (KeyedStateFactory) UnifiedAggregatingState::create),
                Tuple2.of(StateDescriptor.Type.REDUCING, (KeyedStateFactory) UnifiedReducingState::create),
                Tuple2.of(StateDescriptor.Type.LIST, (KeyedStateFactory) UnifiedListState::create),
                Tuple2.of(StateDescriptor.Type.MAP, (KeyedStateFactory) UnifiedMapState::create))
            .collect(Collectors.toMap(t -> t.f0, t -> t.f1));

    private static final Map<StateDescriptor.Type, OckDBKeyedStateBackend.NSKeyedStateFactory>
        NS_KEYED_STATE_FACTORIES =
        Stream.of(Tuple2.of(StateDescriptor.Type.VALUE, (NSKeyedStateFactory) UnifiedNSValueState::create),
                Tuple2.of(StateDescriptor.Type.AGGREGATING, (NSKeyedStateFactory) UnifiedNSAggregatingState::create),
                Tuple2.of(StateDescriptor.Type.REDUCING, (NSKeyedStateFactory) UnifiedNSReducingState::create),
                Tuple2.of(StateDescriptor.Type.LIST, (NSKeyedStateFactory) UnifiedNSListState::create),
                Tuple2.of(StateDescriptor.Type.MAP, (NSKeyedStateFactory) UnifiedNSMapState::create))
            .collect(Collectors.toMap(t -> t.f0, t -> t.f1));

    private boolean disposed = false;

    private BoostStateDB db;

    private BoostStateDB tmpDb;

    private final boolean priorityQueueIsAsyncSnapshot;

    private final String fileCompatibleIdentifier;

    private final File instanceBasePath;

    private final File instanceOckDBPath;

    private final Configuration configuration;

    private final LocalRecoveryConfig localRecoveryConfig;

    private final ResourceGuard resourceGuard;

    private final ResourceContainer resourceContainer;

    private final BoostSnapshotStrategyBase<K> checkpointSnapshotStrategy;

    @Nullable
    private final PriorityQueueSetFactory priorityQueueSetFactory;

    @Nullable
    private final HeapPriorityQueuesManager heapPriorityQueuesManager;

    private final Map<String, Table> tables;

    private final Map<String, KeyedStateDescriptor> keyedStateDescriptorMap;

    private final Map<String, NSKeyedStateDescriptor> nsKeyedStateDescriptorMap;

    private final Map<String, RegisteredStateMetaInfoBase> registeredKvStateMetaInfos;

    public OckDBKeyedStateBackend(BoostStateDB db, Map<String, Table> tables, File instanceBasePath,
        File instanceOckDBPath, Map<String, KeyedStateDescriptor> keyedStateDescriptorMap,
        Map<String, NSKeyedStateDescriptor> namespaceKeyedStateDescriptorMap,
        Map<String, RegisteredStateMetaInfoBase> registeredKvStateMetaInfos,
        Map<String, HeapPriorityQueueSnapshotRestoreWrapper<?>> registeredPQStates,
        boolean priorityQueueIsAsyncSnapshot, KeyGroupRange keyGroupRange, ClassLoader userCodeClassLoader,
        LocalRecoveryConfig localRecoveryConfig, TaskKvStateRegistry kvStateRegistry, String fileCompatibleIdentifier,
        ExecutionConfig executionConfig, Configuration config, MetricGroup metricGroup, TypeSerializer<K> keySerializer,
        TtlTimeProvider ttlTimeProvider, LatencyTrackingStateConfig latencyTrackingStateConfig,
        CloseableRegistry cancelStreamRegistryForBackend, StreamCompressionDecorator keyGroupCompressionDecorator,
        InternalKeyContext<K> keyContext, PriorityQueueSetFactory priorityQueueSetFactory,
        ResourceContainer resourceContainer, HeapPriorityQueuesManager heapPriorityQueuesManager,
        BoostSnapshotStrategyBase<K> checkpointSnapshotStrategy) {
        super(kvStateRegistry, keySerializer, userCodeClassLoader, executionConfig, ttlTimeProvider,
            latencyTrackingStateConfig, cancelStreamRegistryForBackend, keyGroupCompressionDecorator, keyContext);
        this.db = db;
        this.tables = new HashMap<>(tables);
        this.instanceBasePath = instanceBasePath;
        this.instanceOckDBPath = instanceOckDBPath;
        this.registeredKvStateMetaInfos = registeredKvStateMetaInfos;
        this.localRecoveryConfig = localRecoveryConfig;
        this.fileCompatibleIdentifier = fileCompatibleIdentifier;
        this.keyedStateDescriptorMap = keyedStateDescriptorMap;
        this.nsKeyedStateDescriptorMap = namespaceKeyedStateDescriptorMap;
        this.priorityQueueSetFactory = priorityQueueSetFactory;
        this.heapPriorityQueuesManager = heapPriorityQueuesManager;
        this.priorityQueueIsAsyncSnapshot = priorityQueueIsAsyncSnapshot;
        this.resourceContainer = resourceContainer;
        this.resourceGuard = new ResourceGuard();
        this.checkpointSnapshotStrategy = checkpointSnapshotStrategy;
        this.configuration = config;
    }

    /**
     * keyedState接口
     */
    public interface NSKeyedStateFactory {
        /**
         * 创建UnifiedNSKeyedState
         *
         * @param <K>                 key
         * @param <N>                 namespace
         * @param <SV>                中间值
         * @param <S>                 state
         * @param <IS>                UnifiedKeyedState
         * @param stateDesc           状态描述符
         * @param keySerializer       key序列化器
         * @param namespaceSerializer namespace序列化器
         * @param backend             stateBackend
         * @return 返回对应state
         * @throws Exception 创建时异常
         */
        <K, N, SV, S extends org.apache.flink.api.common.state.State, IS extends S> IS createState(
            StateDescriptor<S, SV> stateDesc, TypeSerializer<K> keySerializer, TypeSerializer<N> namespaceSerializer,
            OckDBKeyedStateBackend<K> backend) throws Exception;
    }

    /**
     * namespaceKeyedState接口
     */
    public static interface KeyedStateFactory {
        /**
         * 创建UnifiedKeyedState
         *
         * @param <K>           key
         * @param <SV>          中间值
         * @param <S>           state
         * @param <IS>          UnifiedNSKeyedState
         * @param stateDesc     状态描述符
         * @param keySerializer key序列化器
         * @param backend       stateBackend
         * @return 创建UnifiedNSKeyedState
         * @throws Exception 创建时异常
         */
        <K, SV, S extends org.apache.flink.api.common.state.State, IS extends S> IS createsState(
            StateDescriptor<S, SV> stateDesc, TypeSerializer<K> keySerializer, OckDBKeyedStateBackend<K> backend)
            throws Exception;
    }

    @Override
    public void notifyCheckpointComplete(long completedCheckpointId) throws Exception {
        if (completedCheckpointId < 0) {
            LOG.error("completedCheckpointId is negative: {}", completedCheckpointId);
            return;
        }
        if (checkpointSnapshotStrategyNull()) {
            LOG.debug("Trying to notifyCheckpointComplete, but checkpointSnapshotStrategy is null.");
            return;
        }
        checkpointSnapshotStrategy.notifyCheckpointComplete(completedCheckpointId);
    }

    @Override
    public void notifyCheckpointAborted(long abortedCheckpointId) throws Exception {
        if (abortedCheckpointId < 0) {
            LOG.error("abortedCheckpointId is negative: {}", abortedCheckpointId);
            return;
        }
        if (checkpointSnapshotStrategyNull()) {
            LOG.debug("Trying to notifyCheckpointAborted, but checkpointSnapshotStrategy is null.");
            return;
        }
        checkpointSnapshotStrategy.notifyCheckpointAborted(abortedCheckpointId);
    }

    private boolean checkpointSnapshotStrategyNull() {
        return checkpointSnapshotStrategy == null;
    }

    /**
     * 待定
     *
     * @param checkpointType checkpointType
     * @return boolean
     */
    @Override
    public boolean requiresLegacySynchronousTimerSnapshots(SnapshotType checkpointType) {
        return false;
    }

    @Override
    public int numKeyValueStateEntries() {
        throw new UnsupportedOperationException("NumKeyValueStateEntries not supported now.");
    }

    @Override
    public <N> Stream<K> getKeys(String state, N namespace) {
        Table<K> table = this.tables.get(state);
        if (table == null) {
            return Stream.empty();
        }
        return table.getKeys(namespace);
    }

    @Override
    public <N> Stream<Tuple2<K, N>> getKeysAndNamespaces(final String s) {
        throw new UnsupportedOperationException("getKeysAndNamespaces not supported yet.");
    }

    public InternalKeyContext<K> getKeyContext() {
        return this.keyContext;
    }

    @Override
    public void setCurrentKey(K key) {
        Preconditions.checkNotNull(key);
        super.setCurrentKey(key);
    }

    @Override
    public void dispose() {
        File snapshotBackupPath = new File(this.db.getConfig().getSnapshotBackupDir());
        if (this.disposed) {
            return;
        }
        this.resourceGuard.close();
        try {
            super.dispose();
            if (this.db != null) {
                this.db.close();
                this.db = null;
            }
        } catch (Exception e) {
            LOG.warn("Failed to close BoostStateDB", e);
            throw new BSSRuntimeException("Failed to close BoostStateDB.", e);
        } finally {
            IOUtils.closeQuietly(this.resourceContainer);
            this.tables.clear();
        }
        cleanInstanceBasePath();
        if (localRecoveryConfig.isLocalRecoveryEnabled()) {
            cleanSnapshotBackupPath(snapshotBackupPath);
        }
        this.disposed = true;
    }

    private void cleanInstanceBasePath() {
        LOG.info("Closed Boost State Backend. Cleaning up BoostStateDB working directory {}.",
            instanceBasePath.getName());

        try {
            FileUtils.deleteDirectory(instanceBasePath);
        } catch (IOException ex) {
            LOG.warn("Could not delete BoostStateDB working directory: {}", instanceBasePath.getName(), ex);
        }
    }

    private void cleanSnapshotBackupPath(File snapshotBackupPath) {
        LOG.info("Closed Boost State Backend. Cleaning up BoostStateDB snapshot backup directory {}.",
            snapshotBackupPath.getName());

        try {
            FileUtils.deleteDirectory(snapshotBackupPath);
        } catch (IOException ex) {
            LOG.warn("Could not delete BoostStateDB snapshot backup directory: {}", snapshotBackupPath.getName(), ex);
        }
    }

    /**
     * 获取KeyGroupPrefixBytes
     *
     * @return int
     */
    public int getKeyGroupPrefixBytes() {
        return CompositeKeySerializationUtils.computeRequiredBytesInKeyGroupPrefix(this.numberOfKeyGroups);
    }

    @Nonnull
    @Override
    public <N, SV, SEV, S extends State, IS extends S> IS createOrUpdateInternalState(
        @Nonnull TypeSerializer<N> namespaceSerializer, @Nonnull StateDescriptor<S, SV> stateDesc,
        @Nonnull StateSnapshotTransformer.StateSnapshotTransformFactory<SEV> snapshotTransformFactory)
        throws Exception {
        return createInternalState(namespaceSerializer, stateDesc, snapshotTransformFactory);
    }

    /**
     * 创建unifiedState
     *
     * @param namespaceSerializer      namespace序列化器
     * @param stateDesc                状态描述符
     * @param snapshotTransformFactory snapshotTransformFactory
     * @param <N>                      namespace
     * @param <SV>                     SV
     * @param <SEV>                    SEV
     * @param <S>                      state
     * @param <IS>                     unifiedState
     * @return UnifiedState
     * @throws Exception exception
     */
    @Nonnull
    private <N, SV, SEV, S extends State, IS extends S> IS createInternalState(
        @Nonnull TypeSerializer<N> namespaceSerializer, @Nonnull StateDescriptor<S, SV> stateDesc,
        @Nonnull StateSnapshotTransformer.StateSnapshotTransformFactory<SEV> snapshotTransformFactory)
        throws Exception {
        IS userState;
        tryRegisterStateMeta(namespaceSerializer, stateDesc, snapshotTransformFactory);
        if (namespaceSerializer instanceof org.apache.flink.runtime.state.VoidNamespaceSerializer) {
            KeyedStateFactory keyedStateFactory = KEYED_STATE_FACTORIES.get(stateDesc.getType());
            if (keyedStateFactory == null) {
                String message =
                    String.format("State %s is not supported by %s", stateDesc.getClass(), this.getClass());
                throw new FlinkRuntimeException(message);
            }
            userState = keyedStateFactory.createsState(stateDesc, this.keySerializer, this);
        } else {
            NSKeyedStateFactory nsKeyedStateFactory = NS_KEYED_STATE_FACTORIES.get(stateDesc.getType());
            if (nsKeyedStateFactory == null) {
                String message =
                    String.format("NamespaceState %s is not supported by %s", stateDesc.getClass(), this.getClass());
                throw new FlinkRuntimeException(message);
            }
            userState = nsKeyedStateFactory.createState(stateDesc, this.keySerializer, namespaceSerializer, this);
        }

        updateTableConfig(stateDesc);
        return userState;
    }

    @SuppressWarnings("rawtypes")
    private void updateTableConfig(StateDescriptor stateDescriptor) {
        StateTtlConfig stateTtlConfig = stateDescriptor.getTtlConfig();
        Table table = this.db.getTable(stateDescriptor.getName());
        Preconditions.checkNotNull(table, "Can't find table %s to update ttl",
            stateDescriptor.getName());
        TableConfig tableConfig = (new TableConfig.Builder()).setTableTtl(
                stateTtlConfig.isEnabled() ? stateTtlConfig.getTtl().toMilliseconds() : -1L)
            .setIsEnableKVSeparate(false)
            .build();
        table.updateTableConfig(tableConfig);
    }

    private <N, S extends State, SV, SEV> void tryRegisterStateMeta(
        TypeSerializer<N> namespaceSerializer, StateDescriptor<S, SV> stateDesc,
        @Nonnull StateSnapshotTransformer.StateSnapshotTransformFactory<SEV> snapshotTransformFactory)
        throws Exception {
        RegisteredKeyValueStateBackendMetaInfo<N, SV> stateMetaInfo =
            (RegisteredKeyValueStateBackendMetaInfo<N, SV>) this.registeredKvStateMetaInfos.get(stateDesc.getName());
        RegisteredKeyValueStateBackendMetaInfo<N, SV> newMetaInfo;
        TypeSerializer<SV> newStateSerializer = stateDesc.getSerializer();

        if (stateMetaInfo != null) {
            newMetaInfo =
                updateRestoredStateMetaInfo(stateMetaInfo, stateDesc, namespaceSerializer, newStateSerializer);
            this.registeredKvStateMetaInfos.put(stateDesc.getName(), newMetaInfo);
        } else {
            newMetaInfo = new RegisteredKeyValueStateBackendMetaInfo(stateDesc.getType(), stateDesc.getName(),
                namespaceSerializer, newStateSerializer,
                StateSnapshotTransformer.StateSnapshotTransformFactory.noTransform());
            this.registeredKvStateMetaInfos.put(stateDesc.getName(), newMetaInfo);
        }

        StateSnapshotTransformer.StateSnapshotTransformFactory<SV> wrappedSnapshotTransformFactory =
            OckDBSnapshotTransformFactoryAdaptor.wrapStateSnapshotTransformFactory(
                stateDesc, snapshotTransformFactory, newMetaInfo.getStateSerializer());
        newMetaInfo.updateSnapshotTransformFactory(wrappedSnapshotTransformFactory);
    }

    private <N, S extends org.apache.flink.api.common.state.State, SV> RegisteredKeyValueStateBackendMetaInfo<N, SV>
    updateRestoredStateMetaInfo(
        RegisteredKeyValueStateBackendMetaInfo<N, SV> restoredKvStateMetaInfo, StateDescriptor<S, SV> stateDesc,
        TypeSerializer<N> namespaceSerializer, TypeSerializer<SV> stateSerializer) throws Exception {
        TypeSerializer<N> previousNamespaceSerializer = restoredKvStateMetaInfo.getNamespaceSerializer();
        TypeSerializerSchemaCompatibility<N> s =
            restoredKvStateMetaInfo.updateNamespaceSerializer(namespaceSerializer);

        if (s.isCompatibleAfterMigration() || s.isIncompatible()) {
            throw new StateMigrationException("The new namespace serializer (" + namespaceSerializer
                + ") must be compatible with the old namespace serializer (" + previousNamespaceSerializer + ").");
        }

        restoredKvStateMetaInfo.checkStateMetaInfo(stateDesc);
        TypeSerializer<SV> previousStateSerializer = restoredKvStateMetaInfo.getStateSerializer();
        TypeSerializerSchemaCompatibility<SV> newStateSerializerCompatibility =
            restoredKvStateMetaInfo.updateStateSerializer(stateSerializer);

        if (newStateSerializerCompatibility.isCompatibleAfterMigration()) {
            LOG.info("Migrating state from previous state serializer {} to new state serializer {}.",
                previousStateSerializer.getClass().getSimpleName(), stateSerializer.getClass().getSimpleName());
            migrateStateValues(stateDesc, restoredKvStateMetaInfo);
        } else if (newStateSerializerCompatibility.isIncompatible()) {
            throw new StateMigrationException("The new state serializer (" + stateSerializer
                + ") must not be incompatible with the old state serializer (" + previousStateSerializer + ").");
        }
        return restoredKvStateMetaInfo;
    }

    private <SV, S extends State, N, V> void migrateStateValues(
        StateDescriptor<S, SV> stateDesc,
        RegisteredKeyValueStateBackendMetaInfo<N, SV> restoredKvStateMetaInfo) throws Exception {
        if (stateDesc.getType() == StateDescriptor.Type.MAP) {
            TypeSerializerSnapshot<SV> previousSerializerSnapshot =
                restoredKvStateMetaInfo.getPreviousStateSerializerSnapshot();
            if (previousSerializerSnapshot == null) {
                throw new StateMigrationException("the previous serializer snapshot is null");
            }
            if (!(previousSerializerSnapshot instanceof MapSerializerSnapshot)) {
                throw new StateMigrationException("previous serializer snapshot should be a MapSerializerSnapshot");
            }

            TypeSerializer<SV> newSerializer = restoredKvStateMetaInfo.getStateSerializer();
            if (!(newSerializer instanceof MapSerializer)) {
                throw new StateMigrationException("new serializer should be a MapSerializer");
            }

            MapSerializer<?, ?> mapSerializer = (MapSerializer<?, ?>) newSerializer;
            MapSerializerSnapshot<?, ?> mapSerializerSnapshot =
                (MapSerializerSnapshot<?, ?>) previousSerializerSnapshot;
            if (!checkMapStateKeySchemaCompatibility(mapSerializerSnapshot, mapSerializer)) {
                throw new StateMigrationException(
                    "The new serializer for a MapState requires state migration in order for the job to proceed, "
                        + "since the key schema has changed. However, "
                        + "migration for MapState currently only allows value schema evolutions.");
            }
        }

        Table table = tables.get(stateDesc.getName());
        TableDescription tableDescription = table.getTableDescription();
        tableDescription.getTableSerializer().setValueSerializer(restoredKvStateMetaInfo.getStateSerializer());
        Table newTable = tmpDb.getTableOrCreate(tableDescription);
        newTable.migrateValue(table);
        db.putOrReplaceTable(newTable);
    }

    private <UK> boolean checkMapStateKeySchemaCompatibility(MapSerializerSnapshot<?, ?> mapSerializerSnapshot,
        MapSerializer<?, ?> mapSerializer) {
        TypeSerializerSnapshot<UK> previousKeySerializerSnapshot =
            (TypeSerializerSnapshot<UK>) mapSerializerSnapshot.getKeySerializerSnapshot();
        TypeSerializer<UK> newUserKeySerializer =
            (TypeSerializer<UK>) mapSerializer.getKeySerializer();

        TypeSerializerSchemaCompatibility<UK> keyCompatibility =
            previousKeySerializerSnapshot.resolveSchemaCompatibility(newUserKeySerializer);
        return keyCompatibility.isCompatibleAsIs();
    }

    /**
     * 创建直接操作KVTable的state
     *
     * @param keyedStateDescriptor 描述符
     * @param <V>                  value
     * @return KeyedValueState
     */
    public <V> KeyedValueState<K, V> createKeyedValueState(KeyedValueStateDescriptor<K, V> keyedStateDescriptor) {
        String stateName = keyedStateDescriptor.getName();
        TableDescription tableDescription =
            DescriptionUtils.createTableDescription(getNumberOfKeyGroups(), keyedStateDescriptor);
        KVTable<K, V> table = (KVTable<K, V>) getOrCreateTable(tableDescription);
        KeyedValueState<K, V> keyedValueState = new KeyedValueStateImpl<>(keyedStateDescriptor, table);
        this.keyedStateDescriptorMap.put(stateName, keyedStateDescriptor);
        return keyedValueState;
    }

    /**
     * 创建KeyedListState
     *
     * @param keyedStateDescriptor KeyedListState描述符
     * @param <E>                  element
     * @return KeyedListState
     */
    public <E> KeyedListState<K, E> createKeyedListState(KeyedListStateDescriptor<K, E> keyedStateDescriptor) {
        String stateName = keyedStateDescriptor.getName();
        TableDescription tableDescription =
            DescriptionUtils.createTableDescription(getNumberOfKeyGroups(), keyedStateDescriptor);
        KListTable<K, E> table = (KListTable<K, E>) getOrCreateTable(tableDescription);
        KeyedListState<K, E> keyedListState = new KeyedListStateImpl<>(keyedStateDescriptor, table);
        this.keyedStateDescriptorMap.put(stateName, keyedStateDescriptor);
        return keyedListState;
    }

    /**
     * 创建KeyedMapState
     *
     * @param keyedStateDescriptor KeyedMapState描述符
     * @param <UK>                 userKey
     * @param <UV>                 userValue
     * @return createKeyedMapState
     */
    public <UK, UV> KeyedMapState<K, UK, UV> createKeyedMapState(
        KeyedMapStateDescriptor<K, UK, UV> keyedStateDescriptor) {
        String stateName = keyedStateDescriptor.getName();
        TableDescription tableDescription =
            DescriptionUtils.createTableDescription(getNumberOfKeyGroups(), keyedStateDescriptor);
        KMapTable<K, UK, UV, Map<UK, UV>> table =
            (KMapTable<K, UK, UV, Map<UK, UV>>) getOrCreateTable(tableDescription);
        KeyedMapState<K, UK, UV> keyedMapState = new KeyedMapStateImpl<>(keyedStateDescriptor, table);
        this.keyedStateDescriptorMap.put(stateName, keyedStateDescriptor);
        return keyedMapState;
    }

    /**
     * 创建直接操作KVTable的namespaceState
     *
     * @param descriptor nsKeyedValueStateDescriptor
     * @param <N>        namespace
     * @param <V>        value
     * @return NSKeyedValueState
     */
    public <N, V> NSKeyedValueState<K, N, V> createNSKeyedValueState(NSKeyedValueStateDescriptor<K, N, V> descriptor) {
        String stateName = descriptor.getName();
        TableDescription tableDescription =
            DescriptionUtils.createTableDescription(getNumberOfKeyGroups(), descriptor);
        NsKVTable<K, N, V> table = (NsKVTable<K, N, V>) getOrCreateTable(tableDescription);
        NSKeyedValueState<K, N, V> nsKeyedValueState = new NSKeyedValueStateImpl<>(descriptor, table);
        this.nsKeyedStateDescriptorMap.put(stateName, descriptor);
        return nsKeyedValueState;
    }

    /**
     * 创建NSKeyedListState
     *
     * @param descriptor NSKeyedListState描述符
     * @param <N>        namespace
     * @param <E>        element
     * @return NSKeyedListState
     */
    public <N, E> NSKeyedListState<K, N, E> createNSKeyedListState(NSKeyedListStateDescriptor<K, N, E> descriptor) {
        String stateName = descriptor.getName();
        TableDescription tableDescription =
            DescriptionUtils.createTableDescription(getNumberOfKeyGroups(), descriptor);
        NsKListTable<K, N, E> table = (NsKListTable<K, N, E>) getOrCreateTable(tableDescription);
        NSKeyedListState<K, N, E> nsKeyedListState = new NSKeyedListStateImpl<>(descriptor, table);
        this.nsKeyedStateDescriptorMap.put(stateName, descriptor);
        return nsKeyedListState;
    }

    /**
     * 创建NSKeyedMapState
     *
     * @param descriptor NSKeyedMapState描述符
     * @param <N>        namespace
     * @param <UK>       userKey
     * @param <UV>       userValue
     * @return NSKeyedMapState
     */
    public <N, UK, UV> NSKeyedMapState<K, N, UK, UV> createNSKeyedMapState(
        NSKeyedMapStateDescriptor<K, N, UK, UV> descriptor) {
        String stateName = descriptor.getName();
        TableDescription tableDescription =
            DescriptionUtils.createTableDescription(getNumberOfKeyGroups(), descriptor);
        NsKMapTable<K, N, UK, UV, Map<UK, UV>> table =
            (NsKMapTable<K, N, UK, UV, Map<UK, UV>>) getOrCreateTable(tableDescription);
        NSKeyedMapState<K, N, UK, UV> nsKeyedMapState = new NSKeyedMapStateImpl<>(descriptor, table);
        this.nsKeyedStateDescriptorMap.put(stateName, descriptor);
        return nsKeyedMapState;
    }

    private Table getOrCreateTable(TableDescription tableDescription) {
        return KeyedStateBackendUtils.getOrCreateTable(this.db, this.tables, tableDescription);
    }

    @Nonnull
    @Override
    public <T extends HeapPriorityQueueElement & PriorityComparable<? super T> &
        Keyed<?>> KeyGroupedInternalPriorityQueue<T> create(
        @Nonnull String stateName, @Nonnull TypeSerializer<T> byteOrderedElementSerializer) {
        LOG.info("create PriorityQueue of state:{}", LogSanitizer.sanitize(stateName));
        if (this.heapPriorityQueuesManager != null) {
            return this.heapPriorityQueuesManager.createOrUpdate(stateName, byteOrderedElementSerializer);
        } else {
            return priorityQueueSetFactory.create(stateName, byteOrderedElementSerializer, false);
        }
    }

    @Nonnull
    @Override
    public RunnableFuture<SnapshotResult<KeyedStateHandle>> snapshot(final long checkpointId, final long timestamp,
        @Nonnull final CheckpointStreamFactory streamFactory, @Nonnull final CheckpointOptions checkpointOptions)
        throws Exception {
        return new SnapshotStrategyRunner<>(checkpointSnapshotStrategy.getDescription(), checkpointSnapshotStrategy,
            cancelStreamRegistry, SnapshotExecutionType.ASYNCHRONOUS).snapshot(checkpointId, timestamp, streamFactory,
            checkpointOptions);
    }

    @Nonnull
    @Override
    public SavepointResources<K> savepoint() throws Exception {
        Map<String, HeapPriorityQueueSnapshotRestoreWrapper<?>> registeredPQStates;
        if (heapPriorityQueuesManager == null) {
            registeredPQStates = new HashMap<>();
        } else {
            registeredPQStates = heapPriorityQueuesManager.getRegisteredPQStates();
        }
        int stateNum = registeredKvStateMetaInfos.size() + registeredPQStates.size();
        // kv和pq的snapshot
        final List<StateMetaInfoSnapshot> stateMetaInfoSnapshots = new ArrayList<>(stateNum);
        Map<String, FullBoostSnapshotResources.KvMetaData> kvStateMetaDataMap = new HashMap<>();
        Map<String, FullBoostSnapshotResources.PqMetaData> pqMetaDataMap = new HashMap<>();

        for (Map.Entry<String, RegisteredStateMetaInfoBase> metaInfo : registeredKvStateMetaInfos.entrySet()) {
            int stateId = stateMetaInfoSnapshots.size();
            StateMetaInfoSnapshot stateMetaInfoSnapshot = metaInfo.getValue().snapshot();
            stateMetaInfoSnapshots.add(stateMetaInfoSnapshot);
            if (metaInfo.getValue() instanceof RegisteredKeyValueStateBackendMetaInfo) {
                RegisteredKeyValueStateBackendMetaInfo<?, ?> meta =
                        (RegisteredKeyValueStateBackendMetaInfo<?, ?>) metaInfo.getValue();
                StateSnapshotTransformer<byte[]> stateSnapshotTransformer =
                        meta.getStateSnapshotTransformFactory().createForSerializedState().orElse(null);
                kvStateMetaDataMap.put(metaInfo.getKey(),
                        new FullBoostSnapshotResources.KvMetaData(stateId, meta.getStateType(),
                                stateMetaInfoSnapshot, stateSnapshotTransformer));
            } else {
                kvStateMetaDataMap.put(metaInfo.getKey(),
                        new FullBoostSnapshotResources.KvMetaData(stateId, StateDescriptor.Type.UNKNOWN,
                                stateMetaInfoSnapshot, null));
            }
        }

        for (HeapPriorityQueueSnapshotRestoreWrapper<?> pqStateInfo : registeredPQStates.values()) {
            int stateId = stateMetaInfoSnapshots.size();
            stateMetaInfoSnapshots.add(pqStateInfo.getMetaInfo().snapshot());
            pqMetaDataMap.put(pqStateInfo.getMetaInfo().getName(),
                new FullBoostSnapshotResources.PqMetaData(stateId, pqStateInfo.stateSnapshot()));
        }

        // 通过SavepointDataView直接获取迭代器
        final SavepointDBResult savepointDBResult = new SavepointDBResult(db.getSavepointID());

        FullBoostSnapshotResources<K> resources = new FullBoostSnapshotResources<>(resourceGuard.acquireResource(),
            savepointDBResult, db,
            CompositeKeySerializationUtils.computeRequiredBytesInKeyGroupPrefix(numberOfKeyGroups), keyGroupRange,
            this.numberOfKeyGroups, keySerializer, configuration, keyGroupCompressionDecorator,
            stateMetaInfoSnapshots, kvStateMetaDataMap, pqMetaDataMap);
        return new SavepointResources<>(resources, SnapshotExecutionType.ASYNCHRONOUS);
    }

    /**
     * isSafeToReuseKVState
     *
     * @return boolean
     */
    @Override
    public boolean isSafeToReuseKVState() {
        return true;
    }

    /**
     * 封装需要快照的state相关信息
     */
    public static class BoostKvStateInfo implements AutoCloseable {
        /**
         * 状态元信息
         */
        public final Map<String, RegisteredStateMetaInfoBase> metaInfoMap;

        /**
         * 记录了已存在的keyedStateDescriptor
         */
        public final Map<String, KeyedStateDescriptor> keyedStateDescriptorMap;

        /**
         * 记录了已存在的NSKeyedStateDescriptor
         */
        public final Map<String, NSKeyedStateDescriptor> nsKeyedStateDescriptorMap;

        /**
         * 记录优先队列
         */
        @Nullable
        public final HeapPriorityQueuesManager heapPriorityQueuesManager;

        /**
         * 优先队列是否执行异步快照
         */
        public final boolean priorityQueueIsAsyncSnapshot;

        public BoostKvStateInfo(Map<String, RegisteredStateMetaInfoBase> metaInfoMap,
            Map<String, KeyedStateDescriptor> keyedStateDescriptorMap,
            Map<String, NSKeyedStateDescriptor> nsKeyedStateDescriptorMap,
            @Nullable HeapPriorityQueuesManager heapPriorityQueuesManager,
            boolean priorityQueueIsAsyncSnapshot) {
            this.metaInfoMap = metaInfoMap;
            this.keyedStateDescriptorMap = keyedStateDescriptorMap;
            this.nsKeyedStateDescriptorMap = nsKeyedStateDescriptorMap;
            this.heapPriorityQueuesManager = heapPriorityQueuesManager;
            this.priorityQueueIsAsyncSnapshot = priorityQueueIsAsyncSnapshot;
        }

        /**
         * 关闭
         *
         * @throws Exception exception
         */
        @Override
        public void close() throws Exception {

        }
    }
}
