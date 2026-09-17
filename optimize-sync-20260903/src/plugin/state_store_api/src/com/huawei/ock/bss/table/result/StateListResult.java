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
 * @since 2025-05-27
 */
public class StateListResult {
    private int resId = 0;
    private int readSectionId = 0;
    private int size = 0;
    private long[] addresses;
    private int[] lengths;

    public StateListResult() {
    }

    public StateListResult(int resId, int size) {
        this.resId = resId;
        this.size = size;
    }

    public int getResId() {
        return resId;
    }

    public void setResId(int resId) {
        this.resId = resId;
    }

    public int getSize() {
        return size;
    }

    public void setSize(int size) {
        this.size = size;
    }

    public long[] getAddresses() {
        return addresses;
    }

    public void setAddresses(long[] addresses) {
        this.addresses = addresses;
    }

    public int[] getLengths() {
        return lengths;
    }

    public void setLengths(int[] lengths) {
        this.lengths = lengths;
    }

    public int getReadSectionId() {
        return readSectionId;
    }

    public void reset() {
        this.resId = 0;
        this.size = 0;
        this.addresses = null;
        this.lengths = null;
        this.readSectionId = 0;
    }
}