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
import com.huawei.ock.bss.common.serialize.TableSerializer;
import com.huawei.ock.bss.jni.AbstractNativeHandleReference;
import com.huawei.ock.bss.table.result.EntryResult;
import com.huawei.ock.bss.table.result.StateListResult;

import org.apache.flink.api.common.typeutils.TypeSerializer;
import org.apache.flink.util.CloseableIterator;
import org.apache.flink.util.Preconditions;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import java.io.IOException;
import java.util.AbstractMap;
import java.util.List;
import java.util.Map;
import java.util.NoSuchElementException;

/**
 * map.entry迭代器
 *
 * @param <K> key
 * @param <V> value
 * @since BeiMing 25.0.T1
 */
public class TableListEntryIterator<K, V> extends AbstractNativeHandleReference
    implements CloseableIterator<Map.Entry<K, List<V>>> {
    private static final Logger LOG = LoggerFactory.getLogger(TableListEntryIterator.class);

    private final TypeSerializer<K> keySerializer;

    private final TypeSerializer<V> valueSerializer;

    private final DirectDataInputDeserializer inputView;

    private final StateListResult listResult;

    private final EntryResult entryResult;

    public TableListEntryIterator(long nativeTableAddress, TableSerializer<K, V> tableSerializer) {
        Preconditions.checkNotNull(tableSerializer);
        this.nativeHandle = open(nativeTableAddress);
        Preconditions.checkArgument(nativeHandle != 0);
        this.keySerializer = tableSerializer.getKeySerializer();
        this.valueSerializer = tableSerializer.getValueSerializer();
        this.inputView = new DirectDataInputDeserializer();
        this.listResult = new StateListResult();
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
    @SuppressWarnings("unchecked")
    public Map.Entry<K, List<V>> next() {
        if (isNativeHandleClosed()) {
            throw new NoSuchElementException("native handle closed.");
        }

        EntryResult result = next(nativeHandle, entryResult);
        if (result == null) {
            throw new NoSuchElementException("native get result is null.");
        }
        listResult.reset();
        try {
            K key = KVSerializerUtil.desKey(result.getKeyAddr(), result.getKeyLen(), keySerializer, inputView);
            listResult.setSize(1);
            long[] valueAddrs = {result.getValueAddr()};
            int[] valueLens = {result.getValueLen()};
            listResult.setAddresses(valueAddrs);
            listResult.setLengths(valueLens);
            List<V> value = (List<V>) KVSerializerUtil.desList(listResult, valueSerializer, inputView);
            return new AbstractMap.SimpleEntry<>(key, value);
        } catch (IOException e) {
            LOG.error("deserialize list entry failed.", e);
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