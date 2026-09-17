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
import com.huawei.ock.bss.common.conf.TableConfig;
import com.huawei.ock.bss.common.exception.BSSRuntimeException;
import com.huawei.ock.bss.common.memory.DirectBuffer;
import com.huawei.ock.bss.common.memory.DirectDataOutputSerializer;
import com.huawei.ock.bss.common.serialize.KVSerializerUtil;
import com.huawei.ock.bss.jni.AbstractNativeHandleReference;
import com.huawei.ock.bss.table.api.KVTable;
import com.huawei.ock.bss.table.api.Table;
import com.huawei.ock.bss.table.api.TableDescription;
import com.huawei.ock.bss.table.iterator.TableEntryIterator;
import com.huawei.ock.bss.table.iterator.TableKeyIterator;

import org.apache.flink.core.memory.MemorySegment;
import org.apache.flink.core.memory.MemorySegmentFactory;
import org.apache.flink.runtime.operators.util.BloomFilter;
import org.apache.flink.util.CloseableIterator;
import org.apache.flink.util.MathUtils;
import org.apache.flink.util.Preconditions;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import java.util.Arrays;
import java.util.List;
import java.util.Map;
import java.util.Spliterator;
import java.util.Spliterators;
import java.util.concurrent.atomic.AtomicLong;
import java.util.stream.Stream;
import java.util.stream.StreamSupport;

/**
 * AbstractTable
 *
 * @param <K> key
 * @param <V> value
 */
public abstract class AbstractTable<K, V> extends AbstractNativeHandleReference implements KVTable<K, V> {
    private static final Logger LOG = LoggerFactory.getLogger(AbstractTable.class);

    private static final double BLOOM_FILTER_FPP = 0.01;

    private static final int HASH_PRIME = 31;

    /**
     * table归属db
     */
    protected final BoostStateDB db;

    /**
     * table描述信息
     */
    protected final TableDescription description;

    /**
     * native table type handle
     */
    protected final String tableTypeNativeClass;

    private final BloomFilter keyFilter;

    private final BoostConfig config;

    private MemorySegment segment;

    private final AtomicLong existCount;

    private final AtomicLong absentCount;

    public AbstractTable(String nativeClass, BoostStateDB db, TableDescription description) {
        this.db = Preconditions.checkNotNull(db);
        this.description = Preconditions.checkNotNull(description);
        this.nativeHandle = open(db.getNativeHandle(), nativeClass, description);
        this.tableTypeNativeClass = nativeClass;
        Preconditions.checkArgument(nativeHandle != 0);
        this.config = db.getConfig();
        this.keyFilter = createBloomFilter();
        this.existCount = new AtomicLong(0);
        this.absentCount = new AtomicLong(0);
    }

    public synchronized void updateTableConfig(TableConfig newConfig) {
        long newTtl = newConfig.getTableTtl();
        if (newTtl > 0L) {
            LOG.info("{} update table config with old tableDescription {}, new tableConfig: {}", getClass(),
                this.description, newConfig);
            this.description.updateTableTtl(newTtl);
            updateTtl(db.getNativeHandle(), tableTypeNativeClass, description);
        }
        this.description.configKVSeparate(newConfig.isKVSeparateEnable());
    }

    /**
     * addKeyHash
     *
     * @param keyHash keyHash
     */
    public void addKeyHash(int keyHash) {
        if (!config.isEnableBloomFilter()) {
            return;
        }
        keyFilter.addHash(MathUtils.murmurHash(keyHash));
    }

    /**
     * testKeyHash
     *
     * @param keyHash keyHash
     * @return boolean
     */
    public boolean testKeyHash(int keyHash) {
        if (!config.isEnableBloomFilter()) {
            return true;
        }

        boolean isTestExist = keyFilter.testHash(MathUtils.murmurHash(keyHash));
        addFilterCount(isTestExist);
        return isTestExist;
    }

    /**
     * addKeyHash
     *
     * @param keyHash    keyHash
     * @param subKeyHash subKeyHash
     */
    public void addKeyHash(int keyHash, int subKeyHash) {
        if (!config.isEnableBloomFilter()) {
            return;
        }
        keyFilter.addHash(buildCombinedHash(keyHash, subKeyHash));
    }

    /**
     * testKeyHash
     *
     * @param keyHash    keyHash
     * @param subKeyHash subKeyHash
     * @return boolean
     */
    public boolean testKeyHash(int keyHash, int subKeyHash) {
        if (!config.isEnableBloomFilter()) {
            return true;
        }

        boolean isTestExist = keyFilter.testHash(buildCombinedHash(keyHash, subKeyHash));
        addFilterCount(isTestExist);
        return isTestExist;
    }

    private int buildCombinedHash(int keyHash1, int keyHash2) {
        int hash1 = MathUtils.murmurHash(keyHash1);
        int hash2 = MathUtils.murmurHash(keyHash2);
        return hash1 * HASH_PRIME + hash2;
    }

    private void addFilterCount(boolean isExist) {
        if (isExist) {
            this.existCount.addAndGet(1);
        } else {
            this.absentCount.addAndGet(1);
        }
    }

    private BloomFilter createBloomFilter() {
        if (!config.isEnableBloomFilter()) {
            LOG.info("bloom filter is not enable.");
            return null;
        }

        int bitSize = BloomFilter.optimalNumOfBits(config.getExpectedKeyCount(), BLOOM_FILTER_FPP);
        int byteSize = bitSize >>> 3;
        BloomFilter bloomFilter = new BloomFilter(config.getExpectedKeyCount(), byteSize);
        segment = MemorySegmentFactory.allocateUnpooledSegment(byteSize);
        bloomFilter.setBitsLocation(segment, 0);
        LOG.info("create bloom filter for table:{}", bloomFilter);
        return bloomFilter;
    }

    /**
     * getKeyGroupIdx
     *
     * @param key key
     * @return int
     */
    public int getKeyHashCode(K key) {
        if (key instanceof KeyPair) {
            return MathUtils.murmurHash(((KeyPair<?, ?>) key).getFirstKey().hashCode());
        }
        return MathUtils.murmurHash(key.hashCode());
    }

    @SuppressWarnings("unchecked")
    @Override
    public <N, PK> Stream<PK> getKeys(N namespace) {
        checkNativeHandleValid();
        return StreamSupport.stream(Spliterators.spliteratorUnknownSize(
            new TableKeyIterator<PK>(nativeHandle, this.description.getTableSerializer().getKeySerializer()),
            Spliterator.ORDERED), false);
    }

    /**
     * migrateValue 获取table的所有数据重新序列化写入
     *
     * @param oldTable oldTable
     */
    @Override
    public void migrateValue(Table<K> oldTable) {
        long start = System.currentTimeMillis();
        try (CloseableIterator<Map.Entry<K, V>> entryIterator = ((AbstractTable) oldTable).iterator()) {
            if (entryIterator == null) {
                LOG.error("Fail to migrate, iterator is null");
                return;
            }
            while (entryIterator.hasNext()) {
                Map.Entry<K, V> entry = entryIterator.next();
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
     * 获取Entry迭代器
     *
     * @return org.apache.flink.util.CloseableIterator<java.util.Map.Entry < K, V>>
     */
    @SuppressWarnings("unchecked")
    public CloseableIterator<Map.Entry<K, V>> iterator() {
        checkNativeHandleValid();
        return new TableEntryIterator<K, V>(nativeHandle, this.description.getTableSerializer());
    }

    @Override
    protected void closeInternal() {
        super.closeInternal();
        if (config.isEnableBloomFilter()) {
            segment.free();
            LOG.info("close table instance of {}. bloom filter stat:[existCount:{}, absentCount:{}]",
                description.getTableName(), this.existCount.get(), this.absentCount.get());
        }
    }

    protected static void printKVDataErrMsg(DirectBuffer keyBuffer, DirectBuffer sKeyBuffer, DirectBuffer valueBuffer) {
        byte[] keyBytes = KVSerializerUtil.getCopyOfBuffer(keyBuffer);
        byte[] skBytes = KVSerializerUtil.getCopyOfBuffer(sKeyBuffer);
        byte[] valueBytes = KVSerializerUtil.getCopyOfBuffer(valueBuffer);
        LOG.error("keyBytes:{}, mkBytes:{}, valueBytes: {}",
            Arrays.toString(KVSerializerUtil.toUnsignedArray(keyBytes)),
            Arrays.toString(KVSerializerUtil.toUnsignedArray(skBytes)),
            Arrays.toString(KVSerializerUtil.toUnsignedArray(valueBytes)));
    }

    protected static void printKVDataErrMsg(DirectBuffer keyBuffer, DirectBuffer sKeyBuffer,
        List<DirectBuffer> valueBuffer) {
        byte[] keyBytes = KVSerializerUtil.getCopyOfBuffer(keyBuffer);
        byte[] skBytes = KVSerializerUtil.getCopyOfBuffer(sKeyBuffer);
        LOG.error("keyBytes:{}, mkBytes:{} ", Arrays.toString(KVSerializerUtil.toUnsignedArray(keyBytes)),
            Arrays.toString(KVSerializerUtil.toUnsignedArray(skBytes)));
        if (valueBuffer == null) {
            return;
        }
        for (DirectBuffer directBuffer : valueBuffer) {
            byte[] valueBytes = KVSerializerUtil.getCopyOfBuffer(directBuffer);
            LOG.error("\tvalueBytes: {}", Arrays.toString(KVSerializerUtil.toUnsignedArray(valueBytes)));
        }
    }

    /**
     * print err msg.
     *
     * @param firstKey   firstKey
     * @param secondKey  secondKey
     * @param valueBytes valueBytes
     */
    protected static void printErrMsg(DirectDataOutputSerializer firstKey, DirectDataOutputSerializer secondKey,
        DirectBuffer valueBytes) {
        if (!BoostConfig.DEBUG_MODE) {
            return;
        }
        printKVDataErrMsg(firstKey.wrapDirectData(), secondKey.wrapDirectData(), valueBytes);
    }

    /**
     * 创建C++层实例，并返回实例地址
     *
     * @param nativeDBAddress table实例归属的C++层DB实例地址
     * @param nativeClass     要创建的table类型
     * @param des             description
     * @return long
     */
    public static native long open(long nativeDBAddress, String nativeClass, TableDescription des);

    /**
     * 更新ttl
     *
     * @param nativeDBAddress table实例归属的C++层DB实例地址
     * @param nativeClass     要创建的table类型
     * @param des             description
     */
    public static native void updateTtl(long nativeDBAddress, String nativeClass, TableDescription des);
}
