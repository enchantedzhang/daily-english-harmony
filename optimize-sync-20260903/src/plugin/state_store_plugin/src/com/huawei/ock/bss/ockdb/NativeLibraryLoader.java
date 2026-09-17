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

package com.huawei.ock.bss.ockdb;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import java.io.File;
import java.io.IOException;
import java.io.InputStream;
import java.nio.file.Files;
import java.nio.file.StandardCopyOption;
import java.util.concurrent.locks.Lock;
import java.util.concurrent.locks.ReentrantLock;

/**
 * NativeLibraryLoader类，加载so
 *
 * @since 2025年1月12日16:26:41
 */
public class NativeLibraryLoader {
    private static final NativeLibraryLoader INSTANCE = new NativeLibraryLoader();

    private static final Logger LOG = LoggerFactory.getLogger(NativeLibraryLoader.class);

    private static final String JNI_LIBRARY_FILE_NAME = "libockdbjni-linux64";

    private static final String TEMP_FILE_PREFIX = "libockdbjni";

    private static final String TEMP_FILE_SUFFIX = ".so";

    private static final Lock lock = new ReentrantLock(); // 添加静态Lock对象

    private static boolean initialized = false;

    public static NativeLibraryLoader getInstance() {
        return INSTANCE;
    }

    /**
     * 加载so
     *
     * @param tmpDir 临时目录
     * @throws IOException io异常
     */
    public void loadLibrary(final String tmpDir) throws IOException {
        lock.lock(); // 使用静态Lock对象
        try {
            if (!initialized) {
                System.load(loadLibraryFromJarToTemp(tmpDir).getAbsolutePath());
                LOG.info("success load libockdbjni.so from jar");
                initialized = true;
            }
        } finally {
            lock.unlock();
        }
    }

    File loadLibraryFromJarToTemp(final String tmpDir) throws IOException {
        final File temp;
        if (tmpDir == null || tmpDir.isEmpty()) {
            temp = File.createTempFile(TEMP_FILE_PREFIX, TEMP_FILE_SUFFIX);
        } else {
            final File parentDir = new File(tmpDir);
            if (!parentDir.exists()) {
                throw new IOException("Directory: " + parentDir.getName() + " does not exist!");
            }
            temp = new File(parentDir, JNI_LIBRARY_FILE_NAME + TEMP_FILE_SUFFIX);
            if (temp.exists() && !temp.delete()) {
                throw new IOException("File: " + temp.getName() + " already exists and cannot be removed.");
            }
            if (!temp.createNewFile()) {
                throw new IOException("File: " + temp.getName() + " could not be created.");
            }
        }

        if (!temp.exists()) {
            throw new IOException("File " + temp.getName() + " does not exist.");
        } else {
            temp.deleteOnExit();
        }

        // attempt to copy the library from the Jar file to the temp destination
        try (final InputStream is = getClass().getClassLoader().getResourceAsStream(JNI_LIBRARY_FILE_NAME + ".so")) {
            if (is == null) {
                throw new IOException(JNI_LIBRARY_FILE_NAME + " was not found inside JAR.");
            } else {
                Files.copy(is, temp.toPath(), StandardCopyOption.REPLACE_EXISTING);
            }
        }

        return temp;
    }
}