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

package com.huawei.ock.bss.table.iterator;

import com.huawei.ock.bss.common.exception.BSSRuntimeException;
import com.huawei.ock.bss.jni.AbstractNativeHandleReference;
import com.huawei.ock.bss.table.result.EntryResult;

import org.apache.flink.util.CloseableIterator;

import java.util.Arrays;

/**
 * 功能描述
 *
 * @since 2025-10-21
 */
public class PQKeyIterator<T> extends AbstractNativeHandleReference
        implements CloseableIterator<T> {
    private long nativeHandle;

    private EntryResult result;

    public PQKeyIterator(long pqNativeHandle, byte[] seekHint) {
        this.nativeHandle = open(pqNativeHandle, seekHint);
        if (nativeHandle == 0) {
            throw new BSSRuntimeException("failed to get iterator" + Arrays.toString(seekHint));
        }
        result = new EntryResult();
    }

    @Override
    public boolean hasNext() {
        return hasNext(nativeHandle);
    }

    @Override
    public T next() {
        return (T) next(nativeHandle, result);
    }

    @Override
    public void close() {
        if (owningHandle.compareAndSet(true, false)) {
            close(nativeHandle);
        }
    }

    /**
     * native open
     *
     * @param nativeTableAddress nativeTableAddress
     * @param keyGroupId keyGroupId
     * @return long
     */
    public static native long open(long nativeTableAddress, byte[] keyGroupId);

    /**
     * native hasNext
     *
     * @param nativeAddress nativeAddress
     * @return boolean
     */
    public static native boolean hasNext(long nativeAddress);

    /**
     * native next
     *
     * @param nativeAddress nativeAddress
     * @param result result
     * @return com.huawei.ock.bss.table.result.EntryResult
     */
    public static native EntryResult next(long nativeAddress, EntryResult result);

    /**
     * 关闭C++层的实例
     *
     * @param handle 实例地址
     * @return boolean
     */
    public static native boolean close(long handle);
}