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

import com.huawei.ock.bss.common.BoostStateDB;
import com.huawei.ock.bss.common.exception.BSSRuntimeException;
import com.huawei.ock.bss.snapshot.BoostStateDownloader;
import com.huawei.ock.bss.state.internal.descriptor.keyed.KeyedStateDescriptor;
import com.huawei.ock.bss.state.internal.descriptor.nskeyed.NSKeyedStateDescriptor;
import com.huawei.ock.bss.table.api.Table;

import org.apache.flink.core.fs.CloseableRegistry;
import org.apache.flink.runtime.state.DirectoryStateHandle;
import org.apache.flink.runtime.state.IncrementalKeyedStateHandle;
import org.apache.flink.runtime.state.IncrementalLocalKeyedStateHandle;
import org.apache.flink.runtime.state.IncrementalRemoteKeyedStateHandle;
import org.apache.flink.runtime.state.KeyGroupRange;
import org.apache.flink.runtime.state.KeyedStateHandle;
import org.apache.flink.runtime.state.PriorityQueueSetFactory;
import org.apache.flink.runtime.state.RegisteredStateMetaInfoBase;
import org.apache.flink.runtime.state.StateHandleID;
import org.apache.flink.runtime.state.StateSerializerProvider;
import org.apache.flink.runtime.state.StreamStateHandle;
import org.apache.flink.runtime.state.heap.HeapPriorityQueueSnapshotRestoreWrapper;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import java.io.File;
import java.nio.file.Path;
import java.util.Collection;
import java.util.Map;
import java.util.Objects;
import java.util.SortedMap;
import java.util.TreeMap;

import javax.annotation.Nonnull;

/**
 * 增量恢复
 *
 * @param <K> key
 * @since BeiMing 25.0.T1
 */
public class BoostIncrementalRestoreOperation<K> extends AbstractBoostIncrementalRestoreOperation<K> {
    private static final Logger LOG = LoggerFactory.getLogger(BoostIncrementalRestoreOperation.class);

    private final SortedMap<Long, Map<StateHandleID, StreamStateHandle>> restoredSstFiles;

    public BoostIncrementalRestoreOperation(String jobID, String operatorIdentifier, KeyGroupRange keyGroupRange,
        BoostStateDB db, int keyGroupPrefixBytes, int numberOfTransferringThreads,
        CloseableRegistry cancelStreamRegistry,
        ClassLoader userCodeClassLoader, PriorityQueueSetFactory priorityQueueSetFactory,
        Map<String, RegisteredStateMetaInfoBase> registeredKvStateMetaInfos,
        Map<String, KeyedStateDescriptor> keyedStateDescriptorMap,
        Map<String, NSKeyedStateDescriptor> nsKeyedStateDescriptorMap,
        Map<String, HeapPriorityQueueSnapshotRestoreWrapper<?>> registeredPQStates, Map<String, Table> tables,
        StateSerializerProvider<K> keySerializerProvider,
        File instanceBasePath, @Nonnull Collection<KeyedStateHandle> restoreStateHandles) {
        super(jobID, operatorIdentifier, keyGroupRange, db, keyGroupPrefixBytes, numberOfTransferringThreads,
            cancelStreamRegistry, userCodeClassLoader, priorityQueueSetFactory,
            registeredKvStateMetaInfos, keyedStateDescriptorMap, nsKeyedStateDescriptorMap,
            registeredPQStates, tables, keySerializerProvider, instanceBasePath, restoreStateHandles);
        this.restoredSstFiles = new TreeMap<>();
    }

    @Override
    public BoostRestoreResult restore() throws Exception {
        if (restoreStateHandles == null || restoreStateHandles.isEmpty()) {
            return BoostRestoreResult.EMPTY_RESULT;
        }
        LOG.debug("Begin restoring from {} stateHandles", restoreStateHandles.size());
        for (KeyedStateHandle stateHandle : restoreStateHandles) {
            LOG.debug("stateHandle KeyGroupRange: {}", stateHandle.getKeyGroupRange());
        }
        final KeyedStateHandle theFirstStateHandle = restoreStateHandles.iterator().next();
        if (!Objects.equals(keyGroupRange, theFirstStateHandle.getKeyGroupRange()) || restoreStateHandles.size() > 1) {
            restoreStateHandles.remove(theFirstStateHandle);
            restoreWithRescaling(restoreStateHandles, theFirstStateHandle);
            LOG.info("Finished restoring with rescaling.");
        } else {
            restoreWithoutRescaling(theFirstStateHandle);
            LOG.info("Finished restoring without rescaling.");
        }
        return new BoostRestoreResult(lastCompletedCheckpointId, backendID, restoredSstFiles,
            this.downloadEndTime - this.downloadStartTime);
    }

    @Override
    protected void restorePreviousIncrementalFilesStatus(IncrementalKeyedStateHandle localKeyedStateHandle) {
        backendID = localKeyedStateHandle.getBackendIdentifier();
        restoredSstFiles.put(localKeyedStateHandle.getCheckpointId(), localKeyedStateHandle.getSharedStateHandles());
        lastCompletedCheckpointId = localKeyedStateHandle.getCheckpointId();
    }

    @Override
    protected IncrementalLocalKeyedStateHandle transferRemoteStateToLocalDirectory(
            Path tmpPathToRestore,
            IncrementalRemoteKeyedStateHandle remoteKeyedStateHandle) {
        Path restoreLocalPath = tmpPathToRestore;
        for (StateHandleID localPath : remoteKeyedStateHandle.getPrivateState().keySet()) {
            final String fileName = localPath.toString();
            if (fileName.endsWith("metadata") || fileName.endsWith("dat")) {
                // 获取checkpoint的本地路径
                restoreLocalPath = tmpPathToRestore.resolve(localPath.toString()).getParent();
                break;
            }
        }
        boolean isLazyRestore = db.getConfig().isEnableLazyDownload();
        try (BoostStateDownloader downloader = new BoostStateDownloader(numberOfTransferringThreads, isLazyRestore)) {
            downloader.transferAllStateDataToDirectory(remoteKeyedStateHandle, restoreLocalPath, cancelStreamRegistry,
                    this.remotePaths, this.localPaths);
        } catch (Exception e) {
            throw new BSSRuntimeException("transferRemoteStateToLocalDirectory caught exception", e);
        }
        return new IncrementalLocalKeyedStateHandle(remoteKeyedStateHandle.getBackendIdentifier(),
                remoteKeyedStateHandle.getCheckpointId(), new DirectoryStateHandle(restoreLocalPath),
                remoteKeyedStateHandle.getKeyGroupRange(), remoteKeyedStateHandle.getMetaStateHandle(),
                remoteKeyedStateHandle.getSharedState());
    }
}
