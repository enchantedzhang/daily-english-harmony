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
import com.huawei.ock.bss.common.serialize.TableSerializer;
import com.huawei.ock.bss.table.KListTableImpl;
import com.huawei.ock.bss.table.api.Table;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

/**
 * KListTableDescription
 *
 * @param <K> key
 * @param <E> element
 * @since BeiMing 25.0.T1
 */
public class KListTableDescription<K, E> extends AbstractTableDescription {
    private static final Logger LOG = LoggerFactory.getLogger(KListTableDescription.class);

    private TableSerializer<K, E> tableSerializer;

    public KListTableDescription(String tableName, int maxParallelism, TableSerializer<K, E> tableSerializer) {
        this(tableName, maxParallelism, tableSerializer, -1L);
    }

    public KListTableDescription(String tableName, int maxParallelism, TableSerializer<K, E> tableSerializer,
        long tableTTL) {
        super(tableName, maxParallelism, tableTTL);
        this.tableSerializer = tableSerializer;
    }

    /**
     * createTable
     *
     * @param db db
     * @return com.huawei.ock.bss.table.api.Table<K>
     */
    public Table<K> createTable(BoostStateDB db) {
        LOG.info("Create KListTable with description {}", this);
        return new KListTableImpl<>(db, this);
    }

    public BoostStateType getStateType() {
        return BoostStateType.LIST;
    }

    @Override
    public TableSerializer<K, E> getTableSerializer() {
        return tableSerializer;
    }

    @SuppressWarnings({"rawtypes", "unchecked"})
    @Override
    public void updateTableSerializer(TableSerializer tableSerializer) {
        this.tableSerializer = tableSerializer;
    }

    public boolean isKVSeparateEnable() {
        return false;
    }
}