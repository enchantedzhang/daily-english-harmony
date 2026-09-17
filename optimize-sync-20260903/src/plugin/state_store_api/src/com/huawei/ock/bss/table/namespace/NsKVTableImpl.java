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

package com.huawei.ock.bss.table.namespace;

import com.huawei.ock.bss.common.BoostStateDB;
import com.huawei.ock.bss.common.exception.BSSRuntimeException;
import com.huawei.ock.bss.common.memory.DirectBuffer;
import com.huawei.ock.bss.common.memory.DirectDataOutputSerializer;
import com.huawei.ock.bss.common.serialize.KVSerializerUtil;
import com.huawei.ock.bss.table.KMapTableImpl;
import com.huawei.ock.bss.table.api.NsKVTable;
import com.huawei.ock.bss.table.description.KMapTableDescription;
import com.huawei.ock.bss.table.iterator.SubTableKeyIterator;

import org.apache.flink.api.common.typeutils.TypeSerializer;
import org.apache.flink.util.Preconditions;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import java.io.IOException;
import java.util.Spliterator;
import java.util.Spliterators;
import java.util.stream.Stream;
import java.util.stream.StreamSupport;

/**
 * NsKVTableImpl
 *
 * @param <K> key
 * @param <N> namespace
 * @param <V> value
 * @since BeiMing 25.0.T1
 */
public class NsKVTableImpl<K, N, V> extends KMapTableImpl<K, N, V> implements NsKVTable<K, N, V> {
    private static final Logger LOG = LoggerFactory.getLogger(NsKVTableImpl.class);

    public NsKVTableImpl(BoostStateDB db, KMapTableDescription<K, N, V> description) {
        super(NsKVTableImpl.class.getSimpleName(), db, description);
    }

    protected boolean isMapLikeTable() {
        return false;
    }

    @SuppressWarnings("unchecked")
    @Override
    public <NS, PK> Stream<PK> getKeys(NS namespace) {
        Preconditions.checkNotNull(namespace);
        LOG.debug("get Keys of namespace:{}", namespace);
        return StreamSupport.stream(Spliterators.spliteratorUnknownSize(
            new SubTableKeyIterator<>(nativeHandle, namespace, (TypeSerializer<PK>) this.keySerializer,
                (TypeSerializer<NS>) this.ukSerializer),
            Spliterator.ORDERED), false);
    }

    @Override
    public byte[] getSerializedValue(K key, N namespace) {
        Preconditions.checkNotNull(key);
        Preconditions.checkNotNull(namespace);
        if (!testKeyHash(key.hashCode(), namespace.hashCode())) {
            LOG.debug("the key/namespace {}/{} is filtered by bloom filter.", key, namespace);
            return new byte[0];
        }
        checkNativeHandleValid();
        DirectDataOutputSerializer firstKey = null;
        DirectDataOutputSerializer secondKey = null;
        DirectBuffer valueBytes = null;
        long valueBufferAddressL = 0L;
        try {
            firstKey = KVSerializerUtil.serKey(key, keySerializer);
            secondKey = KVSerializerUtil.serSubKey(namespace, ukSerializer);
            valueBufferAddressL = get(nativeHandle, getKeyHashCode(key), firstKey.data(), firstKey.length(),
                secondKey.data(), secondKey.length());
            valueBytes = DirectBuffer.acquireDirectBuffer(valueBufferAddressL);
            checkContainsConsistency(key, namespace, valueBytes != null, firstKey, secondKey);
            if (valueBytes == null) {
                LOG.debug("key {} and namespace {} is not exist.", key, namespace);
                return null;
            }
            checkGetConsistency(key, namespace, firstKey, secondKey, valueBytes);
            return KVSerializerUtil.getCopyOfBuffer(valueBytes);
        } catch (IOException ioException) {
            LOG.error("Fail to get key {} namespace {} in subKV.", getKeyHashCode(key), namespace, ioException);
            printErrMsg(firstKey, secondKey, valueBytes);
            throw new BSSRuntimeException("Fail to get key " + getKeyHashCode(key) + " namespace " + namespace + ".",
                ioException);
        } finally {
            DirectBuffer.releaseDirectBuffer(valueBufferAddressL);
        }
    }
}
