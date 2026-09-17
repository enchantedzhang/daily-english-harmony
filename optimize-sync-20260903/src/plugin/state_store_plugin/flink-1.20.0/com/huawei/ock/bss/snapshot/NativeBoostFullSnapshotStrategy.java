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
import com.huawei.ock.bss.common.exception.BSSRuntimeException;
import com.huawei.ock.bss.state.internal.descriptor.keyed.KeyedStateDescriptor;
import com.huawei.ock.bss.state.internal.descriptor.nskeyed.NSKeyedStateDescriptor;

import org.apache.flink.api.common.typeutils.TypeSerializer;
import org.apache.flink.core.fs.CloseableRegistry;
import org.apache.flink.runtime.checkpoint.CheckpointOptions;
import org.apache.flink.runtime.state.CheckpointStreamFactory;
import org.apache.flink.runtime.state.CheckpointedStateScope;
import org.apache.flink.runtime.state.IncrementalKeyedStateHandle.HandleAndLocalPath;
import org.apache.flink.runtime.state.IncrementalRemoteKeyedStateHandle;
import org.apache.flink.runtime.state.KeyGroupRange;
import org.apache.flink.runtime.state.KeyedStateHandle;
import org.apache.flink.runtime.state.LocalRecoveryConfig;
import org.apache.flink.runtime.state.RegisteredStateMetaInfoBase;
import org.apache.flink.runtime.state.SnapshotResult;
import org.apache.flink.runtime.state.StreamStateHandle;
import org.apache.flink.runtime.state.heap.HeapPriorityQueueSnapshotRestoreWrapper;
import org.apache.flink.runtime.state.heap.HeapPriorityQueueStateSnapshot;
import org.apache.flink.runtime.state.metainfo.StateMetaInfoSnapshot;
import org.apache.flink.util.Preconditions;
import org.apache.flink.util.ResourceGuard;
import org.apache.flink.util.function.SupplierWithException;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import java.io.File;
import java.nio.file.Path;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.List;
import java.util.Map;
import java.util.Optional;
import java.util.UUID;

import javax.annotation.Nonnull;

/**
 * BSS全量快照
 *
 * @param <K> key
 * @since BeiMing 25.0.T1
 */
public class NativeBoostFullSnapshotStrategy<K> extends BoostSnapshotStrategyBase<K> {
    private static final Logger LOG = LoggerFactory.getLogger(NativeBoostFullSnapshotStrategy.class);

    private static final String DESCRIPTION = "Asynchronous full BoostStateStore snapshot";

    private final BoostStateUploader stateUploader;

    private final String jobID;

    public NativeBoostFullSnapshotStrategy(
        @Nonnull String jobID, @Nonnull BoostStateDB db, @Nonnull ResourceGuard resourceGuard,
        @Nonnull TypeSerializer<K> keySerializer, @Nonnull OckDBKeyedStateBackend.BoostKvStateInfo kvStateInfo,
        @Nonnull KeyGroupRange keyGroupRange, int keyGroupPrefixBytes, @Nonnull LocalRecoveryConfig localRecoveryConfig,
        @Nonnull File instanceBasePath, @Nonnull UUID backendUID, @Nonnull BoostStateUploader stateUploader) {
        super(DESCRIPTION, db, resourceGuard, keySerializer, kvStateInfo, keyGroupRange, keyGroupPrefixBytes,
            localRecoveryConfig, instanceBasePath, backendUID);
        this.jobID = jobID;
        this.stateUploader = stateUploader;
    }

    /**
     * 关闭上传state文件的上传器
     *
     * @throws Exception exception
     */
    @Override
    public void close() {
        stateUploader.close();
    }

    /**
     * 通知已完成Checkpoint
     *
     * @param checkpointId 已完成的检查点的ID
     */
    @Override
    public void notifyCheckpointComplete(long checkpointId) {
        if (checkpointId < 0) {
            LOG.error("checkpointId is negative: {}", checkpointId);
            throw new BSSRuntimeException("Invalid param checkpointId: " + checkpointId);
        }
        db.notifyDBSnapshotCompleted(checkpointId);
    }

    /**
     * 通知Checkpoint被中止
     *
     * @param checkpointId 已中止的检查点的ID
     */
    @Override
    public void notifyCheckpointAborted(long checkpointId) {
        if (checkpointId < 0) {
            LOG.error("checkpointId is negative: {}", checkpointId);
            return;
        }
        db.notifyCheckpointAborted(checkpointId);
    }

    @Override
    protected PreviousSnapshot snapshotMetaData(long checkpointID,
        @Nonnull final List<StateMetaInfoSnapshot> stateMetaInfoSnapshots,
        @Nonnull final List<KeyedStateDescriptor> keyedStateDescriptors,
        @Nonnull final List<NSKeyedStateDescriptor> nsKeyedStateDescriptors,
        final Map<String, HeapPriorityQueueSnapshotRestoreWrapper<?>> registeredPQStates,
        final Map<String, HeapPriorityQueueStateSnapshot<?>> pqStateSnapshots) {
        for (Map.Entry<String, RegisteredStateMetaInfoBase> stateMetaInfoEntry
            : this.metaInfoMap.entrySet()) {
            stateMetaInfoSnapshots.add(stateMetaInfoEntry.getValue().snapshot());
        }

        for (Map.Entry<String, KeyedStateDescriptor> entry : this.keyedStateDescriptorMap.entrySet()) {
            KeyedStateDescriptor descriptor = entry.getValue().duplicate();
            if (descriptor == null) {
                throw new BSSRuntimeException("descriptor is null");
            }
            keyedStateDescriptors.add(descriptor);
        }

        for (Map.Entry<String, NSKeyedStateDescriptor> entry : this.nsKeyedStateDescriptorMap.entrySet()) {
            NSKeyedStateDescriptor descriptor = entry.getValue().duplicate();
            if (descriptor == null) {
                throw new BSSRuntimeException("descriptor is null");
            }
            nsKeyedStateDescriptors.add(descriptor);
        }

        for (Map.Entry<String, HeapPriorityQueueSnapshotRestoreWrapper<?>> entry : registeredPQStates.entrySet()) {
            stateMetaInfoSnapshots.add(entry.getValue().getMetaInfo().snapshot());
            pqStateSnapshots.put(entry.getKey(), entry.getValue().stateSnapshot());
        }
        return EMPTY_PREVIOUS_SNAPSHOT;
    }

    /**
     * 将快照写入由给定流 {@link CheckpointStreamFactory} 并返回一个 {@link SupplierWithException}，这个给快照提供一个状态句柄
     *
     * @param snapshotResources snapshotResources
     * @param checkpointId checkpointId
     * @param timestamp checkpoint时间戳
     * @param checkpointStreamFactory checkpointStreamFactory
     * @param checkpointOptions checkpoint配置项
     * @return 提供{@link StateObject}的supplier.
     */
    @Override
    public SnapshotResultSupplier<KeyedStateHandle> asyncSnapshot(NativeBoostSnapshotResources snapshotResources,
        long checkpointId, long timestamp, @Nonnull CheckpointStreamFactory checkpointStreamFactory,
        @Nonnull CheckpointOptions checkpointOptions) {
        if (snapshotResources.stateMetaInfoSnapshots.isEmpty()) {
            LOG.debug("Asynchronous BoostStateDB snapshot performed on empty keyedState at {}.", timestamp);
            return registry -> SnapshotResult.empty();
        }
        return new BoostNativeFullSnapshotOperation(jobID, checkpointId, checkpointStreamFactory, snapshotResources);
    }

    private final class BoostNativeFullSnapshotOperation extends BoostDBSnapshotOperation {
        private final String jobID;

        private BoostNativeFullSnapshotOperation(
            String jobID, long checkpointId,
            @Nonnull CheckpointStreamFactory checkpointStreamFactory,
            @Nonnull NativeBoostSnapshotResources snapshotResources) {
            super(checkpointId, checkpointStreamFactory, snapshotResources);
            this.jobID = jobID;
        }

        /**
         * 实际执行snapshot的操作
         *
         * @param closeableRegistry 注册可以在快照过程中阻塞I/O的流。
         * 从AsyncSnapshotCallable.taskCancelCloseableRegistry转发关闭。
         * @return 向jobmanager/本地状态管理器报告的快照
         * @throws Exception 快照过程中可能的异常
         */
        @Override
        public SnapshotResult<KeyedStateHandle> get(CloseableRegistry closeableRegistry) throws Exception {
            boolean completed = false;
            // metaData的句柄
            SnapshotResult<StreamStateHandle> metaStateHandle = null;
            // 存储本次checkpoint所有的文件信息
            final List<HandleAndLocalPath> privateFiles = new ArrayList<>();

            try {
                // 将元数据写入一个checkpoint专用流
                metaStateHandle = materializeMetaData(jobID, closeableRegistry, tmpResourcesRegistry, snapshotResources,
                    checkpointStreamFactory, checkpointId);
                // Sanity checks - they should never fail
                Preconditions.checkNotNull(metaStateHandle, "Metadata was not properly created.");
                Preconditions.checkNotNull(metaStateHandle.getJobManagerOwnedSnapshot(),
                    "Metadata for job manager was not properly created.");

                // 通知下层执行异步快照
                checkpoint.createCheckpoint(false);

                long checkpointedSize = metaStateHandle.getStateSize();
                // 将上面的流中的数据传输给flink
                checkpointedSize += uploadSnapshotFiles(privateFiles, closeableRegistry, tmpResourcesRegistry);
                final IncrementalRemoteKeyedStateHandle jmIncrementalKeyedStateHandle
                    = new IncrementalRemoteKeyedStateHandle(backendUID, keyGroupRange, checkpointId,
                    Collections.emptyList(), privateFiles, metaStateHandle.getJobManagerOwnedSnapshot(),
                    checkpointedSize);

                Optional<KeyedStateHandle> localSnapshot = getLocalSnapshot(metaStateHandle.getTaskLocalSnapshot(),
                    Collections.emptyList());

                final SnapshotResult<KeyedStateHandle> snapshotResult = localSnapshot.map(
                        keyedStateHandle -> SnapshotResult.withLocalState(jmIncrementalKeyedStateHandle,
                            keyedStateHandle))
                    .orElseGet(() -> SnapshotResult.of(jmIncrementalKeyedStateHandle));

                completed = true;

                return snapshotResult;
            } finally {
                if (!completed) {
                    cleanupIncompleteSnapshot(tmpResourcesRegistry, localBackupDirectory);
                }
            }
        }

        private long uploadSnapshotFiles(@Nonnull List<HandleAndLocalPath> privateFiles,
            @Nonnull CloseableRegistry snapshotCloseableRegistry, @Nonnull CloseableRegistry tmpResourcesRegistry)
            throws Exception {
            // write state data
            Preconditions.checkState(localBackupDirectory.exists());

            Path[] files = localBackupDirectory.listDirectory();
            long uploadedSize = 0L;
            if (files == null) {
                return uploadedSize;
            }
            List<HandleAndLocalPath> uploadedFiles =
                stateUploader.uploadFilesToCheckpointFs(Arrays.asList(files), checkpointStreamFactory,
                    CheckpointedStateScope.EXCLUSIVE, snapshotCloseableRegistry, tmpResourcesRegistry);
            uploadedSize += uploadedFiles.stream().mapToLong(HandleAndLocalPath::getStateSize).sum();
            privateFiles.addAll(uploadedFiles);
            return uploadedSize;
        }
    }
}
