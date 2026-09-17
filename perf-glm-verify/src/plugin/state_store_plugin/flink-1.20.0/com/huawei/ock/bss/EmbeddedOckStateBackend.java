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

import static com.huawei.ock.bss.OckDBOptions.CHECKPOINT_TRANSFER_THREAD_NUM;
import static com.huawei.ock.bss.util.CleanupManager.JNI_DIR_NAME_PREFIX;
import static org.apache.flink.util.Preconditions.checkNotNull;

import com.huawei.ock.bss.common.memory.SizeAllocator;
import com.huawei.ock.bss.metric.BoostNativeMetricOptions;
import com.huawei.ock.bss.ockdb.NativeLibraryLoader;
import com.huawei.ock.bss.ockdb.OckDBLog;
import com.huawei.ock.bss.resource.HeapMonitor;
import com.huawei.ock.bss.resource.ResourceContainer;
import com.huawei.ock.bss.util.CleanupManager;
import com.huawei.ock.bss.util.LogSanitizer;

import org.apache.flink.annotation.VisibleForTesting;
import org.apache.flink.api.common.ExecutionConfig;
import org.apache.flink.api.common.JobID;
import org.apache.flink.api.common.typeutils.TypeSerializer;
import org.apache.flink.configuration.CheckpointingOptions;
import org.apache.flink.configuration.Configuration;
import org.apache.flink.configuration.IllegalConfigurationException;
import org.apache.flink.configuration.MemorySize;
import org.apache.flink.configuration.ReadableConfig;
import org.apache.flink.core.execution.SavepointFormatType;
import org.apache.flink.core.fs.CloseableRegistry;
import org.apache.flink.metrics.MetricGroup;
import org.apache.flink.runtime.execution.Environment;
import org.apache.flink.runtime.memory.MemoryManager;
import org.apache.flink.runtime.memory.OpaqueMemoryResource;
import org.apache.flink.runtime.query.TaskKvStateRegistry;
import org.apache.flink.runtime.state.AbstractKeyedStateBackend;
import org.apache.flink.runtime.state.AbstractManagedMemoryStateBackend;
import org.apache.flink.runtime.state.ConfigurableStateBackend;
import org.apache.flink.runtime.state.DefaultOperatorStateBackendBuilder;
import org.apache.flink.runtime.state.KeyGroupRange;
import org.apache.flink.runtime.state.KeyedStateHandle;
import org.apache.flink.runtime.state.LocalRecoveryConfig;
import org.apache.flink.runtime.state.OperatorStateBackend;
import org.apache.flink.runtime.state.OperatorStateHandle;
import org.apache.flink.runtime.state.StreamCompressionDecorator;
import org.apache.flink.runtime.state.metrics.LatencyTrackingStateConfig;
import org.apache.flink.runtime.state.ttl.TtlTimeProvider;
import org.apache.flink.util.AbstractID;
import org.apache.flink.util.FileUtils;
import org.apache.flink.util.Preconditions;
import org.apache.flink.util.TernaryBoolean;
import org.apache.flink.util.UserCodeClassLoader;
import org.apache.flink.util.function.LongFunctionWithException;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import java.io.File;
import java.io.IOException;
import java.net.URI;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.util.ArrayList;
import java.util.Collection;
import java.util.List;
import java.util.UUID;
import java.util.concurrent.ThreadLocalRandom;

import javax.annotation.Nonnull;
import javax.annotation.Nullable;

/**
 * OckStateBackend类，基于OckDB后端存储
 *
 * @since 2025年1月12日16:15:42
 */
public class EmbeddedOckStateBackend extends AbstractManagedMemoryStateBackend implements ConfigurableStateBackend {
    private static final Logger LOG = LoggerFactory.getLogger(EmbeddedOckStateBackend.class);

    /**
     * The number of (re)tries for loading the OckDB JNI library.
     */
    private static final int MAX_LIB_LOADING_ATTEMPTS = 3;

    private static final int MIN_LOG_FILE_NUM = 10;

    private static final int MAX_LOG_FILE_NUM = 50;

    private static final int LINUX_PATH_MAX = 4096;

    private static final int UNDEFINED_NUMBER_OF_TRANSFER_THREADS = -1;

    private static final int MIN_CHECKPOINT_TRANSFER_THREAD_NUM = 0;

    private static final int MAX_CHECKPOINT_TRANSFER_THREAD_NUM = 20;

    private static final String MIN_LOG_FILE_SIZE = "10MB";

    private static final String MAX_LOG_FILE_SIZE = "50MB";

    private static final String MANAGED_MEMORY_RESOURCE_ID = "state-bss-managed-memory";

    private static final String[] EMPTY = new String[0];

    /**
     * Flag whether the native library has been loaded.
     */
    private static boolean ockDbInitialized = false;

    /**
     * This determines if incremental checkpointing is enabled.
     */
    private final TernaryBoolean enableIncrementalCheckpointing;

    /**
     * log num configuration for OCKDB LOG.
     */
    private int ockLogNum = -1;

    /**
     * log level configuration for OCKDB LOG.
     */
    private int ockLogLevel = -1;

    /**
     * Thread number used to transfer (download and upload) state, default value: 1.
     */
    private int numberOfTransferThreads;

    /**
     * Base paths for OCKDB LOG, as configured.
     */
    private String ockLogFilePath;

    /**
     * log size configuration for OCKDB LOG.
     */
    private MemorySize ockLogSize;

    private Configuration config;

    /**
     * Base paths for OCKDB directory, as configured. Null if not yet set, in which case the
     * configuration values will be used. The configuration defaults to the TaskManager's temp directories.
     */
    @Nullable
    private File[] localOckDbDirectories;

    /**
     * This determines the type of priority queue state.
     */
    @Nullable
    private EmbeddedOckStateBackend.PriorityQueueStateType priorityQueueStateType;

    // -- runtime values, set on TaskManager when initializing / using the backend

    /**
     * The index of the next directory to be used from {@link #initializedDbBasePaths}.
     */
    private transient int nextDirectory;

    /**
     * Whether we already lazily initialized our local storage directories.
     */
    private transient boolean isInitialized;

    /**
     * JobID for uniquifying backup paths.
     */
    private transient JobID jobId;

    /**
     * Base paths for OckDB directory, as initialized.
     */
    private transient File[] initializedDbBasePaths;

    public EmbeddedOckStateBackend() {
        this(TernaryBoolean.UNDEFINED);
    }

    public EmbeddedOckStateBackend(boolean enableIncrementalCheckpointing) {
        this(TernaryBoolean.fromBoolean(enableIncrementalCheckpointing));
    }

    public EmbeddedOckStateBackend(TernaryBoolean enableIncrementalCheckpointing) {
        Preconditions.checkNotNull(enableIncrementalCheckpointing,
            "enableIncrementalCheckpointing should not be null.");
        this.enableIncrementalCheckpointing = enableIncrementalCheckpointing;
        this.numberOfTransferThreads = UNDEFINED_NUMBER_OF_TRANSFER_THREADS;
    }

    public EmbeddedOckStateBackend(EmbeddedOckStateBackend original, ReadableConfig config)
        throws IllegalConfigurationException {
        // classLoader参数，checkpointStreamBackend使用
        Preconditions.checkNotNull(config, "configuration cannot be null.");
        if (config instanceof Configuration) {
            this.config = ((Configuration) config).clone();
        } else {
            throw new IllegalStateException("Invalid configuration name " + config.getClass().getSimpleName());
        }

        this.enableIncrementalCheckpointing = original.enableIncrementalCheckpointing.resolveUndefined(
            config.get(CheckpointingOptions.INCREMENTAL_CHECKPOINTS));
        if (original.numberOfTransferThreads == UNDEFINED_NUMBER_OF_TRANSFER_THREADS) {
            this.numberOfTransferThreads = config.get(CHECKPOINT_TRANSFER_THREAD_NUM);
            checkTransferThreadNum(this.numberOfTransferThreads, OckDBOptions.CHECKPOINT_TRANSFER_THREAD_NUM.key());
        } else {
            this.numberOfTransferThreads = original.numberOfTransferThreads;
        }

        if (original.priorityQueueStateType == null) {
            this.priorityQueueStateType = getPriorityQueueStateType();
        } else {
            this.priorityQueueStateType = original.priorityQueueStateType;
        }

        if (original.localOckDbDirectories != null) {
            this.localOckDbDirectories = original.localOckDbDirectories;
        } else {
            final String ockDBLocalPaths = config.get(OckDBOptions.LOCAL_DIRECTORIES);
            checkPath(ockDBLocalPaths, OckDBOptions.LOCAL_DIRECTORIES.key());
            String[] directories = ockDBLocalPaths.split(",|" + File.pathSeparator);

            try {
                setDbStoragePaths(directories);
            } catch (IllegalArgumentException e) {
                throw new IllegalConfigurationException(
                    "Invalid configuration for OckDB state backend's local storage directories: " + e.getMessage(), e);
            }
        }
        configOckLog(original, config);
        latencyTrackingConfigBuilder = original.latencyTrackingConfigBuilder.configure(config);
    }

    /**
     * The options to chose for the type of Log Level.
     */
    public enum LogLevel {
        DEBUG(1),
        INFO(2),
        WARN(3),
        ERROR(4);

        private final int level;

        LogLevel(int level) {
            this.level = level;
        }
    }

    /**
     * The options to chose for the type of priority queue state.
     */
    public enum PriorityQueueStateType {
        HEAP(true),
        OCKDB(true);

        private final boolean asyncSnapshot;

        PriorityQueueStateType(boolean asyncSnapshot) {
            this.asyncSnapshot = asyncSnapshot;
        }
    }

    /**
     * 是否支持CLAIM恢复
     *
     * @return boolean
     */
    @Override
    public boolean supportsNoClaimRestoreMode() {
        return true;
    }

    /**
     * 是否支持指定的savepoint数据类型
     *
     * @param formatType formatType
     * @return boolean
     */
    @Override
    public boolean supportsSavepointFormat(SavepointFormatType formatType) {
        return true;
    }

    private void configOckLog(EmbeddedOckStateBackend original, ReadableConfig config) {
        if (original.ockLogFilePath != null) {
            this.ockLogFilePath = original.ockLogFilePath;
        } else {
            this.ockLogFilePath = config.get(OckDBOptions.OCKDB_JNI_LOG_DIRECTORY);
            checkPath(this.ockLogFilePath, OckDBOptions.OCKDB_JNI_LOG_DIRECTORY.key());
        }

        if (original.ockLogLevel != -1) {
            this.ockLogLevel = original.ockLogLevel;
        } else {
            this.ockLogLevel = config.get(OckDBOptions.OCKDB_JNI_LOG_LEVEL);
            checkLogLevel(this.ockLogLevel, OckDBOptions.OCKDB_JNI_LOG_LEVEL.key());
        }

        LOG.info("BoostStateStore SpdLog Level is: {}.", this.ockLogLevel);
        if (original.ockLogSize != null) {
            this.ockLogSize = original.ockLogSize;
        } else {
            this.ockLogSize = config.get(OckDBOptions.OCKDB_JNI_LOG_SIZE);
            checkLogSize(this.ockLogSize, OckDBOptions.OCKDB_JNI_LOG_SIZE.key());
        }

        if (original.ockLogNum != -1) {
            this.ockLogNum = original.ockLogNum;
        } else {
            this.ockLogNum = config.get(OckDBOptions.OCKDB_JNI_LOG_NUM);
            checkLogNum(this.ockLogNum, OckDBOptions.OCKDB_JNI_LOG_NUM.key());
        }
    }

    @Nonnull
    private static String[] splitPaths(@Nonnull String separatedPaths) {
        return (separatedPaths.length() > 0) ? separatedPaths.split(",|" + File.pathSeparator) : EMPTY;
    }

    /**
     * 根据配置项格式化工作目录
     *
     * @param configuration 配置项
     * @return 格式化后的目录
     */
    public static String[] parseWorkingDirectories(Configuration configuration) {
        String configValue = configuration.getString(OckDBOptions.LOCAL_DIRECTORIES, "");
        checkPath(configValue, OckDBOptions.LOCAL_DIRECTORIES.key());
        return splitPaths(configValue);
    }

    @Override
    public EmbeddedOckStateBackend configure(ReadableConfig config, ClassLoader classLoader)
        throws IllegalConfigurationException {
        LOG.info("OmniStateStore service start success.");
        return new EmbeddedOckStateBackend(this, config);
    }

    /**
     * 创建KeyedStateBackend，最外层
     *
     * @param parameters           必备参数
     * @param <K>                  泛型
     * @return                     返回KeyedStateBackend实例
     * @throws IOException         IO异常
     */
    @Override
    public <K> AbstractKeyedStateBackend<K> createKeyedStateBackend(
        KeyedStateBackendParameters<K> parameters) throws IOException {
        LOG.info("enter OckStateBackend createKeyedStateBackend with default managedMemoryFraction.");
        Environment env = parameters.getEnv();
        JobID jobID =env.getJobID();
        return createKeyedStateBackend(env, jobID, parameters.getOperatorIdentifier(),  parameters.getKeySerializer(),
            parameters.getNumberOfKeyGroups(),  parameters.getKeyGroupRange(), parameters.getKvStateRegistry(),
            parameters.getTtlTimeProvider(),  parameters.getMetricGroup(),parameters.getStateHandles(),
            parameters.getCancelStreamRegistry(), parameters.getManagedMemoryFraction());
    }

    /**
     * 创建OperatorStateBackend
     *
     * @param parameters 参数列表
     * @return createOperatorStateBackend
     * @throws Exception 创建时异常
     */
    @Override
    public OperatorStateBackend createOperatorStateBackend(
        OperatorStateBackendParameters parameters) throws Exception {
        LOG.info("create OperatorStateBackend of operator:{}", LogSanitizer.sanitize(parameters.getOperatorIdentifier()));
        final boolean asyncSnapshots = true;
        return new DefaultOperatorStateBackendBuilder(
            parameters.getEnv().getUserCodeClassLoader().asClassLoader(),
            parameters.getEnv().getExecutionConfig(),
            asyncSnapshots,
            parameters.getStateHandles(),
            parameters.getCancelStreamRegistry())
            .build();
    }

    /**
     * 创建KeyedStateBackend
     *
     * @param env                   环境
     * @param jobID                 jobID
     * @param operatorIdentifier    算子标识符
     * @param keySerializer         key序列化器
     * @param numberOfKeyGroups     KeyGroups数量
     * @param keyGroupRange         keyGroupRange
     * @param kvStateRegistry       kvStateRegistry
     * @param ttlTimeProvider       ttlTimeProvider
     * @param metricGroup           metricGroup
     * @param stateHandles          状态句柄
     * @param cancelStreamRegistry  cancelStreamRegistry
     * @param managedMemoryFraction 托管内存比例
     * @param <K>                   泛型
     * @return 返回KeyedStateBackend实例
     * @throws IOException IO异常
     */
    public <K> AbstractKeyedStateBackend<K> createKeyedStateBackend(Environment env, JobID jobID,
        String operatorIdentifier, TypeSerializer<K> keySerializer, int numberOfKeyGroups, KeyGroupRange keyGroupRange,
        TaskKvStateRegistry kvStateRegistry, TtlTimeProvider ttlTimeProvider, MetricGroup metricGroup,
        @Nonnull Collection<KeyedStateHandle> stateHandles, CloseableRegistry cancelStreamRegistry,
        double managedMemoryFraction) throws IOException {
        Preconditions.checkNotNull(env, "env cannot be null while trying to createKeyedStateBackend.");
        Preconditions.checkNotNull(operatorIdentifier,
            "operatorIdentifier cannot be null while trying to createKeyedStateBackend.");

        LOG.info("enter OckStateBackend createKeyedStateBackend method successfully !");
        String tempDir = env.getTaskManagerInfo().getTmpDirectories()[0];
        Preconditions.checkNotNull(tempDir);
        ensureOckDBIsLoaded(tempDir);

        // 初始化日志模块
        OckDBLog.initialOckLog(this.ockLogFilePath, this.ockLogLevel, this.ockLogSize.getMebiBytes(),
            this.ockLogNum);
        lazyInitializeForJob(env);
        String identifier = operatorIdentifier.replaceAll("[^a-zA-Z0-9\\-]", "_");
        File instanceBasePath = new File(getNextStoragePath(),
            "job_" + this.jobId + "_op_" + identifier + "_uuid_" + env.getExecutionId());
        // sharedResource内存管理未添加
        return createKeyedStateBackend(env, identifier, instanceBasePath, keySerializer, numberOfKeyGroups,
            keyGroupRange, ttlTimeProvider, metricGroup, stateHandles, cancelStreamRegistry, managedMemoryFraction);
    }

    /**
     * 使用builder创建KeyedStateBackend
     *
     * @param env                  环境
     * @param identifier           标识符
     * @param instanceBasePath     OckDB working directory
     * @param keySerializer        key序列化器
     * @param numberOfKeyGroups    KeyGroups数量
     * @param keyGroupRange        keyGroupRange
     * @param ttlTimeProvider      ttlTimeProvider
     * @param metricGroup          metricGroup
     * @param stateHandles         状态句柄
     * @param cancelStreamRegistry cancelStreamRegistry
     * @param <K>                  泛型
     * @return 返回KeyedStateBackend实例
     * @throws IOException IO异常
     */
    private <K> OckDBKeyedStateBackend<K> createKeyedStateBackend(Environment env, String identifier,
        File instanceBasePath, TypeSerializer<K> keySerializer, int numberOfKeyGroups, KeyGroupRange keyGroupRange,
        TtlTimeProvider ttlTimeProvider, MetricGroup metricGroup, @Nonnull Collection<KeyedStateHandle> stateHandles,
        CloseableRegistry cancelStreamRegistry, double managedMemoryFraction) throws IOException {
        UserCodeClassLoader userCodeClassLoader = env.getUserCodeClassLoader();
        LocalRecoveryConfig localRecoveryConfig = env.getTaskStateManager().createLocalRecoveryConfig();
        TaskKvStateRegistry kvStateRegistry = env.getTaskKvStateRegistry();
        ExecutionConfig executionConfig = env.getExecutionConfig();
        LatencyTrackingStateConfig latencyTrackingStateConfig = latencyTrackingConfigBuilder.setMetricGroup(metricGroup)
            .build();
        StreamCompressionDecorator keyGroupCompressionDecorator = getCompressionDecorator(env.getExecutionConfig());
        PriorityQueueStateType checkedpriorityQueueStateType = getPriorityQueueStateType();

        // TaskSlot与MemoryManager一一对应，使用MemoryManager实例作为当前Slot的标识，然后通过MemoryManager获取当前Slot可用内存
        int slotFlag = env.getMemoryManager().hashCode();
        long slotMemoryLimit = getTaskSlotMemoryLimit(env.getMemoryManager(), managedMemoryFraction);

        // 启动heap内存监控线程
        double borrowHeapRatio = 0.7;
        HeapMonitor.INSTANCE.start(borrowHeapRatio);

        BoostNativeMetricOptions options = BoostNativeMetricOptions.fromConfig(this.config);

        return new OckDBKeyedStateBackendBuilder<>(numberOfKeyGroups,
            keyGroupRange, userCodeClassLoader, instanceBasePath, localRecoveryConfig, kvStateRegistry, identifier,
            executionConfig, keySerializer, ttlTimeProvider, latencyTrackingStateConfig, cancelStreamRegistry,
            keyGroupCompressionDecorator, stateHandles, checkedpriorityQueueStateType.asyncSnapshot,
            ResourceContainer.INSTANCE, checkedpriorityQueueStateType, this.config)
            .setEnableIncrementalCheckpointing(isIncrementalCheckpointsEnabled())
            .setNumberOfTransferringThreads(getNumberOfTransferThreads())
            .setTaskSlotFlag(slotFlag)
            .setTaskSlotMemoryLimit(slotMemoryLimit)
            .setSlotManagedMemoryFraction(managedMemoryFraction)
            .setJobID(jobId.toString())
            .setNativeMetricOptions(options)
            .build();
    }

    /**
     * 从Flink框架的memoryManager中获取当前Slot的托管内存大小
     *
     * @param memoryManager memoryManager
     * @param fraction      fraction
     * @return long
     */
    private long getTaskSlotMemoryLimit(MemoryManager memoryManager, double fraction)
        throws IOException {
        LongFunctionWithException<SizeAllocator, Exception> allocatorFunc = size -> new SizeAllocator(
            MANAGED_MEMORY_RESOURCE_ID, size);
        long memoryLimit;
        try {
            OpaqueMemoryResource<SizeAllocator> memoryResource = memoryManager.getSharedMemoryResourceForManagedMemory(
                MANAGED_MEMORY_RESOURCE_ID, allocatorFunc, fraction);
            memoryLimit = memoryResource.getResourceHandle().getMemorySize();
            LOG.info("Get slot:{} managed memory by fraction:{}, result size:{}", memoryManager.hashCode(),
                fraction, memoryLimit);
        } catch (Exception e) {
            throw new IOException("Failed to get managed memory size.", e);
        }
        return memoryLimit;
    }

    /**
     * 获取当前PriorityQueueStateType
     *
     * @return PriorityQueueStateType
     */
    public final PriorityQueueStateType getPriorityQueueStateType() {
        String pqType = config.get(OckDBOptions.OCKDB_PRIORITY_QUEUE_TYPE);
        if (PriorityQueueStateType.OCKDB.name().equals(pqType)) {
            return PriorityQueueStateType.OCKDB;
        }

        if (PriorityQueueStateType.HEAP.name().equals(pqType)) {
            return PriorityQueueStateType.HEAP;
        }

        LOG.error("config priority queue type:{} is invalid.", pqType);
        throw new IllegalConfigurationException(
            "Could not parse value for key 'state.backend.ockdb.timer-service.factory',"
                + " Expected one of: [HEAP, OCKDB]");
    }

    /**
     * 设置PriorityQueueStateType
     *
     * @param priorityQueueStateType PriorityQueueStateType
     */
    public void setPriorityQueueStateType(PriorityQueueStateType priorityQueueStateType) {
        this.priorityQueueStateType = checkNotNull(priorityQueueStateType);
    }

    private void lazyInitializeForJob(Environment env) throws IOException {
        if (this.isInitialized) {
            return;
        }

        this.jobId = env.getJobID();
        if (this.localOckDbDirectories == null) {
            String[] directories = parseWorkingDirectories(env.getTaskManagerInfo().getConfiguration());
            if (directories.length == 0) {
                this.initializedDbBasePaths = env.getIOManager().getSpillingDirectories();
                this.nextDirectory = ThreadLocalRandom.current().nextInt(this.initializedDbBasePaths.length);
                this.isInitialized = true;
                return;
            }
            setDbStoragePaths(directories);
        }

        List<File> dirs = new ArrayList<>(localOckDbDirectories.length);
        StringBuilder errMessage = new StringBuilder();

        for (File f : localOckDbDirectories) {
            File testDir = new File(f, UUID.randomUUID().toString());
            if (!testDir.mkdirs()) {
                String msg = "Local OCKDB files directory '" + f + "' does not exist and cannot be created. ";
                LOG.error(msg);
                errMessage.append(msg);
            } else {
                dirs.add(f);
            }
            testDir.delete();
        }

        if (dirs.isEmpty()) {
            throw new IOException("No local storage directories available. " + errMessage);
        }
        this.initializedDbBasePaths = dirs.toArray(new File[0]);
        this.nextDirectory = ThreadLocalRandom.current().nextInt(this.initializedDbBasePaths.length);
        this.isInitialized = true;
    }

    /**
     * Gets whether incremental checkpoints are enabled for this state backend.
     *
     * @return boolean
     */
    public boolean isIncrementalCheckpointsEnabled() {
        return enableIncrementalCheckpointing.getOrDefault(
            CheckpointingOptions.INCREMENTAL_CHECKPOINTS.defaultValue());
    }

    public int getNumberOfTransferThreads() {
        return numberOfTransferThreads == UNDEFINED_NUMBER_OF_TRANSFER_THREADS
            ? CHECKPOINT_TRANSFER_THREAD_NUM.defaultValue()
            : numberOfTransferThreads;
    }

    /**
     * 设置snapshot时的NumberOfTransferThreads
     *
     * @param numberOfTransferThreads numberOfTransferThreads
     */
    public void setNumberOfTransferThreads(int numberOfTransferThreads) {
        Preconditions.checkArgument(
            numberOfTransferThreads > 0,
            "The number of threads used to transfer files in OckDBKeyedStateBackend should be greater than zero.");
        this.numberOfTransferThreads = numberOfTransferThreads;
    }

    private File getNextStoragePath() {
        int ni = nextDirectory + 1;
        ni = ni >= initializedDbBasePaths.length ? 0 : ni;
        nextDirectory = ni;
        return initializedDbBasePaths[ni];
    }

    @VisibleForTesting
    static void ensureOckDBIsLoaded(String tempDirectory) throws IOException {
        synchronized (EmbeddedOckStateBackend.class) {
            if (ockDbInitialized) {
                return;
            }

            final File tempDirParent = new File(tempDirectory).getAbsoluteFile();
            LOG.info("Attempting to load OckDB native library and store it under '{}'", tempDirParent.getName());

            Throwable lastException = null;
            for (int attempt = 1; attempt <= MAX_LIB_LOADING_ATTEMPTS; attempt++) {
                File ockLibFolder = null;
                try {
                    CleanupManager.jniParentDir = tempDirParent.getCanonicalPath();
                    ockLibFolder = new File(tempDirParent, JNI_DIR_NAME_PREFIX + new AbstractID());
                    // make sure the temp path exists
                    LOG.debug("Attempting to create OckDB native library folder {}", ockLibFolder.getName());
                    // noinspection ResultOfMethodCallIgnored
                    ockLibFolder.mkdirs();

                    // 使用 getCanonicalPath() 方法规范化文件路径
                    String canonicalPath = ockLibFolder.getCanonicalPath();

                    // 注册清理钩子
                    CleanupManager.registerCleanupHook(canonicalPath);

                    // explicitly load the JNI dependency if it has not been loaded before
                    NativeLibraryLoader.getInstance().loadLibrary(canonicalPath);

                    // seems to have worked
                    LOG.info("Successfully loaded OckDB native library");
                    ockDbInitialized = true;
                    return;
                } catch (UnsatisfiedLinkError t) {
                    lastException = t;
                    LOG.error("OckDB JNI library loading attempt {} failed", attempt, t);
                    FileUtils.deleteDirectoryQuietly(ockLibFolder);
                }
            }

            throw new IOException("Could not load the native OckDB library", lastException);
        }
    }

    /**
     * 设置DB存储路径
     *
     * @param path path
     */
    public void setDbStoragePath(String path) {
        setDbStoragePaths(path == null ? null : new String[] {path});
    }

    /**
     * 获取当前DB存储路径
     *
     * @return String[]
     */
    public String[] getDbStoragePaths() {
        if (localOckDbDirectories == null) {
            return new String[0];
        }

        String[] paths = new String[localOckDbDirectories.length];
        for (int i = 0; i < paths.length; i++) {
            paths[i] = localOckDbDirectories[i].toString();
        }
        return paths;
    }

    /**
     * 设置db存储路径
     *
     * @param paths 路径
     */
    public void setDbStoragePaths(String... paths) {
        if (paths == null) {
            localOckDbDirectories = null;
            return;
        }

        if (paths.length == 0) {
            throw new IllegalArgumentException("Path is empty.");
        }

        File[] files = new File[paths.length];
        for (int i = 0; i < paths.length; i++) {
            final String rawPath = paths[i];
            final String path = validatePath(rawPath);
            files[i] = new File(path);
            if (!files[i].isAbsolute()) {
                throw new IllegalArgumentException("Error! Relative paths are not supported.");
            }
        }
        localOckDbDirectories = files;
    }

    // 抽取函数减少嵌套深度
    private String validatePath(String rawPath) {
        if (rawPath == null) {
            throw new IllegalArgumentException("Path is null!");
        }

        URI pathUri;
        try {
            pathUri = new org.apache.flink.core.fs.Path(rawPath).toUri();
        } catch (Exception e) {
            throw new IllegalArgumentException("DB path string cannot parse as a path.");
        }

        if (pathUri != null && pathUri.getScheme() != null) {
            if ("file".equalsIgnoreCase(pathUri.getScheme())) {
                return pathUri.getPath();
            }
            throw new IllegalArgumentException("Error! Path has a non-local scheme.");
        }
        return rawPath;
    }

    private static void checkPath(String pathStr, String configKey) {
        if (pathStr == null) {
            throw new IllegalConfigurationException(configKey + " must config!");
        }

        Path path = Paths.get(pathStr);
        if (OckDBOptions.OCKDB_JNI_LOG_DIRECTORY.key().equals(configKey)) {
            // 用户配置的log文件存在时，首先需要单独校验log文件
            if (Files.exists(path)) {
                // 校验是不是文件
                if (!Files.isRegularFile(path)) {
                    throw new IllegalConfigurationException("configure: " + configKey + " should be regular file!");
                }
                validateFilePath(configKey, path);
            }
            // 然后校验log文件的parent路径
            path = path.getParent();
        }

        // 一般的校验流程
        validateFilePath(configKey, path);
    }

    /**
     * checkPathValid
     *
     * @param pathStr   pathStr
     * @param configKey configKey
     */
    public static void checkPathValid(String pathStr, String configKey) {
        if (pathStr == null) {
            throw new IllegalConfigurationException(configKey + " must config!");
        }
        Path path = Paths.get(pathStr);
        validateFilePath(configKey, path, false);
    }

    private void checkTransferThreadNum(int numberOfTransferThreads, String configKey) {
        if (numberOfTransferThreads <= MIN_CHECKPOINT_TRANSFER_THREAD_NUM
            || numberOfTransferThreads > MAX_CHECKPOINT_TRANSFER_THREAD_NUM) {
            throw new IllegalConfigurationException(
                "configure: " + configKey + " should only valid in [" + MIN_CHECKPOINT_TRANSFER_THREAD_NUM + "-"
                    + MAX_CHECKPOINT_TRANSFER_THREAD_NUM + "], and must be Integer!");
        }
    }

    private void checkLogLevel(int logLevel, String configKey) {
        if (logLevel < LogLevel.DEBUG.level || logLevel > LogLevel.ERROR.level) {
            throw new IllegalConfigurationException("configure: " + configKey
                + " should only valid in [1(meaning DEBUG)/2(meaning INFO)/3(meaning WARN)/4(meaning ERROR)]!");
        }
    }

    private void checkLogNum(int logNum, String configKey) {
        if (logNum < MIN_LOG_FILE_NUM || logNum > MAX_LOG_FILE_NUM) {
            throw new IllegalConfigurationException(
                "configure: " + configKey + " should only valid in [10-50],and must be Integer!");
        }
    }

    private void checkLogSize(MemorySize logSize, String configKey) {
        if (MemorySize.parse(MIN_LOG_FILE_SIZE).compareTo(logSize) > 0
            || MemorySize.parse(MAX_LOG_FILE_SIZE).compareTo(logSize) < 0) {
            throw new IllegalConfigurationException("configure: " + configKey + " should only valid in [10MB-50MB]!");
        }
    }

    /**
     * 校验必须存在的路径
     *
     * @param configKey configKey
     * @param path path
     */
    public static void validateFilePath(String configKey, Path path) {
        validateFilePath(configKey, path, true);
    }

    /**
     * 校验路径合法性
     *
     * @param configKey 配置项名称
     * @param path 配置的路径
     */
    public static void validateFilePath(String configKey, Path path, boolean isCheckExists) {
        if (path == null) {
            throw new IllegalConfigurationException("configure: " + configKey + " path or parent path cannot be null!");
        }
        if (path.toString().length() > LINUX_PATH_MAX) {
            throw new IllegalConfigurationException("configure: " + configKey + " path length too long!");
        }
        if (!Files.exists(path)) {
            // savepoint临时目录可以自己创建
            if (!isCheckExists) {
                return;
            }
            throw new IllegalConfigurationException("configure: " + configKey + " path not exist!");
        }
        if (!Files.isReadable(path)) {
            throw new IllegalConfigurationException("configure: " + configKey + " is not readable!");
        }
        if (!Files.isWritable(path)) {
            throw new IllegalConfigurationException("configure: " + configKey + " is not writable!");
        }
        if (Files.isSymbolicLink(path)) {
            throw new IllegalConfigurationException("configure: " + configKey + " path can not be symbolic Link!");
        }
    }
}