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
import com.huawei.ock.bss.common.memory.DirectDataInputDeserializer;
import com.huawei.ock.bss.common.serialize.KVSerializerUtil;
import com.huawei.ock.bss.jni.AbstractNativeHandleReference;
import com.huawei.ock.bss.table.result.EntryResult;

import org.apache.flink.api.common.typeutils.TypeSerializer;
import org.apache.flink.api.java.tuple.Tuple3;
import org.apache.flink.util.CloseableIterator;
import org.apache.flink.util.Preconditions;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import java.io.IOException;
import java.util.NoSuchElementException;

/**
 * Value为Map类型Table的Entry迭代器（包括KMapTable/SubKMapTable/SubKVTable）
 *
 * @param <K>  一级key
 * @param <MK> 内层key
 * @param <MV> 内层value
 * @since BeiMing 25.0.T1
 */
public class SubTableFullEntryIterator<K, MK, MV> extends AbstractNativeHandleReference
    implements CloseableIterator<Tuple3<K, MK, MV>> {
    private static final Logger LOG = LoggerFactory.getLogger(SubTableFullEntryIterator.class);

    private final TypeSerializer<K> keySerializer;

    private final TypeSerializer<MK> subKeySerializer;

    private final TypeSerializer<MV> valueSerializer;

    private final DirectDataInputDeserializer inputView;

    private final EntryResult entryResult;

    public SubTableFullEntryIterator(long nativeTableAddress, TypeSerializer<K> keySerializer,
        TypeSerializer<MK> subKeySerializer, TypeSerializer<MV> valueSerializer) {
        this.nativeHandle = open(nativeTableAddress);
        Preconditions.checkArgument(nativeHandle != 0);
        this.keySerializer = keySerializer;
        this.subKeySerializer = subKeySerializer;
        this.valueSerializer = valueSerializer;
        this.inputView = new DirectDataInputDeserializer();
        this.entryResult = new EntryResult();
    }

    @Override
    public boolean hasNext() {
        if (isNativeHandleClosed()) {
            return false;
        }

        boolean ret = hasNext(nativeHandle);
        if (!ret) {
            LOG.debug("the iterator has no more elements, close it.");
            close();
        }
        return ret;
    }

    @Override
    public Tuple3<K, MK, MV> next() {
        if (isNativeHandleClosed()) {
            throw new NoSuchElementException("native handle closed.");
        }
        try {
            EntryResult result = next(nativeHandle, entryResult);
            if (result == null) {
                throw new NoSuchElementException("native get result is null.");
            }

            return KVSerializerUtil.desTuple3(result, keySerializer, subKeySerializer, valueSerializer, inputView);
        } catch (IOException e) {
            LOG.error("deserialize sub map entry failed.", e);
            throw new BSSRuntimeException(e);
        }
    }

    @Override
    public void close() {
        if (owningHandle.compareAndSet(true, false)) {
            close(nativeHandle);
        }
    }

    /**
     * open
     *
     * @param nativeTableAddress nativeTableAddress
     * @return long
     */
    public static native long open(long nativeTableAddress);

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
     * @param entryResult   entryResult
     * @return EntryResult
     */
    public static native EntryResult next(long nativeAddress, EntryResult entryResult);

    /**
     * 关闭C++层的实例
     *
     * @param handle 实例地址
     * @return boolean
     */
    public static native boolean close(long handle);
}