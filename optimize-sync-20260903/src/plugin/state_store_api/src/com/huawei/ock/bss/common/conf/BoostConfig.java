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

package com.huawei.ock.bss.common.conf;

import org.apache.flink.runtime.state.KeyGroupRange;

import java.util.Locale;

public class BoostConfig {
    /**
     * DEBUG_MODE 一致性校验debug的开关
     */
    public static final boolean DEBUG_MODE = false;

    private final KeyGroupRange keyGroupRange;

    private long globalTtl;

    private int maxParallelism;

    private String instanceBasePath;

    private String backendUID;

    private long totalDBSize = 0L;

    private float sliceMemWaterMark = 0.8F;

    private int lsmCompactionSwitch = 1; // 默认开启lsmStore的compaction操作.

    private boolean ttlFilterSwitch = false; // 默认关闭stateBackend的ttl后台压缩.

    private boolean zeroCopySwitch = false; // ListState compaction file reuse is disabled by default.

    private boolean cacheFilterAndIndexSwitch = true; // 默认开启FilterBlock和IndexBlock的缓存.

    /**
     * KV separation is turned off by default
     */
    private boolean isKVSeparateSwitch = false; // 默认关闭KV分离.

    /**
     * KV separation threshold, default: 200
     */
    private int kVSeparateThreshold = 200;

    private float cacheFilterAndIndexRatio = 0.0F; // 默认不使用独立缓存，复用LRU缓存.

    private String lsmStoreCompressionPolicy;

    private String lsmStoreCompressionLevelPolicy;

    private String freshTableSnapshotCompressionPolicy = "none";

    private String sliceTableSnapshotCompressionPolicy = "none";

    private boolean isEnableBloomFilter = true;

    private int taskSlotFlag;

    private long taskSlotMemoryLimit;

    private float fileMemoryFraction;

    private long borrowHeapSize;

    private int expectedKeyCount;

    private boolean isLazyDownload = true;

    private int peakFilterElemNum = 0;

    private String snapshotBackupDir = "";

    private boolean enableLocalRecovery;

    public BoostConfig(KeyGroupRange keyGroupRange) {
        this.keyGroupRange = keyGroupRange;
    }

    public KeyGroupRange getKeyGroupRange() {
        return this.keyGroupRange;
    }

    public int getMaxParallelism() {
        return maxParallelism;
    }

    public String getInstanceBasePath() {
        return instanceBasePath;
    }

    public String getBackendUID() {
        return backendUID;
    }

    public long getGlobalTtl() {
        return globalTtl;
    }

    public long getTotalDBSize() {
        return totalDBSize;
    }

    public float getSliceMemWaterMark() {
        return this.sliceMemWaterMark;
    }

    public int getLsmCompactionSwitch() { return this.lsmCompactionSwitch; }

    public void setGlobalTtl(long globalTtl) {
        this.globalTtl = globalTtl;
    }

    public void setMaxParallelism(int maxParallelism) {
        this.maxParallelism = maxParallelism;
    }

    public void setInstanceBasePath(String instanceBasePath) {
        this.instanceBasePath = instanceBasePath;
    }

    public void setBackendUID(String backendUID) {
        this.backendUID = backendUID;
    }

    public void setTotalDBSize(long totalDBSize) {
        this.totalDBSize = totalDBSize;
    }

    public void setSliceMemWaterMark(float sliceMemWaterMark) {
        this.sliceMemWaterMark = sliceMemWaterMark;
    }

    public void setLsmCompactionSwitch(int switchValue) {
        this.lsmCompactionSwitch = switchValue;
    }

    public void setTtlFilterSwitch(boolean switchValue) {
        this.ttlFilterSwitch = switchValue;
    }

    public boolean getTtlFilterSwitch() {
        return this.ttlFilterSwitch;
    }

    public void setZeroCopySwitch(boolean switchValue) {
        this.zeroCopySwitch = switchValue;
    }

    public boolean getZeroCopySwitch() {
        return this.zeroCopySwitch;
    }

    public void setCacheFilterAndIndexSwitch(boolean switchValue) {
        this.cacheFilterAndIndexSwitch = switchValue;
    }

    public void setKVSeparateSwitch(boolean isKVSeparate) {
        this.isKVSeparateSwitch = isKVSeparate;
    }

    public void setKVSeparateThreshold(int threshold) {
        this.kVSeparateThreshold = threshold;
    }

    public float getCacheFilterAndIndexRatio() {
        return this.cacheFilterAndIndexRatio;
    }

    public void setCacheFilterAndIndexRatio(float cacheFilterAndIndexRatio) {
        this.cacheFilterAndIndexRatio = cacheFilterAndIndexRatio;
    }

    public boolean getCacheFilterAndIndexSwitch() {
        return this.cacheFilterAndIndexSwitch;
    }

    public boolean isKVSeparate() {
        return this.isKVSeparateSwitch;
    }

    public int getKVSeparateThreshold() {
        return this.kVSeparateThreshold;
    }

    public void setLsmStoreCompressionPolicy(String lsmStoreCompressionPolicy) {
        this.lsmStoreCompressionPolicy = lsmStoreCompressionPolicy.toLowerCase(Locale.ROOT);
    }

    public void setLsmStoreCompressionLevelPolicy(String lsmStoreCompressionLevelPolicy) {
        this.lsmStoreCompressionLevelPolicy = lsmStoreCompressionLevelPolicy.toLowerCase(Locale.ROOT);
    }

    public void setFreshTableSnapshotCompressionPolicy(String freshTableSnapshotCompressionPolicy) {
        this.freshTableSnapshotCompressionPolicy = freshTableSnapshotCompressionPolicy.toLowerCase(Locale.ROOT);
    }

    public void setSliceTableSnapshotCompressionPolicy(String sliceTableSnapshotCompressionPolicy) {
        this.sliceTableSnapshotCompressionPolicy = sliceTableSnapshotCompressionPolicy.toLowerCase(Locale.ROOT);
    }

    public String getLsmStoreCompressionPolicy() {
        return lsmStoreCompressionPolicy;
    }

    public String getLsmStoreCompressionLevelPolicy() {
        return lsmStoreCompressionLevelPolicy;
    }

    public String getFreshTableSnapshotCompressionPolicy() {
        return freshTableSnapshotCompressionPolicy;
    }

    public String getSliceTableSnapshotCompressionPolicy() {
        return sliceTableSnapshotCompressionPolicy;
    }

    public boolean isEnableBloomFilter() {
        return isEnableBloomFilter;
    }

    public void setEnableBloomFilter(boolean enableBloomFilter) {
        isEnableBloomFilter = enableBloomFilter;
    }

    public int getTaskSlotFlag() {
        return taskSlotFlag;
    }

    public void setTaskSlotFlag(int taskSlotFlag) {
        this.taskSlotFlag = taskSlotFlag;
    }

    public long getTaskSlotMemoryLimit() {
        return taskSlotMemoryLimit;
    }

    public void setTaskSlotMemoryLimit(long taskSlotMemoryLimit) {
        this.taskSlotMemoryLimit = taskSlotMemoryLimit;
    }

    public float getFileMemoryFraction() {
        return fileMemoryFraction;
    }

    public void setFileMemoryFraction(float fileMemoryFraction) {
        this.fileMemoryFraction = fileMemoryFraction;
    }

    public long getBorrowHeapSize() {
        return borrowHeapSize;
    }

    public void setBorrowHeapSize(long borrowHeapSize) {
        this.borrowHeapSize = borrowHeapSize;
    }

    public int getExpectedKeyCount() {
        return expectedKeyCount;
    }

    public void setExpectedKeyCount(int expectedKeyCount) {
        this.expectedKeyCount = expectedKeyCount;
    }

    public void setEnableLazyDownload(boolean enableLazyDownload) {
        this.isLazyDownload = enableLazyDownload;
    }

    public boolean isEnableLazyDownload() {
        return isLazyDownload;
    }

    public void setPeakFilterElemNum(int peakFilterElemNum) {
        this.peakFilterElemNum = peakFilterElemNum;
    }

    public int peakFilterElemNum() {
        return peakFilterElemNum;
    }

    public void setSnapshotBackupDir(String snapshotBackupDir) {
        this.snapshotBackupDir = snapshotBackupDir;
    }

    public String getSnapshotBackupDir() {
        return snapshotBackupDir;
    }

    public void setEnableLocalRecovery(boolean enableLocalRecovery) {
        this.enableLocalRecovery = enableLocalRecovery;
    }

    public boolean isEnableLocalRecovery() {
        return enableLocalRecovery;
    }
}
