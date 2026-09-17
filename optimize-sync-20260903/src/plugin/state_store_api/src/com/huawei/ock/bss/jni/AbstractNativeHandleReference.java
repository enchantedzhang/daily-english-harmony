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

package com.huawei.ock.bss.jni;

import com.huawei.ock.bss.common.exception.BSSRuntimeException;

import org.apache.flink.util.Preconditions;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

public abstract class AbstractNativeHandleReference extends AbstractNativeReference {
    private static final Logger LOG = LoggerFactory.getLogger(AbstractNativeHandleReference.class);

    protected long nativeHandle;

    protected AbstractNativeHandleReference() {
        super(true);
    }

    public long getNativeHandle() {
        Preconditions.checkArgument(nativeHandle != 0);
        return nativeHandle;
    }

    @Override
    protected void closeInternal() {
        closeInternal(nativeHandle);
    }

    protected void closeInternal(final long handle) {
        LOG.debug("close native instance");
        close(handle);
    }

    /**
     * 检查C++层实例是否关闭
     */
    public void checkNativeHandleValid() {
        if (isNativeHandleClosed()) {
            throw new BSSRuntimeException("native handle closed.");
        }
    }

    /**
     * 关闭C++层的实例
     *
     * @param handle 实例地址
     * @return boolean
     */
    public static native boolean close(long handle);
}
