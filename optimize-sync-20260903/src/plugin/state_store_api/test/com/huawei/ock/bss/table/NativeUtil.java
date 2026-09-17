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

package com.huawei.ock.bss.table;

import com.huawei.ock.bss.common.BoostStateDB;
import com.huawei.ock.bss.common.conf.BoostConfig;
import com.huawei.ock.bss.common.serialize.KeyPairSerializer;
import com.huawei.ock.bss.common.serialize.SubTableSerializer;
import com.huawei.ock.bss.common.serialize.TableSerializer;
import com.huawei.ock.bss.table.description.KListTableDescription;
import com.huawei.ock.bss.table.description.KMapTableDescription;
import com.huawei.ock.bss.table.description.KVTableDescription;
import com.huawei.ock.bss.table.description.NsKListTableDescription;
import com.huawei.ock.bss.table.description.NsKMapTableDescription;
import com.huawei.ock.bss.table.namespace.NsKListTableImpl;
import com.huawei.ock.bss.table.namespace.NsKMapTableImpl;
import com.huawei.ock.bss.table.namespace.NsKVTableImpl;

import org.apache.flink.api.common.typeutils.TypeSerializer;
import org.apache.flink.api.common.typeutils.base.StringSerializer;
import org.apache.flink.runtime.state.KeyGroupRange;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import java.io.BufferedReader;
import java.io.File;
import java.io.IOException;
import java.io.InputStreamReader;
import java.nio.charset.StandardCharsets;
import java.security.SecureRandom;
import java.util.ArrayList;
import java.util.List;
import java.util.Map;
import java.util.regex.Pattern;

/**
 * NativeUtil
 *
 * @since BeiMing 25.0.T1
 */
public class NativeUtil {
    private static NativeUtil instance;
    private static boolean initialized = false;
    private static final String JNI_LIBRARY_FILE_NAME = "libockdbjni-linux64.so";
    private static final String DATA_RELATIVE_PATH = "../../data";
    private static final String NATIVE_RELATIVE_PATH = "../../../build/src/core/jni/";
    private static final String DATA_DEFAULT_DIR_NAME = "job_test_op_uuid_";
    private static final List<String> textDataElements = new ArrayList<>();
    private static final Logger LOGGER = LoggerFactory.getLogger(NativeUtil.class);

    private String instanceBasePath;
    private int taskSlotFlag;

    public static synchronized NativeUtil getInstance() {
        if (instance == null) {
            instance = new NativeUtil();
        }
        return instance;
    }

    public void setTaskSlotFlag(int taskSlotFlag) {
        this.taskSlotFlag = taskSlotFlag;
    }

    /**
     * 加载so
     *
     * @throws IOException io异常
     */
    public void loadLibrary() throws IOException {
        if (initialized) {
            return;
        }
        synchronized (NativeUtil.class) {
            if (!initialized) {
                File parentDir = new File(NATIVE_RELATIVE_PATH);
                if (!parentDir.exists()) {
                    throw new IOException("Directory: " + parentDir.getAbsolutePath() + " does not exist!");
                }
                File soFile = new File(parentDir, JNI_LIBRARY_FILE_NAME);
                if (!soFile.exists()) {
                    throw new IOException("File " + soFile.getAbsolutePath() + " does not exist.");
                }
                System.load(soFile.getCanonicalPath());
                LOGGER.info("success load {} from local file", JNI_LIBRARY_FILE_NAME);
                initialized = true;
            }
        }
    }

    public List<String> getTextDataElements(int elemNums) {
        if (!textDataElements.isEmpty() && textDataElements.size() == elemNums) {
            return textDataElements;
        }
        synchronized (NativeUtil.class) {
            if (textDataElements.isEmpty() || textDataElements.size() != elemNums) {
                if (!textDataElements.isEmpty()) {
                    textDataElements.clear();
                }
                // 7MB*3 数据，触发单segment 16MB写slice
                String charsTemp = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
                int length = 7 * 1024 * 1024;
                for (int i = 0; i < elemNums; i++) {
                    StringBuilder randomString = new StringBuilder(length);
                    SecureRandom random = new SecureRandom();
                    for (int j = 0; j < length; j++) {
                        int idx = random.nextInt(charsTemp.length());
                        randomString.append(charsTemp.charAt(idx));
                    }
                    textDataElements.add(randomString.toString());
                }
            }
        }
        return textDataElements;
    }

    /**
     * createKVTableImpl
     *
     * @param tableName         表名
     * @param sliceMemWaterMark 水位线
     * @return KVTableImpl<String, String>
     * @throws IOException IO异常
     */
    public KVTableImpl<String, String> createKVTableImpl(String tableName, float sliceMemWaterMark)
            throws IOException {
        int numberOfKeyGroups = 2;
        TypeSerializer<String> keySerializer = new StringSerializer();
        TypeSerializer<String> valueSerializer = new StringSerializer();
        TableSerializer<String, String> tableSerializer = new TableSerializer<>(keySerializer, valueSerializer);
        KVTableDescription<String, String> description = new KVTableDescription<>(tableName, numberOfKeyGroups,
                tableSerializer);
        BoostStateDB db = createBoostStateDB(numberOfKeyGroups, sliceMemWaterMark);
        return new KVTableImpl<>(db, description);
    }

    /**
     * createKMapTableImpl
     *
     * @param tableName         表名
     * @param sliceMemWaterMark 水位线
     * @return KMapTableImpl<String, String, String>
     * @throws IOException IO异常
     */
    public KMapTableImpl<String, String, String> createKMapTableImpl(String tableName, float sliceMemWaterMark)
            throws IOException {
        int numberOfKeyGroups = 2;
        TypeSerializer<String> keySerializer = new StringSerializer();
        TypeSerializer<String> subKeySerializer = new StringSerializer();
        TypeSerializer<String> valueSerializer = new StringSerializer();
        SubTableSerializer<String, String, String> subTableSerializer = new SubTableSerializer<>(keySerializer,
                subKeySerializer, valueSerializer);
        KMapTableDescription<String, String, String> kMapTableDescription = new KMapTableDescription<>(tableName,
                numberOfKeyGroups, subTableSerializer);
        BoostStateDB db = createBoostStateDB(numberOfKeyGroups, sliceMemWaterMark);
        return new KMapTableImpl<>(db, kMapTableDescription);
    }

    /**
     * createKListTableImpl
     *
     * @param tableName         表名
     * @param sliceMemWaterMark 水位线
     * @return KListTableImpl<String, String>
     * @throws IOException IO异常
     */
    public KListTableImpl<String, String> createKListTableImpl(String tableName, float sliceMemWaterMark)
            throws IOException {
        int numberOfKeyGroups = 2;
        TypeSerializer<String> keySerializer = new StringSerializer();
        TypeSerializer<String> valueSerializer = new StringSerializer();
        TableSerializer<String, String> tableSerializer = new TableSerializer<>(keySerializer, valueSerializer);
        KListTableDescription<String, String> kListTableDescription = new KListTableDescription<>(tableName,
                numberOfKeyGroups, tableSerializer);
        BoostStateDB db = createBoostStateDB(numberOfKeyGroups, sliceMemWaterMark);
        return new KListTableImpl<>(db, kListTableDescription);
    }

    /**
     * createNsKVTableImpl
     *
     * @param tableName 表名
     * @return NsKVTableImpl<String, String, String>
     * @throws IOException IO异常
     */
    public NsKVTableImpl<String, String, String> createNsKVTableImpl(String tableName) throws IOException {
        int numberOfKeyGroups = 2;
        TypeSerializer<String> keySerializer = new StringSerializer();
        TypeSerializer<String> valueSerializer = new StringSerializer();
        TypeSerializer<String> subKeySerializer = new StringSerializer();
        SubTableSerializer<String, String, String> subTableSerializer = new SubTableSerializer<>(keySerializer,
                subKeySerializer, valueSerializer);
        KMapTableDescription<String, String, String> description = new KMapTableDescription<>(tableName,
                numberOfKeyGroups, subTableSerializer);
        BoostStateDB db = createBoostStateDB(numberOfKeyGroups, 0.8F);
        return new NsKVTableImpl<>(db, description);
    }

    /**
     * createNsKMapTableImpl
     *
     * @param tableName 表名
     * @return NsKMapTableImpl<String, String, String, String, Map < String, String>>
     * @throws IOException IO异常
     */
    public NsKMapTableImpl<String, String, String, String, Map<String, String>> createNsKMapTableImpl(
            String tableName) throws IOException {
        int numberOfKeyGroups = 2;
        TypeSerializer<String> keySerializer = new StringSerializer();
        TypeSerializer<String> nsSerializer = new StringSerializer();
        TypeSerializer<String> subKeySerializer = new StringSerializer();
        TypeSerializer<String> valueSerializer = new StringSerializer();
        SubTableSerializer<KeyPair<String, String>, String, String> subTableSerializer =
                new SubTableSerializer<>(new KeyPairSerializer<>(keySerializer, nsSerializer), subKeySerializer,
                        valueSerializer);
        NsKMapTableDescription<String, String, String, String> description = new NsKMapTableDescription<>(tableName,
                numberOfKeyGroups, subTableSerializer);
        BoostStateDB db = createBoostStateDB(numberOfKeyGroups, 0.8F);
        return new NsKMapTableImpl<>(db, description);
    }

    /**
     * createNsKListTableImpl
     *
     * @param tableName 表名
     * @return NsKListTableImpl<String, String, String>
     * @throws IOException IO异常
     */
    public NsKListTableImpl<String, String, String> createNsKListTableImpl(String tableName) throws IOException {
        int numberOfKeyGroups = 2;
        TypeSerializer<String> keySerializer = new StringSerializer();
        TypeSerializer<String> nsSerializer = new StringSerializer();
        TypeSerializer<String> valueSerializer = new StringSerializer();
        TableSerializer<KeyPair<String, String>, String> tableSerializer =
                new TableSerializer<>(new KeyPairSerializer<>(keySerializer, nsSerializer), valueSerializer);
        NsKListTableDescription<String, String, String> description = new NsKListTableDescription<>(tableName,
                numberOfKeyGroups, tableSerializer);
        BoostStateDB db = createBoostStateDB(numberOfKeyGroups, 0.8F);
        return new NsKListTableImpl<>(db, description);
    }

    /**
     * createPQTable
     *
     * @return BoostPQTable
     * @throws IOException IO异常
     */
    public BoostPQTable createPQTable() throws IOException {
        int numberOfKeyGroups = 2;
        BoostStateDB db = createBoostStateDB(numberOfKeyGroups, 0.8F);
        return new BoostPQTable(db, "_timer_state/event_window-timers");
    }

    /**
     * createBoostStateDB
     *
     * @param numberOfKeyGroups numberOfKeyGroups
     * @param sliceMemWaterMark sliceMemWaterMark
     * @return BoostStateDB
     * @throws IOException IO异常
     */
    public BoostStateDB createBoostStateDB(int numberOfKeyGroups, float sliceMemWaterMark) throws IOException {
        KeyGroupRange keyGroupRange = new KeyGroupRange(0, numberOfKeyGroups - 1);
        BoostConfig boostConfig = new BoostConfig(keyGroupRange);
        boostConfig.setMaxParallelism(numberOfKeyGroups);
        String executionID = String.valueOf(taskSlotFlag);
        File instanceBasePathFile = new File(DATA_RELATIVE_PATH, DATA_DEFAULT_DIR_NAME + executionID);
        instanceBasePathFile.mkdirs();
        instanceBasePath = instanceBasePathFile.getCanonicalPath();
        boostConfig.setInstanceBasePath(instanceBasePath);
        boostConfig.setBackendUID(executionID);
        boostConfig.setLsmCompactionSwitch(1);
        boostConfig.setLsmStoreCompressionPolicy("lz4");
        boostConfig.setLsmStoreCompressionLevelPolicy("lz4,lz4,lz4");
        boostConfig.setTtlFilterSwitch(false);
        boostConfig.setCacheFilterAndIndexSwitch(true);
        boostConfig.setKVSeparateSwitch(false);
        boostConfig.setKVSeparateThreshold(200);
        boostConfig.setCacheFilterAndIndexRatio(0.0F);
        boostConfig.setEnableBloomFilter(true);
        boostConfig.setExpectedKeyCount(8000000);
        boostConfig.setTaskSlotFlag(taskSlotFlag);
        long taskSlotMemoryLimit = 128 * 1024 * 1024;
        boostConfig.setTaskSlotMemoryLimit(taskSlotMemoryLimit);
        // db内存总大小，100MB，fresh table 32MB，slice table 48MB，store mem 20MB
        boostConfig.setTotalDBSize(taskSlotMemoryLimit);
        boostConfig.setFileMemoryFraction(0.2F);
        // 设置写LSM的水位线
        boostConfig.setSliceMemWaterMark(sliceMemWaterMark);
        // 设置允许借用的heap大小
        boostConfig.setBorrowHeapSize(512 * 1024 * 1024);
        return new BoostStateDB(boostConfig);
    }

    /**
     * removeInstancePathData 移除lsm目录
     */
    public static void removeInstancePathData() {
        File instanceBasePathFiles = new File(DATA_RELATIVE_PATH);
        if (!instanceBasePathFiles.exists()) {
            LOGGER.error("{} does not exist!", instanceBasePathFiles.getAbsolutePath());
            return;
        }
        deleteFiles(instanceBasePathFiles);
    }

    /**
     * isExistInstanceBasePathFile
     *
     * @return boolean
     * @throws IOException IOException
     */
    public boolean isExistInstanceBasePathFile() throws IOException {
        List<String> commandResult = new ArrayList<>();
        synchronized (NativeUtil.class) {
            // 由于进程无法读取子文件，通过shell命令列举
            BufferedReader reader = null;
            try {
                ProcessBuilder processBuilder = new ProcessBuilder().command("ls")
                        .directory(new java.io.File(instanceBasePath)).redirectErrorStream(true);
                Process process = processBuilder.start();
                reader = new BufferedReader(new InputStreamReader(process.getInputStream(), StandardCharsets.UTF_8));
                String line;
                while ((line = reader.readLine()) != null) {
                    commandResult.add(line);
                }
                LOGGER.debug("ls file path command result list: {}", commandResult);
                int exitCode = process.waitFor();
                if (exitCode != 0) {
                    LOGGER.error("Command failed with exit code {}, instanceBasePath: {}", exitCode, instanceBasePath);
                    return false;
                }
            } catch (IOException | InterruptedException e) {
                e.printStackTrace();
                LOGGER.error("Command failed, instanceBasePath: {}, exception: {}", instanceBasePath, e.getMessage());
            } finally {
                if (reader != null) {
                    reader.close();
                }
            }
        }
        for (String fileName : commandResult) {
            if (Pattern.matches("^" + taskSlotFlag + ".*\\.sst$", fileName)) {
                return true;
            }
        }
        return false;
    }

    /**
     * deleteFiles 循环删除子目录和文件
     *
     * @param fileDir 目录
     * @return 是否删除
     */
    public static boolean deleteFiles(File fileDir) {
        File[] files = fileDir.listFiles();
        if (files == null) {
            return true;
        }
        for (File file : files) {
            String filePath = file.getAbsolutePath();
            if (file.isDirectory()) {
                if (!deleteFiles(file)) {
                    LOGGER.error("Failed to delete {}", filePath);
                    return false;
                }
            } else {
                if (!file.delete()) {
                    LOGGER.error("Failed to delete {}", filePath);
                    return false;
                }
            }
        }
        return true;
    }
}