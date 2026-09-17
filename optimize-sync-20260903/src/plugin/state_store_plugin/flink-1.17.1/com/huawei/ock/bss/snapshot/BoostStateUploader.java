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

import com.huawei.ock.bss.common.exception.BSSRuntimeException;

import org.apache.flink.core.fs.CloseableRegistry;
import org.apache.flink.runtime.state.CheckpointStateOutputStream;
import org.apache.flink.runtime.state.CheckpointStreamFactory;
import org.apache.flink.runtime.state.CheckpointedStateScope;
import org.apache.flink.runtime.state.StateUtil;
import org.apache.flink.runtime.state.StreamStateHandle;
import org.apache.flink.util.ExceptionUtils;
import org.apache.flink.util.IOUtils;
import org.apache.flink.runtime.state.StateHandleID;
import org.apache.flink.util.concurrent.FutureUtils;
import org.apache.flink.util.function.CheckedSupplier;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import java.io.IOException;
import java.io.InputStream;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.List;
import java.util.Map;
import java.util.HashMap;
import java.util.concurrent.CompletableFuture;
import java.util.concurrent.ExecutionException;
import java.util.stream.Collectors;

import javax.annotation.Nonnull;

/**
 * checkpoint时向jobmanager上传数据
 *
 * @since BeiMing 25.0.T1
 */
public class BoostStateUploader extends BoostStateDataTransfer {
    private static final Logger LOG = LoggerFactory.getLogger(BoostStateUploader.class);

    private static final int READ_BUFFER_SIZE = 16 * 1024;

    private static final int EOF = -1;

    public BoostStateUploader(int snapshotThreadNum) {
        super(snapshotThreadNum);
    }

    /**
     * 使用指定数量的线程将所有文件上传到checkpoint fileSystem
     *
     * @param files 要上传的文件
     * @param checkpointStreamFactory 用于创建输出流
     * @param stateScope 定义状态是否共享
     * @param closeableRegistry closeableRegistry
     * @param tmpResourcesRegistry tmpResourcesRegistry
     * @return 上传的文件的句柄
     * @throws Exception exception
     */
    public Map<StateHandleID, StreamStateHandle> uploadFilesToCheckpointFs(@Nonnull List<Path> files,
        CheckpointStreamFactory checkpointStreamFactory, CheckpointedStateScope stateScope,
        CloseableRegistry closeableRegistry, CloseableRegistry tmpResourcesRegistry) throws Exception {
        Map<StateHandleID, StreamStateHandle> handles = new HashMap<>(files.size());

        Map<StateHandleID, CompletableFuture<StreamStateHandle>> futures = createUploadFutures(files,
            checkpointStreamFactory, stateScope, closeableRegistry, tmpResourcesRegistry);
        try {
            FutureUtils.waitForAll(futures.values()).get();

            for (Map.Entry<StateHandleID, CompletableFuture<StreamStateHandle>> entry : futures.entrySet()) {
                handles.put(entry.getKey(), entry.getValue().get());
            }
        } catch (ExecutionException e) {
            Throwable throwable = ExceptionUtils.stripException(ExceptionUtils.stripExecutionException(e),
                RuntimeException.class);
            if (throwable instanceof IOException) {
                throw (IOException) throwable;
            }
            LOG.warn("Failed to upload data for state handles, may caused by cancellation while checkpoint,"
                + " warn message: {}, warn trace: {}", e.getMessage(), e.getStackTrace());
            throw new BSSRuntimeException("Failed to upload data for state handles.", e);
        }

        return handles;
    }

    private Map<StateHandleID, CompletableFuture<StreamStateHandle>> createUploadFutures(List<Path> files,
        CheckpointStreamFactory checkpointStreamFactory, CheckpointedStateScope stateScope,
        CloseableRegistry closeableRegistry, CloseableRegistry tmpResourcesRegistry) {
        return files.stream()
            .collect(Collectors.toMap(
                file -> {
                    String fileName = file.getFileName().getFileName().toString();
                    String localPath = fileName;
                    if (fileName.endsWith("metadata") || fileName.endsWith("dat")) {
                        // metadata结尾的是元数据，dat结尾的是fresh table的全量数据
                        localPath = file.toAbsolutePath().toString();
                    }
                    return new StateHandleID(localPath);
                },
                file -> CompletableFuture.supplyAsync(CheckedSupplier.unchecked(
                    () -> uploadLocalFileToCheckpointFs(file, checkpointStreamFactory, stateScope,
                        closeableRegistry, tmpResourcesRegistry)),
                    executorService)
            ));
    }

    private StreamStateHandle uploadLocalFileToCheckpointFs(Path filePath,
        CheckpointStreamFactory checkpointStreamFactory, CheckpointedStateScope stateScope,
        CloseableRegistry closeableRegistry, CloseableRegistry tmpResourcesRegistry) throws IOException {
        InputStream inputStream = null;
        CheckpointStateOutputStream outputStream = null;
        try {
            final String fileName = filePath.getFileName().toString();
            // 记录当前文件的本地文件地址，如果是共享文件，记录的只是文件名，如果是独享的文件，记录的是当前本地checkpoint的绝对路径。
            if (fileName.endsWith("metadata") || fileName.endsWith("dat")) {
                // metadata结尾的是元数据，dat结尾的是fresh table的全量数据，要上传到exclusive目录
                stateScope = CheckpointedStateScope.EXCLUSIVE;
            }

            outputStream = checkpointStreamFactory.createCheckpointStateOutputStream(stateScope);
            closeableRegistry.registerCloseable(outputStream);

            inputStream = Files.newInputStream(filePath);
            closeableRegistry.registerCloseable(inputStream);

            readInputDataToOutput(inputStream, outputStream);

            final StreamStateHandle result;
            if (closeableRegistry.unregisterCloseable(outputStream)) {
                result = outputStream.closeAndGetHandle();
                outputStream = null;
            } else {
                result = null;
            }
            if (result == null) {
                LOG.warn("Failed to upload data for state handles, output stream may be null. path: {}",
                    filePath.getFileName());
            }
            tmpResourcesRegistry.registerCloseable(() -> StateUtil.discardStateObjectQuietly(result));
            return result;
        } finally {
            if (closeableRegistry.unregisterCloseable(outputStream)) {
                IOUtils.closeQuietly(outputStream);
            }
            if (closeableRegistry.unregisterCloseable(inputStream)) {
                IOUtils.closeQuietly(inputStream);
            }
        }
    }

    private void readInputDataToOutput(InputStream inputStream, CheckpointStateOutputStream outputStream)
        throws IOException {
        final byte[] buffer = new byte[READ_BUFFER_SIZE];
        while (true) {
            int bytesRead = inputStream.read(buffer);
            if (bytesRead == EOF) {
                break;
            }
            outputStream.write(buffer, 0, bytesRead);
        }
    }
}
