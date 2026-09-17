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

package com.huawei.ock.bss.table.namespace;

import com.huawei.ock.bss.table.NativeUtil;

import org.apache.flink.api.common.typeutils.base.StringSerializer;
import org.apache.flink.queryablestate.client.state.serialization.KvStateSerializer;
import org.junit.After;
import org.junit.AfterClass;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestName;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import java.io.IOException;
import java.util.Iterator;
import java.util.Random;
import java.util.stream.Stream;

/**
 * NsKVTableImplNativeTest
 *
 * @since BeiMing 25.0.T1
 */
public class NsKVTableImplNativeTest {
    private static final Logger LOGGER = LoggerFactory.getLogger(NsKVTableImplNativeTest.class);
    private NsKVTableImpl<String, String, String> nsKVTable;
    private final TestName testName = new TestName();

    @Rule
    public TestName getTestName() {
        return testName;
    }


    @Before
    public void setUp() throws IOException {
        LOGGER.info("SubKVTable start to test");
        NativeUtil.getInstance().loadLibrary();
        NativeUtil nativeUtil = new NativeUtil();
        int taskSlotFlag = new Random().nextInt(1000000000);
        nativeUtil.setTaskSlotFlag(taskSlotFlag);
        String tableName = "table_" + getTestName().getMethodName() + taskSlotFlag;
        nsKVTable = nativeUtil.createNsKVTableImpl(tableName);
    }

    @AfterClass
    public static void clear() {
        NativeUtil.removeInstancePathData();
    }

    @After
    public void tearDown() {
        LOGGER.info("SubKVTable test done");
    }

    @Test
    public void test_get_keys() {
        String key = getTestName().getMethodName();
        String key2 = getTestName().getMethodName() + 1;
        String subKey = getTestName().getMethodName() + "SubKey";
        String value1 = "testValue1";
        String value2 = "testValue2";
        nsKVTable.add(key, subKey, value1);
        nsKVTable.add(key2, subKey, value2);
        Stream<String> keyStream = nsKVTable.getKeys(subKey);
        Iterator<String> iterator = keyStream.iterator();
        int count = 0;
        while (iterator.hasNext()) {
            String pKey = iterator.next();
            Assert.assertTrue(key.equals(pKey) || key2.equals(pKey));
            count++;
        }
        Assert.assertEquals(2, count);
    }

    @Test(expected = NullPointerException.class)
    public void test_get_keys_null() {
        String key = getTestName().getMethodName();
        String key2 = getTestName().getMethodName() + 1;
        String subKey = getTestName().getMethodName() + "SubKey";
        String value1 = "testValue1";
        String value2 = "testValue2";
        nsKVTable.add(key, subKey, value1);
        nsKVTable.add(key2, subKey, value2);
        nsKVTable.getKeys(null);
    }

    @Test
    public void test_getSerializedValue_normal() throws IOException {
        String key = getTestName().getMethodName();
        String key2 = getTestName().getMethodName() + 1;
        String subKey = getTestName().getMethodName() + "SubKey";
        String value1 = "testValue1";
        String value2 = "testValue2";
        nsKVTable.add(key, subKey, value1);
        nsKVTable.add(key2, subKey, value2);
        byte[] res1 = nsKVTable.getSerializedValue(key, subKey);
        byte[] res2 = nsKVTable.getSerializedValue(key2, subKey);
        Assert.assertEquals(value1, KvStateSerializer.deserializeValue(res1, StringSerializer.INSTANCE));
        Assert.assertEquals(value2, KvStateSerializer.deserializeValue(res2, StringSerializer.INSTANCE));
    }
}
