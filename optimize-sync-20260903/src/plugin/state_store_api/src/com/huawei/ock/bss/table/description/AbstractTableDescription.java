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

package com.huawei.ock.bss.table.description;

import com.huawei.ock.bss.common.serialize.TableSerializer;
import com.huawei.ock.bss.table.api.TableDescription;

import org.apache.flink.util.Preconditions;

/**
 * AbstractTableDescription
 *
 * @since BeiMing 25.0.T1
 */
public abstract class AbstractTableDescription implements TableDescription {
    protected final String tableName;

    protected final int maxParallelism;

    protected volatile long tableTTL;

    protected volatile boolean kvSeparateEnable;

    public AbstractTableDescription(String tableName, int maxParallelism, long tableTTL) {
        this.tableName = (String) Preconditions.checkNotNull(tableName);
        this.tableTTL = tableTTL;
        this.maxParallelism = maxParallelism;
        this.kvSeparateEnable = false;
    }

    public String getTableName() {
        return this.tableName;
    }

    public int getMaxParallelism() {
        return maxParallelism;
    }

    @SuppressWarnings("rawtypes")
    public abstract TableSerializer getTableSerializer();

    @SuppressWarnings("rawtypes")
    public abstract void updateTableSerializer(TableSerializer tableSerializer);

    public long getTableTTL() {
        return this.tableTTL;
    }

    public boolean hasTableTTL() {
        return (this.tableTTL > 0L);
    }

    public void updateTableTtl(long newTtl) {
        this.tableTTL = newTtl;
    }

    public boolean isKVSeparateEnable() {
        return this.kvSeparateEnable;
    }

    public void configKVSeparate(boolean enable) {
        this.kvSeparateEnable = enable;
    }

    public String toString() {
        return "{tableName=" + this.tableName + ", tableTTL=" + this.tableTTL + "}";
    }
}
