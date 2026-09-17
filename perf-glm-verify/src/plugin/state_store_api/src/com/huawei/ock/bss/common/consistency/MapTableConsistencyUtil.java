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
import java.util.concurrent.ConcurrentHashMap;

/**
 * MapTableConsistencyUtil
 *
 * @since BeiMing 25.0.T1
 */
public class MapTableConsistencyUtil {
    private static final Logger LOG = LoggerFactory.getLogger(MapTableConsistencyUtil.class);

    /**
     * checkForGet
     *
     * @param debugMap debugMap
     * @param param    param
     * @param <K>      K
     * @param <MK>     MK
     */
    public static <K, MK> void checkForGet(Map<K, Map<MK, byte[]>> debugMap, MapTableUtilParam<K, MK> param) {
        Map<MK, byte[]> innerMap = debugMap.get(param.getKey());
        if (innerMap != null && innerMap.get(param.getMapKey()) != null) {
            byte[] actualValue = KVSerializerUtil.getCopyOfBuffer(param.getMapValue());
            byte[] expectValue = innerMap.get(param.getMapKey());
            if (!Arrays.equals(actualValue, expectValue)) {
                LOG.error("Unexpected: value not equals. expect:{}, actual:{}, key:{}, keyHashCode:{}, subKey:{}",
                    Arrays.toString(KVSerializerUtil.toUnsignedArray(expectValue)),
                    Arrays.toString(KVSerializerUtil.toUnsignedArray(actualValue)),
                    param.getFirstKeyStr(), param.getKeyHashCode(), param.getSecondKeyStr());
            }
        }
    }

    /**
     * checkContains
     *
     * @param debugMap debugMap
     * @param param    param
     * @param <K>      K
     * @param <MK>     MK
     */
    public static <K, MK> void checkContains(Map<K, Map<MK, byte[]>> debugMap, MapTableUtilParam<K, MK> param) {
        boolean expectContainsKey = debugMap.containsKey(param.getKey());
        if (param.getMapKey() == null) {
            if (param.getActualContains() != expectContainsKey) {
                LOG.error("Unexpected contains result, expect:{}, actual:{}, key:{}, keyHashCode:{}", expectContainsKey,
                    param.getActualContains(), param.getFirstKeyStr(), param.getKeyHashCode());
            }
            return;
        }
        Map<MK, byte[]> innerMap = debugMap.get(param.getKey());
        boolean expectContains = innerMap != null && innerMap.get(param.getMapKey()) != null;
        if (param.getActualContains() != expectContains) {
            LOG.error("Unexpected: exist expect:{}, actual:{}, key:{}, keyHashCode:{}, subKey:{}", expectContains,
                param.getActualContains(), param.getFirstKeyStr(), param.getKeyHashCode(),
                param.getSecondKeyStr());
        }
    }

    /**
     * add
     *
     * @param debugMap debugMap
     * @param param    param
     * @param <K>      K
     * @param <MK>     MK
     */
    public static <K, MK> void add(Map<K, Map<MK, byte[]>> debugMap, MapTableUtilParam<K, MK> param) {
        boolean isContainsKey = debugMap.containsKey(param.getKey());
        if (!isContainsKey) {
            LOG.debug("First add primary key:{}, keyHashCode:{}, table:{}", param.getFirstKeyStr(),
                param.getKeyHashCode(), param.getTableName());
        }
        Map<MK, byte[]> newInnerMap = debugMap.computeIfAbsent(param.getKey(), k -> new ConcurrentHashMap<>());
        newInnerMap.put(param.getMapKey(), KVSerializerUtil.getCopyOfBuffer(param.getMapValue()));
    }

    /**
     * remove
     *
     * @param debugMap debugMap
     * @param param    param
     * @param <K>      K
     * @param <MK>     MK
     */
    public static <K, MK> void remove(Map<K, Map<MK, byte[]>> debugMap, MapTableUtilParam<K, MK> param) {
        boolean isContainsKey = debugMap.containsKey(param.getKey());
        if (param.getMapKey() == null) {
            if (isContainsKey) {
                debugMap.remove(param.getKey());
                LOG.debug("Remove key:{}, keyHashCode:{}, table:{}", param.getFirstKeyStr(),
                    param.getKeyHashCode(), param.getTableName());
            }
            return;
        }
        Map<MK, byte[]> innerMap = debugMap.get(param.getKey());
        if (innerMap != null) {
            innerMap.remove(param.getMapKey());
            if (innerMap.isEmpty()) {
                debugMap.remove(param.getKey());
                LOG.debug("Remove key:{}, keyHashCode:{}, subKey:{}, table:{}", param.getFirstKeyStr(),
                    param.getKeyHashCode(), param.getSecondKeyStr(), param.getTableName());
            }
        }
    }
}