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

/**
 * KVTable
 *
 * @param <K> key
 * @param <V> value
 * @since BeiMing 25.0.T1
 */
public interface KVTable<K, V> extends Table<K> {
    /**
     * put
     *
     * @param paramK paramK
     * @param paramV paramV
     */
    void put(K paramK, V paramV);

    /**
     * get
     *
     * @param paramK paramK
     * @return V
     */
    V get(K paramK);

    /**
     * remove
     *
     * @param paramK paramK
     */
    void remove(K paramK);

    /**
     * contains
     *
     * @param paramK paramK
     * @return boolean
     */
    boolean contains(K paramK);

    /**
     * getSerializedValue
     *
     * @param paramK paramK
     * @return byte[]
     */
    byte[] getSerializedValue(K paramK);
}
