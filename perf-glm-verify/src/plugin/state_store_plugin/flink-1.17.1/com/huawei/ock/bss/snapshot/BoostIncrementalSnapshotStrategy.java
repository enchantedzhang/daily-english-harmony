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
import org.apache.flink.runtime.checkpoint.CheckpointType;
import org.apache.flink.runtime.checkpoint.SnapshotType;
import org.apache.flink.runtime.state.CheckpointStreamFactory;
import org.apache.flink.runtime.state.CheckpointedStateScope;
import org.apache.flink.runtime.state.IncrementalRemoteKeyedStateHandle;
import org.apache.flink.runtime.state.KeyGroupRange;
import org.apache.flink.runtime.state.KeyedStateHandle;
import org.apache.flink.runtime.state.RegisteredStateMetaInfoBase;
import org.apache.flink.runtime.state.StateHandleID;
import org.apache.flink.runtime.state.StateObject;
import org.apache.flink.runtime.state.LocalRecoveryConfig;
import org.apache.flink.runtime.state.SnapshotResult;
import org.apache.flink.runtime.state.StreamStateHandle;
import org.apache.flink.runtime.state.heap.HeapPriorityQueueSnapshotRestoreWrapper;
import org.apache.flink.runtime.state.heap.HeapPriorityQueueStateSnapshot;
import org.apache.flink.runtime.state.metainfo.StateMetaInfoSnapshot;
import org.apache.flink.util.ResourceGuard;
import org.apache.flink.util.function.SupplierWithException;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import java.io.File;
import java.io.IOException;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import java.util.Map;
import java.util.HashMap;
import java.util.Optional;
import java.util.SortedMap;
import java.util.TreeMap;
import java.util.UUID;

import javax.annotation.Nonnull;

/**
 * BSS增量快照
 *
 * @param <K> key
 * @since BeiMing 25.0.T1
 */
public class BoostIncrementalSnapshotStrategy<K> extends BoostSnapshotStrategyBase<K> {
    private static final Logger LOG = LoggerFactory.getLogger(BoostIncrementalSnapshotStrategy.class);

    private static final String DESCRIPTION = "Asynchronous incremental BoostStateStore snapshot";

    @Nonnull
    private final SortedMap<Long, Map<StateHandleID, StreamStateHandle>> uploadedSstFiles;

    private final BoostStateUploader stateUploader;

    private long lastCompletedCheckpointId;

    private String jobID;

    public BoostIncrementalSnapshotStrategy(
        @Nonnull String jobID,
        @Nonnull BoostStateDB db,
        @Nonnull ResourceGuard resourceGuard,
        @Nonnull TypeSerializer<K> keySerializer,
        @Nonnull OckDBKeyedStateBackend.BoostKvStateInfo kvStateInfo,
        @Nonnull KeyGroupRange keyGroupRange,
        int keyGroupPrefixBytes, @Nonnull LocalRecoveryConfig localRecoveryConfig,
        @Nonnull File instanceBasePath, @Nonnull UUID backendUID,
        @Nonnull SortedMap<Long, Map<StateHandleID, StreamStateHandle>> uploadedStateHandles,
        @Nonnull BoostStateUploader stateUploader, long lastCompletedCheckpointId) {
        super(DESCRIPTION, db, resourceGuard, keySerializer, kvStateInfo, keyGroupRange, keyGroupPrefixBytes,
            localRecoveryConfig, instanceBasePath, backendUID);
        this.jobID = jobID;
        this.stateUploader = stateUploader;
        this.lastCompletedCheckpointId = lastCompletedCheckpointId;
        this.uploadedSstFiles = new TreeMap<>(uploadedStateHandles);
    }

    /**
     * 关闭上传state文件的上传器
     */
    @Override
    public void close() {
        stateUploader.close();
    }

    /**
     * 通知已完成Checkpoint
     *
     * @param completedCheckpointId 已完成的检查点的ID
     * @throws Exception This method can propagate exceptions, which leads to a failure/recovery for the task or job
     */
    @Override
    public void notifyCheckpointComplete(long completedCheckpointId) throws Exception {
        if (completedCheckpointId < 0) {
            LOG.error("completedCheckpointId is negative: {}", completedCheckpointId);
            throw new BSSRuntimeException("Invalid param checkpointId: " + completedCheckpointId);
        }
        synchronized (uploadedSstFiles) {
            if (completedCheckpointId > lastCompletedCheckpointId && uploadedSstFiles.containsKey(
                completedCheckpointId)) {
                uploadedSstFiles.keySet().removeIf(checkpointId -> checkpointId < completedCheckpointId);
                lastCompletedCheckpointId = completedCheckpointId;
            }
        }
        db.notifyDBSnapshotCompleted(completedCheckpointId);
    }

    /**
     * 通知Checkpoint被中止
     *
     * @param abortedCheckpointId 已中止的检查点的ID
     * @throws Exception This method can propagate exceptions, which leads to a failure/recovery for the task or job.
     */
    @Override
    public void notifyCheckpointAborted(long abortedCheckpointId) throws Exception {
        if (abortedCheckpointId < 0) {
            LOG.error("abortedCheckpointId is negative: {}", abortedCheckpointId);
            return;
        }
        synchronized (uploadedSstFiles) {
            uploadedSstFiles.keySet().remove(abortedCheckpointId);
        }
        db.notifyCheckpointAborted(abortedCheckpointId);
    }

    /**
     * 将快照写入由给定流 {@link CheckpointStreamFactory} 并返回一个 {@link SupplierWithException}，这个给快照提供一个状态句柄
     *
     * @param snapshotResources       完成快照需要的资源
     * @param checkpointId            checkpointId
     * @param timestamp               checkpoint时间戳
     * @param checkpointStreamFactory 将state写入此流
     * @param checkpointOptions       checkpoint配置项
     * @return 提供{@link StateObject}的supplier.
     */
    @Override
    public SnapshotResultSupplier<KeyedStateHandle> asyncSnapshot(NativeBoostSnapshotResources snapshotResources,
        long checkpointId, long timestamp, @Nonnull CheckpointStreamFactory checkpointStreamFactory,
        @Nonnull CheckpointOptions checkpointOptions) {
        if (snapshotResources == null) {
            LOG.error(
                "Asynchronous BoostStateDB snapshot performed on null NativeBoostSnapshotResources. Returning null.");
            return registry -> SnapshotResult.empty();
        }
        if (snapshotResources.stateMetaInfoSnapshots.isEmpty()) {
            LOG.debug("Asynchronous BoostStateDB snapshot performed on empty keyedState at {}. Returning null.",
                timestamp);
            return registry -> SnapshotResult.empty();
        }
        final AbstractPreviousSnapshot previousSnapshot;
        final CheckpointType.SharingFilesStrategy sharingFilesStrategy = checkpointOptions.getCheckpointType()
            .getSharingFilesStrategy();
        switch (sharingFilesStrategy) {
            case FORWARD:
            case NO_SHARING:
                previousSnapshot = EMPTY_PREVIOUS_SNAPSHOT;
                break;
            case FORWARD_BACKWARD:
                previousSnapshot = snapshotResources.previousSnapshot;
                break;
            default:
                throw new BSSRuntimeException("Unsupported sharing files strategy: " + sharingFilesStrategy);
        }
        return new BoostIncrementalSnapshotOperation(jobID, checkpointId, checkpointStreamFactory, snapshotResources,
            previousSnapshot, sharingFilesStrategy);
    }

    @Override
    protected PreviousSnapshot snapshotMetaData(long checkpointID,
        @Nonnull final List<StateMetaInfoSnapshot> stateMetaInfoSnapshots,
        @Nonnull final List<KeyedStateDescriptor> keyedStateDescriptors,
        @Nonnull final List<NSKeyedStateDescriptor> nsKeyedStateDescriptors,
        final Map<String, HeapPriorityQueueSnapshotRestoreWrapper<?>> registeredPQStates,
        final Map<String, HeapPriorityQueueStateSnapshot<?>> pqStateSnapshots) {
        final long lastCompletedCheckpoint;
        final Map<StateHandleID, StreamStateHandle> confirmedSstFiles;

        synchronized (this.uploadedSstFiles) {
            lastCompletedCheckpoint = this.lastCompletedCheckpointId;
            confirmedSstFiles = uploadedSstFiles.get(lastCompletedCheckpoint);
            LOG.trace("Use confirmed SST files for checkpoint {}: {}", checkpointID, confirmedSstFiles);
        }
        LOG.trace("Taking incremental snapshot for checkpoint {}. "
                + "This snapshot is based on last completed checkpoint {} "
                + "assuming the following (shared) files as base: {}.", checkpointID, lastCompletedCheckpoint,
            confirmedSstFiles);
        for (Map.Entry<String, RegisteredStateMetaInfoBase> stateMetaInfoEntry
            : this.metaInfoMap.entrySet()) {
            LOG.debug("RegisteredKeyValueStateBackendMetaInfo snapshot, name = {}", stateMetaInfoEntry.getKey());
            stateMetaInfoSnapshots.add(stateMetaInfoEntry.getValue().snapshot());
        }
        for (Map.Entry<String, KeyedStateDescriptor> entry : this.keyedStateDescriptorMap.entrySet()) {
            KeyedStateDescriptor descriptor = entry.getValue().duplicate();
            if (descriptor == null) {
                throw new BSSRuntimeException("descriptor: " + null);
            }
            keyedStateDescriptors.add(descriptor);
        }
        for (Map.Entry<String, NSKeyedStateDescriptor> entry : this.nsKeyedStateDescriptorMap.entrySet()) {
            NSKeyedStateDescriptor descriptor = entry.getValue().duplicate();
            if (descriptor == null) {
                throw new BSSRuntimeException("descriptor: " + null);
            }
            nsKeyedStateDescriptors.add(descriptor);
        }
        for (Map.Entry<String, HeapPriorityQueueSnapshotRestoreWrapper<?>> entry : registeredPQStates.entrySet()) {
            stateMetaInfoSnapshots.add(entry.getValue().getMetaInfo().snapshot());
            LOG.debug("registeredPQStates snapshot, name = {}", entry.getKey());
            pqStateSnapshots.put(entry.getKey(), entry.getValue().stateSnapshot());
        }
        return new PreviousSnapshot(confirmedSstFiles);
    }

    /**
     * 封装了对OckDBKeyedStateBackend进行增量快照的过程。
     */
    private final class BoostIncrementalSnapshotOperation extends BoostDBSnapshotOperation {
        static final String SST_FILE_SUFFIX = ".sst";

        static final String SLICE_FILE_SUFFIX = ".slice";

        @Nonnull
        SnapshotType.SharingFilesStrategy sharingFilesStrategy;

        @Nonnull
        private final AbstractPreviousSnapshot previousSnapshot;

        @Nonnull
        private String jobID;

        private BoostIncrementalSnapshotOperation(String jobID, long checkpointId,
            @Nonnull CheckpointStreamFactory checkpointStreamFactory,
            @Nonnull NativeBoostSnapshotResources snapshotResources, @Nonnull AbstractPreviousSnapshot previousSnapshot,
            @Nonnull SnapshotType.SharingFilesStrategy sharingFilesStrategy) {
            super(checkpointId, checkpointStreamFactory, snapshotResources);
            this.jobID = jobID;
            this.previousSnapshot = previousSnapshot;
            this.sharingFilesStrategy = sharingFilesStrategy;
        }

        /**
         * 实际执行snapshot的操作
         *
         * @param closeableRegistry 注册可以在快照过程中阻塞I/O的流
         * @return 向jobmanager/本地状态管理器报告的快照
         * @throws Exception 快照过程中可能的异常，带详细分析
         */
        @Override
        public SnapshotResult<KeyedStateHandle> get(CloseableRegistry closeableRegistry) throws Exception {
            LOG.debug("Async checkpoint start, checkpointId:{}.", checkpointId);
            boolean completed = false;

            // metaData的句柄
            SnapshotResult<StreamStateHandle> metaStateHandle = null;
            // 存储上次checkpoint之后新增的sst文件句柄
            final Map<StateHandleID, StreamStateHandle> sstFiles = new HashMap<>();
            // Handles to the misc files in the current snapshot will go here
            final Map<StateHandleID, StreamStateHandle> miscFiles = new HashMap<>();
            try {
                // 将元数据写入一个checkpoint专用流
                metaStateHandle = materializeMetaData(jobID, closeableRegistry, tmpResourcesRegistry, snapshotResources,
                    checkpointStreamFactory, checkpointId);
                if (metaStateHandle == null) {
                    throw new BSSRuntimeException("Failed to create metadata.");
                }
                if (metaStateHandle.getJobManagerOwnedSnapshot() == null) {
                    throw new BSSRuntimeException("Failed to create metadata for jobManager.");
                }

                // 通知下层执行异步快照
                checkpoint.createCheckpoint(true);

                long checkpointedSize = metaStateHandle.getStateSize();
                uploadStartTime = System.currentTimeMillis();
                // 将上面的流中的数据传输给flink
                checkpointedSize += uploadSnapshotFiles(sstFiles, miscFiles, closeableRegistry, tmpResourcesRegistry);
                if (snapshotMetric != null) {
                    snapshotMetric.setUploadTime(System.currentTimeMillis() - uploadStartTime);
                }
                final IncrementalRemoteKeyedStateHandle jmIncrementalKeyedStateHandle
                    = new IncrementalRemoteKeyedStateHandle(backendUID, keyGroupRange, checkpointId, sstFiles,
                    miscFiles, metaStateHandle.getJobManagerOwnedSnapshot(), checkpointedSize);
                Optional<KeyedStateHandle> localSnapshot = getLocalSnapshot(metaStateHandle.getTaskLocalSnapshot(),
                    sstFiles);
                final SnapshotResult<KeyedStateHandle> snapshotResult = localSnapshot.map(
                        keyedStateHandle -> SnapshotResult.withLocalState(jmIncrementalKeyedStateHandle,
                            keyedStateHandle))
                    .orElseGet(() -> SnapshotResult.of(jmIncrementalKeyedStateHandle));

                completed = true;
                if (snapshotMetric != null) {
                    snapshotMetric.setTotalTime(System.currentTimeMillis() - snapshotStartTime);
                }
                return snapshotResult;
            } finally {
                if (!completed) {
                    cleanupIncompleteSnapshot(tmpResourcesRegistry, localBackupDirectory);
                }
                LOG.debug("Async checkpoint success, checkpointId:{}.", checkpointId);
            }
        }

        private long uploadSnapshotFiles(@Nonnull Map<StateHandleID, StreamStateHandle> sstFiles,
            @Nonnull Map<StateHandleID, StreamStateHandle> miscFiles,
            @Nonnull CloseableRegistry snapshotCloseableRegistry, @Nonnull CloseableRegistry tmpResourcesRegistry)
            throws Exception {
            if (!localBackupDirectory.exists()) {
                throw new BSSRuntimeException("Failed to get localBackupDirectory, does not exist.");
            }

            Path[] localDirList = localBackupDirectory.listDirectory();
            long uploadedSize = 0L;
            if (localDirList == null) {
                return uploadedSize;
            }
            List<Path> sstFilePaths = new ArrayList<>(localDirList.length);
            List<Path> miscFilePaths = new ArrayList<>(localDirList.length);
            createUploadFilePaths(localDirList, sstFiles, sstFilePaths, miscFilePaths);
            if (snapshotMetric != null) {
                snapshotMetric.setSnapshotFileCount(sstFiles.size() + sstFilePaths.size() + miscFilePaths.size());
            }

            final CheckpointedStateScope scope = sharingFilesStrategy == SnapshotType.SharingFilesStrategy.NO_SHARING
                ? CheckpointedStateScope.EXCLUSIVE
                : CheckpointedStateScope.SHARED;

            Map<StateHandleID, StreamStateHandle> sstFilesUploadResult = stateUploader.uploadFilesToCheckpointFs(
                sstFilePaths, checkpointStreamFactory, scope, snapshotCloseableRegistry, tmpResourcesRegistry);
            uploadedSize += sstFilesUploadResult.values().stream().mapToLong(StateObject::getStateSize).sum();
            sstFiles.putAll(sstFilesUploadResult);

            Map<StateHandleID, StreamStateHandle> miscFilesUploadResult = stateUploader.uploadFilesToCheckpointFs(
                miscFilePaths, checkpointStreamFactory, scope, snapshotCloseableRegistry, tmpResourcesRegistry);
            uploadedSize += miscFilesUploadResult.values().stream().mapToLong(StateObject::getStateSize).sum();
            miscFiles.putAll(miscFilesUploadResult);

            synchronized (uploadedSstFiles) {
                switch (sharingFilesStrategy) {
                    case NO_SHARING:
                        break;
                    case FORWARD_BACKWARD:
                    case FORWARD:
                        uploadedSstFiles.put(checkpointId, Collections.unmodifiableMap(sstFiles));
                        break;
                    default:
                        throw new BSSRuntimeException(
                            "Unsupported sharing files strategy: " + sharingFilesStrategy);
                }
            }
            LOG.info("checkpoint uploadSnapshotFiles success");
            return uploadedSize;
        }

        private void createUploadFilePaths(Path[] files, Map<StateHandleID, StreamStateHandle> sstFiles,
           List<Path> sstFilePaths, List<Path> miscFilePaths) {
            if (snapshotMetric != null) {
                snapshotMetric.clearPreviousMetric();
            }
            for (Path filePath : files) {
                if (filePath == null) {
                    LOG.error("Got null filePath from localDirList while createUploadFilePaths.");
                    continue;
                }
                handleSingleFile(sstFiles, sstFilePaths, miscFilePaths, filePath);
            }
        }

        private void handleSingleFile(Map<StateHandleID, StreamStateHandle> sstFiles, List<Path> sstFilePaths,
            List<Path> miscFilePaths, Path filePath) {
            final String fileName = filePath.getFileName().toString();
            if (!fileName.endsWith(SST_FILE_SUFFIX) && !fileName.endsWith(SLICE_FILE_SUFFIX)) {
                try {
                    long fileSize = Files.size(filePath);
                    if (fileSize <= 0) {
                        LOG.warn("file size <= 0, file path: {}", fileName);
                        return;
                    }
                    addSnapshotFileSize(fileSize);
                } catch (IOException e) {
                    LOG.warn("read file failed: " + e.getMessage());
                }
                miscFilePaths.add(filePath);
                return;
            }

            // sst 和 slice文件都是采用的增量快照，这里是获取上一个snapshot已经上传过的快照文件
            Optional<StreamStateHandle> uploaded = previousSnapshot.getUploaded(fileName);
            if (uploaded.isPresent()) {
                long size = uploaded.get().getStateSize();
                addSnapshotFileSize(size);
                if (fileName.endsWith(SLICE_FILE_SUFFIX)) {
                    addSnapshotSliceFileCount();
                    addSnapshotSliceFileSize(size);
                } else {
                    addSnapshotSstFileCount();
                    addSnapshotSstFileSize(size);
                }
                sstFiles.put(new StateHandleID(fileName), uploaded.get());
            } else {
                try {
                    long fileSize = Files.size(filePath);
                    if (fileSize <= 0) {
                        LOG.warn("file size <= 0, file path: {}", fileName);
                        return;
                    }
                    addSnapshotFileSize(fileSize);
                    addSnapshotIncrementalSize(fileSize);
                    if (fileName.endsWith(SLICE_FILE_SUFFIX)) {
                        addSnapshotSliceFileCount();
                        addSnapshotSliceIncrementalSize(fileSize);
                        addSnapshotSliceFileSize(fileSize);
                    } else {
                        addSnapshotSstFileCount();
                        addSnapshotSstIncrementalSize(fileSize);
                        addSnapshotSstFileSize(fileSize);
                    }
                } catch (IOException e) {
                    LOG.warn("read file failed: " + e.getMessage());
                }
                sstFilePaths.add(filePath);
            }
        }
    }
}
