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
import com.huawei.ock.bss.table.api.Table;
import com.huawei.ock.bss.table.KeyPair;
import com.huawei.ock.bss.table.namespace.NsKListTableImpl;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

/**
 * NsKListTableDescription
 *
 * @param <K> key
 * @param <N> namespace
 * @param <E> element
 * @since BeiMing 25.0.T1
 */
public class NsKListTableDescription<K, N, E> extends KListTableDescription<KeyPair<K, N>, E> {
    private static final Logger LOG = LoggerFactory.getLogger(NsKListTableDescription.class);

    public NsKListTableDescription(String tableName, int maxParallelism,
        TableSerializer<KeyPair<K, N>, E> tableSerializer) {
        super(tableName, maxParallelism, tableSerializer, -1L);
    }

    /**
     * createTable
     *
     * @param db db
     * @return com.huawei.ock.bss.table.api.Table<com.huawei.ock.bss.table.KeyPair < K, N>>
     */
    @Override
    public Table<KeyPair<K, N>> createTable(BoostStateDB db) {
        LOG.info("Create SubKListTable with description {}", this);
        return new NsKListTableImpl<>(db, this);
    }

    /**
     * get state type
     *
     * @return SUB_LIST boost state type
     */
    @Override
    public BoostStateType getStateType() {
        return BoostStateType.SUB_LIST;
    }
}