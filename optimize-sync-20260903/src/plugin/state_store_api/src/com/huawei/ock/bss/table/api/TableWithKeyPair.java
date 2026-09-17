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

import com.huawei.ock.bss.table.KeyPair;

/**
 * TableWithKeyPair
 *
 * @param <K> key
 * @param <N> namespace
 * @since BeiMing 25.0.T1
 */
public interface TableWithKeyPair<K, N> {
    /**
     * getKeyPair
     *
     * @param paramK paramK
     * @param paramN paramN
     * @return com.huawei.ock.bss.table.KeyPair<K, N>
     */
    KeyPair<K, N> getKeyPair(K paramK, N paramN);
}
