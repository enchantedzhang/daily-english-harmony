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

package com.huawei.ock.bss.common.conf;

public class TableConfig {
    private final long ttl;

    private final boolean isKVSeparateEnable;

    private TableConfig(long tableTtl, boolean isKVSeparateEnable) {
        this.ttl = tableTtl;
        this.isKVSeparateEnable = isKVSeparateEnable;
    }

    public long getTableTtl() {
        return this.ttl;
    }

    public boolean isKVSeparateEnable() {
        return this.isKVSeparateEnable;
    }

    @Override
    public String toString() {
        return "TableConfig {ttl=" + this.ttl + "}";
    }

    public static class Builder {
        private long ttl = -1L;

        private boolean isKVSeparateEnable;

        public Builder setTableTtl(long ttl) {
            this.ttl = ttl;
            return this;
        }

        public Builder setIsEnableKVSeparate(boolean enable) {
            this.isKVSeparateEnable = enable;
            return this;
        }

        public TableConfig build() {
            return new TableConfig(this.ttl, this.isKVSeparateEnable);
        }
    }
}
