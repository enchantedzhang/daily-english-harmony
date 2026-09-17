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
import com.huawei.ock.bss.common.serialize.SubTableSerializer;
import com.huawei.ock.bss.jni.AbstractNativeHandleReference;
import com.huawei.ock.bss.table.KeyPair;
import com.huawei.ock.bss.table.result.EntryResult;

import org.apache.flink.api.common.typeutils.TypeSerializer;
import org.apache.flink.util.CloseableIterator;
import org.apache.flink.util.MathUtils;
import org.apache.flink.util.Preconditions;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import java.io.IOException;
import java.util.Map;
import java.util.NoSuchElementException;

/**
 * Value为Map类型Table的Entry迭代器（包括KMapTable/SubKMapTable/SubKVTable）
 *
 * @param <K>  一级key
 * @param <MK> 内层key
 * @param <MV> 内层value
 * @since BeiMing 25.0.T1
 */
public class SubTableEntryIterator<K, MK, MV> extends AbstractNativeHandleReference
    implements CloseableIterator<Map.Entry<MK, MV>> {
    private static final Logger LOG = LoggerFactory.getLogger(SubTableEntryIterator.class);

    private final TypeSerializer<MK> keySerializer;

    private final TypeSerializer<MV> valueSerializer;

    private final DirectDataInputDeserializer inputView;

    private EntryResult entryResult;

    public SubTableEntryIterator(long nativeTableAddress, K key, SubTableSerializer<K, MK, MV> tableSerializer) {
        Preconditions.checkNotNull(key);
        Preconditions.checkNotNull(tableSerializer);
        DirectDataOutputSerializer keyBytes = getKeyBytes(key, tableSerializer.getKeySerializer());
        int keyHash = 0;
        if (key instanceof KeyPair) {
            keyHash = ((KeyPair<?, ?>) key).keyHashCode();
        } else {
            keyHash = key.hashCode();
        }
        this.nativeHandle = open(nativeTableAddress, MathUtils.murmurHash(keyHash), keyBytes.data(),
            keyBytes.length());
        Preconditions.checkArgument(nativeHandle != 0);
        this.keySerializer = tableSerializer.getKey2Serializer();
        this.valueSerializer = tableSerializer.getValueSerializer();
        this.inputView = new DirectDataInputDeserializer();
        this.entryResult = new EntryResult();
    }

    private DirectDataOutputSerializer getKeyBytes(K key, TypeSerializer<K> pkSerializer) {
        try {
            return KVSerializerUtil.serKey(key, pkSerializer);
        } catch (IOException e) {
            LOG.error("serialize key {} failed.", key, e);
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
    public Map.Entry<MK, MV> next() {
        if (isNativeHandleClosed()) {
            throw new NoSuchElementException("native handle closed.");
        }

        EntryResult result = next(nativeHandle, entryResult);
        if (result == null) {
            throw new NoSuchElementException("native get result is null.");
        }

        try {
            return KVSerializerUtil.desEntry(result, keySerializer, valueSerializer, inputView);
        } catch (IOException e) {
            LOG.error("deserialize sub map entry failed.", e);
            throw new BSSRuntimeException(e);
        }
    }

    /**
     * nextEntryResult
     *
     * @return com.huawei.ock.bss.table.result.EntryResult
     */
    public EntryResult nextEntryResult() {
        if (isNativeHandleClosed()) {
            throw new NoSuchElementException("native handle closed.");
        }
        return next(nativeHandle, entryResult);
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
     * @param keyHashCode        keyHashCode
     * @param keyBytes           keyBytes
     * @param keySize            keySize
     * @return long
     */
    public static native long open(long nativeTableAddress, int keyHashCode, long keyBytes, int keySize);

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