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

import com.huawei.ock.bss.common.BoostStateDB;
import com.huawei.ock.bss.common.BoostStateType;
import com.huawei.ock.bss.common.serialize.TableSerializer;
import com.huawei.ock.bss.table.KVTableImpl;
import com.huawei.ock.bss.table.api.KVTable;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

/**
 * KVTableDescription
 *
 * @param <K> key
 * @param <S> state
 */
// 检查泛型S是否真的是state
public class KVTableDescription<K, S> extends AbstractTableDescription {
    private static final Logger LOG = LoggerFactory.getLogger(KVTableDescription.class);

    private TableSerializer<K, S> tableSerializer;

    public KVTableDescription(String tableName, int maxParallelism, TableSerializer<K, S> tableSerializer) {
        this(tableName, maxParallelism, tableSerializer, -1L);
    }

    public KVTableDescription(String tableName, int maxParallelism, TableSerializer<K, S> tableSerializer,
        long tableTTL) {
        super(tableName, maxParallelism, tableTTL);
        this.tableSerializer = tableSerializer;
    }

    public BoostStateType getStateType() {
        return BoostStateType.VALUE;
    }

    public TableSerializer<K, S> getTableSerializer() {
        return this.tableSerializer;
    }

    @SuppressWarnings({"rawtypes", "unchecked"})
    public void updateTableSerializer(TableSerializer tableSerializer) {
        this.tableSerializer = tableSerializer;
    }

    /**
     * createTable
     *
     * @param db db
     * @return com.huawei.ock.bss.table.api.KVTable<K, S>
     */
    public KVTable<K, S> createTable(BoostStateDB db) {
        LOG.info("Create KVTable with description {}", this);
        return new KVTableImpl<>(db, this);
    }
}