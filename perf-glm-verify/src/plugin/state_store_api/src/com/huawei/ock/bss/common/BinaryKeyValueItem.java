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

package com.huawei.ock.bss.common;

import com.huawei.ock.bss.common.serialize.KVSerializerUtil;

import javax.annotation.Nullable;

/**
 * savepoint下层返回的迭代器的数据类型
 *
 * @since BeiMing 25.0.T1
 */
public class BinaryKeyValueItem {
    private BoostStateType stateType;

    private String stateName;

    private int keyGroup;

    private long key;

    private int keyLen;

    @Nullable
    private long namespace;

    private int namespaceLen;

    @Nullable
    private long mapKey;

    private int mapKeyLen;

    private long value;

    private int valueLen;

    public BinaryKeyValueItem() {
    }

    public BinaryKeyValueItem(BoostStateType stateType, String stateName, int keyGroup, long key, int keyLen,
        long namespace, int namespaceLen, long mapKey, int mapKeyLen, long value, int valueLen) {
        this.stateType = stateType;
        this.stateName = stateName;
        this.keyGroup = keyGroup;
        this.key = key;
        this.keyLen = keyLen;
        this.namespace = namespace;
        this.namespaceLen = namespaceLen;
        this.mapKey = mapKey;
        this.mapKeyLen = mapKeyLen;
        this.value = value;
        this.valueLen = valueLen;
    }

    public BoostStateType getStateType() {
        return this.stateType;
    }

    public void setStateType(BoostStateType stateType) {
        this.stateType = stateType;
    }

    public String getStateName() {
        return this.stateName;
    }

    public void setStateName(String stateName) {
        this.stateName = stateName;
    }

    public int getKeyGroup() {
        return this.keyGroup;
    }

    public void setKeyGroup(int keyGroup) {
        this.keyGroup = keyGroup;
    }

    public byte[] getKey() {
        return KVSerializerUtil.getCopyOfBuffer(key, keyLen);
    }

    public void setKey(long key) {
        this.key = key;
    }

    public int getKeyLen() {
        return keyLen;
    }

    public void setKeyLen(int keyLen) {
        this.keyLen = keyLen;
    }

    public byte[] getNamespace() {
        return KVSerializerUtil.getCopyOfBuffer(namespace, namespaceLen);
    }

    public void setNamespace(long namespace) {
        this.namespace = namespace;
    }

    public int getNamespaceLen() {
        return namespaceLen;
    }

    public void setNamespaceLen(int namespaceLen) {
        this.namespaceLen = namespaceLen;
    }

    public byte[] getMapKey() {
        return KVSerializerUtil.getCopyOfBuffer(mapKey, mapKeyLen);
    }

    public void setMapKey(long mapKey) {
        this.mapKey = mapKey;
    }

    public int getMapKeyLen() {
        return mapKeyLen;
    }

    public void setMapKeyLen(int mapKeyLen) {
        this.mapKeyLen = mapKeyLen;
    }

    public byte[] getValue() {
        return KVSerializerUtil.getCopyOfBuffer(value, valueLen);
    }

    public void setValue(long value) {
        this.value = value;
    }

    public int getValueLen() {
        return valueLen;
    }

    public void setValueLen(int valueLen) {
        this.valueLen = valueLen;
    }
}