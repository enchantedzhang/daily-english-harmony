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

import com.huawei.ock.bss.common.conf.BoostConfig;
import com.huawei.ock.bss.common.memory.DirectBuffer;
import com.huawei.ock.bss.common.serialize.KVSerializerUtil;

import org.apache.flink.api.common.typeutils.TypeSerializer;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import java.io.IOException;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;

/**
 * ListTableConsistencyUtil
 *
 * @since BeiMing 25.0.T1
 */
public class ListTableConsistencyUtil {
    private static final Logger LOG = LoggerFactory.getLogger(ListTableConsistencyUtil.class);

    /**
     * checkForGet
     *
     * @param debugMap debugMap
     * @param param    param
     * @throws IOException IOException
     */
    public static <K, E> void checkForGet(Map<K, List<E>> debugMap, ListTableUtilParam<K, E> param)
        throws IOException {
        if (!BoostConfig.DEBUG_MODE) {
            return;
        }
        List<E> expectValues = debugMap.get(param.getKey());
        LOG.debug("DebugMap check key:{}, keyHashCode:{}, list value size is:{}", param.getKeyStr(),
            param.getKeyHashCode(), calculateValueSize(param.getDirectBuffers()));
        checkValueEquals(param.getKeyHashCode(), param.getKeyStr(), expectValues, param.getValueList(),
            param.getElementSerializer());
    }

    /**
     * checkContains
     *
     * @param debugMap debugMap
     * @param param    param
     */
    public static <K, E> void checkContains(Map<K, List<E>> debugMap, ListTableUtilParam<K, E> param) {
        if (!BoostConfig.DEBUG_MODE) {
            return;
        }
        boolean expectContainsKey = debugMap.containsKey(param.getKey());
        if (param.getActualContains() != expectContainsKey) {
            LOG.error("Unexpected: exist expect:{}, actual:{}, key:{}, keyHashCode:{}", expectContainsKey,
                param.getActualContains(), param.getKeyStr(), param.getKeyHashCode());
        }
    }

    /**
     * add
     *
     * @param debugMap debugMap
     * @param param    param
     */
    public static <K, E> void add(Map<K, List<E>> debugMap, ListTableUtilParam<K, E> param) {
        if (param.getElement() != null) {
            LOG.debug("DebugMap add key:{}, keyHashCode:{}", param.getKeyStr(), param.getKeyHashCode());
            List<E> valueSet = debugMap.computeIfAbsent(param.getKey(), k -> new ArrayList<>());
            valueSet.add(param.getElement());
        }
        if (param.getElements() != null) {
            LOG.debug("DebugMap add key:{}, keyHashCode:{}", param.getKeyStr(), param.getKeyHashCode());
            List<E> valueSet = debugMap.computeIfAbsent(param.getKey(), k -> new ArrayList<>());
            valueSet.addAll(param.getElements());
        }
    }

    /**
     * remove
     *
     * @param debugMap debugMap
     * @param param    param
     */
    public static <K, E> void remove(Map<K, List<E>> debugMap, ListTableUtilParam<K, E> param) {
        LOG.debug("DebugMap remove key:{}, keyHashCode:{}", param.getKeyStr(), param.getKeyHashCode());
        debugMap.remove(param.getKey());
    }

    /**
     * put
     *
     * @param debugMap debugMap
     * @param param    param
     */
    public static <K, E> void put(Map<K, List<E>> debugMap, ListTableUtilParam<K, E> param) {
        LOG.debug("DebugMap put key:{}, keyHashCode:{}, list size:{}", param.getKeyStr(), param.getKeyHashCode(),
            param.getValueList().size());
        List<E> valueSet = new ArrayList<>(param.getValueList());
        debugMap.put(param.getKey(), valueSet);
    }

    private static <E> void checkValueEquals(int keyHashCode, String keyStr, List<E> expectValue, List<E> actualValue,
        TypeSerializer<E> elementSerializer) throws IOException {
        if (expectValue == null) {
            LOG.error("Unexpected: key should not exist. key:{}, keyHashCode:{}", keyStr, keyHashCode);
            return;
        }

        if (expectValue.isEmpty()) {
            LOG.error("Unexpected: value should not exist. key:{}, keyHashCode:{}", keyStr, keyHashCode);
            return;
        }

        if (actualValue.isEmpty()) {
            LOG.error("Unexpected: value should exist. key:{}, keyHashCode:{}", keyStr, keyHashCode);
            return;
        }

        if (expectValue.size() != actualValue.size()) {
            LOG.error("Unexpected: value size not exist. key:{}, keyHashCode:{}, expect:{}, actual:{}", keyStr,
                keyHashCode, expectValue.size(), actualValue.size());
            return;
        }

        Set<E> expectSet = new HashSet<>(expectValue);
        for (E e : actualValue) {
            if (!expectSet.contains(e)) {
                LOG.error("Unexpected: value not equals. key:{}, keyHashCode:{}, bad value:{}", keyStr, keyHashCode,
                    Arrays.toString(KVSerializerUtil.toUnsignedArray(KVSerializerUtil.getCopyOfBuffer(
                        KVSerializerUtil.serValue(e, elementSerializer).wrapDirectData()))));
                return;
            }
        }
        LOG.debug("check value for key:{}, keyHashCode:{} success.", keyStr, keyHashCode);
    }

    private static long calculateValueSize(List<DirectBuffer> valueBytes) {
        if (valueBytes == null) {
            return 0;
        }

        long totalSize = 0L;
        for (DirectBuffer buffer : valueBytes) {
            totalSize += buffer.length();
        }
        return totalSize;
    }
}