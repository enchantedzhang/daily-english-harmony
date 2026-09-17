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
import com.huawei.ock.bss.table.BoostPQTable;

import org.apache.flink.api.common.typeutils.TypeSerializer;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

/**
 * PQTableDescription
 *
 * @param <K> key
 * @since BeiMing 25.0.T1
 */
public class PQTableDescription<K> {
    private static final Logger LOG = LoggerFactory.getLogger(PQTableDescription.class);

    private String stateName;

    private TypeSerializer<K> keySerializer;

    public PQTableDescription(String stateName, TypeSerializer<K> typeSerializer) {
        this.stateName = stateName;
        this.keySerializer = typeSerializer;
    }

    public BoostStateType getStateType() {
        return BoostStateType.PQ;
    }

    public String getStateName() {
        return stateName;
    }

    public TypeSerializer<K> getTableSerializer() {
        return keySerializer;
    }

    public void updateKeySerializer(TypeSerializer<K> keySerializer) {
        this.keySerializer = keySerializer;
    }

    /**
     * createTable
     *
     * @param db db
     * @return com.huawei.ock.bss.table.api.KVTable<K, S>
     */
    public BoostPQTable createTable(BoostStateDB db) {
        LOG.info("Create BoostPQTable with description {}", this);
        return db.createPQTable(this);
    }
}