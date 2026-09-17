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

package com.huawei.ock.bss.restore;

import org.apache.flink.runtime.state.StateHandleID;
import org.apache.flink.runtime.state.StreamStateHandle;

import java.util.Map;
import java.util.SortedMap;
import java.util.TreeMap;
import java.util.UUID;

/**
 * 封装恢复结果
 *
 * @since BeiMing 25.0.T1
 */
public class BoostRestoreResult {
    /**
     * NoneRestore时返回结果
     */
    public static final BoostRestoreResult EMPTY_RESULT = new BoostRestoreResult();

    private final long lastCompletedCheckpointId;

    private final UUID backendUID;

    private final SortedMap<Long, Map<StateHandleID, StreamStateHandle>> restoredSstFiles;

    private final long downloadTime;

    public BoostRestoreResult(long lastCompletedCheckpointId, UUID backendUID,
        SortedMap<Long, Map<StateHandleID, StreamStateHandle>> restoredSstFiles, long downloadTime) {
        this.lastCompletedCheckpointId = lastCompletedCheckpointId;
        this.backendUID = backendUID;
        this.restoredSstFiles = restoredSstFiles;
        this.downloadTime = downloadTime;
    }

    private BoostRestoreResult() {
        this(-1, UUID.randomUUID(), new TreeMap<>(), 0L);
    }

    /**
     * 获取上次完成的checkpointId
     *
     * @return lastCompletedCheckpointId
     */
    public long getLastCompletedCheckpointId() {
        return lastCompletedCheckpointId;
    }

    /**
     * 获取恢复的backendUID
     *
     * @return backendUID
     */
    public UUID getBackendUID() {
        return backendUID;
    }

    /**
     * 获取已经打快照的增量checkpoint文件
     *
     * @return restoredSstFiles
     */
    public SortedMap<Long, Map<StateHandleID, StreamStateHandle>> getRestoredSstFiles() {
        return restoredSstFiles;
    }

    /**
     * 获取以秒计的下载文件用时
     *
     * @return 下载用时，单位秒
     */
    public long getDownloadTime() {
        return downloadTime / 1000;
    }
}
