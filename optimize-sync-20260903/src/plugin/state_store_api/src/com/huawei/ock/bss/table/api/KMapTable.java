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

import java.util.Iterator;
import java.util.Map;

/**
 * KMapTable
 *
 * @param <K>  key
 * @param <MK> subKey
 * @param <MV> value
 * @param <M>  mapValue
 * @since BeiMing 25.0.T1
 */
public interface KMapTable<K, MK, MV, M extends Map<MK, MV>> extends KVTable<K, M> {
    /**
     * contains
     *
     * @param paramK  paramK
     * @param paramMK paramMK
     * @return boolean
     */
    boolean contains(K paramK, MK paramMK);

    /**
     * get
     *
     * @param paramK  paramK
     * @param paramMK paramMK
     * @return MV
     */
    MV get(K paramK, MK paramMK);

    /**
     * getOrDefault
     *
     * @param paramK  paramK
     * @param paramMK paramMK
     * @param paramMV paramMV
     * @return MV
     */
    MV getOrDefault(K paramK, MK paramMK, MV paramMV);

    /**
     * add
     *
     * @param paramK  paramK
     * @param paramMK paramMK
     * @param paramMV paramMV
     */
    void add(K paramK, MK paramMK, MV paramMV);

    /**
     * addForIterator
     *
     * @param paramK  paramK
     * @param paramMK paramMK
     * @param paramMV paramMV
     */
    void addForIterator(K paramK, MK paramMK, MV paramMV);

    /**
     * addAll
     *
     * @param paramK   paramK
     * @param paramMap paramMap
     */
    void addAll(K paramK, Map<? extends MK, ? extends MV> paramMap);

    /**
     * remove
     *
     * @param paramK  paramK
     * @param paramMK paramMK
     */
    void remove(K paramK, MK paramMK);

    /**
     * removeForIterator
     *
     * @param paramK  paramK
     * @param paramMK paramMK
     */
    void removeForIterator(K paramK, MK paramMK);

    /**
     * entries
     *
     * @param paramK paramK
     * @return java.lang.Iterable<java.util.Map.Entry < MK, MV>>
     */
    Iterable<Map.Entry<MK, MV>> entries(K paramK);

    /**
     * iterator
     *
     * @param paramK paramK
     * @return java.util.Iterator<java.util.Map.Entry < MK, MV>>
     */
    Iterator<Map.Entry<MK, MV>> iterator(K paramK);
}
