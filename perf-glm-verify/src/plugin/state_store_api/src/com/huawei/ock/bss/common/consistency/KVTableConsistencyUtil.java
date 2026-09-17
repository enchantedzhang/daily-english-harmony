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

package com.huawei.ock.bss.common.consistency;

import com.huawei.ock.bss.common.serialize.KVSerializerUtil;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import java.util.Arrays;
import java.util.Map;

/**
 * KVTableConsistencyUtil
 *
 * @since BeiMing 25.0.T1
 */
public class KVTableConsistencyUtil {
    private static final Logger LOG = LoggerFactory.getLogger(KVTableConsistencyUtil.class);

    /**
     * checkForGet
     *
     * @param debugMap debugMap
     * @param param    param
     */
    public static <K, V> void checkForGet(Map<K, KVTableUtilParam<K, V>> debugMap, KVTableUtilParam<K, V> param) {
        byte[] expectValue = debugMap.get(param.getKey()).getValueBytes();
        byte[] actualValue = param.getValueBytes();
        if (!Arrays.equals(expectValue, actualValue)) {
            LOG.error("Unexpected: key:{} value not match expect, keyHashCode:{}, expectValue:{}, actualValue:{}",
                param.getKeyStr(), param.getKeyHashCode(),
                Arrays.toString(KVSerializerUtil.toUnsignedArray(expectValue)),
                Arrays.toString(KVSerializerUtil.toUnsignedArray(actualValue)));
        }
    }

    /**
     * checkContains
     *
     * @param debugMap debugMap
     * @param param    param
     */
    public static <K, V> void checkContains(Map<K, KVTableUtilParam<K, V>> debugMap, KVTableUtilParam<K, V> param) {
        boolean expectContainsKey = debugMap.containsKey(param.getKey());
        if (param.getActualContains() != expectContainsKey) {
            LOG.error("Unexpected: exist expect:{}, actual:{}, key:{}, keyHashCode:{}", expectContainsKey,
                param.getActualContains(), param.getKeyStr(), param.getKeyHashCode());
        }
    }

    /**
     * remove
     *
     * @param debugMap debugMap
     * @param param    param
     */
    public static <K, V> void remove(Map<K, KVTableUtilParam<K, V>> debugMap, KVTableUtilParam<K, V> param) {
        LOG.debug("DebugMap remove key:{}, keyHashCode:{}", param.getKeyStr(), param.getKeyHashCode());
        debugMap.remove(param.getKey());
    }

    /**
     * put
     *
     * @param debugMap debugMap
     * @param param    param
     */
    public static <K, V> void put(Map<K, KVTableUtilParam<K, V>> debugMap, KVTableUtilParam<K, V> param) {
        LOG.debug("DebugMap put key:{}, keyHashCode:{}", param.getKeyStr(), param.getKeyHashCode());
        debugMap.put(param.getKey(), param);
    }
}