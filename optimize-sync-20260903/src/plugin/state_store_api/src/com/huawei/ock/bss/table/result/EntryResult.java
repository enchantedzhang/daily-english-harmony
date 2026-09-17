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

package com.huawei.ock.bss.table.result;

/**
 * 功能描述
 *
 * @since 2025-06-14
 */
public class EntryResult {
    private long keyAddr;

    private int keyLen;

    private long valueAddr;

    private int valueLen;

    private long subKeyAddr;

    private int subKeyLen;

    public long getKeyAddr() {
        return keyAddr;
    }

    public void setKeyAddr(long keyAddr) {
        this.keyAddr = keyAddr;
    }

    public int getKeyLen() {
        return keyLen;
    }

    public void setKeyLen(int keyLen) {
        this.keyLen = keyLen;
    }

    public long getValueAddr() {
        return valueAddr;
    }

    public long getSubKeyAddr() {
        return subKeyAddr;
    }

    public void setSubKeyAddr(long subKeyAddr) {
        this.subKeyAddr = subKeyAddr;
    }

    public int getSubKeyLen() {
        return subKeyLen;
    }

    public void setSubKeyLen(int subKeyLen) {
        this.subKeyLen = subKeyLen;
    }

    public void setValueAddr(long valueAddr) {
        this.valueAddr = valueAddr;
    }

    public int getValueLen() {
        return valueLen;
    }

    public void setValueLen(int valueLen) {
        this.valueLen = valueLen;
    }

    @Override
    public String toString() {
        return "EntryResult{" + "keyAddr=" + keyAddr + ", keyLen=" + keyLen + ", valueAddr=" + valueAddr + ", valueLen="
            + valueLen + ", subKeyAddr=" + subKeyAddr + ", subKeyLen=" + subKeyLen + '}';
    }
}