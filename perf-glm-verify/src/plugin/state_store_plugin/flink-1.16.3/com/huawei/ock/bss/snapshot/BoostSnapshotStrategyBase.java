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

import org.apache.flink.api.common.typeutils.TypeSerializer;
import org.apache.flink.runtime.state.CheckpointStreamFactory;
import org.apache.flink.runtime.state.DirectoryStateHandle;
import org.apache.flink.runtime.state.IncrementalKeyedStateHandle;
import org.apache.flink.runtime.state.IncrementalLocalKeyedStateHandle;
import org.apache.flink.runtime.state.KeyGroupRange;
import org.apache.flink.runtime.state.KeyedStateHandle;
import org.apache.flink.runtime.state.LocalRecoveryConfig;
import org.apache.flink.runtime.state.PlaceholderStreamStateHandle;
import org.apache.flink.runtime.state.StreamStateHandle;
import org.apache.flink.util.ResourceGuard;

import java.io.File;
import java.io.IOException;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.util.Collection;
import java.util.Collections;
import java.util.List;
import java.util.Map;
import java.util.Optional;
import java.util.UUID;
import java.util.stream.Collectors;

import javax.annotation.Nonnegative;
import javax.annotation.Nonnull;
import javax.annotation.Nullable;

/**
 * BSS快照基类
 *
 * @param <K> key
 * @since BeiMing 25.0.T1
 */
public abstract class BoostSnapshotStrategyBase<K> extends AbstractBoostSnapshotStrategy<K> {
    /**
     * EMPTY_PREVIOUS_SNAPSHOT
     */
    protected static final PreviousSnapshot EMPTY_PREVIOUS_SNAPSHOT = new PreviousSnapshot(Collections.emptyList());

    public BoostSnapshotStrategyBase(@Nonnull String description, @Nonnull BoostStateDB db,
        @Nonnull ResourceGuard resourceGuard, @Nonnull TypeSerializer<K> keySerializer,
        @Nonnull OckDBKeyedStateBackend.BoostKvStateInfo kvStateInfo, @Nonnull KeyGroupRange keyGroupRange,
        @Nonnegative int keyGroupPrefixBytes, @Nonnull LocalRecoveryConfig localRecoveryConfig,
        @Nonnull File instanceBasePath, @Nonnull UUID backendUID) {
        super(description, db, resourceGuard, keySerializer, kvStateInfo, keyGroupRange, keyGroupPrefixBytes,
            localRecoveryConfig, instanceBasePath, backendUID);
    }

    /**
     * 封装了对OckDBKeyedStateBackend进行增量快照的过程。
     */
    protected abstract class BoostDBSnapshotOperation extends AbstractBoostDBSnapshotOperation {
        protected BoostDBSnapshotOperation(long checkpointId, @Nonnull CheckpointStreamFactory checkpointStreamFactory,
            @Nonnull NativeBoostSnapshotResources snapshotResources) {
            super(checkpointId, checkpointStreamFactory, snapshotResources);
        }

        /**
         * 获取Local快照
         *
         * @param localStreamStateHandle 本地stream状态句柄
         * @param sharedStateHandleIDs   共享的状态句柄
         * @return 返回本地快照句柄
         * @throws IOException 异常
         */
        protected Optional<KeyedStateHandle> getLocalSnapshot(@Nullable StreamStateHandle localStreamStateHandle,
            List<IncrementalKeyedStateHandle.HandleAndLocalPath> sharedStateHandleIDs) throws IOException {
            final DirectoryStateHandle directoryStateHandle = localBackupDirectory.completeSnapshotAndGetHandle();
            if (directoryStateHandle == null || localStreamStateHandle == null) {
                return Optional.empty();
            }
            return Optional.of(
                new IncrementalLocalKeyedStateHandle(backendUID, checkpointId, directoryStateHandle, keyGroupRange,
                    localStreamStateHandle, sharedStateHandleIDs));
        }
    }

    /**
     * Previous snapshot with uploaded sst files.
     */
    protected static class PreviousSnapshot extends AbstractPreviousSnapshot {
        @Nonnull
        private final Map<String, StreamStateHandle> confirmedSstFiles;

        protected PreviousSnapshot(
            @Nullable Collection<IncrementalKeyedStateHandle.HandleAndLocalPath> confirmedSstFiles) {
            // 因为我们当前的localPath存储的是全路径，所以这里需要解析一下fileName,作为confirmedSstFiles的key来进行查找
            this.confirmedSstFiles = confirmedSstFiles != null ? confirmedSstFiles.stream()
                .collect(Collectors.toMap(handleAndLocalPath -> {
                    Path path = Paths.get(handleAndLocalPath.getLocalPath());
                    return path.getFileName().toString();
                }, IncrementalKeyedStateHandle.HandleAndLocalPath::getHandle)) : Collections.emptyMap();
        }

        /**
         * 获取之前上传过的文件
         */
        protected Optional<StreamStateHandle> getUploaded(String filename) {
            if (confirmedSstFiles.containsKey(filename)) {
                StreamStateHandle handle = confirmedSstFiles.get(filename);
                // We introduce a placeholder state handle to reduce network transfer overhead,
                // it will be replaced by the original handle from the shared state registry
                // (created from a previous checkpoint).
                return Optional.of(
                    new PlaceholderStreamStateHandle(handle.getStreamStateHandleID(), handle.getStateSize()));
            } else {
                // Don't use any uploaded but not confirmed handles because they might be deleted
                // (by TM) if the previous checkpoint failed. See FLINK-25395
                return Optional.empty();
            }
        }
    }
}