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

package com.huawei.ock.bss.util;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import java.io.IOException;
import java.nio.file.FileVisitResult;
import java.nio.file.Files;
import java.nio.file.InvalidPathException;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.nio.file.SimpleFileVisitor;
import java.nio.file.attribute.BasicFileAttributes;
import java.util.concurrent.atomic.AtomicBoolean;
import java.util.concurrent.atomic.AtomicReference;

/**
 * Register the deleted utility class of the directory
 * 
 * @since BeiMing 25.0.T1
 */
public class CleanupManager {
    /**
     * The prefix of the jni directory load
     */
    public static final String JNI_DIR_NAME_PREFIX = "ockdb-lib-";

    /**
     * Specify the parent directory
     */
    public static String jniParentDir = "/tmp";
    private static final Logger LOG = LoggerFactory.getLogger(CleanupManager.class);
    private static final AtomicBoolean HOOK_REGISTERED = new AtomicBoolean(false);
    private static final AtomicReference<Path> TARGET_DIR_REF = new AtomicReference<>();

    /**
     * Register the Shutdown Hook to clean up the normalized directory path
     *
     * @param canonicalPath canonicalPath
     */
    public static void registerCleanupHook(String canonicalPath) {
        if (HOOK_REGISTERED.compareAndSet(false, true)) {
            try {
                Path path = validateCanonicalPath(canonicalPath);
                if (path != null) {
                    TARGET_DIR_REF.set(path);
                    Runtime.getRuntime().addShutdownHook(new Thread(CleanupManager::deleteTargetDir));
                    LOG.info("Registered cleanup hook for path: {}", path.getFileName());
                } else {
                    LOG.error("The path is invalid, register cleanup hook failed.");
                }
            } catch (InvalidPathException e) {
                LOG.error("Register cleanup hook failed, invalid path : {}", e.getMessage());
            }
        }
    }

    /**
     * Verify the validity of the path
     *
     * @param canonicalPath canonicalPath
     * @return path
     * @throws InvalidPathException InvalidPathException
     */
    private static Path validateCanonicalPath(String canonicalPath) throws InvalidPathException {
        if (canonicalPath == null || canonicalPath.isEmpty()) {
            LOG.warn("Path cannot be null or empty");
            return null;
        }

        try {
            Path path = Paths.get(canonicalPath);

            // 解析真实路径（自动处理符号链接和路径规范化）
            Path realPath = path.toRealPath();

            // 必须为目录
            if (!Files.isDirectory(realPath)) {
                LOG.warn("Path is not a directory: {}", realPath.getFileName());
                return null;
            }

            // 验证父目录为flink传入目录
            Path expectedParent = Paths.get(jniParentDir);
            if (!realPath.getParent().equals(expectedParent)) {
                LOG.warn("The parent path is invalid: {}", realPath.getFileName());
                return null;
            }

            // 验证目录名前缀
            String dirName = realPath.getFileName().toString();
            if (!dirName.startsWith(JNI_DIR_NAME_PREFIX)) {
                LOG.warn("The path is invalid: {}", realPath.getFileName());
                return null;
            }

            return realPath;
        } catch (IOException e) {
            LOG.warn("Path validation failed: {}", e.getMessage());
            return null;
        }
    }

    /**
     * Shutdown Hook triggered deletion actions
     */
    private static void deleteTargetDir() {
        Path targetDir = TARGET_DIR_REF.get();
        try {
            if (targetDir != null && validateCanonicalPath(targetDir.toString()) != null) {
                deleteDirectory(targetDir);
                LOG.info("Shutdown hook deleted directory: {}", targetDir.getFileName());
            }
        } catch (IOException e) {
            LOG.error("Failed to delete directory {}: {}", targetDir, e.getMessage());
        } finally {
            TARGET_DIR_REF.set(null);
            HOOK_REGISTERED.set(false);
        }
    }

    /**
     * The core method of deleting a directory
     *
     * @param dir path
     * @throws IOException IOException
     */
    private static void deleteDirectory(Path dir) throws IOException {
        Files.walkFileTree(dir, new SimpleFileVisitor<Path>() {
            @Override
            public FileVisitResult visitFile(Path file, BasicFileAttributes attrs) throws IOException {
                Files.delete(file);
                LOG.debug("Deleted file: {}", file.getFileName());
                return FileVisitResult.CONTINUE;
            }

            @Override
            public FileVisitResult postVisitDirectory(Path dir, IOException exc) throws IOException {
                Files.delete(dir);
                LOG.debug("Deleted directory: {}", dir.getFileName());
                return FileVisitResult.CONTINUE;
            }
        });
    }
}