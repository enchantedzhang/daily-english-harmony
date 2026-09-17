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

import org.apache.flink.core.fs.CloseableRegistry;
import org.apache.flink.core.fs.FSDataInputStream;
import org.apache.flink.runtime.state.IncrementalRemoteKeyedStateHandle;
import org.apache.flink.runtime.state.StateHandleID;
import org.apache.flink.runtime.state.StreamStateHandle;
import org.apache.flink.runtime.state.filesystem.FileStateHandle;
import org.apache.flink.util.ExceptionUtils;
import org.apache.flink.util.FlinkRuntimeException;
import org.apache.flink.util.IOUtils;
import org.apache.flink.util.concurrent.FutureUtils;
import org.apache.flink.util.function.ThrowingRunnable;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import java.io.IOException;
import java.io.OutputStream;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.Iterator;
import java.util.List;
import java.util.Map;
import java.util.concurrent.CompletableFuture;
import java.util.concurrent.ConcurrentHashMap;
import java.util.concurrent.ExecutionException;
import java.util.stream.Collectors;

/**
 * checkpoint时从jobmanager下载数据
 *
 * @since BeiMing 25.0.T1
 */
public class BoostStateDownloader extends BoostStateDataTransfer {
    private static final Logger LOG = LoggerFactory.getLogger(BoostStateDownloader.class);

    private static final int SIZE_8KB = 8 * 1024;

    private static final int EOF = -1;

    private static final ConcurrentHashMap<String, Object> DOWNLOAD_LOCK_MAP = new ConcurrentHashMap<>();

    private final boolean isLazyRestore;

    public BoostStateDownloader(int restoreThreadNum, boolean isLazyRestore) {
        super(restoreThreadNum);
        this.isLazyRestore = isLazyRestore;
    }

    /**
     * 从远端下载state的数据，写入到指定目录
     *
     * @param restoreStateHandle 用于检索状态数据的句柄
     * @param dest               写入的目录
     * @param closeableRegistry  closeableRegistry
     * @param remotePaths        远程文件列表
     * @param localPaths         本地文件列表
     * @throws Exception Exception
     */

    public void transferAllStateDataToDirectory(IncrementalRemoteKeyedStateHandle restoreStateHandle, Path dest,
                                                CloseableRegistry closeableRegistry, List<String> remotePaths,
                                                List<String> localPaths) throws Exception {
        final Map<StateHandleID, StreamStateHandle> sstFiles = restoreStateHandle.getSharedState();
        final Map<StateHandleID, StreamStateHandle> miscFiles = restoreStateHandle.getPrivateState();
        if (sstFiles.isEmpty()) {
            // 全量CP恢复时，无共享文件，sst文件均为私有文件，需要重新抽离
            Iterator<Map.Entry<StateHandleID, StreamStateHandle>> miscFilesIterator = miscFiles.entrySet().iterator();
            while (miscFilesIterator.hasNext()) {
                Map.Entry<StateHandleID, StreamStateHandle> miscFile = miscFilesIterator.next();
                String localPath = miscFile.getKey().toString();
                if (localPath == null || localPath.endsWith("metadata") || localPath.endsWith("dat")) {
                    continue;
                }
                sstFiles.put(miscFile.getKey(), miscFile.getValue());
                miscFilesIterator.remove();
            }
        }

        for (Map.Entry<StateHandleID, StreamStateHandle> entry : sstFiles.entrySet()) {
            Path normalizedPath = dest.resolve(entry.getKey().toString()).normalize();
            String localPath = normalizedPath.toUri().getPath();
            if (localPath.endsWith("slice")) {
                if (isLazyRestore) {
                    miscFiles.put(entry.getKey(), entry.getValue());
                }
                continue;
            }
            if (normalizedPath.getFileName().toString().startsWith("blob_")) {
                miscFiles.put(entry.getKey(), entry.getValue());
                continue;
            }
            if (localPath.endsWith("/")) {
                localPath = localPath.substring(0, localPath.length() - 1);
            }
            if (entry.getValue() instanceof FileStateHandle) {
                String remotePath = ((FileStateHandle) entry.getValue()).getFilePath().getPath();
                localPaths.add(localPath);
                remotePaths.add(remotePath);
            } else {
                LOG.error("Unexpect handle type:{}", entry.getValue().getClass().getCanonicalName());
            }
        }
        if (!isLazyRestore) {
            downloadDataForAllStateHandles(sstFiles, dest, closeableRegistry);
        }
        downloadDataForAllStateHandles(miscFiles, dest, closeableRegistry);
    }

    private void downloadDataForAllStateHandles(Map<StateHandleID, StreamStateHandle> stateHandleMap,
                                                Path restoreInstancePath,
                                                CloseableRegistry closeableRegistry) throws Exception {
        if (stateHandleMap.isEmpty()) {
            return;
        }
        try {
            List<Runnable> downloadTasks = createDownloadTasks(stateHandleMap, restoreInstancePath, closeableRegistry);
            List<CompletableFuture<Void>> futures = downloadTasks.stream()
                .map(runnable -> CompletableFuture.runAsync(runnable, executorService))
                .collect(Collectors.toList());
            FutureUtils.waitForAll(futures).get();
        } catch (ExecutionException e) {
            LOG.error("downloadDataForAllStateHandles exception. stackTrace: {}, message: {}",
                    e.getStackTrace(), e.getMessage());
            Throwable throwable = ExceptionUtils.stripException(ExceptionUtils.stripExecutionException(e),
                RuntimeException.class);
            if (throwable instanceof IOException) {
                throw (IOException) throwable;
            }
            throw new FlinkRuntimeException("Failed to download data for state handles.", e);
        }
    }

    private List<Runnable> createDownloadTasks(Map<StateHandleID, StreamStateHandle> stateHandleMap,
                                                   Path restoreInstancePath, CloseableRegistry closeableRegistry) {
        return stateHandleMap.entrySet().stream()
            .map(entry -> {
                StateHandleID stateHandleID = entry.getKey();
                StreamStateHandle remoteFileHandle = entry.getValue();
                Path path = restoreInstancePath.resolve(stateHandleID.toString());
                return ThrowingRunnable.unchecked(
                    () -> downloadDataForStateHandle(path, remoteFileHandle, closeableRegistry)
                );
            })
            .collect(Collectors.toList());
    }

    private void downloadDataForStateHandle(Path restoreFilePath, StreamStateHandle remoteFileHandle,
        CloseableRegistry closeableRegistry) throws IOException {
        OutputStream outputStream = null;
        FSDataInputStream inputStream = null;
        Object lock = getLock(restoreFilePath.toAbsolutePath().toString());
        synchronized (lock) {
            try {
                if (Files.notExists(restoreFilePath.getParent())) {
                    Files.createDirectories(restoreFilePath.getParent());
                }

                if (Files.exists(restoreFilePath)) {
                    return;
                }

                outputStream = Files.newOutputStream(restoreFilePath);
                closeableRegistry.registerCloseable(outputStream);

                inputStream = remoteFileHandle.openInputStream();
                closeableRegistry.registerCloseable(inputStream);

                readInputDataToOutput(outputStream, inputStream);
            } catch (Exception e) {
                if (Files.exists(restoreFilePath)) {
                    Files.delete(restoreFilePath);
                }
                LOG.warn("Download failed.", e);
                throw e;
            } finally {
                if (closeableRegistry.unregisterCloseable(outputStream)) {
                    IOUtils.closeQuietly(outputStream);
                }
                if (closeableRegistry.unregisterCloseable(inputStream)) {
                    IOUtils.closeQuietly(inputStream);
                }
            }
        }
    }

    private Object getLock(String path) {
        Object lock = new Object();
        DOWNLOAD_LOCK_MAP.putIfAbsent(path, lock);
        return DOWNLOAD_LOCK_MAP.get(path);
    }

    private void readInputDataToOutput(OutputStream outputStream, FSDataInputStream inputStream) throws IOException {
        byte[] buffer = new byte[SIZE_8KB];
        while (true) {
            int bytesRead = inputStream.read(buffer);
            if (bytesRead == EOF) {
                break;
            }
            outputStream.write(buffer, 0, bytesRead);
        }
    }
}
