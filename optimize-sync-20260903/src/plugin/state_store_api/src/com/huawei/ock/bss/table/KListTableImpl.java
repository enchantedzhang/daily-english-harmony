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
import com.huawei.ock.bss.common.consistency.ListTableConsistencyUtil;
import com.huawei.ock.bss.common.consistency.ListTableUtilParam;
import com.huawei.ock.bss.common.exception.BSSRuntimeException;
import com.huawei.ock.bss.common.memory.DirectBuffer;
import com.huawei.ock.bss.common.memory.DirectDataInputDeserializer;
import com.huawei.ock.bss.common.memory.DirectDataOutputSerializer;
import com.huawei.ock.bss.common.serialize.KVSerializerUtil;
import com.huawei.ock.bss.table.api.KListTable;
import com.huawei.ock.bss.table.api.Table;
import com.huawei.ock.bss.table.api.TableDescription;
import com.huawei.ock.bss.table.description.KListTableDescription;
import com.huawei.ock.bss.table.iterator.TableListEntryIterator;
import com.huawei.ock.bss.table.result.StateListResult;

import org.apache.flink.api.common.typeutils.TypeSerializer;
import org.apache.flink.util.CloseableIterator;
import org.apache.flink.util.Preconditions;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import java.io.IOException;
import java.util.ArrayList;
import java.util.Collection;
import java.util.Collections;
import java.util.List;
import java.util.Map;
import java.util.concurrent.ConcurrentHashMap;

/**
 * KListTableImpl
 *
 * @param <K> key
 * @param <E> element
 * @since BeiMing 25.0.T1
 */
public class KListTableImpl<K, E> extends AbstractTable<K, List<E>> implements KListTable<K, E> {
    private static final Logger LOG = LoggerFactory.getLogger(KListTableImpl.class);

    /**
     * key序列化器
     */
    protected final TypeSerializer<K> keySerializer;

    /**
     * element序列化器
     */
    protected final TypeSerializer<E> elementSerializer;

    private final DirectDataInputDeserializer inputView;

    private final Map<K, List<E>> debugMap;

    private final StateListResult stateListResult;

    private final Object lock = new Object();

    public KListTableImpl(BoostStateDB db, KListTableDescription<K, E> description) {
        this(KListTableImpl.class.getSimpleName(), db, description);
    }

    public KListTableImpl(String nativeClass, BoostStateDB db, KListTableDescription<K, E> description) {
        super(nativeClass, db, description);
        this.keySerializer = description.getTableSerializer().getKeySerializer();
        this.elementSerializer = description.getTableSerializer().getValueSerializer();
        this.inputView = new DirectDataInputDeserializer();
        this.stateListResult = new StateListResult();
        this.debugMap = new ConcurrentHashMap<>();
    }

    /**
     * put
     *
     * @param key   key
     * @param value value
     */
    public void put(K key, List<E> value) {
        Preconditions.checkNotNull(key);
        Preconditions.checkNotNull(value);
        checkNativeHandleValid();

        try {
            DirectDataOutputSerializer keyBytes = KVSerializerUtil.serKey(key, keySerializer);
            DirectBuffer valueBytes = KVSerializerUtil.serList(value, elementSerializer);
            put(nativeHandle, getKeyHashCode(key), keyBytes.data(), keyBytes.length(), valueBytes.data(),
                valueBytes.length());
            addKeyHash(key.hashCode());
            checkPutConsistency(key, value, keyBytes);
        } catch (IOException ioException) {
            LOG.error("Fail to put {} with list.", key, ioException);
            throw new BSSRuntimeException("Fail to put " + key + " with list.", ioException);
        }
    }

    /**
     * get
     *
     * @param key key
     * @return java.util.List<E>
     */
    @SuppressWarnings("unchecked")
    public List<E> get(K key) {
        Preconditions.checkNotNull(key);
        if (!testKeyHash(key.hashCode())) {
            LOG.debug("the key {} is filtered by bloom filter.", key);
            return null;
        }

        checkNativeHandleValid();
        DirectDataOutputSerializer keyBytes = null;
        stateListResult.reset();
        try {
            keyBytes = KVSerializerUtil.serKey(key, keySerializer);
            get(nativeHandle, getKeyHashCode(key), keyBytes.data(), keyBytes.length(), stateListResult);
            checkContainsConsistency(key, keyBytes, stateListResult.getSize() != 0);
            if (stateListResult.getSize() == 0 || stateListResult.getAddresses().length == 0
                || stateListResult.getLengths().length == 0) {
                LOG.debug("the key {} of list table is not exist.", key);
                return null;
            }
            List<E> valueList = (List<E>) KVSerializerUtil.desList(stateListResult, elementSerializer, inputView);
            doSectionRead(valueList, stateListResult, key);
            checkGetListConsistency(key, keyBytes, valueList, stateListResult);
            return valueList;
        } catch (IOException e) {
            LOG.error("Fail to get key {} from KListTable.", key, e);
            printErrMsg(keyBytes, stateListResult);
            throw new BSSRuntimeException("Fail to get " + key, e);
        } finally {
            if (stateListResult.getResId() > 0) {
                releaseAllBuffer(nativeHandle, stateListResult.getResId());
            }
        }
    }

    @SuppressWarnings("unchecked")
    private void doSectionRead(List<E> valueList, StateListResult stateListResult, K key) {
        int readSectionId = stateListResult.getReadSectionId();
        if (readSectionId == 0) {
            return;
        }
        StateListResult temp = stateListResult;
        try {
            while (readSectionId > 0) {
                temp = doNativeSectionRead(nativeHandle, temp);
                List<E> tempList = (List<E>) KVSerializerUtil.desList(temp, elementSerializer, inputView);
                if (!tempList.isEmpty()) {
                    valueList.addAll(0, tempList);
                }
                readSectionId = temp.getReadSectionId();
            }
        } catch (IOException e) {
            LOG.error("Fail to get key {} from KListTable.", key, e);
            throw new BSSRuntimeException("Fail to get " + key, e);
        } finally {
            if (stateListResult.getResId() > 0) {
                releaseAllBuffer(nativeHandle, stateListResult.getResId());
            }
        }
    }

    /**
     * getOrDefault
     *
     * @param key          key
     * @param defaultValue defaultValue
     * @return java.util.List<E>
     */
    public List<E> getOrDefault(K key, List<E> defaultValue) {
        List<E> value = get(key);
        return (value == null) ? defaultValue : value;
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
            DirectDataOutputSerializer keyBytes = KVSerializerUtil.serKey(key, keySerializer);
            remove(nativeHandle, getKeyHashCode(key), keyBytes.data(), keyBytes.length());
            checkRemoveConsistency(key, keyBytes);
        } catch (IOException ioException) {
            LOG.error("Fail to remove key: {} of KListTable", key, ioException);
            throw new BSSRuntimeException("Fail to remove " + key, ioException);
        }
    }

    /**
     * contains
     *
     * @param key key
     * @return boolean
     */
    public boolean contains(K key) {
        Preconditions.checkNotNull(key);
        if (!testKeyHash(key.hashCode())) {
            LOG.debug("the key {} is filtered by bloom filter.", key);
            return false;
        }

        checkNativeHandleValid();
        try {
            DirectDataOutputSerializer keyBytes = KVSerializerUtil.serKey(key, keySerializer);
            boolean isContains = contains(nativeHandle, getKeyHashCode(key), keyBytes.data(), keyBytes.length());
            checkContainsConsistency(key, keyBytes, isContains);
            return isContains;
        } catch (IOException ioException) {
            LOG.error("Fail to check contains key: {} of KListTable", key, ioException);
            throw new BSSRuntimeException("Fail to check contains key: " + key, ioException);
        }
    }

    /**
     * add
     *
     * @param key     key
     * @param element element
     */
    public void add(K key, E element) {
        Preconditions.checkNotNull(key);
        checkNativeHandleValid();

        try {
            DirectDataOutputSerializer keyBytes = KVSerializerUtil.serKey(key, keySerializer);
            DirectBuffer valueBytes = KVSerializerUtil.serList(Collections.singletonList(element), elementSerializer);
            add(nativeHandle, getKeyHashCode(key), keyBytes.data(), keyBytes.length(), valueBytes.data(),
                valueBytes.length());
            addKeyHash(key.hashCode());
            checkAddConsistency(key, null, element, keyBytes);
        } catch (IOException ioException) {
            LOG.error("Fail to add <{}, {}>.", key, element, ioException);
            throw new BSSRuntimeException("Fail to add " + key + ", " + element, ioException);
        }
    }

    /**
     * addAll
     *
     * @param key       key
     * @param paramList elements
     */
    public void addAll(K key, Collection<? extends E> paramList) {
        Preconditions.checkNotNull(key);
        Preconditions.checkNotNull(paramList);
        checkNativeHandleValid();

        try {
            DirectDataOutputSerializer keyBytes = KVSerializerUtil.serKey(key, keySerializer);
            List<DirectBuffer> valueBuffers = KVSerializerUtil.serListByBatch(new ArrayList<>(paramList),
                elementSerializer);
            for (DirectBuffer buffer : valueBuffers) {
                addAll(nativeHandle, getKeyHashCode(key), keyBytes.data(), keyBytes.length(), buffer.data(),
                    buffer.length());
            }
            addKeyHash(key.hashCode());
            checkAddConsistency(key, paramList, null, keyBytes);
        } catch (IOException ioException) {
            LOG.error("Fail to add <{}, {}>.", key, paramList, ioException);
            throw new BSSRuntimeException("Fail to add " + key + ", " + paramList, ioException);
        }
    }

    @Override
    public byte[] getSerializedValue(K key) {
        // cannot get byte[] from cpp directly now, should not be used
        LOG.error("Should not use this method to getSerializedValue for listState.");
        throw new BSSRuntimeException("Should not use this method to getSerializedValue for listState.");
    }

    @Override
    public TableDescription getTableDescription() {
        return description;
    }

    @Override
    public CloseableIterator<Map.Entry<K, List<E>>> iterator() {
        checkNativeHandleValid();
        return new TableListEntryIterator<K, E>(nativeHandle, this.description.getTableSerializer());
    }

    /**
     * migrateValue 获取table的所有数据重新序列化写入
     *
     * @param oldTable oldTable
     */
    @Override
    public void migrateValue(Table<K> oldTable) {
        long start = System.currentTimeMillis();
        LOG.info("Migrate start");
        if (!(oldTable instanceof KListTableImpl)) {
            LOG.error("UnExcept table type:{}", oldTable.getClass().getCanonicalName());
            return;
        }
        try (CloseableIterator<Map.Entry<K, List<E>>> entryIterator = ((KListTableImpl) oldTable).iterator()) {
            if (entryIterator == null) {
                LOG.error("Fail to migrate, iterator is null");
                return;
            }
            while (entryIterator.hasNext()) {
                Map.Entry<K, List<E>> entry = entryIterator.next();
                if (entry == null) {
                    LOG.error("Iterator null entry.");
                    continue;
                }
                this.put(entry.getKey(), entry.getValue());
            }
        } catch (Exception e) {
            LOG.error("Fail to migrate :", e);
            throw new BSSRuntimeException("Fail to migrate kvTable:", e);
        }
        long end = System.currentTimeMillis();
        LOG.info("Migrate end, cost {} ms", end - start);
    }

    /**
     * print err msg
     *
     * @param keyBytes keyBytes
     * @param result   result
     */
    private static void printErrMsg(DirectDataOutputSerializer keyBytes, StateListResult result) {
        if (!BoostConfig.DEBUG_MODE) {
            return;
        }
        List<DirectBuffer> directBuffers = new ArrayList<>(result.getSize());
        for (int i = 0; i < result.getSize(); i++) {
            directBuffers.add(new DirectBuffer(result.getAddresses()[i], result.getLengths()[i]));
        }
        printKVDataErrMsg(keyBytes.wrapDirectData(), null, directBuffers);
    }

    /**
     * check put consistency.
     *
     * @param key      key
     * @param value    value
     * @param keyBytes keyBytes
     */
    private void checkPutConsistency(K key, List<E> value, DirectDataOutputSerializer keyBytes) {
        if (!BoostConfig.DEBUG_MODE) {
            return;
        }
        synchronized (lock) {
            ListTableConsistencyUtil.put(debugMap, new ListTableUtilParam.Builder<K, E>().setKey(key)
                .setKeyHashCode(getKeyHashCode(key)).setKeyBytes(keyBytes.wrapDirectData())
                .setValueList(value).build());
        }
    }

    /**
     * check contains Consistency.
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
            ListTableConsistencyUtil.checkContains(debugMap, new ListTableUtilParam.Builder<K, E>().setKey(key)
                .setKeyHashCode(getKeyHashCode(key)).setKeyBytes(keyBytes.wrapDirectData())
                .setActualContains(actualContains).build());
        }
    }

    /**
     * check Get listConsistency.
     *
     * @param key       key
     * @param keyBytes  keyBytes
     * @param valueList valueList
     * @param result    result
     * @throws IOException IOException
     */
    private void checkGetListConsistency(K key, DirectDataOutputSerializer keyBytes, List<E> valueList,
        StateListResult result) throws IOException {
        if (!BoostConfig.DEBUG_MODE) {
            return;
        }
        synchronized (lock) {
            List<DirectBuffer> directBuffers = new ArrayList<>(result.getSize());
            for (int i = 0; i < result.getSize(); i++) {
                directBuffers.add(new DirectBuffer(result.getAddresses()[i], result.getLengths()[i]));
            }
            ListTableConsistencyUtil.checkForGet(debugMap,
                new ListTableUtilParam.Builder<K, E>().setKey(key).setKeyHashCode(getKeyHashCode(key))
                    .setKeyBytes(keyBytes.wrapDirectData()).setValueList(valueList)
                    .setDirectBuffers(directBuffers).setElementSerializer(elementSerializer).build());
        }
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
            ListTableConsistencyUtil.remove(debugMap, new ListTableUtilParam.Builder<K, E>()
                .setKey(key).setKeyHashCode(getKeyHashCode(key))
                .setKeyBytes(keyBytes.wrapDirectData()).build());
        }
    }

    /**
     * check add consistency.
     *
     * @param key      key
     * @param elements elements
     * @param element  element
     * @param keyBytes keyBytes
     */
    private void checkAddConsistency(K key, Collection<? extends E> elements, E element,
        DirectDataOutputSerializer keyBytes) {
        if (!BoostConfig.DEBUG_MODE) {
            return;
        }
        ListTableConsistencyUtil.add(debugMap, new ListTableUtilParam.Builder<K, E>().setKey(key)
            .setKeyHashCode(getKeyHashCode(key)).setKeyBytes(keyBytes.wrapDirectData())
            .setElement(element).setElements(elements).build());
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
    public static native void put(long nativeHandle, int keyHashCode, long kBytes, int kSize, long vBytes, int vSize);

    /**
     * native get
     *
     * @param nativeHandle    nativeHandle
     * @param keyHashCode     keyHashCode
     * @param kBytes          kBytes
     * @param kSize           kSize
     * @param stateListResult stateListCount
     */
    public static native void get(long nativeHandle, int keyHashCode, long kBytes, int kSize,
        StateListResult stateListResult);

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

    /**
     * native add
     *
     * @param nativeHandle nativeHandle
     * @param keyHashCode  keyHashCode
     * @param kBytes       kBytes
     * @param kSize        kSize
     * @param vBytes       vBytes
     * @param vSize        vSize
     */
    public static native void add(long nativeHandle, int keyHashCode, long kBytes, int kSize, long vBytes, int vSize);

    /**
     * native addAll
     *
     * @param nativeHandle nativeHandle
     * @param keyHashCode  keyHashCode
     * @param kBytes       kBytes
     * @param kSize        kSize
     * @param vBytes       vBytes
     * @param vSize        vSize
     */
    public static native void addAll(long nativeHandle, int keyHashCode, long kBytes, int kSize, long vBytes,
        int vSize);

    /**
     * release all jni buffer.
     *
     * @param nativeHandle nativeHandle
     * @param resId        resId
     */
    private static native void releaseAllBuffer(long nativeHandle, int resId);

    private static native StateListResult doNativeSectionRead(long nativeHandle, StateListResult stateListResult);
}