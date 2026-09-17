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
import com.huawei.ock.bss.common.exception.BSSRuntimeException;
import com.huawei.ock.bss.common.memory.DirectDataOutputSerializer;
import com.huawei.ock.bss.common.serialize.KVSerializerUtil;
import com.huawei.ock.bss.table.api.TableDescription;
import com.huawei.ock.bss.table.description.KMapTableDescription;

import org.apache.flink.util.Preconditions;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import java.io.IOException;
import java.util.Map;

/**
 * KMapTableImpl
 *
 * @param <K>  key
 * @param <UK> userKey
 * @param <UV> userValue
 * @since BeiMing 25.0.T1
 */
public class KMapTableImpl<K, UK, UV> extends AbstractKMapTable<K, UK, UV, Map<UK, UV>> {
    private static final Logger LOGGER = LoggerFactory.getLogger(KMapTableImpl.class);

    public KMapTableImpl(BoostStateDB db, KMapTableDescription<K, UK, UV> description) {
        this(KMapTableImpl.class.getSimpleName(), db, description);
    }

    public KMapTableImpl(String nativeClass, BoostStateDB db, KMapTableDescription<K, UK, UV> description) {
        super(nativeClass, db, description);
    }

    @Override
    public boolean contains(K key) {
        Preconditions.checkNotNull(key);
        checkNativeHandleValid();
        if (!testKeyHash(key.hashCode())) {
            LOGGER.debug("the key {} is filtered by bloom filter.", key);
            return false;
        }

        try {
            DirectDataOutputSerializer keyBytes = KVSerializerUtil.serKey(key, keySerializer);
            boolean actualContains = contains(nativeHandle, getKeyHashCode(key), keyBytes.data(), keyBytes.length());
            checkContainsConsistency(key, null, actualContains, keyBytes, null);
            return actualContains;
        } catch (IOException e) {
            LOGGER.error("Fail to contains key {} in map.", key, e);
            throw new BSSRuntimeException("Fail to contains key " + key + " with list.", e);
        }
    }

    @Override
    public TableDescription getTableDescription() {
        return description;
    }

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
}