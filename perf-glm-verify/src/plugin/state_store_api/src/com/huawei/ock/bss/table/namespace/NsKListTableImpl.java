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
import com.huawei.ock.bss.common.serialize.KeyPairSerializer;
import com.huawei.ock.bss.table.KListTableImpl;
import com.huawei.ock.bss.table.KeyPair;
import com.huawei.ock.bss.table.api.NsKListTable;
import com.huawei.ock.bss.table.description.NsKListTableDescription;
import com.huawei.ock.bss.table.iterator.SubTableKeyIterator;

import org.apache.flink.api.common.typeutils.TypeSerializer;
import org.apache.flink.util.Preconditions;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import java.util.Spliterator;
import java.util.Spliterators;
import java.util.stream.Stream;
import java.util.stream.StreamSupport;

/**
 * NsKListTableImpl
 *
 * @param <K> key
 * @param <N> namespace
 * @param <E> element
 * @since BeiMing 25.0.T1
 */
public class NsKListTableImpl<K, N, E> extends KListTableImpl<KeyPair<K, N>, E> implements NsKListTable<K, N, E> {
    private static final Logger LOG = LoggerFactory.getLogger(NsKListTableImpl.class);

    public NsKListTableImpl(BoostStateDB db, NsKListTableDescription<K, N, E> description) {
        super(NsKListTableImpl.class.getSimpleName(), db, description);
    }

    /**
     * getKeyPair
     *
     * @param key1 key1
     * @param key2 key2
     * @return com.huawei.ock.bss.table.KeyPair<K, N>
     */
    public KeyPair<K, N> getKeyPair(K key1, N key2) {
        return new KeyPair<>(key1, key2);
    }

    @SuppressWarnings("unchecked")
    @Override
    public <NS, PK> Stream<PK> getKeys(NS namespace) {
        Preconditions.checkNotNull(namespace);
        LOG.debug("get SubKListTable Keys of namespace:{}", namespace);
        KeyPairSerializer<K, N> keyPairSerializer = (KeyPairSerializer<K, N>) this.keySerializer;
        return StreamSupport.stream(Spliterators.spliteratorUnknownSize(
            new SubTableKeyIterator<>(nativeHandle, namespace,
                (TypeSerializer<PK>) keyPairSerializer.getK1Serializer(),
                (TypeSerializer<NS>) keyPairSerializer.getK2Serializer()),
            Spliterator.ORDERED), false);
    }
}
