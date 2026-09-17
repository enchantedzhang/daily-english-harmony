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

package com.huawei.ock.bss.snapshot;

import com.huawei.ock.bss.OckDBKeyedStateBackend;
import com.huawei.ock.bss.common.BoostStateDB;
import com.huawei.ock.bss.common.Checkpoint;
import com.huawei.ock.bss.metric.BoostSnapshotMetric;
import com.huawei.ock.bss.state.internal.descriptor.keyed.KeyedStateDescriptor;
import com.huawei.ock.bss.state.internal.descriptor.nskeyed.NSKeyedStateDescriptor;

import org.apache.flink.api.common.state.CheckpointListener;
import org.apache.flink.api.common.typeutils.TypeSerializer;
import org.apache.flink.core.fs.CloseableRegistry;
import org.apache.flink.core.memory.DataOutputView;
import org.apache.flink.core.memory.DataOutputViewStreamWrapper;
import org.apache.flink.runtime.state.CheckpointStateOutputStream;
import org.apache.flink.runtime.state.CheckpointStreamFactory;
import org.apache.flink.runtime.state.CheckpointStreamWithResultProvider;
import org.apache.flink.runtime.state.CheckpointedStateScope;
import org.apache.flink.runtime.state.DirectoryStateHandle;
import org.apache.flink.runtime.state.HeapPriorityQueuesManager;
import org.apache.flink.runtime.state.KeyGroupRange;
import org.apache.flink.runtime.state.KeyedBackendSerializationProxy;
import org.apache.flink.runtime.state.KeyedStateHandle;
import org.apache.flink.runtime.state.LocalRecoveryConfig;
import org.apache.flink.runtime.state.LocalRecoveryDirectoryProvider;
import org.apache.flink.runtime.state.RegisteredStateMetaInfoBase;
import org.apache.flink.runtime.state.SnapshotDirectory;
import org.apache.flink.runtime.state.SnapshotResources;
import org.apache.flink.runtime.state.SnapshotResult;
import org.apache.flink.runtime.state.SnapshotStrategy;
import org.apache.flink.runtime.state.StateSnapshot;
import org.apache.flink.runtime.state.StateUtil;
import org.apache.flink.runtime.state.StreamStateHandle;
import org.apache.flink.runtime.state.heap.HeapPriorityQueueSnapshotRestoreWrapper;
import org.apache.flink.runtime.state.heap.HeapPriorityQueueStateSnapshot;
import org.apache.flink.runtime.state.metainfo.StateMetaInfoSnapshot;
import org.apache.flink.util.ExceptionUtils;
import org.apache.flink.util.FileUtils;
import org.apache.flink.util.IOUtils;
import org.apache.flink.util.InstantiationUtil;
import org.apache.flink.util.ResourceGuard;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import java.io.File;
import java.io.IOException;
import java.nio.charset.StandardCharsets;
import java.nio.file.Path;
import java.util.ArrayList;
import java.util.Collections;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.Optional;
import java.util.UUID;

import javax.annotation.Nonnegative;
import javax.annotation.Nonnull;
import javax.annotation.Nullable;

/**
 * BSS快照基类
 *
 * @param <K> key
 * @since BeiMing 25.0.T1
 */
public abstract class AbstractBoostSnapshotStrategy<K> implements CheckpointListener,
    SnapshotStrategy<KeyedStateHandle, AbstractBoostSnapshotStrategy.NativeBoostSnapshotResources>, AutoCloseable {
    private static final Logger LOG = LoggerFactory.getLogger(AbstractBoostSnapshotStrategy.class);

    /**
     * 优先队列是否执行异步快照
     */
    public final boolean priorityQueueIsAsyncSnapshot;

    /**
     * 记录了已存在的keyedStateDescriptor
     */
    @Nonnull
    protected final Map<String, KeyedStateDescriptor> keyedStateDescriptorMap;

    /**
     * 记录了已存在的NSKeyedStateDescriptor
     */
    @Nonnull
    protected final Map<String, NSKeyedStateDescriptor> nsKeyedStateDescriptorMap;

    /**
     * 状态元信息
     */
    @Nonnull
    protected final Map<String, RegisteredStateMetaInfoBase> metaInfoMap;

    /**
     * 获取注册的优先队列
     */
    @Nullable
    protected final HeapPriorityQueuesManager heapPriorityQueuesManager;

    /**
     * key序列化器
     */
    @Nonnull
    protected final TypeSerializer<K> keySerializer;

    /**
     * keyGroupRange
     */
    @Nonnull
    protected final KeyGroupRange keyGroupRange;

    /**
     * resourceGuard
     */
    @Nonnull
    protected final ResourceGuard resourceGuard;

    /**
     * keyGroupPrefixBytes
     */
    @Nonnegative
    protected final int keyGroupPrefixBytes;

    /**
     * localRecoveryConfig
     */
    @Nonnull
    protected final LocalRecoveryConfig localRecoveryConfig;

    /**
     * db
     */
    @Nonnull
    protected BoostStateDB db;

    /**
     * description
     */
    @Nonnull
    private final String description;

    /**
     * db base path
     */
    @Nonnull
    protected final File instanceBasePath;

    /**
     * backendUID
     */
    @Nonnull
    protected final UUID backendUID;

    /**
     * 本地checkpoint文件夹名
     */
    @Nonnull
    protected final String localDirectoryName;

    /**
     * Snapshot监控类
     */
    @Nullable
    protected BoostSnapshotMetric snapshotMetric;

    /**
     * snapshot开始时间
     */
    protected long snapshotStartTime = 0L;

    /**
     * upload开始时间
     */
    protected long uploadStartTime = 0L;

    public AbstractBoostSnapshotStrategy(@Nonnull String description, @Nonnull BoostStateDB db,
        @Nonnull ResourceGuard resourceGuard, @Nonnull TypeSerializer<K> keySerializer,
        @Nonnull OckDBKeyedStateBackend.BoostKvStateInfo kvStateInfo, @Nonnull KeyGroupRange keyGroupRange,
        @Nonnegative int keyGroupPrefixBytes, @Nonnull LocalRecoveryConfig localRecoveryConfig,
        @Nonnull File instanceBasePath, @Nonnull UUID backendUID) {
        this.keyedStateDescriptorMap = kvStateInfo.keyedStateDescriptorMap;
        this.nsKeyedStateDescriptorMap = kvStateInfo.nsKeyedStateDescriptorMap;
        this.metaInfoMap = kvStateInfo.metaInfoMap;
        this.heapPriorityQueuesManager = kvStateInfo.heapPriorityQueuesManager;
        this.priorityQueueIsAsyncSnapshot = kvStateInfo.priorityQueueIsAsyncSnapshot;
        this.keySerializer = keySerializer;
        this.keyGroupRange = keyGroupRange;
        this.resourceGuard = resourceGuard;
        this.keyGroupPrefixBytes = keyGroupPrefixBytes;
        this.localRecoveryConfig = localRecoveryConfig;
        this.description = description;
        this.instanceBasePath = instanceBasePath;
        this.localDirectoryName = backendUID.toString().replaceAll("[\\-]", "");
        this.backendUID = backendUID;
        this.db = db;
    }

    /**
     * 获取已注册的pqState
     *
     * @return 已注册的pqState
     */
    protected Map<String, HeapPriorityQueueSnapshotRestoreWrapper<?>> getRegisteredPQStates() {
        if (this.heapPriorityQueuesManager == null || !this.priorityQueueIsAsyncSnapshot) {
            return Collections.emptyMap();
        }
        return new HashMap<>(this.heapPriorityQueuesManager.getRegisteredPQStates());
    }

    /**
     * 注册metric
     *
     * @param metric BoostSnapshotMetric
     */
    public void setSnapshotMetric(BoostSnapshotMetric metric) {
        this.snapshotMetric = metric;
    }

    public String getDescription() {
        return this.description;
    }

    /**
     * 同步地准备快照需要的资源，返回的资源在以后可用于异步部分。
     *
     * @param checkpointId checkpointId
     * @return 完成快照需要的资源
     * @throws Exception exception
     */
    @Override
    public NativeBoostSnapshotResources syncPrepareResources(long checkpointId) throws Exception {
        if (snapshotMetric != null) {
            snapshotMetric.setSnapshotId(checkpointId);
        }
        snapshotStartTime = System.currentTimeMillis();
        final SnapshotDirectory snapshotDirectory = prepareLocalSnapshotDirectory(checkpointId);
        LOG.trace("Local BSS checkpoint goes to backup path {}.", snapshotDirectory);
        if (!snapshotDirectory.getDirectory().toFile().exists()) {
            throw new IOException("snapshotDirectory does not exist, " + snapshotDirectory.getDirectory());
        }

        final List<StateMetaInfoSnapshot> stateMetaInfoSnapshots = new ArrayList<>(this.metaInfoMap.size());
        final List<KeyedStateDescriptor> keyedStateDescriptors = new ArrayList<>(this.keyedStateDescriptorMap.size());
        final List<NSKeyedStateDescriptor> nsKeyedStateDescriptors = new ArrayList<>(
            this.nsKeyedStateDescriptorMap.size());

        final Map<String, HeapPriorityQueueSnapshotRestoreWrapper<?>> registeredPQStates = getRegisteredPQStates();
        final Map<String, HeapPriorityQueueStateSnapshot<?>> pqStateSnapshots;
        if (registeredPQStates.isEmpty()) {
            pqStateSnapshots = Collections.emptyMap();
            LOG.debug("checkpointId: {}, and registeredPQStates is empty", checkpointId);
        } else {
            LOG.debug("checkpointId: {}, and registeredPQStates size : {}", checkpointId, registeredPQStates.size());
            pqStateSnapshots = new HashMap<>(registeredPQStates.size());
        }

        final AbstractPreviousSnapshot previousSnapshot = snapshotMetaData(checkpointId, stateMetaInfoSnapshots,
            keyedStateDescriptors, nsKeyedStateDescriptors, registeredPQStates, pqStateSnapshots);

        Checkpoint checkpoint = takeDBNativeCheckpoint(snapshotDirectory, checkpointId);
        LOG.info("checkpointId: {} takeDBNativeCheckpoint success!", checkpointId);
        return new NativeBoostSnapshotResources(snapshotDirectory, previousSnapshot, stateMetaInfoSnapshots,
            keyedStateDescriptors, nsKeyedStateDescriptors, registeredPQStates, pqStateSnapshots, checkpoint);
    }

    /**
     * 对元数据打快照
     *
     * @param checkpointID            本次快照ID
     * @param stateMetaInfoSnapshots  状态元数据快照
     * @param keyedStateDescriptors   keyedState描述器
     * @param nsKeyedStateDescriptors namespace keyedState描述器
     * @param registeredPQStates      注册的PQ状态
     * @param pqStateSnapshots        PQ状态快照
     * @return 前一次快照已确认的文件信息
     */
    protected abstract AbstractPreviousSnapshot snapshotMetaData(long checkpointID,
        @Nonnull final List<StateMetaInfoSnapshot> stateMetaInfoSnapshots,
        @Nonnull final List<KeyedStateDescriptor> keyedStateDescriptors,
        @Nonnull final List<NSKeyedStateDescriptor> nsKeyedStateDescriptors,
        final Map<String, HeapPriorityQueueSnapshotRestoreWrapper<?>> registeredPQStates,
        final Map<String, HeapPriorityQueueStateSnapshot<?>> pqStateSnapshots);

    private Checkpoint takeDBNativeCheckpoint(@Nonnull SnapshotDirectory outputDir, long checkpointId)
        throws Exception {
        try (ResourceGuard.Lease ignored = resourceGuard.acquireResource()) {
            return Checkpoint.create(db, checkpointId, outputDir.getDirectory().toString());
        } catch (IOException exception) {
            try {
                LOG.error("Failed to sync create checkpoint");
                outputDir.cleanup();
            } catch (IOException e) {
                exception = ExceptionUtils.firstOrSuppressed(e, exception);
            }
            throw exception;
        }
    }

    private SnapshotDirectory prepareLocalSnapshotDirectory(long checkpointID) throws IOException {
        if (localRecoveryConfig.isLocalRecoveryEnabled()) {
            LocalRecoveryDirectoryProvider directoryProvider = localRecoveryConfig.getLocalStateDirectoryProvider()
                .orElseThrow(LocalRecoveryConfig.localRecoveryNotEnabled());
            File dir = directoryProvider.subtaskSpecificCheckpointDirectory(checkpointID);

            if (!dir.exists() && !dir.mkdirs()) {
                throw new IOException("Failed to get or create " + dir + " for checkpoint " + checkpointID);
            }
            File ockSnapshotDir = new File(dir, localDirectoryName);
            if (ockSnapshotDir.exists()) {
                FileUtils.deleteDirectory(ockSnapshotDir);
            }
            if (!ockSnapshotDir.mkdirs()) {
                throw new IOException("Failed to recreate ockSnapshotDir for checkpoint.");
            }

            Path path = ockSnapshotDir.toPath();
            try {
                return SnapshotDirectory.permanent(path);
            } catch (IOException ex) {
                try {
                    FileUtils.deleteDirectory(dir);
                } catch (IOException deleteEx) {
                    ex = ExceptionUtils.firstOrSuppressed(deleteEx, ex);
                }
                throw ex;
            }
        } else {
            File snapshotDirectory = new File(instanceBasePath, "checkpoint-" + checkpointID);
            if (!snapshotDirectory.exists() && !snapshotDirectory.mkdirs()) {
                throw new IOException("Failed to get or create " + snapshotDirectory.getName()
                    + " for checkpoint " + checkpointID);
            }
            return SnapshotDirectory.temporary(snapshotDirectory);
        }
    }

    /**
     * @param tmpResourcesRegistry tmpResourcesRegistry
     * @param localBackupDirectory 本地checkpoint目录, 清理未完成的snapshot
     */
    protected void cleanupIncompleteSnapshot(@Nonnull CloseableRegistry tmpResourcesRegistry,
        @Nonnull SnapshotDirectory localBackupDirectory) {
        try {
            tmpResourcesRegistry.close();
        } catch (Exception e) {
            LOG.warn("Could not properly clean tmp resources.", e);
        }

        if (!localBackupDirectory.isSnapshotCompleted()) {
            return;
        }

        try {
            DirectoryStateHandle directoryStateHandle = localBackupDirectory.completeSnapshotAndGetHandle();
            if (directoryStateHandle != null) {
                directoryStateHandle.discardState();
            }
        } catch (Exception e) {
            LOG.warn("discard local state failed.", e);
        }
    }

    /**
     * 元数据写入输出流
     *
     * @param jobID                     jobID
     * @param tmpResourcesRegistry      tmpResourcesRegistry
     * @param snapshotCloseableRegistry snapshotCloseableRegistry
     * @param snapshotResources         当前snapshot resource
     * @param checkpointStreamFactory   快照输出流factory
     * @param checkpointId              当前快照ID
     * @return 输出流状态句柄
     * @throws Exception 处理过程的异常
     */
    @Nonnull
    protected SnapshotResult<StreamStateHandle> materializeMetaData(
        @Nonnull String jobID,
        @Nonnull CloseableRegistry snapshotCloseableRegistry, @Nonnull CloseableRegistry tmpResourcesRegistry,
        @Nonnull NativeBoostSnapshotResources snapshotResources,
        @Nonnull CheckpointStreamFactory checkpointStreamFactory, long checkpointId) throws Exception {
        List<StateMetaInfoSnapshot> stateMetaInfoSnapshots = snapshotResources.stateMetaInfoSnapshots;
        List<KeyedStateDescriptor> keyedStateDescriptors = snapshotResources.keyedStateDescriptors;
        List<NSKeyedStateDescriptor> nsKeyedStateDescriptors = snapshotResources.nsKeyedStateDescriptors;
        Map<String, HeapPriorityQueueSnapshotRestoreWrapper<?>> registeredPQStates
            = snapshotResources.registeredPQStates;
        Map<String, HeapPriorityQueueStateSnapshot<?>> pqStateSnapshots = snapshotResources.pqStateSnapshots;
        CheckpointStreamWithResultProvider streamWithResultProvider = localRecoveryConfig.isLocalRecoveryEnabled()
            // 开启LocalRecovery时
            ? CheckpointStreamWithResultProvider.createDuplicatingStream(checkpointId, CheckpointedStateScope.EXCLUSIVE,
            checkpointStreamFactory, localRecoveryConfig.getLocalStateDirectoryProvider().get())
            // 未开启LocalRecovery时
            : CheckpointStreamWithResultProvider.createSimpleStream(CheckpointedStateScope.EXCLUSIVE,
                checkpointStreamFactory);
        snapshotCloseableRegistry.registerCloseable(streamWithResultProvider);

        try {
            CheckpointStateOutputStream checkpointStateOutputStream
                = streamWithResultProvider.getCheckpointOutputStream();
            DataOutputView out = new DataOutputViewStreamWrapper(checkpointStateOutputStream);
            out.writeInt(3);
            // 写入32字节的JobID
            out.write(jobID.getBytes(StandardCharsets.UTF_8));
            KeyedBackendSerializationProxy<K> serializationProxy = new KeyedBackendSerializationProxy<>(keySerializer,
                stateMetaInfoSnapshots, false);
            serializationProxy.write(out);

            writeKeyedStateDescriptors(out, keyedStateDescriptors);

            writeNSKeyedStateDescriptors(out, nsKeyedStateDescriptors);

            writePQStates(checkpointStateOutputStream, out, registeredPQStates, pqStateSnapshots);

            if (snapshotCloseableRegistry.unregisterCloseable(streamWithResultProvider)) {
                SnapshotResult<StreamStateHandle> result
                    = streamWithResultProvider.closeAndFinalizeCheckpointStreamResult();
                streamWithResultProvider = null;
                tmpResourcesRegistry.registerCloseable(() -> StateUtil.discardStateObjectQuietly(result));
                return result;
            } else {
                throw new IOException("Failed to return a handle because stream already closed.");
            }
        } finally {
            if (streamWithResultProvider != null) {
                if (snapshotCloseableRegistry.unregisterCloseable(streamWithResultProvider)) {
                    IOUtils.closeQuietly(streamWithResultProvider);
                }
            }
            LOG.debug("Async checkpoint write metadata success, checkpointId:{}.", checkpointId);
        }
    }

    private void writeKeyedStateDescriptors(DataOutputView out, List<KeyedStateDescriptor> keyedStateDescriptors)
        throws IOException {
        out.writeInt(keyedStateDescriptors.size());
        for (KeyedStateDescriptor descriptor : keyedStateDescriptors) {
            if (descriptor == null) {
                throw new NullPointerException("Failed to get descriptor name. size: " + keyedStateDescriptors.size());
            }
            LOG.debug("KeyedStateDescriptors.name = {}", descriptor.getName());
            out.writeUTF(descriptor.getName());
            out.write(InstantiationUtil.serializeObject(descriptor));
        }
        LOG.debug("writeKeyedStateDescriptors success! and keyedStateDescriptors.size = {}",
            keyedStateDescriptors.size());
    }

    private void writeNSKeyedStateDescriptors(DataOutputView out, List<NSKeyedStateDescriptor> nsKeyedStateDescriptors)
        throws IOException {
        out.writeInt(nsKeyedStateDescriptors.size());
        for (NSKeyedStateDescriptor descriptor : nsKeyedStateDescriptors) {
            if (descriptor == null) {
                throw new NullPointerException(
                    "Failed to get descriptor name. size: " + nsKeyedStateDescriptors.size());
            }
            LOG.debug("NSKeyedStateDescriptors.name = {}", descriptor.getName());
            out.writeUTF(descriptor.getName());
            out.write(InstantiationUtil.serializeObject(descriptor));
        }
        LOG.debug("writeNSKeyedStateDescriptors success! and nsKeyedStateDescriptors.size = {}",
            nsKeyedStateDescriptors.size());
    }

    private void writePQStates(CheckpointStateOutputStream checkpointStateOutputStream, DataOutputView out,
        Map<String, HeapPriorityQueueSnapshotRestoreWrapper<?>> registeredPQStates,
        Map<String, HeapPriorityQueueStateSnapshot<?>> pqStateSnapshots) throws IOException {
        out.writeInt(registeredPQStates.size());
        boolean hasItem = !registeredPQStates.isEmpty();
        long pqStatePos = checkpointStateOutputStream.getPos();
        int numberOfKeyGroups = keyGroupRange.getNumberOfKeyGroups();
        long[] keyGroupRangeOffsets = hasItem ? new long[numberOfKeyGroups] : null;
        if (hasItem) {
            Map<String, Short> pqState = new HashMap<>(registeredPQStates.size());
            short stateId = 0;
            for (String name : registeredPQStates.keySet()) {
                out.writeShort(stateId);
                out.writeUTF(name);
                pqState.put(name, stateId);
                stateId = (short) (stateId + 1);
            }
            for (int keyGroupPos = 0; keyGroupPos < numberOfKeyGroups; keyGroupPos++) {
                int keyGroupId = keyGroupRange.getKeyGroupId(keyGroupPos);
                keyGroupRangeOffsets[keyGroupPos] = checkpointStateOutputStream.getPos();
                out.writeInt(keyGroupId);
                int ii = 0;
                for (Map.Entry<String, HeapPriorityQueueStateSnapshot<?>> entry : pqStateSnapshots.entrySet()) {
                    StateSnapshot.StateKeyGroupWriter keyGroupWriter = entry.getValue().getKeyGroupWriter();
                    LOG.debug(" StateSnapshot.StateKeyGroupWriter ii = {}, and name = {}", ii, entry.getKey());
                    out.writeShort(pqState.get(entry.getKey()));
                    keyGroupWriter.writeStateInKeyGroup(out, keyGroupId);
                    ii++;
                }
            }
        }

        long snapshotResPos = checkpointStateOutputStream.getPos();
        if (hasItem) {
            out.writeLong(pqStatePos);
            for (long keyGroupRangeOffset : keyGroupRangeOffsets) {
                out.writeLong(keyGroupRangeOffset);
            }
            out.writeLong(snapshotResPos);
        }
        LOG.debug("writePQStates PQ hasItem = {}, and pqStatePos = {}, and snapshotResPos = {}", hasItem, pqStatePos,
            snapshotResPos);
    }

    @Override
    public abstract void close();

    /**
     * 封装了对OckDBKeyedStateBackend进行增量快照的过程。
     */
    protected abstract class AbstractBoostDBSnapshotOperation implements SnapshotResultSupplier<KeyedStateHandle> {
        /**
         * CheckpointId
         */
        protected final long checkpointId;

        /**
         * Checkpoint写入的流
         */
        @Nonnull
        protected final CheckpointStreamFactory checkpointStreamFactory;

        /**
         * 本次snapshot资源
         */
        @Nonnull
        protected final NativeBoostSnapshotResources snapshotResources;

        /**
         * 本地快照目录
         */
        @Nonnull
        protected final SnapshotDirectory localBackupDirectory;

        /**
         * 状态元数据快照
         */
        @Nonnull
        protected final List<StateMetaInfoSnapshot> stateMetaInfoSnapshots;

        /**
         * keyedState描述器
         */
        @Nonnull
        protected final List<KeyedStateDescriptor> keyedStateDescriptors;

        /**
         * namespace keyedState描述器
         */
        @Nonnull
        protected final List<NSKeyedStateDescriptor> nsKeyedStateDescriptors;

        /**
         * registeredPQStates
         */
        @Nonnull
        protected final Map<String, HeapPriorityQueueSnapshotRestoreWrapper<?>> registeredPQStates;

        /**
         * pqStateSnapshots
         */
        @Nonnull
        protected final Map<String, HeapPriorityQueueStateSnapshot<?>> pqStateSnapshots;

        /**
         * 本次Checkpoint对象
         */
        @Nonnull
        protected final Checkpoint checkpoint;

        /**
         * 用于清理异步cp的临时资源
         */
        @Nonnull
        protected final CloseableRegistry tmpResourcesRegistry;

        protected AbstractBoostDBSnapshotOperation(long checkpointId,
            @Nonnull CheckpointStreamFactory checkpointStreamFactory,
            @Nonnull NativeBoostSnapshotResources snapshotResources) {
            this.snapshotResources = snapshotResources;
            this.checkpointId = checkpointId;
            this.checkpointStreamFactory = checkpointStreamFactory;
            this.localBackupDirectory = snapshotResources.snapshotDirectory;
            this.stateMetaInfoSnapshots = snapshotResources.stateMetaInfoSnapshots;
            this.keyedStateDescriptors = snapshotResources.keyedStateDescriptors;
            this.nsKeyedStateDescriptors = snapshotResources.nsKeyedStateDescriptors;
            this.registeredPQStates = snapshotResources.registeredPQStates;
            this.pqStateSnapshots = snapshotResources.pqStateSnapshots;
            this.checkpoint = snapshotResources.checkpoint;
            this.tmpResourcesRegistry = new CloseableRegistry();
        }

        /**
         * addSnapshotIncrementalSize
         *
         * @param size size
         */
        protected void addSnapshotIncrementalSize(long size) {
            if (AbstractBoostSnapshotStrategy.this.snapshotMetric != null) {
                AbstractBoostSnapshotStrategy.this.snapshotMetric.addSnapshotIncrementalSize(size);
            }
        }

        /**
         * addSnapshotFileSize
         *
         * @param size size
         */
        protected void addSnapshotFileSize(long size) {
            if (AbstractBoostSnapshotStrategy.this.snapshotMetric != null) {
                AbstractBoostSnapshotStrategy.this.snapshotMetric.addSnapshotFileSize(size);
            }
        }

        /**
         * addSnapshotSliceFileCount
         */
        protected void addSnapshotSliceFileCount() {
            if (AbstractBoostSnapshotStrategy.this.snapshotMetric != null) {
                AbstractBoostSnapshotStrategy.this.snapshotMetric.addSnapshotSliceFileCount();
            }
        }

        /**
         * addSnapshotSliceIncrementalSize
         *
         * @param size size
         */
        protected void addSnapshotSliceIncrementalSize(long size) {
            if (AbstractBoostSnapshotStrategy.this.snapshotMetric != null) {
                AbstractBoostSnapshotStrategy.this.snapshotMetric.addSnapshotSliceIncrementalSize(size);
            }
        }

        /**
         * addSnapshotSliceFileSize
         *
         * @param size size
         */
        protected void addSnapshotSliceFileSize(long size) {
            if (AbstractBoostSnapshotStrategy.this.snapshotMetric != null) {
                AbstractBoostSnapshotStrategy.this.snapshotMetric.addSnapshotSliceFileSize(size);
            }
        }

        /**
         * addSnapshotSstFileCount
         */
        protected void addSnapshotSstFileCount() {
            if (AbstractBoostSnapshotStrategy.this.snapshotMetric != null) {
                AbstractBoostSnapshotStrategy.this.snapshotMetric.addSnapshotSstFileCount();
            }
        }

        /**
         * addSnapshotSstIncrementalSize
         *
         * @param size size
         */
        protected void addSnapshotSstIncrementalSize(long size) {
            if (AbstractBoostSnapshotStrategy.this.snapshotMetric != null) {
                AbstractBoostSnapshotStrategy.this.snapshotMetric.addSnapshotSstIncrementalSize(size);
            }
        }

        /**
         * addSnapshotSstFileSize
         *
         * @param size size
         */
        protected void addSnapshotSstFileSize(long size) {
            if (AbstractBoostSnapshotStrategy.this.snapshotMetric != null) {
                AbstractBoostSnapshotStrategy.this.snapshotMetric.addSnapshotSstFileSize(size);
            }
        }
    }

    /**
     * Previous snapshot with uploaded sst files.
     */
    protected static abstract class AbstractPreviousSnapshot {
        /**
         * 获取之前上传过的文件
         */
        protected abstract Optional<StreamStateHandle> getUploaded(String filename);
    }

    /**
     * 封装syncPrepareResources时返回的source对象，包含所有需要进行快照的文件的副本
     */
    protected static class NativeBoostSnapshotResources implements SnapshotResources {
        /**
         * keyedStateDescriptors
         */
        @Nonnull
        protected final List<KeyedStateDescriptor> keyedStateDescriptors;

        /**
         * nsKeyedStateDescriptors
         */
        @Nonnull
        protected final List<NSKeyedStateDescriptor> nsKeyedStateDescriptors;

        /**
         * registeredPQStates
         */
        @Nonnull
        protected final Map<String, HeapPriorityQueueSnapshotRestoreWrapper<?>> registeredPQStates;

        /**
         * pqStateSnapshots
         */
        @Nonnull
        protected final Map<String, HeapPriorityQueueStateSnapshot<?>> pqStateSnapshots;

        /**
         * snapshotDirectory
         */
        @Nonnull
        protected final SnapshotDirectory snapshotDirectory;

        /**
         * previousSnapshot
         */
        @Nonnull
        protected final AbstractPreviousSnapshot previousSnapshot;

        /**
         * stateMetaInfoSnapshots
         */
        @Nonnull
        protected final List<StateMetaInfoSnapshot> stateMetaInfoSnapshots;

        /**
         * checkpoint
         */
        @Nonnull
        protected final Checkpoint checkpoint;

        public NativeBoostSnapshotResources(SnapshotDirectory snapshotDirectory,
            AbstractPreviousSnapshot previousSnapshot,
            List<StateMetaInfoSnapshot> stateMetaInfoSnapshots, List<KeyedStateDescriptor> keyedStateDescriptors,
            List<NSKeyedStateDescriptor> nsKeyedStateDescriptors,
            Map<String, HeapPriorityQueueSnapshotRestoreWrapper<?>> registeredPQStates,
            Map<String, HeapPriorityQueueStateSnapshot<?>> pqStateSnapshots, Checkpoint checkpoint) {
            this.previousSnapshot = previousSnapshot;
            this.snapshotDirectory = snapshotDirectory;
            this.stateMetaInfoSnapshots = stateMetaInfoSnapshots;
            this.keyedStateDescriptors = keyedStateDescriptors;
            this.nsKeyedStateDescriptors = nsKeyedStateDescriptors;
            this.registeredPQStates = registeredPQStates;
            this.pqStateSnapshots = pqStateSnapshots;
            this.checkpoint = checkpoint;
        }

        /**
         * 清理下层做checkpoint的目录
         */
        @Override
        public void release() {
            try {
                if (snapshotDirectory.exists()) {
                    LOG.trace("Running cleanup for local BoostStateDB backup directory {}.", snapshotDirectory);
                }
                boolean cleanupDone = snapshotDirectory.cleanup();
                if (!cleanupDone) {
                    LOG.debug("Failed to properly cleanup local BoostStateDB backup directory.");
                }
            } catch (IOException e) {
                LOG.warn("Failed to properly cleanup local BoostStateDB backup directory.", e);
            }
        }
    }
}