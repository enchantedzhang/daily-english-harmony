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
import org.apache.flink.util.CloseableIterator;
import org.apache.flink.util.Preconditions;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import java.io.IOException;
import java.util.NoSuchElementException;

/**
 * key迭代器
 *
 * @param <K> key
 * @since BeiMing 25.0.T1
 */
public class TableKeyIterator<K> extends AbstractNativeHandleReference implements CloseableIterator<K> {
    private static final Logger LOG = LoggerFactory.getLogger(TableKeyIterator.class);

    private final TypeSerializer<K> keySerializer;

    private final DirectDataInputDeserializer inputView;

    private EntryResult entryResult;

    public TableKeyIterator(long nativeTableAddress, TypeSerializer<K> keySerializer) {
        Preconditions.checkNotNull(keySerializer);
        this.nativeHandle = open(nativeTableAddress);
        Preconditions.checkArgument(nativeHandle != 0);
        this.keySerializer = keySerializer;
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
    public K next() {
        if (isNativeHandleClosed()) {
            throw new NoSuchElementException("native handle closed.");
        }

        try {
            EntryResult result = next(nativeHandle, entryResult);
            if (result == null) {
                throw new NoSuchElementException("native get result is null.");
            }
            return KVSerializerUtil.desKey(result, keySerializer, inputView);
        } catch (IOException e) {
            LOG.error("deserialize map key failed.", e);
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
     * native open
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