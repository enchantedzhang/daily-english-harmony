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
import com.huawei.ock.bss.table.api.Table;
import com.huawei.ock.bss.table.KeyPair;
import com.huawei.ock.bss.table.namespace.NsKMapTableImpl;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

/**
 * NsKMapTableDescription
 *
 * @param <K>  key
 * @param <N>  namespace
 * @param <MK> mapKey
 * @param <MV> mapValue
 * @since BeiMing 25.0.T1
 */
public class NsKMapTableDescription<K, N, MK, MV>
    extends KMapTableDescription<KeyPair<K, N>, MK, MV> {
    private static final Logger LOG = LoggerFactory.getLogger(NsKMapTableDescription.class);

    public NsKMapTableDescription(String tableName, int maxParallelism,
        SubTableSerializer<KeyPair<K, N>, MK, MV> subTableSerializer) {
        super(tableName, maxParallelism, subTableSerializer, -1L);
    }

    /**
     * createTable
     *
     * @param db db
     * @return com.huawei.ock.bss.table.api.Table<com.huawei.ock.bss.table.KeyPair < K, N>>
     */
    @Override
    public Table<KeyPair<K, N>> createTable(BoostStateDB db) {
        LOG.info("Create SubKMapTable with description {}", this);
        return new NsKMapTableImpl<>(db, this);
    }

    /**
     * get state type
     *
     * @return SUB_MAP boost state type
     */
    @Override
    public BoostStateType getStateType() {
        return BoostStateType.SUB_MAP;
    }
}