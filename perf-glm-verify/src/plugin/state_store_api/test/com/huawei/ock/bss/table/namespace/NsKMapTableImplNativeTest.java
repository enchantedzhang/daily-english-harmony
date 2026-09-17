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

import com.huawei.ock.bss.table.KeyPair;
import com.huawei.ock.bss.table.NativeUtil;

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
import java.util.Map;
import java.util.Random;
import java.util.stream.Stream;

/**
 * NsKMapTableImplNativeTest
 *
 * @since BeiMing 25.0.T1
 */

public class NsKMapTableImplNativeTest {
    private static final Logger LOGGER = LoggerFactory.getLogger(NsKMapTableImplNativeTest.class);
    private NsKMapTableImpl<String, String, String, String, Map<String, String>> nsKMapTable;
    private final TestName testName = new TestName();

    @Rule
    public TestName getTestName() {
        return testName;
    }

    @Before
    public void setUp() throws IOException {
        LOGGER.info("SubKMapTable start to test");
        NativeUtil.getInstance().loadLibrary();
        NativeUtil nativeUtil = new NativeUtil();
        int taskSlotFlag = new Random().nextInt(1000000000);
        nativeUtil.setTaskSlotFlag(taskSlotFlag);
        String tableName = "table_" + getTestName().getMethodName() + taskSlotFlag;
        nsKMapTable = nativeUtil.createNsKMapTableImpl(tableName);
    }

    @AfterClass
    public static void clear() {
        NativeUtil.removeInstancePathData();
    }

    @After
    public void tearDown() {
        LOGGER.info("SubKMapTable tear down");
    }

    @Test
    public void test_get_keys() {
        String key = getTestName().getMethodName();
        String key2 = getTestName().getMethodName() + 1;
        String namespace = getTestName().getMethodName() + "Namespace";
        String subKey = getTestName().getMethodName() + "SubKey";

        String value1 = "testValue1";
        String value2 = "testValue2";

        nsKMapTable.add(nsKMapTable.getKeyPair(key, namespace), subKey, value1);
        nsKMapTable.add(nsKMapTable.getKeyPair(key2, namespace), subKey, value2);
        Stream<String> keyStream = nsKMapTable.getKeys(namespace);
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
        KeyPair<String, String> keyPair = new KeyPair<>(key, key);
        KeyPair<String, String> keyPair2 = new KeyPair<>(key2, key2);
        String subKey = getTestName().getMethodName() + "SubKey";
        String value1 = "testValue1";
        String value2 = "testValue2";
        nsKMapTable.add(keyPair, subKey, value1);
        nsKMapTable.add(keyPair2, subKey, value2);
        nsKMapTable.getKeys(null);
    }

    @Test
    public void test_contains_key_exist() {
        String key = getTestName().getMethodName();
        String subKey = getTestName().getMethodName() + "SubKey";
        KeyPair<String, String> keyPair = new KeyPair<>(key, subKey);
        String value1 = "testValue1";
        nsKMapTable.add(keyPair, subKey, value1);
        Assert.assertTrue(nsKMapTable.contains(keyPair, subKey));
    }

    @Test
    public void test_contains_key_null() {
        String key = getTestName().getMethodName();
        String subKey = getTestName().getMethodName() + "SubKey";
        KeyPair<String, String> keyPair = new KeyPair<>(key, subKey);
        Assert.assertFalse(nsKMapTable.contains(keyPair, subKey));
    }
}
