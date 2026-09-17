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

package com.huawei.ock.bss.table;

import com.huawei.ock.bss.common.BoostStateDB;
import com.huawei.ock.bss.common.conf.BoostConfig;
import com.huawei.ock.bss.common.consistency.KVTableConsistencyUtil;
import com.huawei.ock.bss.common.consistency.KVTableUtilParam;
import com.huawei.ock.bss.common.exception.BSSRuntimeException;
import com.huawei.ock.bss.common.memory.DirectBuffer;
import com.huawei.ock.bss.common.memory.DirectDataInputDeserializer;
import com.huawei.ock.bss.common.memory.DirectDataOutputSerializer;
import com.huawei.ock.bss.common.serialize.KVSerializerUtil;
import com.huawei.ock.bss.table.api.TableDescription;
import com.huawei.ock.bss.table.description.KVTableDescription;

import org.apache.flink.api.common.typeutils.TypeSerializer;
import org.apache.flink.util.Preconditions;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import java.io.IOException;
import java.util.Map;
import java.util.concurrent.ConcurrentHashMap;

import javax.annotation.Nullable;

/**
 * KVTableImpl
 *
 * @since BeiMing 25.0.T1
 */
public class KVTableImpl<K, V> extends AbstractTable<K, V> {
    private static final Logger LOG = LoggerFactory.getLogger(KVTableImpl.class);

    /**
     * key序列化器
     */
    protected final TypeSerializer<K> keySerializer;

    /**
     * value序列化器
     */
    protected final TypeSerializer<V> valueSerializer;

    private final DirectDataInputDeserializer inputView;

    private final Map<K, KVTableUtilParam<K, V>> debugMap;

    private final Object lock = new Object();

    public KVTableImpl(BoostStateDB db, KVTableDescription<K, V> description) {
        super(KVTableImpl.class.getSimpleName(), db, description);
        this.keySerializer = description.getTableSerializer().getKeySerializer();
        this.valueSerializer = description.getTableSerializer().getValueSerializer();
        this.inputView = new DirectDataInputDeserializer();
        this.debugMap = new ConcurrentHashMap<>();
    }

    /**
     * put
     *
     * @param key   key
     * @param value value
     */
    public void put(K key, V value) {
        Preconditions.checkNotNull(key, "input key should not be null.");
        Preconditions.checkNotNull(value, "input value should not be null.");
        checkNativeHandleValid();

        try {
            DirectDataOutputSerializer keyBytes = KVSerializerUtil.serKey(key, keySerializer);
            DirectDataOutputSerializer valueBytes = KVSerializerUtil.serValue(value, valueSerializer);
            put(nativeHandle, getKeyHashCode(key), keyBytes.data(), keyBytes.length(), valueBytes.data(),
                valueBytes.length());
            addKeyHash(key.hashCode());
            checkPutConsistency(key, value, keyBytes, valueBytes);
        } catch (IOException ioException) {
            LOG.error("Fail to put <{}, {}>.", key, value, ioException);
            throw new BSSRuntimeException("Fail to put " + key + ", " + value, ioException);
        }
    }

    /**
     * get
     *
     * @param key key
     * @return V
     */
    @Nullable
    public V get(K key) {
        Preconditions.checkNotNull(key, "input key should not be null.");
        if (!testKeyHash(key.hashCode())) {
            LOG.debug("the key {} is filtered by bloom filter.", key);
            return null;
        }

        checkNativeHandleValid();
        DirectDataOutputSerializer keyBytes = null;
        DirectBuffer valueBytes = null;
        long valueBufferAddressL = 0L;
        try {
            keyBytes = KVSerializerUtil.serKey(key, keySerializer);
            valueBufferAddressL = get(nativeHandle, getKeyHashCode(key), keyBytes.data(), keyBytes.length());
            valueBytes = DirectBuffer.acquireDirectBuffer(valueBufferAddressL);
            checkContainsConsistency(key, keyBytes, valueBytes != null);

            if (valueBytes == null) {
                LOG.debug("the key {} is not exist.", key);
                return null;
            }

            V actualValue = KVSerializerUtil.desValue(valueBytes, valueSerializer, inputView);
            checkGetConsistency(key, keyBytes, actualValue, valueBytes);
            return actualValue;
        } catch (IOException ioException) {
            LOG.error("Fail to get {}.", key, ioException);
            printErrMsg(keyBytes, null, valueBytes);
            throw new BSSRuntimeException("Fail to get " + key, ioException);
        } finally {
            DirectBuffer.releaseDirectBuffer(valueBufferAddressL);
        }
    }

    /**
     * contains
     *
     * @param key key
     * @return boolean
     */
    public boolean contains(K key) {
        Preconditions.checkNotNull(key, "input key should not be null.");
        if (!testKeyHash(key.hashCode())) {
            LOG.debug("the key {} is filtered by bloom filter.", key);
            return false;
        }

        checkNativeHandleValid();

        try {
            DirectDataOutputSerializer keyBytes = KVSerializerUtil.serKey(key, keySerializer);
            boolean actualContains = contains(nativeHandle, getKeyHashCode(key), keyBytes.data(), keyBytes.length());
            checkContainsConsistency(key, keyBytes, actualContains);
            return actualContains;
        } catch (IOException ioException) {
            LOG.error("Fail to check contains key: {}", key, ioException);
            throw new BSSRuntimeException("Fail to check contains key: " + key, ioException);
        }
    }

    /**
     * remove
     *
     * @param key key
     */
    public void remove(K key) {
        Preconditions.checkNotNull(key, "input key should not be null.");
        checkNativeHandleValid();

        try {
            DirectDataOutputSerializer keyBytes = KVSerializerUtil.serKey(key, keySerializer);
            remove(nativeHandle, getKeyHashCode(key), keyBytes.data(), keyBytes.length());
            checkRemoveConsistency(key, keyBytes);
        } catch (IOException ioException) {
            LOG.error("Fail to remove key: {}", key, ioException);
            throw new BSSRuntimeException("Fail to remove " + key, ioException);
        }
    }

    /**
     * getSerializedValue
     *
     * @param key key
     * @return byte[]
     */
    @Override
    public byte[] getSerializedValue(K key) {
        Preconditions.checkNotNull(key);
        if (!testKeyHash(key.hashCode())) {
            LOG.debug("the key {} is filtered by bloom filter.", key);
            return null;
        }
        DirectDataOutputSerializer keyBytes = null;
        DirectBuffer valueBytes = null;
        long valueBufferAddressL = 0L;
        try {
            keyBytes = KVSerializerUtil.serKey(key, keySerializer);
            valueBufferAddressL = get(nativeHandle, getKeyHashCode(key), keyBytes.data(), keyBytes.length());
            valueBytes = DirectBuffer.acquireDirectBuffer(valueBufferAddressL);
            if (valueBytes == null) {
                LOG.debug("the key {} is not exist.", key);
                return null;
            }
            return KVSerializerUtil.getCopyOfBuffer(valueBytes);
        } catch (IOException ioException) {
            LOG.error("Fail to getSerializedValue {}.", key, ioException);
            printErrMsg(keyBytes, null, valueBytes);
            throw new BSSRuntimeException("Fail to getSerializedValue " + key, ioException);
        } finally {
            DirectBuffer.releaseDirectBuffer(valueBufferAddressL);
        }
    }

    @Override
    public TableDescription getTableDescription() {
        return description;
    }

    /**
     * check put consistency.
     *
     * @param key        key
     * @param value      value
     * @param keyBytes   keyBytes
     * @param valueBytes valueBytes
     */
    private void checkPutConsistency(K key, V value, DirectDataOutputSerializer keyBytes,
        DirectDataOutputSerializer valueBytes) {
        if (!BoostConfig.DEBUG_MODE) {
            return;
        }
        synchronized (lock) {
            KVTableConsistencyUtil.put(debugMap, new KVTableUtilParam.Builder<K, V>()
                .setKeyHashCode(getKeyHashCode(key)).setKey(key).setKeyBytes(keyBytes.wrapDirectData())
                .setValue(value).setValueBytes(valueBytes.wrapDirectData()).build());
        }
    }

    /**
     * check contains consistency.
     *
     * @param key            key
     * @param keyBytes       keyBytes
     * @param actualContains actualContains
     */
    private void checkContainsConsistency(K key, DirectDataOutputSerializer keyBytes, boolean actualContains) {
        if (!BoostConfig.DEBUG_MODE) {
            return;
        }
        synchronized (lock) {
            KVTableConsistencyUtil.checkContains(debugMap, new KVTableUtilParam.Builder<K, V>().setKey(key)
                .setKeyHashCode(getKeyHashCode(key)).setKeyBytes(keyBytes.wrapDirectData())
                .setActualContains(actualContains).build());
        }
    }

    /**
     * check get consistency
     *
     * @param key         key
     * @param keyBytes    keyBytes
     * @param actualValue actualValue
     * @param valueBytes  valueBytes
     */
    private void checkGetConsistency(K key, DirectDataOutputSerializer keyBytes, V actualValue,
        DirectBuffer valueBytes) {
        if (!BoostConfig.DEBUG_MODE) {
            return;
        }
        KVTableConsistencyUtil.checkForGet(debugMap, new KVTableUtilParam.Builder<K, V>()
            .setKeyHashCode(getKeyHashCode(key)).setKey(key).setKeyBytes(keyBytes.wrapDirectData())
            .setValue(actualValue).setValueBytes(valueBytes).build());
    }

    /**
     * check remove consistency.
     *
     * @param key      key
     * @param keyBytes keyBytes
     */
    private void checkRemoveConsistency(K key, DirectDataOutputSerializer keyBytes) {
        if (!BoostConfig.DEBUG_MODE) {
            return;
        }
        synchronized (lock) {
            KVTableConsistencyUtil.remove(debugMap, new KVTableUtilParam.Builder<K, V>().setKey(key)
                .setKeyHashCode(getKeyHashCode(key)).setKeyBytes(keyBytes.wrapDirectData()).build());
        }
    }

    /**
     * native put
     *
     * @param nativeHandle nativeHandle
     * @param keyHashCode  keyHashCode
     * @param kBytes       kBytes
     * @param kSize        kSize
     * @param vBytes       vBytes
     * @param vSize        vSize
     */
    public static native void put(long nativeHandle, int keyHashCode, long kBytes, int kSize, long vBytes,
        int vSize);

    /**
     * native get
     *
     * @param nativeHandle nativeHandle
     * @param keyHashCode  keyHashCode
     * @param kBytes       kBytes
     * @param kSize        kSize
     * @return long
     */
    public static native long get(long nativeHandle, int keyHashCode, long kBytes, int kSize);

    /**
     * native contains
     *
     * @param nativeHandle nativeHandle
     * @param keyHashCode  keyHashCode
     * @param kBytes       kBytes
     * @param kSize        kSize
     * @return boolean
     */
    public static native boolean contains(long nativeHandle, int keyHashCode, long kBytes, int kSize);

    /**
     * native remove
     *
     * @param nativeHandle nativeHandle
     * @param keyHashCode  keyHashCode
     * @param kBytes       kBytes
     * @param kSize        kSize
     */
    public static native void remove(long nativeHandle, int keyHashCode, long kBytes, int kSize);
}