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
import com.huawei.ock.bss.common.serialize.TableSerializer;
import com.huawei.ock.bss.table.KMapTableImpl;
import com.huawei.ock.bss.table.api.Table;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

/**
 * KMapTableDescription
 *
 * @param <K>  key
 * @param <MK> subKey
 * @param <MV> value
 * @since BeiMing 25.0.T1
 */
public class KMapTableDescription<K, MK, MV> extends AbstractTableDescription {
    private static final Logger LOG = LoggerFactory.getLogger(KMapTableDescription.class);

    private SubTableSerializer<K, MK, MV> subTableSerializer;

    public KMapTableDescription(String tableName, int maxParallelism,
        SubTableSerializer<K, MK, MV> subTableSerializer) {
        this(tableName, maxParallelism, subTableSerializer, -1L);
    }

    public KMapTableDescription(String tableName, int maxParallelism, SubTableSerializer<K, MK, MV> subTableSerializer,
        long tableTTL) {
        super(tableName, maxParallelism, tableTTL);
        this.subTableSerializer = subTableSerializer;
    }

    @Override
    public SubTableSerializer<K, MK, MV> getTableSerializer() {
        return subTableSerializer;
    }

    @Override
    @SuppressWarnings("rawtypes")
    public void updateTableSerializer(TableSerializer tableSerializer) {
        if (tableSerializer instanceof SubTableSerializer) {
            this.subTableSerializer = (SubTableSerializer<K, MK, MV>) tableSerializer;
        } else {
            throw new UnsupportedOperationException(
                String.format("Cannot support update %s to %s", subTableSerializer.getClass(),
                    SubTableSerializer.class));
        }
    }

    /**
     * createTable
     *
     * @param db db
     * @return com.huawei.ock.bss.table.api.Table<K>
     */
    public Table<K> createTable(BoostStateDB db) {
        LOG.info("Create KMapTable with description {}", this);
        return new KMapTableImpl<>(db, this);
    }

    public BoostStateType getStateType() {
        return BoostStateType.MAP;
    }
}