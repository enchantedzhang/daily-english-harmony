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
import com.huawei.ock.bss.common.consistency.MapTableConsistencyUtil;
import com.huawei.ock.bss.common.consistency.MapTableUtilParam;
import com.huawei.ock.bss.common.exception.BSSRuntimeException;
import com.huawei.ock.bss.common.memory.DirectBuffer;
import com.huawei.ock.bss.common.memory.DirectDataInputDeserializer;
import com.huawei.ock.bss.common.memory.DirectDataOutputSerializer;
import com.huawei.ock.bss.common.serialize.KVSerializerUtil;
import com.huawei.ock.bss.common.serialize.SubTableSerializer;
import com.huawei.ock.bss.table.api.KMapTable;
import com.huawei.ock.bss.table.api.Table;
import com.huawei.ock.bss.table.description.KMapTableDescription;
import com.huawei.ock.bss.table.iterator.SubTableEntryIterator;
import com.huawei.ock.bss.table.iterator.SubTableFullEntryIterator;
import com.huawei.ock.bss.table.result.EntryResult;

import org.apache.flink.api.common.typeutils.TypeSerializer;
import org.apache.flink.api.java.tuple.Tuple3;
import org.apache.flink.core.memory.DataOutputSerializer;
import org.apache.flink.util.CloseableIterator;
import org.apache.flink.util.Preconditions;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import java.io.IOException;
import java.util.HashMap;
import java.util.Iterator;
import java.util.Map;
import java.util.concurrent.ConcurrentHashMap;

/**
 * AbstractKMapTable
 *
 * @param <K>  key
 * @param <UK> userKey
 * @param <UV> userValue
 * @param <M>  mapValue
 * @since BeiMing 25.0.T1
 */
public abstract class AbstractKMapTable<K, UK, UV, M extends Map<UK, UV>> extends AbstractTable<K, M>
    implements KMapTable<K, UK, UV, M> {
    private static final Logger LOG = LoggerFactory.getLogger(AbstractKMapTable.class);

    /**
     * DataOutputSerializer中buffer的初始大小，和KvStateSerializer.serializeMap中的保持一致
     */
    private static final int DOS_START_SIZE = 32;

    /**
     * key序列化器
     */
    protected final TypeSerializer<K> keySerializer;

    /**
     * subKey序列化器
     */
    protected final TypeSerializer<UK> ukSerializer;

    /**
     * value序列化器
     */
    protected final TypeSerializer<UV> uvSerializer;

    /**
     * inputView
     */
    protected final DirectDataInputDeserializer inputView;

    /**
     * debugMap 一致性校验debugMap
     */
    protected final Map<K, Map<UK, byte[]>> debugMap;

    private final Object lock = new Object();

    private DataOutputSerializer dos;

    public AbstractKMapTable(String nativeClass, BoostStateDB db, KMapTableDescription<K, UK, UV> description) {
        super(nativeClass, db, description);
        SubTableSerializer<K, UK, UV> subTableSerializer = description.getTableSerializer();
        this.keySerializer = subTableSerializer.getKeySerializer();
        this.ukSerializer = subTableSerializer.getKey2Serializer();
        this.uvSerializer = subTableSerializer.getValueSerializer();
        this.inputView = new DirectDataInputDeserializer();
        this.debugMap = new ConcurrentHashMap<>();
        this.dos = new DataOutputSerializer(DOS_START_SIZE);
    }

    /**
     * contains
     *
     * @param key     key
     * @param userKey userKey
     * @return boolean
     */
    public boolean contains(K key, UK userKey) {
        Preconditions.checkNotNull(key);
        Preconditions.checkNotNull(userKey);
        if (!testKeyHash(key.hashCode(), userKey.hashCode())) {
            LOG.debug("the key {}/{} is filtered by bloom filter.", key, userKey);
            return false;
        }

        checkNativeHandleValid();
        try {
            DirectDataOutputSerializer firstKey = KVSerializerUtil.serKey(key, keySerializer);
            DirectDataOutputSerializer secondKey = KVSerializerUtil.serSubKey(userKey, ukSerializer);
            boolean isContains = contains(nativeHandle, getKeyHashCode(key), firstKey.data(), firstKey.length(),
                secondKey.data(), secondKey.length());
            checkContainsConsistency(key, userKey, isContains, firstKey, secondKey);
            return isContains;
        } catch (IOException ioException) {
            LOG.error("Fail to get key {} map key {} in map.", key, userKey, ioException);
            throw new BSSRuntimeException("Fail to get key " + key + " map key " + userKey + " with list.",
                ioException);
        }
    }

    /**
     * get
     *
     * @param key     key
     * @param userKey userKey
     * @return UV
     */
    public UV get(K key, UK userKey) {
        Preconditions.checkNotNull(key);
        Preconditions.checkNotNull(userKey);
        if (!testKeyHash(key.hashCode(), userKey.hashCode())) {
            LOG.debug("the key {}/{} is filtered by bloom filter.", key, userKey);
            return null;
        }

        checkNativeHandleValid();
        DirectDataOutputSerializer firstKey = null;
        DirectDataOutputSerializer secondKey = null;
        DirectBuffer valueBytes = null;
        long valueBufferAddressL = 0;
        try {
            firstKey = KVSerializerUtil.serKey(key, keySerializer);
            secondKey = KVSerializerUtil.serSubKey(userKey, ukSerializer);
            valueBufferAddressL = get(nativeHandle, getKeyHashCode(key), firstKey.data(), firstKey.length(),
                secondKey.data(), secondKey.length());
            valueBytes = DirectBuffer.acquireDirectBuffer(valueBufferAddressL);
            checkContainsConsistency(key, userKey, valueBytes != null, firstKey, secondKey);
            if (valueBytes == null) {
                LOG.debug("key {} and map key {} is not exist.", key, userKey);
                return null;
            }
            checkGetConsistency(key, userKey, firstKey, secondKey, valueBytes);
            return KVSerializerUtil.desValue(valueBytes, uvSerializer, inputView);
        } catch (IOException ioException) {
            LOG.error("Fail to get key {} map key {} in map.", getKeyHashCode(key), userKey, ioException);
            printErrMsg(firstKey, secondKey, valueBytes);
            throw new BSSRuntimeException("Fail to get key " + getKeyHashCode(key) + " map key " + userKey
                + " with list.", ioException);
        } finally {
            DirectBuffer.releaseDirectBuffer(valueBufferAddressL);
        }
    }

    /**
     * getOrDefault
     *
     * @param key              key
     * @param userKey          userKey
     * @param defaultUserValue defaultUserValue
     * @return UV
     */
    public UV getOrDefault(K key, UK userKey, UV defaultUserValue) {
        UV value = get(key, userKey);
        return (value == null) ? defaultUserValue : value;
    }

    /**
     * add
     *
     * @param key       key
     * @param userKey   userKey
     * @param userValue userValue
     */
    public void add(K key, UK userKey, UV userValue) {
        Preconditions.checkNotNull(key);
        Preconditions.checkNotNull(userKey);
        checkNativeHandleValid();

        try {
            DirectDataOutputSerializer firstKey = KVSerializerUtil.serKey(key, keySerializer);
            DirectDataOutputSerializer secondKey = KVSerializerUtil.serSubKey(userKey, ukSerializer);
            DirectDataOutputSerializer value = KVSerializerUtil.serValue(userValue, uvSerializer);

            add(nativeHandle, getKeyHashCode(key), firstKey.data(), firstKey.length(), secondKey.data(),
                secondKey.length(), value.data(), value.length());
            addKeyHash(key.hashCode());
            addKeyHash(key.hashCode(), userKey.hashCode());
            checkAddConsistency(key, userKey, firstKey, value);
        } catch (IOException ioException) {
            LOG.error("Fail to add key {} map key {} in map.", key, userKey, ioException);
            throw new BSSRuntimeException("Fail to add key " + key + " map key " + userKey + " with list.",
                ioException);
        }
    }

    /**
     * addForIterator
     *
     * @param key       key
     * @param userKey   userKey
     * @param userValue userValue
     */
    public void addForIterator(K key, UK userKey, UV userValue) {
        add(key, userKey, userValue);
    }

    /**
     * addAll
     *
     * @param key      key
     * @param mappings mappings
     */
    public void addAll(final K key, final Map<? extends UK, ? extends UV> mappings) {
        for (Map.Entry<? extends UK, ? extends UV> entry : mappings.entrySet()) {
            add(key, entry.getKey(), entry.getValue());
        }
    }

    /**
     * put
     *
     * @param key   key
     * @param value value
     */
    public void put(K key, M value) {
        throw new BSSRuntimeException("Put(Override) a whole map is not supported.");
    }

    /**
     * remove
     *
     * @param key     key
     * @param userKey userKey
     */
    public void remove(K key, UK userKey) {
        Preconditions.checkNotNull(key);
        Preconditions.checkNotNull(userKey);
        checkNativeHandleValid();

        try {
            DirectDataOutputSerializer firstKey = KVSerializerUtil.serKey(key, keySerializer);
            DirectDataOutputSerializer secondKey = KVSerializerUtil.serSubKey(userKey, ukSerializer);
            remove(nativeHandle, getKeyHashCode(key), firstKey.data(), firstKey.length(), secondKey.data(),
                secondKey.length());
            checkRemoveConsistency(key, userKey, firstKey, secondKey);
        } catch (IOException ioException) {
            LOG.error("Fail to remove key {} map key {} in map.", key, userKey, ioException);
            throw new BSSRuntimeException("Fail to remove key " + key + " map key " + userKey + " with list.",
                ioException);
        }
    }

    /**
     * entries
     *
     * @param key key
     * @return java.lang.Iterable<java.util.Map.Entry < UK, UV>>
     */
    public Iterable<Map.Entry<UK, UV>> entries(K key) {
        return () -> iterator(key);
    }

    /**
     * iterator
     *
     * @param key key
     * @return java.util.Iterator<java.util.Map.Entry < UK, UV>>
     */
    @SuppressWarnings("unchecked")
    public Iterator<Map.Entry<UK, UV>> iterator(K key) {
        Preconditions.checkNotNull(key);
        if (!testKeyHash(key.hashCode())) {
            LOG.debug("the key {} is filtered by bloom filter.", key);
            return null;
        }
        checkNativeHandleValid();
        LOG.debug("get iterator of key:{}", key);
        return new SubTableEntryIterator<>(nativeHandle, key,
            (SubTableSerializer<K, UK, UV>) this.description.getTableSerializer());
    }

    /**
     * get
     *
     * @param key key
     * @return M
     */
    @SuppressWarnings("unchecked")
    public M get(K key) {
        Map<UK, UV> retMap = new HashMap<>();
        Iterator<Map.Entry<UK, UV>> iterator = this.iterator(key);
        while (iterator.hasNext()) {
            Map.Entry<UK, UV> entry = iterator.next();
            retMap.put(entry.getKey(), entry.getValue());
        }

        if (retMap.isEmpty()) {
            LOG.info("iterator of key {} is empty.", key);
            return null;
        }

        return (M) retMap;
    }

    /**
     * remove
     *
     * @param key key
     */
    public void remove(K key) {
        Preconditions.checkNotNull(key);
        checkNativeHandleValid();

        try {
            DirectDataOutputSerializer firstKey = KVSerializerUtil.serKey(key, keySerializer);
            removeAll(nativeHandle, getKeyHashCode(key), firstKey.data(), firstKey.length());
            checkRemoveConsistency(key, null, firstKey, null);
        } catch (IOException ioException) {
            LOG.error("Fail to remove key {} in map.", key, ioException);
            throw new BSSRuntimeException("Fail to remove key " + key + " with list.", ioException);
        }
    }

    /**
     * removeForIterator
     *
     * @param key    key
     * @param mapKey mapKey
     */
    public void removeForIterator(K key, UK mapKey) {
        throw new BSSRuntimeException("Unsupported.");
    }

    @Override
    public byte[] getSerializedValue(K key) {
        Preconditions.checkNotNull(key);
        SubTableEntryIterator<K, UK, UV> iterator = (SubTableEntryIterator<K, UK, UV>) this.iterator(key);
        try {
            while (iterator.hasNext()) {
                EntryResult result = iterator.nextEntryResult();
                DirectBuffer keyBuffer = new DirectBuffer(result.getKeyAddr(), result.getKeyLen());
                dos.write(KVSerializerUtil.getCopyOfBuffer(keyBuffer));
                int valueLen = result.getValueLen();
                if (valueLen == 0) {
                    dos.writeBoolean(true);
                } else {
                    dos.writeBoolean(false);
                    DirectBuffer valueBuffer = new DirectBuffer(result.getValueAddr(), result.getValueLen());
                    dos.write(KVSerializerUtil.getCopyOfBuffer(valueBuffer));
                }
            }
            return dos.getCopyOfBuffer();
        } catch (IOException ioException) {
            LOG.error("Failed to write entry bytes to tmp DataOutputSerializer, key: {}", key, ioException);
            throw new BSSRuntimeException("Failed to getSerializedValue for mapState, key: " + key, ioException);
        } finally {
            dos.clear();
        }
    }

    /**
     * migrateValue 获取table的所有数据重新序列化写入
     *
     * @param oldTable oldTable
     */
    @Override
    public void migrateValue(Table<K> oldTable) {
        long start = System.currentTimeMillis();
        if (!(oldTable instanceof AbstractKMapTable)) {
            LOG.error("UnExcept table type:{}", oldTable.getClass().getCanonicalName());
            return;
        }
        try (CloseableIterator<Tuple3<K, UK, UV>> entryIterator = ((AbstractKMapTable) oldTable).entryIterator()) {
            if (entryIterator == null) {
                LOG.error("Fail to migrate, iterator is null");
                return;
            }
            while (entryIterator.hasNext()) {
                Tuple3<K, UK, UV> tuple = entryIterator.next();
                this.add(tuple.f0, tuple.f1, tuple.f2);
            }
        } catch (Exception e) {
            LOG.error("Fail to migrate :", e);
            throw new BSSRuntimeException("Fail to migrate mapTable:", e);
        }
        long end = System.currentTimeMillis();
        LOG.info("Migrate end, cost {} ms", end - start);
    }

    /**
     * 全量迭代器
     *
     * @return 迭代器
     */
    public CloseableIterator<Tuple3<K, UK, UV>> entryIterator() {
        checkNativeHandleValid();
        return new SubTableFullEntryIterator<>(nativeHandle, keySerializer, ukSerializer, uvSerializer);
    }

    /**
     * check get consistency.
     *
     * @param key        key
     * @param userKey    userKey
     * @param firstKey   firstKey
     * @param secondKey  secondKey
     * @param valueBytes valueBytes
     */
    protected void checkGetConsistency(K key, UK userKey, DirectDataOutputSerializer firstKey,
        DirectDataOutputSerializer secondKey, DirectBuffer valueBytes) {
        if (!BoostConfig.DEBUG_MODE) {
            return;
        }
        synchronized (lock) {
            MapTableConsistencyUtil.checkForGet(debugMap,
                new MapTableUtilParam.Builder<K, UK>().setKey(key).setKeyHashCode(getKeyHashCode(key))
                    .setMapKey(userKey).setFirstKey(firstKey.wrapDirectData())
                    .setSecondKey(secondKey.wrapDirectData()).setMapValue(valueBytes).build());
        }
    }

    /**
     * check contains consistency.
     *
     * @param key        key
     * @param userKey    userKey
     * @param isContains isContains
     * @param firstKey   firstKey
     * @param secondKey  secondKey
     */
    protected void checkContainsConsistency(K key, UK userKey, boolean isContains,
        DirectDataOutputSerializer firstKey, DirectDataOutputSerializer secondKey) {
        if (!BoostConfig.DEBUG_MODE) {
            return;
        }
        synchronized (lock) {
            MapTableConsistencyUtil.checkContains(debugMap,
                new MapTableUtilParam.Builder<K, UK>().setActualContains(isContains).setKey(key)
                    .setKeyHashCode(getKeyHashCode(key)).setMapKey(userKey)
                    .setFirstKey(firstKey.wrapDirectData())
                    .setSecondKey(secondKey.wrapDirectData()).build());
        }
    }

    /**
     * check add Consistency
     *
     * @param key      key
     * @param userKey  userKey
     * @param firstKey firstKey
     * @param value    value
     */
    protected void checkAddConsistency(K key, UK userKey, DirectDataOutputSerializer firstKey,
        DirectDataOutputSerializer value) {
        if (!BoostConfig.DEBUG_MODE) {
            return;
        }
        synchronized (lock) {
            MapTableConsistencyUtil.add(debugMap, new MapTableUtilParam.Builder<K, UK>().setKey(key)
                .setKeyHashCode(getKeyHashCode(key)).setMapKey(userKey).setFirstKey(firstKey.wrapDirectData())
                .setMapValue(value.wrapDirectData()).setTableName(description.getTableName()).build());
        }
    }

    /**
     * check remove consistency.
     *
     * @param key       key
     * @param userKey   userKey
     * @param firstKey  firstKey
     * @param secondKey secondKey
     */
    protected void checkRemoveConsistency(K key, UK userKey, DirectDataOutputSerializer firstKey,
        DirectDataOutputSerializer secondKey) {
        if (BoostConfig.DEBUG_MODE) {
            synchronized (lock) {
                MapTableConsistencyUtil.remove(debugMap, new MapTableUtilParam.Builder<K, UK>().setKey(key)
                    .setKeyHashCode(getKeyHashCode(key)).setMapKey(userKey).setFirstKey(firstKey.wrapDirectData())
                    .setSecondKey(secondKey.wrapDirectData()).setTableName(description.getTableName()).build());
            }
        }
    }

    /**
     * native contains
     *
     * @param nativeHandle nativeHandle
     * @param keyHashCode  keyHashCode
     * @param kBytes       kBytes
     * @param kSize        kSize
     * @param userKBytes   userKBytes
     * @param userKSize    userKSize
     * @return boolean
     */
    public static native boolean contains(long nativeHandle, int keyHashCode, long kBytes, int kSize, long userKBytes,
        int userKSize);

    /**
     * native get
     *
     * @param nativeHandle nativeHandle
     * @param keyHashCode  keyHashCode
     * @param kBytes       kBytes
     * @param kSize        kSize
     * @param userKBytes   userKBytes
     * @param userKSize    userKSize
     * @return long
     */
    public static native long get(long nativeHandle, int keyHashCode, long kBytes, int kSize, long userKBytes,
        int userKSize);

    /**
     * native add
     *
     * @param nativeHandle nativeHandle
     * @param keyHashCode  keyHashCode
     * @param kBytes       kBytes
     * @param kSize        kSize
     * @param userKBytes   userKBytes
     * @param userKSize    userKSize
     * @param userVBytes   userVBytes
     * @param userVSize    userVSize
     */
    public static native void add(long nativeHandle, int keyHashCode, long kBytes, int kSize, long userKBytes,
        int userKSize, long userVBytes, int userVSize);

    /**
     * native remove
     *
     * @param nativeHandle nativeHandle
     * @param keyHashCode  keyHashCode
     * @param kBytes       kBytes
     * @param kSize        kSize
     * @param userKBytes   userKBytes
     * @param userKSize    userKSize
     */
    public static native void remove(long nativeHandle, int keyHashCode, long kBytes, int kSize, long userKBytes,
        int userKSize);

    /**
     * native remove
     *
     * @param nativeHandle nativeHandle
     * @param keyHashCode  keyHashCode
     * @param kBytes       kBytes
     * @param kSize        kSize
     */
    public static native void removeAll(long nativeHandle, int keyHashCode, long kBytes, int kSize);
}