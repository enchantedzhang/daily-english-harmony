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

package com.huawei.ock.bss.common;

import com.huawei.ock.bss.common.exception.BSSRuntimeException;
import com.huawei.ock.bss.jni.AbstractNativeHandleReference;

/**
 * Checkpoint类，通知下层db执行checkpoint
 *
 * @since BeiMing 25.0.T1
 */
public class Checkpoint extends AbstractNativeHandleReference {
    private final BoostStateDB db_;

    private final long checkpointId;

    private Checkpoint(BoostStateDB db, long checkpointId, String directory) {
        super();
        this.nativeHandle = newCheckpoint(db.getNativeHandle(), checkpointId, directory);
        if (this.nativeHandle == 0) {
            throw new BSSRuntimeException("Failed to Sync create checkpoint");
        }
        this.checkpointId = checkpointId;
        this.db_ = db;
    }

    /**
     * 创建checkpoint实例
     *
     * @param db db
     * @param checkpointId checkpointId对于下层一个checkpoint类实例唯一
     * @param directory 落盘目录
     * @return checkpoint实例
     */
    public static Checkpoint create(BoostStateDB db, long checkpointId, String directory) {
        if (db == null) {
            throw new IllegalArgumentException("BoostStateDB instance shall not be null.");
        } else if (db.isNativeHandleClosed()) {
            throw new IllegalArgumentException("BoostStateDB instance must be initialized.");
        } else {
            return new Checkpoint(db, checkpointId, directory);
        }
    }

    /**
     * 下层db落盘
     *
     * @param isIncremental 是否是增量checkpoint
     * @throws BSSRuntimeException BSSRuntimeException
     */
    public void createCheckpoint(boolean isIncremental) throws BSSRuntimeException {
        boolean res = this.createCheckpoint(this.db_.getNativeHandle(), this.checkpointId, isIncremental);
        if (!res) {
            throw new BSSRuntimeException("Failed to async create checkpoint");
        }
    }

    // 准备
    private static native long newCheckpoint(long handle, long checkpointId, String directory);

    // 待定义新异常类型
    // 落盘
    private native boolean createCheckpoint(long nativeHandle, long checkpointId, boolean isIncremental)
        throws BSSRuntimeException;
}
