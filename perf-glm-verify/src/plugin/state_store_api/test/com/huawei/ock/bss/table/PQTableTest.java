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

import static com.huawei.ock.bss.common.BoostStateType.PQ;
import static org.junit.Assert.fail;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.anyObject;

import com.huawei.ock.bss.common.BoostStateDB;
import com.huawei.ock.bss.table.description.PQTableDescription;

import org.apache.flink.api.common.typeutils.base.LongSerializer;
import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.powermock.api.mockito.PowerMockito;
import org.powermock.modules.junit4.PowerMockRunner;

/**
 * PQTableTest
 *
 * @since BeiMing 25.3.0
 */
@RunWith(PowerMockRunner.class)
public class PQTableTest {
    private final byte[] addKey = {1, 2, 3, 4};

    private final byte[] removeKey = {4, 1, 3, 4};

    private BoostPQTable pqTable = null;

    private PQTableDescription descriptor;

    private BoostStateDB db;

    @Before
    public void setUp() throws Exception {
        descriptor = new PQTableDescription("pqstate", LongSerializer.INSTANCE);
        db = PowerMockito.mock(BoostStateDB.class);
        pqTable = PowerMockito.mock(BoostPQTable.class);
        PowerMockito.when(descriptor.createTable(db)).thenReturn(pqTable);
        PowerMockito.doNothing().when(pqTable, "add", anyObject(), anyInt());
        PowerMockito.doNothing().when(pqTable, "remove", anyObject(), anyInt());
    }

    @After
    public void tearDown() {

    }

    @Test
    public void test_pq_add_remove() {
        try {
            pqTable.add(addKey, 42345);
            pqTable.remove(removeKey, 12345);
            descriptor.updateKeySerializer(LongSerializer.INSTANCE);
            descriptor.createTable(db);
            Assert.assertEquals(descriptor.getStateName(), "pqstate");
            Assert.assertEquals(descriptor.getStateType(), PQ);
        } catch (Exception e) {
            fail("Should not have thrown any exception");
        }
    }
}
