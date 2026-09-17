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

package com.huawei.ock.bss.common.memory;

import org.apache.flink.core.memory.MemoryUtils;
import org.apache.flink.util.Preconditions;

import java.lang.reflect.Field;
import java.nio.ByteBuffer;

/**
 * 直接内存Buffer
 *
 * @since BeiMing 25.0.T1
 */
public class DirectBuffer {
    private long data;

    private int length;

    /**
     * construct
     */
    public DirectBuffer() {

    }

    /**
     * construct by data and length
     *
     * @param data   native data address
     * @param length data length
     */
    public DirectBuffer(long data, int length) {
        Preconditions.checkState(data != 0, "native data address should not be zero.");
        this.data = data;
        this.length = length;
    }

    /**
     * get native data address
     *
     * @return native data address
     */
    public long data() {
        return data;
    }

    /**
     * get data length
     *
     * @return data length
     */
    public int length() {
        return length;
    }

    @SuppressWarnings("restriction")
    private static final sun.misc.Unsafe UNSAFE = MemoryUtils.UNSAFE;

    public DirectBuffer getCopy() {
        ByteBuffer buffer = ByteBuffer.allocateDirect(length);
        long dest = getDirectAddress(buffer);
        UNSAFE.copyMemory(data, dest, length);
        return new DirectBuffer(dest, length);
    }

    private long getDirectAddress(ByteBuffer buffer) {
        try {
            Field addressField = buffer.getClass()
                .getSuperclass()
                .getSuperclass()
                .getSuperclass()
                .getDeclaredField("address");
            addressField.setAccessible(true);
            return (long) addressField.get(buffer);
        } catch (NoSuchFieldException | IllegalAccessException e) {
            throw new RuntimeException(e);
        }
    }

    /**
     * create a java direct buffer object by native direct buffer object.
     * native direct buffer object is from native slide, it is off-heap memory,
     * call releaseDirectBuffer to release it when we don't use it again.
     *
     * @param bufferAddress native direct buffer address.
     * @return java direct buffer object.
     */
    public static DirectBuffer acquireDirectBuffer(long bufferAddress) {
        if (bufferAddress == 0) {
            return null;
        }
        return new DirectBuffer(nativeGetDirectBufferData(bufferAddress), nativeGetDirectBufferLength(bufferAddress));
    }

    /**
     * release native direct buffer.
     *
     * @param bufferAddress direct buffer.
     */
    public static void releaseDirectBuffer(long bufferAddress) {
        if (bufferAddress == 0) {
            return;
        }
        nativeReleaseDirectBuffer(bufferAddress);
    }

    /**
     * freeDirectBuffer
     *
     * @param buffer buffer
     */
    public static void freeDirectBuffer(long buffer) {
        nativeFreeDirectBuffer(buffer);
    }

    /**
     * get native buffer from jni by native direct buffer object.
     *
     * @param buffer native direct buffer.
     * @return native data address.
     */
    public static native long nativeGetDirectBufferData(long buffer);

    /**
     * get data length from jni by native direct buffer object.
     *
     * @param buffer native direct buffer.
     * @return data length.
     */
    public static native int nativeGetDirectBufferLength(long buffer);

    /**
     * release native direct buffer object.
     *
     * @param buffer native direct buffer object.
     */
    public static native void nativeReleaseDirectBuffer(long buffer);

    /**
     * just for testing, we create a native direct buffer object by data address and length.
     *
     * @param data   native data address.
     * @param length data length.
     * @return native direct buffer address.
     */
    public static native long nativeAcquireDirectBuffer(long data, int length);

    /**
     * we create a native direct buffer object by data address and length.
     *
     * @param length data length.
     * @return native direct buffer address.
     */
    public static native ByteBuffer nativeAllocDirectBuffer(int length);

    /**
     * we create a native direct buffer object by data address and length.
     *
     * @param buffer data addr.
     */
    public static native void nativeFreeDirectBuffer(long buffer);
}