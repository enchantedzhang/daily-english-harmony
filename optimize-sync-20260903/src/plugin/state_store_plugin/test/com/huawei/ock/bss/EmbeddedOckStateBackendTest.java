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

package com.huawei.ock.bss;

import static org.junit.Assert.fail;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyDouble;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.ArgumentMatchers.anyString;

import com.huawei.ock.bss.common.BoostStateDB;
import com.huawei.ock.bss.common.memory.SizeAllocator;
import com.huawei.ock.bss.metric.BoostNativeMetricOptions;
import com.huawei.ock.bss.ockdb.NativeLibraryLoader;
import com.huawei.ock.bss.ockdb.OckDBLog;

import org.apache.flink.api.common.JobID;
import org.apache.flink.api.common.typeutils.TypeSerializer;
import org.apache.flink.api.common.typeutils.base.StringSerializer;
import org.apache.flink.configuration.Configuration;
import org.apache.flink.configuration.IllegalConfigurationException;
import org.apache.flink.configuration.MemorySize;
import org.apache.flink.configuration.TaskManagerOptions;
import org.apache.flink.core.execution.SavepointFormatType;
import org.apache.flink.core.fs.CloseableRegistry;
import org.apache.flink.metrics.MetricGroup;
import org.apache.flink.runtime.execution.Environment;
import org.apache.flink.runtime.io.disk.iomanager.IOManager;
import org.apache.flink.runtime.memory.MemoryManager;
import org.apache.flink.runtime.memory.OpaqueMemoryResource;
import org.apache.flink.runtime.metrics.groups.GenericMetricGroup;
import org.apache.flink.runtime.query.TaskKvStateRegistry;
import org.apache.flink.runtime.state.AbstractKeyedStateBackend;
import org.apache.flink.runtime.state.KeyGroupRange;
import org.apache.flink.runtime.state.KeyedStateHandle;
import org.apache.flink.runtime.state.LocalRecoveryConfig;
import org.apache.flink.runtime.state.TaskStateManager;
import org.apache.flink.runtime.state.ttl.TtlTimeProvider;
import org.apache.flink.runtime.taskmanager.TaskManagerRuntimeInfo;
import org.apache.flink.util.UserCodeClassLoader;
import org.apache.flink.util.function.ThrowingRunnable;
import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.BeforeClass;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.powermock.api.mockito.PowerMockito;
import org.powermock.core.classloader.annotations.PrepareForTest;
import org.powermock.modules.junit4.PowerMockRunner;
import org.powermock.reflect.Whitebox;

import java.io.File;
import java.io.IOException;
import java.util.Collection;
import java.util.HashSet;

/**
 * EmbeddedOckStateBackendTest
 *
 * @since BeiMing 25.0.T1
 */
@RunWith(PowerMockRunner.class)
@PrepareForTest(value = {
    EmbeddedOckStateBackend.class, NativeLibraryLoader.class, OckDBLog.class,
    BoostStateDB.class, OckDBKeyedStateBackend.class
})
public class EmbeddedOckStateBackendTest {
    private static final String WINDOWS_TEST_LOCAL_DIR = "D:\\tmp\\ockdb\\db";

    private static final String WINDOWS_TEST_LOG_FILE = "D:\\tmp\\ockdb\\log\\bss.log";

    private static final String LINUX_TEST_LOCAL_DIR = "/tmp/ock/db";

    private static final String LINUX_TEST_LOG_FILE = "/tmp/ock/log/bss.log";

    private static final String NO_EXIST_PATH = "/no/exist/path";

    private static String dbPath;

    private static String logFilePath;

    private Configuration config;

    @BeforeClass
    public static void init() {
        String osName = System.getProperty("os.name").toLowerCase();

        if (osName.contains("windows")) {
            dbPath = WINDOWS_TEST_LOCAL_DIR;
            logFilePath = WINDOWS_TEST_LOG_FILE;
        } else if (osName.contains("linux")) {
            dbPath = LINUX_TEST_LOCAL_DIR;
            logFilePath = LINUX_TEST_LOG_FILE;
        } else {
            throw new RuntimeException("Unsupported operating system: " + osName);
        }

        File logDir = new File(dbPath);
        if (!logDir.exists()) {
            logDir.mkdirs();
        }

        File logFile = new File(logFilePath);
        if (!logFile.exists()) {
            // 确保父目录存在
            File parentDir = logFile.getParentFile();
            if (!parentDir.exists()) {
                parentDir.mkdirs();
            }

            try {
                logFile.createNewFile();
            } catch (IOException e) {
                throw new RuntimeException("Failed to create log file: " + logFilePath, e);
            }
        }
    }

    @Before
    public void setUp() {
        config = new Configuration();
        String osName = System.getProperty("os.name").toLowerCase();
        if (osName.contains("windows")) {
            config.setString(OckDBOptions.LOCAL_DIRECTORIES, WINDOWS_TEST_LOCAL_DIR);
            config.setString(OckDBOptions.OCKDB_JNI_LOG_DIRECTORY, WINDOWS_TEST_LOG_FILE);
            config.setString(OckDBOptions.BACKUP_DIRECTORY, WINDOWS_TEST_LOCAL_DIR);
        } else if (osName.contains("linux")) {
            config.setString(OckDBOptions.LOCAL_DIRECTORIES, LINUX_TEST_LOCAL_DIR);
            config.setString(OckDBOptions.OCKDB_JNI_LOG_DIRECTORY, LINUX_TEST_LOG_FILE);
            config.setString(OckDBOptions.BACKUP_DIRECTORY, LINUX_TEST_LOCAL_DIR);
        } else {
            throw new RuntimeException("Unsupported operating system: " + osName);
        }
        config.set(TaskManagerOptions.TOTAL_PROCESS_MEMORY, MemorySize.parse("5gb"));
        config.set(OckDBOptions.OCKDB_JNI_SLICE_WATERMARK_RATIO, 0.8F);
    }

    @After
    public void tearDown() {

    }

    @Test
    public void test_create_embedded_ock_state_backend() {
        try {
            OckDBStateBackendFactory factory = new OckDBStateBackendFactory();
            EmbeddedOckStateBackend stateBackend = factory.createFromConfig(config, this.getClass().getClassLoader());
            Assert.assertNotNull(stateBackend);
        } catch (Exception e) {
            fail("Should not have thrown any exception");
        }
    }

    @Test
    public void test_zero_copy_switch_defaults_to_disabled() {
        Assert.assertFalse(config.get(OckDBOptions.OCKDB_ZERO_COPY_SWITCH));
        config.set(OckDBOptions.OCKDB_ZERO_COPY_SWITCH, true);
        Assert.assertTrue(config.get(OckDBOptions.OCKDB_ZERO_COPY_SWITCH));
    }

    @Test(expected = IllegalConfigurationException.class)
    public void test_create_state_backend_with_log_level_beyond_left_bound() throws IOException {
        OckDBStateBackendFactory factory = new OckDBStateBackendFactory();
        config.set(OckDBOptions.OCKDB_JNI_LOG_LEVEL, 0);
        EmbeddedOckStateBackend stateBackend = factory.createFromConfig(config, this.getClass().getClassLoader());
        Assert.assertNotNull(stateBackend);
    }

    @Test
    public void test_create_state_backend_with_log_level_normal() throws IOException {
        for (int i = 1; i <= 4; i++) {
            OckDBStateBackendFactory factory = new OckDBStateBackendFactory();
            config.set(OckDBOptions.OCKDB_JNI_LOG_LEVEL, i);
            EmbeddedOckStateBackend stateBackend = factory.createFromConfig(config, this.getClass().getClassLoader());
            Assert.assertNotNull(stateBackend);
        }
    }

    @Test(expected = IllegalConfigurationException.class)
    public void test_create_state_backend_with_log_level_beyond_right_bound() throws IOException {
        OckDBStateBackendFactory factory = new OckDBStateBackendFactory();
        config.set(OckDBOptions.OCKDB_JNI_LOG_LEVEL, 5);
        EmbeddedOckStateBackend stateBackend = factory.createFromConfig(config, this.getClass().getClassLoader());
        Assert.assertNotNull(stateBackend);
    }

    @Test(expected = IllegalConfigurationException.class)
    public void test_create_state_backend_with_log_num_beyond_left_bound() throws IOException {
        OckDBStateBackendFactory factory = new OckDBStateBackendFactory();
        config.set(OckDBOptions.OCKDB_JNI_LOG_NUM, 9);
        EmbeddedOckStateBackend stateBackend = factory.createFromConfig(config, this.getClass().getClassLoader());
        Assert.assertNotNull(stateBackend);
    }

    @Test
    public void test_create_state_backend_with_log_num_normal() throws IOException {
        for (int i = 10; i <= 50; i++) {
            OckDBStateBackendFactory factory = new OckDBStateBackendFactory();
            config.set(OckDBOptions.OCKDB_JNI_LOG_NUM, i);
            EmbeddedOckStateBackend stateBackend = factory.createFromConfig(config, this.getClass().getClassLoader());
            Assert.assertNotNull(stateBackend);
        }
    }

    @Test(expected = IllegalConfigurationException.class)
    public void test_create_state_backend_with_log_num_beyond_right_bound() throws IOException {
        OckDBStateBackendFactory factory = new OckDBStateBackendFactory();
        config.set(OckDBOptions.OCKDB_JNI_LOG_NUM, 51);
        EmbeddedOckStateBackend stateBackend = factory.createFromConfig(config, this.getClass().getClassLoader());
        Assert.assertNotNull(stateBackend);
    }

    @Test(expected = IllegalConfigurationException.class)
    public void test_create_state_backend_with_log_size_beyond_left_bound() throws IOException {
        OckDBStateBackendFactory factory = new OckDBStateBackendFactory();
        config.set(OckDBOptions.OCKDB_JNI_LOG_SIZE, MemorySize.parse("9mb"));
        EmbeddedOckStateBackend stateBackend = factory.createFromConfig(config, this.getClass().getClassLoader());
        Assert.assertNotNull(stateBackend);
    }

    @Test
    public void test_create_state_backend_with_log_size_normal() throws IOException {
        for (int i = 10; i <= 50; i++) {
            OckDBStateBackendFactory factory = new OckDBStateBackendFactory();
            String logSize = i + "mb";
            config.set(OckDBOptions.OCKDB_JNI_LOG_SIZE, MemorySize.parse(logSize));
            EmbeddedOckStateBackend stateBackend = factory.createFromConfig(config, this.getClass().getClassLoader());
            Assert.assertNotNull(stateBackend);
        }
    }

    @Test(expected = IllegalConfigurationException.class)
    public void test_create_state_backend_with_log_size_beyond_right_bound() throws IOException {
        OckDBStateBackendFactory factory = new OckDBStateBackendFactory();
        config.set(OckDBOptions.OCKDB_JNI_LOG_SIZE, MemorySize.parse("51mb"));
        EmbeddedOckStateBackend stateBackend = factory.createFromConfig(config, this.getClass().getClassLoader());
        Assert.assertNotNull(stateBackend);
    }

    @Test
    public void test_create_keyed_state_backend() {
        try {
            // prepare construct param
            OckDBStateBackendFactory factory = new OckDBStateBackendFactory();
            EmbeddedOckStateBackend stateBackend = factory.createFromConfig(config, this.getClass().getClassLoader());
            Environment env = PowerMockito.mock(Environment.class);
            JobID jobID = JobID.fromHexString("fd72014d4c864993a2e5a9287b4a9c5d");
            String operatorIdentifier = "test";
            TypeSerializer<String> keySerializer = new StringSerializer();
            int numberOfKeyGroups = 8;
            KeyGroupRange keyGroupRange = new KeyGroupRange(0, 7);
            TaskKvStateRegistry kvStateRegistry = PowerMockito.mock(TaskKvStateRegistry.class);
            TtlTimeProvider ttlTimeProvider = TtlTimeProvider.DEFAULT;
            MetricGroup metricGroup = PowerMockito.mock(GenericMetricGroup.class);
            Collection<KeyedStateHandle> stateHandles = new HashSet<>();
            CloseableRegistry cancelStreamRegistry = new CloseableRegistry();
            double managedMemoryFraction = 0.8;

            // prepare mock
            NativeLibraryLoader mockLoader = PowerMockito.mock(NativeLibraryLoader.class);
            Whitebox.setInternalState(NativeLibraryLoader.class, "INSTANCE", mockLoader);
            PowerMockito.doNothing().when(mockLoader, "loadLibrary", anyString());
            TaskManagerRuntimeInfo mockRuntimeInfo = PowerMockito.mock(TaskManagerRuntimeInfo.class);
            PowerMockito.when(env, "getTaskManagerInfo").thenReturn(mockRuntimeInfo);
            String[] tmpDir = new String[] {dbPath};
            PowerMockito.when(mockRuntimeInfo, "getTmpDirectories").thenReturn(tmpDir);
            PowerMockito.mockStatic(OckDBLog.class);
            PowerMockito.when(OckDBLog.class, "initial", anyString(), anyInt(), anyInt(), anyInt()).thenReturn(1L);
            TaskStateManager mockStateManager = PowerMockito.mock(TaskStateManager.class);
            PowerMockito.when(env, "getTaskStateManager").thenReturn(mockStateManager);
            LocalRecoveryConfig lrConfigMock = PowerMockito.mock(LocalRecoveryConfig.class);
            PowerMockito.when(mockStateManager, "createLocalRecoveryConfig").thenReturn(lrConfigMock);
            PowerMockito.when(lrConfigMock, "isLocalRecoveryEnabled").thenReturn(true);
            MemoryManager memoryManagerMock = PowerMockito.mock(MemoryManager.class);
            PowerMockito.when(env, "getMemoryManager").thenReturn(memoryManagerMock);
            PowerMockito.when(
                    memoryManagerMock.getSharedMemoryResourceForManagedMemory(anyString(), any(), anyDouble()))
                .thenReturn(createMemoryResourceMock());
            OckDBKeyedStateBackendBuilder<String> builderMock = PowerMockito.mock(OckDBKeyedStateBackendBuilder.class);
            PowerMockito.whenNew(OckDBKeyedStateBackendBuilder.class).withAnyArguments().thenReturn(builderMock);
            OckDBKeyedStateBackend<String> backendMock = PowerMockito.mock(OckDBKeyedStateBackend.class);
            PowerMockito.when(builderMock, "build").thenReturn(backendMock);
            PowerMockito.when(builderMock, "setEnableIncrementalCheckpointing", anyBoolean()).thenReturn(builderMock);
            PowerMockito.when(builderMock, "setNumberOfTransferringThreads", anyInt()).thenReturn(builderMock);
            PowerMockito.when(builderMock, "setTaskSlotFlag", anyInt()).thenReturn(builderMock);
            PowerMockito.when(builderMock, "setTaskSlotMemoryLimit", anyLong()).thenReturn(builderMock);
            PowerMockito.when(builderMock, "setSlotManagedMemoryFraction", anyDouble()).thenReturn(builderMock);
            PowerMockito.when(builderMock, "setJobID", anyString()).thenReturn(builderMock);
            PowerMockito.when(env, "getJobID").thenReturn(jobID);
            PowerMockito.when(builderMock, "setNativeMetricOptions", any(BoostNativeMetricOptions.class))
                .thenReturn(builderMock);
            // run the test
            AbstractKeyedStateBackend<String> backend = stateBackend.createKeyedStateBackend(env, jobID,
                operatorIdentifier, keySerializer, numberOfKeyGroups,
                keyGroupRange, kvStateRegistry, ttlTimeProvider, metricGroup, stateHandles, cancelStreamRegistry,
                managedMemoryFraction);
            Assert.assertNotNull(backend);
        } catch (Exception e) {
            e.printStackTrace();
            fail("Should not have thrown any exception");
        }
    }

    @Test(expected = IllegalConfigurationException.class)
    public void test_create_with_bf_key_count_beyond_left_bound() throws Exception {
        config.set(OckDBOptions.OCKDB_BLOOM_FILTER_EXPECTED_KEY_COUNT, 999999);
        buildKeyedStateBackEnd();
    }

    @Test(expected = IllegalConfigurationException.class)
    public void test_create_with_bf_key_count_beyond_right_bound() throws Exception {
        config.set(OckDBOptions.OCKDB_BLOOM_FILTER_EXPECTED_KEY_COUNT, 10000001);
        buildKeyedStateBackEnd();
    }

    @Test(expected = IllegalConfigurationException.class)
    public void test_create_with_file_mem_fraction_beyond_left_bound() throws Exception {
        config.set(OckDBOptions.OCKDB_FILE_MEMORY_FRACTION, 0.09f);
        buildKeyedStateBackEnd();
    }

    @Test(expected = IllegalConfigurationException.class)
    public void test_create_with_file_mem_fraction_beyond_right_bound() throws Exception {
        config.set(OckDBOptions.OCKDB_FILE_MEMORY_FRACTION, 0.51f);
        buildKeyedStateBackEnd();
    }

    @Test(expected = IllegalConfigurationException.class)
    public void test_create_with_slice_watermark_beyond_left_bound() throws Exception {
        config.set(OckDBOptions.OCKDB_JNI_SLICE_WATERMARK_RATIO, -0.01f);
        buildKeyedStateBackEnd();
    }

    @Test(expected = IllegalConfigurationException.class)
    public void test_create_with_slice_watermark_beyond_right_bound() throws Exception {
        config.set(OckDBOptions.OCKDB_JNI_SLICE_WATERMARK_RATIO, 1.01f);
        buildKeyedStateBackEnd();
    }

    private void buildKeyedStateBackEnd() throws Exception {
        // prepare construct param
        OckDBStateBackendFactory factory = new OckDBStateBackendFactory();
        EmbeddedOckStateBackend stateBackend = factory.createFromConfig(config, this.getClass().getClassLoader());
        Environment env = PowerMockito.mock(Environment.class);
        JobID jobID = JobID.fromHexString("fd72014d4c864993a2e5a9287b4a9c5d");
        String operatorIdentifier = "test";
        TypeSerializer<String> keySerializer = new StringSerializer();
        int numberOfKeyGroups = 8;
        KeyGroupRange keyGroupRange = new KeyGroupRange(0, 7);
        TaskKvStateRegistry kvStateRegistry = PowerMockito.mock(TaskKvStateRegistry.class);
        TtlTimeProvider ttlTimeProvider = TtlTimeProvider.DEFAULT;
        MetricGroup metricGroup = PowerMockito.mock(GenericMetricGroup.class);
        Collection<KeyedStateHandle> stateHandles = new HashSet<>();
        CloseableRegistry cancelStreamRegistry = new CloseableRegistry();
        double managedMemoryFraction = 0.8;

        // prepare mock
        NativeLibraryLoader mockLoader = PowerMockito.mock(NativeLibraryLoader.class);
        Whitebox.setInternalState(NativeLibraryLoader.class, "INSTANCE", mockLoader);
        PowerMockito.doNothing().when(mockLoader, "loadLibrary", anyString());
        TaskManagerRuntimeInfo mockRuntimeInfo = PowerMockito.mock(TaskManagerRuntimeInfo.class);
        PowerMockito.when(env, "getTaskManagerInfo").thenReturn(mockRuntimeInfo);
        String[] tmpDir = new String[] {dbPath};
        PowerMockito.when(mockRuntimeInfo, "getTmpDirectories").thenReturn(tmpDir);
        PowerMockito.mockStatic(OckDBLog.class);
        PowerMockito.when(OckDBLog.class, "initial", anyString(), anyInt(), anyInt(), anyInt()).thenReturn(1L);
        TaskStateManager mockStateManager = PowerMockito.mock(TaskStateManager.class);
        PowerMockito.when(env, "getTaskStateManager").thenReturn(mockStateManager);
        LocalRecoveryConfig lrConfigMock = PowerMockito.mock(LocalRecoveryConfig.class);
        PowerMockito.when(mockStateManager, "createLocalRecoveryConfig").thenReturn(lrConfigMock);
        MemoryManager memoryManagerMock = PowerMockito.mock(MemoryManager.class);
        PowerMockito.when(env, "getMemoryManager").thenReturn(memoryManagerMock);
        PowerMockito.when(
                memoryManagerMock.getSharedMemoryResourceForManagedMemory(anyString(), any(), anyDouble()))
            .thenReturn(createMemoryResourceMock());
        UserCodeClassLoader userCodeClassLoaderMock = PowerMockito.mock(UserCodeClassLoader.class);
        PowerMockito.when(env, "getUserCodeClassLoader").thenReturn(userCodeClassLoaderMock);
        PowerMockito.when(env, "getJobID").thenReturn(jobID);
        stateBackend.createKeyedStateBackend(env, jobID, operatorIdentifier, keySerializer, numberOfKeyGroups,
            keyGroupRange, kvStateRegistry, ttlTimeProvider, metricGroup, stateHandles, cancelStreamRegistry,
            managedMemoryFraction);
    }

    private OpaqueMemoryResource<AutoCloseable> createMemoryResourceMock() {
        ThrowingRunnable<Exception> disposer = () -> {
        };
        return new OpaqueMemoryResource<>(new SizeAllocator("test", 1073741824), 1073741824, disposer);
    }

    @Test
    public void test_create_keyed_state_backend_when_local_dir_null() {
        try {
            // prepare construct param
            OckDBStateBackendFactory factory = new OckDBStateBackendFactory();
            EmbeddedOckStateBackend stateBackend = factory.createFromConfig(config, this.getClass().getClassLoader());
            Environment env = PowerMockito.mock(Environment.class);
            JobID jobID = JobID.fromHexString("fd72014d4c864993a2e5a9287b4a9c5d");
            String operatorIdentifier = "test";
            TypeSerializer<String> keySerializer = new StringSerializer();
            int numberOfKeyGroups = 8;
            KeyGroupRange keyGroupRange = new KeyGroupRange(0, 7);
            TaskKvStateRegistry kvStateRegistry = PowerMockito.mock(TaskKvStateRegistry.class);
            TtlTimeProvider ttlTimeProvider = TtlTimeProvider.DEFAULT;
            MetricGroup metricGroup = PowerMockito.mock(GenericMetricGroup.class);
            Collection<KeyedStateHandle> stateHandles = new HashSet<>();
            CloseableRegistry cancelStreamRegistry = new CloseableRegistry();
            double managedMemoryFraction = 0.8;

            // prepare mock
            NativeLibraryLoader mockLoader = PowerMockito.mock(NativeLibraryLoader.class);
            Whitebox.setInternalState(NativeLibraryLoader.class, "INSTANCE", mockLoader);
            PowerMockito.doNothing().when(mockLoader, "loadLibrary", anyString());
            TaskManagerRuntimeInfo mockRuntimeInfo = PowerMockito.mock(TaskManagerRuntimeInfo.class);
            PowerMockito.when(env, "getTaskManagerInfo").thenReturn(mockRuntimeInfo);
            String[] tmpDir = new String[] {dbPath};
            PowerMockito.when(mockRuntimeInfo, "getTmpDirectories").thenReturn(tmpDir);
            PowerMockito.mockStatic(OckDBLog.class);
            PowerMockito.when(OckDBLog.class, "initial", anyString(), anyInt(), anyInt(), anyInt()).thenReturn(1L);
            TaskStateManager mockStateManager = PowerMockito.mock(TaskStateManager.class);
            PowerMockito.when(env, "getTaskStateManager").thenReturn(mockStateManager);
            LocalRecoveryConfig lrConfigMock = PowerMockito.mock(LocalRecoveryConfig.class);
            PowerMockito.when(mockStateManager, "createLocalRecoveryConfig").thenReturn(lrConfigMock);
            OckDBKeyedStateBackendBuilder<String> builderMock = PowerMockito.mock(OckDBKeyedStateBackendBuilder.class);
            PowerMockito.whenNew(OckDBKeyedStateBackendBuilder.class).withAnyArguments().thenReturn(builderMock);
            OckDBKeyedStateBackend<String> backendMock = PowerMockito.mock(OckDBKeyedStateBackend.class);
            PowerMockito.when(builderMock, "build").thenReturn(backendMock);
            PowerMockito.when(builderMock, "setEnableIncrementalCheckpointing", anyBoolean()).thenReturn(builderMock);
            PowerMockito.when(builderMock, "setNumberOfTransferringThreads", anyInt()).thenReturn(builderMock);
            Whitebox.setInternalState(stateBackend, "localOckDbDirectories", (Object[]) null);
            PowerMockito.when(mockRuntimeInfo, "getConfiguration").thenReturn(config);
            IOManager ioManagerMock = PowerMockito.mock(IOManager.class);
            PowerMockito.when(env, "getIOManager").thenReturn(ioManagerMock);
            File[] dbPaths = new File[] {new File(dbPath)};
            PowerMockito.when(ioManagerMock, "getSpillingDirectories").thenReturn(dbPaths);
            MemoryManager memoryManagerMock = PowerMockito.mock(MemoryManager.class);
            PowerMockito.when(env, "getMemoryManager").thenReturn(memoryManagerMock);
            PowerMockito.when(
                    memoryManagerMock.getSharedMemoryResourceForManagedMemory(anyString(), any(), anyDouble()))
                .thenReturn(createMemoryResourceMock());
            PowerMockito.when(builderMock, "setTaskSlotFlag", anyInt()).thenReturn(builderMock);
            PowerMockito.when(builderMock, "setTaskSlotMemoryLimit", anyLong()).thenReturn(builderMock);
            PowerMockito.when(builderMock, "setSlotManagedMemoryFraction", anyDouble()).thenReturn(builderMock);
            PowerMockito.when(builderMock, "setJobID", anyString()).thenReturn(builderMock);
            PowerMockito.when(env, "getJobID").thenReturn(jobID);
            PowerMockito.when(builderMock, "setNativeMetricOptions", any(BoostNativeMetricOptions.class))
                .thenReturn(builderMock);
            // run the test
            AbstractKeyedStateBackend<String> backend = stateBackend.createKeyedStateBackend(env, jobID,
                operatorIdentifier, keySerializer, numberOfKeyGroups,
                keyGroupRange, kvStateRegistry, ttlTimeProvider, metricGroup, stateHandles, cancelStreamRegistry,
                managedMemoryFraction);
            Assert.assertNotNull(backend);
        } catch (Exception e) {
            e.printStackTrace();
            fail("Should not have thrown any exception");
        }
    }

    @Test
    public void test_interface_for_outside() throws IOException {
        OckDBStateBackendFactory factory = new OckDBStateBackendFactory();
        EmbeddedOckStateBackend stateBackend = factory.createFromConfig(config, this.getClass().getClassLoader());
        Assert.assertTrue(stateBackend.supportsNoClaimRestoreMode());
        Assert.assertTrue(stateBackend.supportsSavepointFormat(SavepointFormatType.CANONICAL));
        Assert.assertTrue(stateBackend.supportsSavepointFormat(SavepointFormatType.NATIVE));
    }

    @Test
    public void test_construct_method_for_outside_() {
        try {
            EmbeddedOckStateBackend embeddedOckStateBackend = new EmbeddedOckStateBackend(true);
        } catch (Exception e) {
            fail("Should not throw any exception.");
        }
    }

    @Test
    public void test_interface_for_outside_1() throws IOException {
        OckDBStateBackendFactory factory = new OckDBStateBackendFactory();
        EmbeddedOckStateBackend stateBackend = factory.createFromConfig(config, this.getClass().getClassLoader());
        stateBackend.setNumberOfTransferThreads(8);
        Assert.assertEquals(8, stateBackend.getNumberOfTransferThreads());
    }

    @Test
    public void test_put_get_DB_storage_path() throws IOException {
        OckDBStateBackendFactory factory = new OckDBStateBackendFactory();
        EmbeddedOckStateBackend stateBackend = factory.createFromConfig(config, this.getClass().getClassLoader());
        stateBackend.setDbStoragePath(dbPath);
        String[] paths = stateBackend.getDbStoragePaths();
        Assert.assertEquals(paths[0], dbPath);
    }

    @Test
    public void test_setPriorityQueueStateType_normal() throws IOException {
        OckDBStateBackendFactory factory = new OckDBStateBackendFactory();
        config.set(OckDBOptions.OCKDB_PRIORITY_QUEUE_TYPE, "HEAP");
        EmbeddedOckStateBackend stateBackend = factory.createFromConfig(config, this.getClass().getClassLoader());
        Assert.assertEquals(EmbeddedOckStateBackend.PriorityQueueStateType.HEAP,
            stateBackend.getPriorityQueueStateType());

        config.set(OckDBOptions.OCKDB_PRIORITY_QUEUE_TYPE, "OCKDB");
        stateBackend = factory.createFromConfig(config, this.getClass().getClassLoader());
        Assert.assertEquals(EmbeddedOckStateBackend.PriorityQueueStateType.OCKDB,
            stateBackend.getPriorityQueueStateType());
    }

    @Test(expected = IllegalConfigurationException.class)
    public void test_getPriorityQueueStateType_abnormal() throws IOException {
        OckDBStateBackendFactory factory = new OckDBStateBackendFactory();
        config.set(OckDBOptions.OCKDB_PRIORITY_QUEUE_TYPE, "XXX");
        EmbeddedOckStateBackend stateBackend = factory.createFromConfig(config, this.getClass().getClassLoader());
        stateBackend.getPriorityQueueStateType();
    }
}
