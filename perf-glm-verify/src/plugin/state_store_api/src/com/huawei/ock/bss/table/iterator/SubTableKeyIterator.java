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
import com.huawei.ock.bss.common.memory.DirectDataOutputSerializer;
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
 * 包含Namespace的Table类型的Key迭代器
 *
 * @param <K> 分区key
 * @param <N> namespace
 * @since BeiMing 25.0.T1
 */
public class SubTableKeyIterator<K, N> extends AbstractNativeHandleReference implements CloseableIterator<K> {
    private static final Logger LOG = LoggerFactory.getLogger(SubTableKeyIterator.class);

    private final TypeSerializer<K> keySerializer;

    private final DirectDataInputDeserializer inputView;

    private EntryResult entryResult;

    public SubTableKeyIterator(long nativeTableAddress, N namespace, TypeSerializer<K> keySerializer,
                               TypeSerializer<N> nsSerializer) {
        Preconditions.checkNotNull(namespace);
        Preconditions.checkNotNull(keySerializer);
        Preconditions.checkNotNull(nsSerializer);
        DirectDataOutputSerializer nsBytes = getNamespaceBytes(namespace, nsSerializer);
        this.nativeHandle = open(nativeTableAddress, nsBytes.data(), nsBytes.length());
        Preconditions.checkArgument(nativeHandle != 0);
        this.keySerializer = keySerializer;
        this.inputView = new DirectDataInputDeserializer();
        this.entryResult = new EntryResult();
    }

    private DirectDataOutputSerializer getNamespaceBytes(N namespace, TypeSerializer<N> nsSerializer) {
        try {
            return KVSerializerUtil.serSubKey(namespace, nsSerializer);
        } catch (IOException e) {
            throw new BSSRuntimeException(e);
        }
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

        EntryResult result = next(nativeHandle, entryResult);
        if (result == null) {
            throw new NoSuchElementException("native get result is null.");
        }
        try {
            return KVSerializerUtil.desKey(result, keySerializer, inputView);
        } catch (IOException e) {
            LOG.error("deserialize sub map key failed.", e);
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
     * @param nsBytes            nsBytes
     * @param nsSize             nsSize
     * @return long
     */
    public static native long open(long nativeTableAddress, long nsBytes, int nsSize);

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
     * @param entryResult entryResult
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