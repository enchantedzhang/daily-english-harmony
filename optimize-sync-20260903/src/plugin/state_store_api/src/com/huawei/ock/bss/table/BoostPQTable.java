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

package com.huawei.ock.bss.table;

import com.huawei.ock.bss.common.BoostStateDB;
import com.huawei.ock.bss.common.exception.BSSRuntimeException;
import com.huawei.ock.bss.jni.AbstractNativeReference;
import com.huawei.ock.bss.table.description.PQTableDescription;
import com.huawei.ock.bss.table.iterator.PQKeyIterator;
import com.huawei.ock.bss.table.result.EntryResult;

import org.apache.commons.lang3.StringUtils;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import java.util.Arrays;

import javax.annotation.Nonnull;

/**
 * 功能描述
 *
 * @since 2025-10-22
 */
public class BoostPQTable extends AbstractNativeReference {
    private static final Logger LOG = LoggerFactory.getLogger(BoostPQTable.class);

    private final long nativeHandle;

    private final BoostStateDB db;

    private PQTableDescription pqDes;

    public BoostPQTable(BoostStateDB db, String stateName) {
        super(true);
        if (stateName == null || stateName.isEmpty()) {
            throw new BSSRuntimeException("wrong stateName" + stateName);
        }
        this.db = db;
        this.nativeHandle = open(db.getNativeHandle(), stateName);
        if (nativeHandle == 0) {
            throw new BSSRuntimeException("create pq native table failed!");
        }
    }

    public BoostPQTable(BoostStateDB db, PQTableDescription des) {
        super(true);
        if (des == null || StringUtils.isEmpty(des.getStateName())) {
            throw new BSSRuntimeException("wrong stateName" + des.getStateName());
        }
        this.db = db;
        this.pqDes = des;
        this.nativeHandle = open(db.getNativeHandle(), des.getStateName());
        if (nativeHandle == 0) {
            throw new BSSRuntimeException("create pq native table failed!");
        }
    }

    /**
     * add pq data.
     *
     * @param toAddBytes toAddBytes
     * @param hashCode hashCode
     */
    public void add(@Nonnull byte[] toAddBytes, int hashCode) {
        if (!add(nativeHandle, toAddBytes, hashCode)) {
            LOG.info("add key failed, toAddBytes: {}", Arrays.toString(toAddBytes));
            throw new BSSRuntimeException("add key failed!");
        }
    }

    /**
     * remove pq data.
     *
     * @param toRemoveBytes toRemoveBytes
     * @param hashCode hashCode
     */
    public void remove(@Nonnull byte[] toRemoveBytes, int hashCode) {
        if (!remove(nativeHandle, toRemoveBytes, hashCode)) {
            LOG.info("remove key failed, toRemoveBytes: {}", Arrays.toString(toRemoveBytes));
            throw new BSSRuntimeException("remove key failed!");
        }
    }

    /**
     * new Iterator.
     *
     * @param seekHint seekHint
     * @return PQKeyIterator
     */
    public PQKeyIterator<EntryResult> newIterator(byte[] seekHint) {
        return new PQKeyIterator<>(nativeHandle, seekHint);
    }

    public BoostStateDB getDb() {
        return db;
    }

    @Override
    protected void closeInternal() {
        super.close();
    }

    /**
     * open native Iterator.
     *
     * @param dbNativeHandle dbNativeHandle
     * @param stateName stateName
     * @return long
     */
    public native long open(long dbNativeHandle, String stateName);

    private native boolean add(long pqNativeHandle, byte[] key, int hashCode);

    private native boolean remove(long pqNativeHandle, byte[] key, int hashCode);
}
