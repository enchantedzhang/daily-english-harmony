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

package com.huawei.ock.bss.restore;

import static org.apache.flink.runtime.state.StateUtil.unexpectedStateHandleException;

import com.huawei.ock.bss.common.BoostStateDB;
import com.huawei.ock.bss.common.exception.BSSRuntimeException;
import com.huawei.ock.bss.state.internal.descriptor.keyed.KeyedStateDescriptor;
import com.huawei.ock.bss.state.internal.descriptor.nskeyed.NSKeyedStateDescriptor;
import com.huawei.ock.bss.table.api.Table;
import com.huawei.ock.bss.table.api.TableDescription;
import com.huawei.ock.bss.util.DescriptionUtils;
import com.huawei.ock.bss.util.KeyedStateBackendUtils;

import org.apache.flink.api.common.typeutils.TypeSerializer;
import org.apache.flink.api.common.typeutils.TypeSerializerSchemaCompatibility;
import org.apache.flink.core.fs.CloseableRegistry;
import org.apache.flink.core.fs.FSDataInputStream;
import org.apache.flink.core.memory.DataInputView;
import org.apache.flink.core.memory.DataInputViewStreamWrapper;
import org.apache.flink.runtime.state.IncrementalKeyedStateHandle;
import org.apache.flink.runtime.state.IncrementalLocalKeyedStateHandle;
import org.apache.flink.runtime.state.IncrementalRemoteKeyedStateHandle;
import org.apache.flink.runtime.state.KeyExtractorFunction;
import org.apache.flink.runtime.state.KeyGroupRange;
import org.apache.flink.runtime.state.KeyedBackendSerializationProxy;
import org.apache.flink.runtime.state.KeyedStateHandle;
import org.apache.flink.runtime.state.PriorityComparable;
import org.apache.flink.runtime.state.PriorityQueueSetFactory;
import org.apache.flink.runtime.state.RegisteredKeyValueStateBackendMetaInfo;
import org.apache.flink.runtime.state.RegisteredPriorityQueueStateBackendMetaInfo;
import org.apache.flink.runtime.state.RegisteredStateMetaInfoBase;
import org.apache.flink.runtime.state.StateSerializerProvider;
import org.apache.flink.runtime.state.StateSnapshotKeyGroupReader;
import org.apache.flink.runtime.state.heap.HeapPriorityQueueElement;
import org.apache.flink.runtime.state.heap.HeapPriorityQueueSet;
import org.apache.flink.runtime.state.heap.HeapPriorityQueueSetFactory;
import org.apache.flink.runtime.state.heap.HeapPriorityQueueSnapshotRestoreWrapper;
import org.apache.flink.runtime.state.metainfo.StateMetaInfoSnapshot;
import org.apache.flink.util.FileUtils;
import org.apache.flink.util.IOUtils;
import org.apache.flink.util.InstantiationUtil;
import org.apache.flink.util.Preconditions;
import org.apache.flink.util.StateMigrationException;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import java.io.File;
import java.io.IOException;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.util.ArrayList;
import java.util.Collection;
import java.util.Collections;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.UUID;
import java.util.stream.Stream;

import javax.annotation.Nonnull;

/**
 * 增量恢复
 *
 * @param <K> key
 * @since BeiMing 25.0.T1
 */
public abstract class AbstractBoostIncrementalRestoreOperation<K> extends AbstractBoostRestoreOperation<K> {
    private static final Logger LOG = LoggerFactory.getLogger(AbstractBoostIncrementalRestoreOperation.class);

    private final String jobID;

    private final String operatorIdentifier;

    private final File instanceBasePath;

    private final Path tmpPathToRestore;

    private final ClassLoader userCodeClassLoader;

    private final PriorityQueueSetFactory priorityQueueSetFactory;

    private final StateSerializerProvider<K> keySerializerProvider;

    private final Map<String, Table> tables;

    private final Map<String, KeyedStateDescriptor> keyedStateDescriptorMap;

    private final Map<String, NSKeyedStateDescriptor> nsKeyedStateDescriptorMap;

    private final Map<String, HeapPriorityQueueSnapshotRestoreWrapper<?>> registeredPQStates;

    private final Map<String, RegisteredStateMetaInfoBase> registeredKvStateMetaInfos;

    private boolean isKeySerializerCompatibilityChecked = false;

    /**
     * 下载文件开始时间
     */
    protected long downloadStartTime = 0L;

    /**
     * 下载文件结束时间
     */
    protected long downloadEndTime = 0L;

    protected final int numberOfTransferringThreads;

    protected final BoostStateDB db;

    protected final KeyGroupRange keyGroupRange;

    protected final CloseableRegistry cancelStreamRegistry;

    protected final Collection<KeyedStateHandle> restoreStateHandles;

    protected long lastCompletedCheckpointId;

    protected UUID backendID;

    protected List<String> remotePaths;

    protected List<String> localPaths;

    public AbstractBoostIncrementalRestoreOperation(String jobID, String operatorIdentifier,
        KeyGroupRange keyGroupRange,
        BoostStateDB db, int keyGroupPrefixBytes, int numberOfTransferringThreads,
        CloseableRegistry cancelStreamRegistry,
        ClassLoader userCodeClassLoader, PriorityQueueSetFactory priorityQueueSetFactory,
        Map<String, RegisteredStateMetaInfoBase> registeredKvStateMetaInfos,
        Map<String, KeyedStateDescriptor> keyedStateDescriptorMap,
        Map<String, NSKeyedStateDescriptor> nsKeyedStateDescriptorMap,
        Map<String, HeapPriorityQueueSnapshotRestoreWrapper<?>> registeredPQStates, Map<String, Table> tables,
        StateSerializerProvider<K> keySerializerProvider,
        // 考虑随着db中的boostConfig传下来
        File instanceBasePath, @Nonnull Collection<KeyedStateHandle> restoreStateHandles) {
        this.jobID = jobID;
        this.operatorIdentifier = operatorIdentifier;
        this.keyGroupRange = keyGroupRange;
        this.db = db;
        this.numberOfTransferringThreads = numberOfTransferringThreads;
        this.cancelStreamRegistry = cancelStreamRegistry;
        this.userCodeClassLoader = userCodeClassLoader;
        this.priorityQueueSetFactory = priorityQueueSetFactory;
        this.registeredKvStateMetaInfos = registeredKvStateMetaInfos;
        this.keyedStateDescriptorMap = keyedStateDescriptorMap;
        this.nsKeyedStateDescriptorMap = nsKeyedStateDescriptorMap;
        this.registeredPQStates = registeredPQStates;
        // table通过db和keyedStateDescriptorMap/nsKeyedStateDescriptorMap恢复
        this.tables = tables;
        this.keySerializerProvider = keySerializerProvider;
        this.instanceBasePath = instanceBasePath;
        this.tmpPathToRestore = instanceBasePath.getAbsoluteFile().toPath().resolve(UUID.randomUUID().toString());
        this.restoreStateHandles = restoreStateHandles;
        this.lastCompletedCheckpointId = -1L;
        this.remotePaths = new ArrayList<>();
        this.localPaths = new ArrayList<>();
        this.backendID = UUID.randomUUID();
    }

    // 并行度变更的场景restoreStateHandles都为IncrementalRemoteKeyedStateHandle
    protected void restoreWithRescaling(Collection<KeyedStateHandle> restoreStateHandles,
        KeyedStateHandle theFirstStateHandle) throws Exception {
        if (restoreStateHandles.isEmpty()) {
            restoreWithoutRescaling(theFirstStateHandle);
            return;
        }
        restoreStateHandles.add(theFirstStateHandle);
        restoreFromRemoteState(restoreStateHandles);
    }

    protected void restoreWithoutRescaling(KeyedStateHandle keyedStateHandle) throws Exception {
        restorePreviousIncrementalFilesStatus((IncrementalKeyedStateHandle) keyedStateHandle);
        if (keyedStateHandle instanceof IncrementalRemoteKeyedStateHandle) {
            restoreFromRemoteState(Collections.singletonList(keyedStateHandle));
        } else if (keyedStateHandle instanceof IncrementalLocalKeyedStateHandle) {
            restoreFromLocalState(Collections.singletonList((IncrementalLocalKeyedStateHandle) keyedStateHandle), true);
        } else {
            throw unexpectedStateHandleException(new Class[] {
                IncrementalRemoteKeyedStateHandle.class, IncrementalLocalKeyedStateHandle.class
            }, keyedStateHandle.getClass());
        }
    }

    protected abstract void restorePreviousIncrementalFilesStatus(IncrementalKeyedStateHandle localKeyedStateHandle);

    private void restoreFromRemoteState(Collection<KeyedStateHandle> restoreStateHandles) throws Exception {
        List<IncrementalLocalKeyedStateHandle> localKeyedStateHandles = new ArrayList<>();
        try {
            this.downloadStartTime = System.currentTimeMillis();
            for (KeyedStateHandle stateHandle : restoreStateHandles) {
                if (!(stateHandle instanceof IncrementalRemoteKeyedStateHandle)) {
                    throw unexpectedStateHandleException(new Class[] {
                        IncrementalRemoteKeyedStateHandle.class
                    }, stateHandle.getClass());
                }
                IncrementalLocalKeyedStateHandle localKeyedStateHandle =
                    transferRemoteStateToLocalDirectory(this.tmpPathToRestore,
                        (IncrementalRemoteKeyedStateHandle) stateHandle);
                localKeyedStateHandles.add(localKeyedStateHandle);
            }
            this.downloadEndTime = System.currentTimeMillis();
            restoreFromLocalState(localKeyedStateHandles, false);
        } catch (Exception e) {
            LOG.error("Failed to restoreFromRemoteState.", e);
            throw new BSSRuntimeException("restoreFromRemoteState caught exception.", e);
        }
    }

    private boolean restoreMeta(IncrementalLocalKeyedStateHandle localKeyedStateHandle) {
        FSDataInputStream inputStream = null;
        boolean isNewjob = false;
        try {
            // FSDataInputStream
            inputStream = localKeyedStateHandle.getMetaDataState().openInputStream();
            cancelStreamRegistry.registerCloseable(inputStream);
            // DataInputViewStreamWrapper
            DataInputViewStreamWrapper inputView = new DataInputViewStreamWrapper(inputStream);
            // 版本号应当小于等于3
            int currentVersion = inputView.readInt();
            LOG.debug("currentVersion: {}. Restoring keyed backend uid in operator {} from incremental snapshot to {}",
                currentVersion, operatorIdentifier, backendID);
            if (currentVersion > 3) {
                throw new BSSRuntimeException("Failed to restore, currentVersion > 3");
            }
            // 读取32字节
            byte[] buffer = new byte[32];
            inputView.readFully(buffer);
            String oldJobID = new String(buffer, StandardCharsets.UTF_8);
            if (!jobID.equals(oldJobID)) {
                isNewjob = true;
            }
            LOG.info("Restore meta oldJobID: {}, newJobID: {}.", oldJobID, jobID);

            int readVersion = 0;
            if (currentVersion >= 2) {
                KeyedBackendSerializationProxy<K> serializationProxy = readMetaData(inputView);
                readVersion = serializationProxy.getReadVersion();
                List<StateMetaInfoSnapshot> stateMetaInfoSnapshots = serializationProxy.getStateMetaInfoSnapshots();
                restoreStateMeta(stateMetaInfoSnapshots);
            }
            restoreKeyedState(inputStream, inputView);

            restoreNSKeyedState(inputStream, inputView);

            restorePQStates(inputStream, inputView, localKeyedStateHandle, readVersion);
            return isNewjob;
        } catch (Exception e) {
            LOG.error("Failed to restore meta from IncrementalLocalKeyedStateHandle.", e);
            throw new BSSRuntimeException("Failed to restore meta from IncrementalLocalKeyedStateHandle.", e);
        } finally {
            if (cancelStreamRegistry.unregisterCloseable(inputStream)) {
                IOUtils.closeQuietly(inputStream);
            }
        }
    }

    private void restoreFromLocalState(List<IncrementalLocalKeyedStateHandle> stateHandles, boolean isLocalRecovery) {
        List<Path> restorePaths = new ArrayList<>();
        if (stateHandles == null || stateHandles.isEmpty()) {
            return;
        }
        try {
            boolean isNewjob = false;
            for (IncrementalLocalKeyedStateHandle stateHandle : stateHandles) {
                isNewjob = restoreMeta(stateHandle);
                restorePaths.add(stateHandle.getDirectoryStateHandle().getDirectory());
            }
            // 本地恢复场景需要恢复backup目录
            if (db.getConfig().isEnableLocalRecovery() && !isNewjob) {
                // 本地恢复场景不涉及并行度变更，仅会有一个句柄，也就只会有一个恢复临时目录
                Path restorePath = restorePaths.get(0);
                Path backupPath = Paths.get(db.getConfig().getSnapshotBackupDir());
                try (Stream<Path> files = Files.list(restorePath)) {
                    files.forEach(file -> {
                        if (file.toAbsolutePath().toString().endsWith("slice")) {
                            try {
                                Files.createLink(backupPath.resolve(file.getFileName()), file.toAbsolutePath());
                            } catch (IOException e) {
                                throw new BSSRuntimeException(
                                    "Failed to create hardLink for " + file.getFileName() + ".", e);
                            }
                        }
                    });
                }
            }
            restoreDb(restorePaths, isNewjob);
        } catch (Exception e) {
            LOG.error("Failed to restore from IncrementalLocalKeyedStateHandle.", e);
            throw new BSSRuntimeException("Failed to restore from IncrementalLocalKeyedStateHandle.", e);
        }
    }

    protected abstract IncrementalLocalKeyedStateHandle transferRemoteStateToLocalDirectory(
        Path tmpPathToRestore,
        IncrementalRemoteKeyedStateHandle remoteKeyedStateHandle);

    private KeyedBackendSerializationProxy<K> readMetaData(DataInputView inputView)
        throws IOException, StateMigrationException {
        KeyedBackendSerializationProxy<K> serializationProxy = new KeyedBackendSerializationProxy<>(
            userCodeClassLoader);
        serializationProxy.read(inputView);
        if (!isKeySerializerCompatibilityChecked) {
            TypeSerializer<K> currentSerializer = keySerializerProvider.currentSchemaSerializer();

            TypeSerializerSchemaCompatibility<K> keySerializerSchemaCompatibility
                = keySerializerProvider.setPreviousSerializerSnapshotForRestoredState(
                serializationProxy.getKeySerializerSnapshot());
            if (keySerializerSchemaCompatibility.isCompatibleAfterMigration()
                || keySerializerSchemaCompatibility.isIncompatible()) {
                throw new StateMigrationException(
                    "Failed to restore because the new key serializer (" + currentSerializer
                        + ") must be compatible with the previous key serializer ("
                        + keySerializerProvider.previousSchemaSerializer() + ").");
            }
            isKeySerializerCompatibilityChecked = true;
        }
        return serializationProxy;
    }

    private <T extends HeapPriorityQueueElement & PriorityComparable<? super T>> void restoreStateMeta(
        List<StateMetaInfoSnapshot> stateMetaInfoSnapshots) {
        int i = 0;
        int j = 0;
        for (StateMetaInfoSnapshot stateMetaInfoSnapshot : stateMetaInfoSnapshots) {
            String name = stateMetaInfoSnapshot.getName();
            switch (stateMetaInfoSnapshot.getBackendStateType()) {
                case KEY_VALUE:
                    registeredKvStateMetaInfos.computeIfAbsent(name,
                        key -> new RegisteredKeyValueStateBackendMetaInfo<>(stateMetaInfoSnapshot));
                    LOG.info("RegisteredKeyValueStateBackendMetaInfo restore, i = {}, and name = {}", i, name);
                    i++;
                    break;
                case PRIORITY_QUEUE:
                    if (priorityQueueSetFactory instanceof HeapPriorityQueueSetFactory) {
                        RegisteredPriorityQueueStateBackendMetaInfo pqMetaInfo
                                = new RegisteredPriorityQueueStateBackendMetaInfo<>(stateMetaInfoSnapshot);
                        HeapPriorityQueueSet<T> priorityQueueSet =
                                (HeapPriorityQueueSet<T>) this.priorityQueueSetFactory.create(pqMetaInfo.getName(),
                                pqMetaInfo.getElementSerializer());
                        registeredPQStates.computeIfAbsent(name,
                                key -> new HeapPriorityQueueSnapshotRestoreWrapper(priorityQueueSet, pqMetaInfo,
                                        KeyExtractorFunction.forKeyedObjects(), this.keyGroupRange,
                                        this.db.getConfig().getMaxParallelism()));
                        j++;
                    } else {
                        RegisteredPriorityQueueStateBackendMetaInfo pqMetaInfo
                                = new RegisteredPriorityQueueStateBackendMetaInfo<>(stateMetaInfoSnapshot);
                        this.priorityQueueSetFactory.create(pqMetaInfo.getName(), pqMetaInfo.getElementSerializer());
                        registeredKvStateMetaInfos.put(name, pqMetaInfo);
                        i++;
                    }
                    LOG.info("registeredPQStates restore, j = {}, and name = {}", j, name);
                    break;
                default:
                    throw new IllegalStateException("Failed to restore Meta info because of invalid state type: "
                        + stateMetaInfoSnapshot.getBackendStateType() + ".");
            }
        }
        LOG.info("restoreStateMeta success!");
    }

    private void restoreKeyedState(FSDataInputStream inputStream, DataInputViewStreamWrapper inputView) {
        try {
            int keyedStateSize = inputView.readInt();
            for (int i = 0; i < keyedStateSize; i++) {
                String name = inputView.readUTF();
                KeyedStateDescriptor descriptor = InstantiationUtil.deserializeObject(inputStream,
                    Thread.currentThread().getContextClassLoader());
                keyedStateDescriptorMap.put(name, descriptor);
                TableDescription tableDescription = DescriptionUtils.createTableDescription(
                    this.db.getConfig().getMaxParallelism(), descriptor);
                KeyedStateBackendUtils.getOrCreateTable(db, tables, tableDescription);
                KeyedStateBackendUtils.createKeyedStateMetaInfo(descriptor, registeredKvStateMetaInfos);
            }
            LOG.info("restoreKeyedState success! and keyedStateSize = {}", keyedStateSize);
        } catch (Exception e) {
            LOG.error("Failed to restore KeyedState.", e);
            throw new BSSRuntimeException("Error while reading data from DataInputView or deserialize descriptor", e);
        }
    }

    private void restoreNSKeyedState(FSDataInputStream inputStream, DataInputViewStreamWrapper inputView) {
        try {
            int nsKeyedStateSize = inputView.readInt();
            for (int i = 0; i < nsKeyedStateSize; i++) {
                String name = inputView.readUTF();
                NSKeyedStateDescriptor descriptor = InstantiationUtil.deserializeObject(inputStream,
                    Thread.currentThread().getContextClassLoader());
                nsKeyedStateDescriptorMap.put(name, descriptor);
                TableDescription tableDescription = DescriptionUtils.createTableDescription(
                    this.db.getConfig().getMaxParallelism(), descriptor);
                KeyedStateBackendUtils.getOrCreateTable(db, tables, tableDescription);
                KeyedStateBackendUtils.createNSKeyedStateMetaInfo(descriptor, registeredKvStateMetaInfos);
            }
            LOG.info("restoreNSKeyedState success! and nsKeyedStateSize = {}", nsKeyedStateSize);
        } catch (Exception e) {
            LOG.error("Failed to restore NSKeyedState.", e);
            throw new BSSRuntimeException("Error while reading data from DataInputView or deserialize descriptor", e);
        }
    }

    private void restorePQStates(FSDataInputStream inputStream, DataInputViewStreamWrapper inputView,
        IncrementalLocalKeyedStateHandle stateHandle, int readVersion) {
        try {
            KeyGroupRange previous = stateHandle.getKeyGroupRange();
            LOG.debug("Previous KeyGroupRange in stateHandle: {}", previous);
            LOG.debug("Now KeyGroupRange: {}", keyGroupRange);
            int numberOfKeyGroups = previous.getNumberOfKeyGroups();
            int numTimerState = inputView.readInt();
            LOG.info("restorePQStates start! and numTimerState = {}", numTimerState);
            if (numTimerState < 1) {
                return;
            }
            // seek到pqStatePos位置
            inputStream.seek(stateHandle.getMetaDataState().getStateSize() - 8L * (numberOfKeyGroups + 2));
            long pqStatePos = inputView.readLong();
            Map<Short, String> pqStateMap = new HashMap<>(numTimerState);
            long[] keyGroupRangeOffsets = new long[numberOfKeyGroups];
            for (int i = 0; i < numberOfKeyGroups; i++) {
                keyGroupRangeOffsets[i] = inputView.readLong();
            }
            long snapShotResPos = inputView.readLong();

            inputStream.seek(pqStatePos);
            for (int i = 0; i < numTimerState; i++) {
                short stateId = inputView.readShort();
                String name = inputView.readUTF();
                pqStateMap.put(stateId, name);
            }

            for (Integer keyGroup : previous) {
                if (keyGroupRange.contains(keyGroup)) {
                    inputStream.seek(keyGroupRangeOffsets[keyGroup - previous.getStartKeyGroup()]);
                    int writtenKeyGroup = inputView.readInt();
                    Preconditions.checkState(writtenKeyGroup == keyGroup);
                    for (int i = 0; i < numTimerState; i++) {
                        HeapPriorityQueueSnapshotRestoreWrapper<?> restoreWrapper =
                            registeredPQStates.get(pqStateMap.get(inputView.readShort()));
                        StateSnapshotKeyGroupReader keyGroupReader = restoreWrapper.keyGroupReader(readVersion);
                        keyGroupReader.readMappingsInKeyGroup(inputView, keyGroup);
                    }
                }
            }
            inputStream.seek(snapShotResPos);
            LOG.info("restorePQStates success! and numTimerState = {}, pqStatePos = {}, snapShotResPos = {}",
                numTimerState, pqStatePos, snapShotResPos);
        } catch (IOException e) {
            LOG.error("Failed to restore timeState.");
            throw new BSSRuntimeException("Error while reading data from DataInputView.", e);
        }
    }

    private void restoreDb(List<Path> previousDbPath, boolean isNewjob) {
        try {
            db.restore(previousDbPath, this.remotePaths, this.localPaths, isNewjob);
        } catch (BSSRuntimeException e) {
            LOG.error("Failed to restore DB from IncrementalLocalKeyedStateHandle.", e);
            throw new BSSRuntimeException("Failed to restore DB from IncrementalLocalKeyedStateHandle.", e);
        }
    }

    private void cleanUpPathQuietly(@Nonnull List<Path> paths) {
        Path currentPath = null;
        try {
            for (Path path : paths) {
                currentPath = path.getFileName();
                FileUtils.deleteDirectory(path.toFile());
            }
        } catch (IOException ex) {
            LOG.warn("Failed to clean up path: {}", currentPath, ex);
        }
    }

    @Override
    public void close() throws Exception {

    }
}
