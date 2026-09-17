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

import org.apache.flink.configuration.ConfigOption;
import org.apache.flink.configuration.ConfigOptions;
import org.apache.flink.configuration.MemorySize;

/**
 * OckDB配置类
 *
 * @since 2025年1月12日16:19:05
 */
public class OckDBOptions {
    /**
     * The local directory (on the TaskManager) where ockDB puts its db files.
     */
    public static final ConfigOption<String> LOCAL_DIRECTORIES = ConfigOptions.key("state.backend.ockdb.localdir")
        .stringType()
        .noDefaultValue()
        .withDeprecatedKeys("state.backend.ockdb.checkpointdir")
        .withDescription("The local directory (on the TaskManager) where ockDB puts its files.");

    /**
     * The local backup directory (on the TaskManager) where ockDB puts full slice files for checkpoints.
     * Must config while enable localRecovery.
     */
    public static final ConfigOption<String> BACKUP_DIRECTORY =
        ConfigOptions.key("state.backend.ockdb.checkpoint.backup")
            .stringType()
            .noDefaultValue()
            .withDescription(
                "The local backup directory (on the TaskManager) where ockDB puts full slice files for checkpoints.");

    /**
     * The local directory (on the TaskManager) where ockDB puts its log files.
     */
    public static final ConfigOption<String> OCKDB_JNI_LOG_DIRECTORY =
        ConfigOptions.key("state.backend.ockdb.jni.logfile")
            .stringType()
            .defaultValue("/usr/local/flink/log/kv.log")
            .withDescription("The local file path where log record");

    /**
     * ockdb spdlog size for per log file
     */
    public static final ConfigOption<MemorySize> OCKDB_JNI_LOG_SIZE =
        ConfigOptions.key("state.backend.ockdb.jni.logsize")
            .memoryType()
            .defaultValue(MemorySize.parse("20mb"))
            .withDescription("ockdb spdlog size for per log file");

    /**
     * ockdb spdlog log file count
     */
    public static final ConfigOption<Integer> OCKDB_JNI_LOG_NUM = ConfigOptions.key("state.backend.ockdb.jni.lognum")
        .intType()
        .defaultValue(20)
        .withDescription("ockdb spdlog log file count");

    /**
     * ockdb spdlog log file level,DEBUG/INFO/WARN/ERROR if legal
     */
    public static final ConfigOption<Integer> OCKDB_JNI_LOG_LEVEL =
        ConfigOptions.key("state.backend.ockdb.jni.loglevel")
            .intType()
            .defaultValue(2)
            .withDescription("ockdb spdlog log file level, valid value is 1,meaning DEBUG, "
                + "2, meaning INFO, 3 meaning WARN, 4 meaning ERROR。");

    /**
     * The number of threads used to transfer (download and upload) files in
     * ockDBStateBackend.
     */
    public static final ConfigOption<Integer> CHECKPOINT_TRANSFER_THREAD_NUM =
        ConfigOptions.key("state.backend.ockdb.checkpoint.transfer.thread.num")
            .intType()
            .defaultValue(4)
            .withDescription("The number of threads (per stateful operator) used to transfer "
                + "(download and upload) files in ockDBStateBackend.");

    /**
     * watermark
     */
    public static final ConfigOption<Float> OCKDB_JNI_SLICE_WATERMARK_RATIO =
        ConfigOptions.key("state.backend.ockdb.jni.slice.watermark.ratio")
            .floatType()
            .defaultValue(0.8F)
            .withDescription("slice watermark ratio");

    /**
     * lsmStoreCompactionSwitch
     */
    public static final ConfigOption<Integer> OCKDB_JNI_LSM_CMPCT_SWITCH =
        ConfigOptions.key("state.backend.ockdb.jni.lsmstore.compaction.switch")
            .intType()
            .defaultValue(1)
            .withDescription("lsm store compaction switch");

    /**
     * Enable ListState compaction file reuse.
     */
    public static final ConfigOption<Boolean> OCKDB_ZERO_COPY_SWITCH =
        ConfigOptions.key("state.backend.ockdb.zero-copy.switch")
            .booleanType()
            .defaultValue(false)
            .withDescription("enable ListState compaction file reuse");

    /**
     * enable ttl filter switch
     */
    public static final ConfigOption<Boolean> OCKDB_TTL_FILTER_SWITCH =
        ConfigOptions.key("state.backend.ockdb.ttl.filter.switch")
            .booleanType()
            .defaultValue(false)
            .withDescription("enable time to live filter switch");

    /**
     * enable lsm cache filter and index switch
     */
    public static final ConfigOption<Boolean> OCKDB_CACHE_FILTER_AND_INDEX_SWITCH =
            ConfigOptions.key("state.backend.ockdb.cache.filter.and.index.switch")
                    .booleanType()
                    .defaultValue(true)
                    .withDescription("enable filter and index block cache");

    /**
     * enable Key-Value Separate Storage
     */
    public static final ConfigOption<Boolean> OCKDB_KV_SEPARATE_SWITCH =
            ConfigOptions.key("state.backend.ockdb.kv-separate.switch")
                    .booleanType()
                    .defaultValue(false)
                    .withDescription("enable KV Separate");

    /**
     * Key-Value Separate Threshold, KV separation is used only if it is greater than this value
     */
    public static final ConfigOption<Integer> OCKDB_KV_SEPARATE_THRESHOLD =
            ConfigOptions.key("state.backend.ockdb.kv-separate.threshold")
                    .intType()
                    .defaultValue(200)
                    .withDescription("KV separate threshold, default: 200");

    /**
     * lsmCacheFilterAndIndexRatio
     */
    public static final ConfigOption<Float> OCKDB_FILTER_AND_INDEX_OWN_CACHE_RATIO =
            ConfigOptions.key("state.backend.ockdb.cache.filter.and.index.ratio")
                    .floatType()
                    .defaultValue(0.0F)
                    .withDescription("filter and index block own cache as a percentage of total caches");

    /**
     * enable bloom filter switch
     */
    public static final ConfigOption<Boolean> OCKDB_BLOOM_FILTER_SWITCH =
        ConfigOptions.key("state.backend.ockdb.bloom.filter.switch")
            .booleanType()
            .defaultValue(true)
            .withDescription("enable bloom filter switch");

    /**
     * enable bloom filter switch
     */
    public static final ConfigOption<Boolean> OCKDB_LAZY_DOWN_SWITCH =
        ConfigOptions.key("state.backend.ockdb.lazy.download.switch")
            .booleanType()
            .defaultValue(false)
            .withDescription("enable lazy download switch");

    /**
     * slot managed memory fraction
     */
    public static final ConfigOption<Float> OCKDB_FILE_MEMORY_FRACTION =
        ConfigOptions.key("state.backend.ockdb.file.memory.fraction")
            .floatType()
            .defaultValue(0.2F)
            .withDescription("slot managed memory fraction");

    /**
     * bloom filter expected key count
     */
    public static final ConfigOption<Integer> OCKDB_BLOOM_FILTER_EXPECTED_KEY_COUNT =
        ConfigOptions.key("state.backend.bloom.filter.expected.key.count")
            .intType()
            .defaultValue(8000000)
            .withDescription("bloom filter expected key count");

    /**
     * lsm compression policy
     */
    public static final ConfigOption<String> OCKDB_LSM_COMPRESSION_POLICY =
        ConfigOptions.key("state.backend.ockdb.lsmstore.compression.policy")
            .stringType()
            .defaultValue("lz4")
            .withDescription("default_value:(lz4), lsm store compression policy");

    /**
     * lsm compression level policy
     */
    public static final ConfigOption<String> OCKDB_LSM_COMPRESSION_LEVEL_POLICY =
        ConfigOptions.key("state.backend.ockdb.lsmstore.compression.level.policy")
            .stringType()
            .defaultValue("none,none,lz4")
            .withDescription("default_value:(none,none,lz4), different levels of compression strategies");

    /**
     * FreshTable snapshot compression policy
     */
    public static final ConfigOption<String> OCKDB_FRESH_TABLE_SNAPSHOT_COMPRESSION_POLICY =
        ConfigOptions.key("state.backend.ockdb.freshtable.snapshot.compression.policy")
            .stringType()
            .defaultValue("none")
            .withDescription("default_value:(none), FreshTable checkpoint file compression policy");

    /**
     * SliceTable snapshot compression policy
     */
    public static final ConfigOption<String> OCKDB_SLICE_TABLE_SNAPSHOT_COMPRESSION_POLICY =
        ConfigOptions.key("state.backend.ockdb.slicetable.snapshot.compression.policy")
            .stringType()
            .defaultValue("none")
            .withDescription("default_value:(none), SliceTable checkpoint file compression policy");

    /**
     * peak filter element number
     */
    public static final ConfigOption<Integer> OCKDB_PEAK_FILTER_ELEM_NUM =
        ConfigOptions.key("state.backend.ockdb.peak.filter.elem.num")
            .intType()
            .defaultValue(0)
            .withDescription("elements number of peak filter");

    /**
     * priority queue type
     */
    public static final ConfigOption<String> OCKDB_PRIORITY_QUEUE_TYPE =
        ConfigOptions.key("state.backend.ockdb.timer-service.factory")
            .stringType()
            .defaultValue(EmbeddedOckStateBackend.PriorityQueueStateType.HEAP.name())
            .withDescription("priority queue type of state backend");
}
