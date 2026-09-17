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

package com.huawei.ock.bss.table.description;

import com.huawei.ock.bss.common.BoostStateType;
import com.huawei.ock.bss.common.BoostStateDB;
import com.huawei.ock.bss.common.serialize.SubTableSerializer;
import com.huawei.ock.bss.table.namespace.NsKVTableImpl;
import com.huawei.ock.bss.table.api.NsKVTable;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

/**
 * NsKVTableDescription
 *
 * @param <K> key
 * @param <N> namespace
 * @param <V> value
 * @since BeiMing 25.0.T1
 */
public class NsKVTableDescription<K, N, V> extends KMapTableDescription<K, N, V> {
    private static final Logger LOG = LoggerFactory.getLogger(NsKVTableDescription.class);

    public NsKVTableDescription(String tableName, int maxParallelism, SubTableSerializer<K, N, V> subTableSerializer) {
        super(tableName, maxParallelism, subTableSerializer, -1L);
    }

    /**
     * createTable
     *
     * @param db db
     * @return com.huawei.ock.bss.table.api.NsKVTable<K, N, V>
     */
    @Override
    public NsKVTable<K, N, V> createTable(BoostStateDB db) {
        LOG.info("Create SubKVTable with description {}", this);
        return new NsKVTableImpl<>(db, this);
    }

    /**
     * get state type
     *
     * @return SUB_VALUE boost state type
     */
    @Override
    public BoostStateType getStateType() {
        return BoostStateType.SUB_VALUE;
    }
}