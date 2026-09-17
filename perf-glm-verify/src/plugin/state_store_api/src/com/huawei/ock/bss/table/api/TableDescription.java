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

package com.huawei.ock.bss.table.api;

import com.huawei.ock.bss.common.BoostStateDB;
import com.huawei.ock.bss.common.BoostStateType;
import com.huawei.ock.bss.common.serialize.TableSerializer;

/**
 * TableDescription interface
 *
 * @since BeiMing 25.0.T1
 */
public interface TableDescription {
    /**
     * getTableName
     *
     * @return java.lang.String
     */
    String getTableName();

    /**
     * getStateType
     *
     * @return com.huawei.ock.bss.common.BoostStateType
     */
    BoostStateType getStateType();

    /**
     * getMaxParallelism
     *
     * @return int
     */
    int getMaxParallelism();

    /**
     * getTableTTL
     *
     * @return long
     */
    default long getTableTTL() {
        return -1L;
    }

    /**
     * hasTableTTL
     *
     * @return boolean
     */
    default boolean hasTableTTL() {
        return false;
    }

    /**
     * updateTableTtl
     *
     * @param newTtl newTtl
     */
    default void updateTableTtl(long newTtl) {
        throw new UnsupportedOperationException();
    }

    /**
     * isKVSeparateEnable
     *
     * @return boolean
     */
    boolean isKVSeparateEnable();

    /**
     * configKVSeparate
     *
     * @param paramBoolean paramBoolean
     */
    void configKVSeparate(boolean paramBoolean);

    /**
     * getTableSerializer
     *
     * @return com.huawei.ock.bss.common.serialize.TableSerializer
     */
    @SuppressWarnings("rawtypes")
    TableSerializer getTableSerializer();

    /**
     * updateTableSerializer
     *
     * @param tableSerializer tableSerializer
     */
    @SuppressWarnings("rawtypes")
    void updateTableSerializer(TableSerializer tableSerializer);

    /**
     * createTable
     *
     * @param boostStateDB boostStateDB
     * @return com.huawei.ock.bss.table.api.Table
     */
    @SuppressWarnings("rawtypes")
    Table createTable(BoostStateDB boostStateDB);
}