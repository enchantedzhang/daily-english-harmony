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

import org.apache.flink.api.common.typeutils.base.StringSerializer;
import org.apache.flink.queryablestate.client.state.serialization.KvStateSerializer;
import org.apache.flink.util.CloseableIterator;
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
import java.util.AbstractMap;
import java.util.ArrayList;
import java.util.List;
import java.util.Locale;
import java.util.Map;
import java.util.NoSuchElementException;
import java.util.Random;
import java.util.regex.Pattern;

/**
 * KVTableImplNativeTest
 *
 * @since BeiMing 25.0.T1
 */
public class KVTableImplNativeTest {
    private static final Logger LOGGER = LoggerFactory.getLogger(KVTableImplNativeTest.class);
    private KVTableImpl<String, String> kvTable;
    private final TestName testName = new TestName();

    private NativeUtil nativeUtil;

    @Rule
    public TestName getTestName() {
        return testName;
    }

    @Before
    public void setUp() throws IOException {
        LOGGER.info("[{}] KVTable start to test", getTestName().getMethodName());
        NativeUtil.getInstance().loadLibrary();
        nativeUtil = new NativeUtil();
        float sliceMemWaterMark = 0.8F;
        // lsm写入，水位线设置0.01
        if (Pattern.matches("^test_lsm.*", getTestName().getMethodName())) {
            sliceMemWaterMark = 0.01F;
        }
        int taskSlotFlag = new Random().nextInt(1000000000);
        nativeUtil.setTaskSlotFlag(taskSlotFlag);
        String tableName = "table_" + getTestName().getMethodName() + taskSlotFlag;
        kvTable = nativeUtil.createKVTableImpl(tableName, sliceMemWaterMark);
    }

    @AfterClass
    public static void clear() {
        NativeUtil.removeInstancePathData();
    }

    @After
    public void tearDown() {
        kvTable.db.close();
        LOGGER.info("[{}] KVTable test done", getTestName().getMethodName());
    }

    @Test
    public void test_put_success() {
        String key = getTestName().getMethodName();
        String value = "testValue";
        kvTable.put(key, value);
        Assert.assertTrue(kvTable.contains(key));
    }

    @Test(expected = NullPointerException.class)
    public void test_put_key_null() {
        String value = "testValue";
        kvTable.put(null, value);
    }

    @Test(expected = NullPointerException.class)
    public void test_put_value_null() {
        String key = getTestName().getMethodName();
        kvTable.put(key, null);
    }

    @Test(expected = NullPointerException.class)
    public void test_put_key_value_null() {
        kvTable.put(null, null);
    }

    @Test
    public void test_get_success() {
        String key = getTestName().getMethodName();
        String value = "testValue";
        kvTable.put(key, value);
        String result = kvTable.get(key);
        Assert.assertEquals(value, result);
    }

    @Test
    public void test_get_value_null() {
        String key = getTestName().getMethodName();
        Assert.assertNull(kvTable.get(key));
    }

    @Test(expected = NullPointerException.class)
    public void test_get_key_null() {
        kvTable.get(null);
    }

    @Test
    public void test_contains_is_exist() {
        String key = getTestName().getMethodName();
        String value = "testValue";
        kvTable.put(key, value);
        Assert.assertTrue(kvTable.contains(key));
    }

    @Test
    public void test_contains_not_exist() {
        String key = getTestName().getMethodName();
        Assert.assertFalse(kvTable.contains(key));
    }

    @Test
    public void test_remove_success() {
        String key = getTestName().getMethodName();
        String value = "testValue";
        kvTable.put(key, value);
        String result = kvTable.get(key);
        Assert.assertEquals(value, result);
        kvTable.remove(key);
        Assert.assertFalse(kvTable.contains(key));
    }

    @Test
    public void test_remove_not_exist_key() {
        String key = getTestName().getMethodName();
        Assert.assertNull(kvTable.get(key));
        kvTable.remove(key);
    }

    @Test
    public void test_iterator_normal() throws Exception {
        String key1 = getTestName().getMethodName() + "1";
        String value1 = "testValue1";
        String key2 = getTestName().getMethodName() + "2";
        String value2 = "testValue2";
        Map.Entry<String, String> entry1 = new AbstractMap.SimpleEntry<>(key1, value1);
        Map.Entry<String, String> entry2 = new AbstractMap.SimpleEntry<>(key2, value2);
        CloseableIterator<Map.Entry<String, String>> iterator = null;
        try {
            kvTable.put(key1, value1);
            kvTable.put(key2, value2);
            iterator = kvTable.iterator();
            for (int i = 0; i < 2; i++) {
                Assert.assertTrue(iterator.hasNext());
                Map.Entry<String, String> entry = iterator.next();
                Assert.assertTrue(entry.equals(entry1) || entry.equals(entry2));
            }
        } finally {
            if (iterator != null) {
                iterator.close();
            }
        }
    }

    @Test(expected = NoSuchElementException.class)
    public void test_iterator_null() {
        CloseableIterator<Map.Entry<String, String>> iterator = kvTable.iterator();
        Assert.assertFalse(iterator.hasNext());
        iterator.next();
    }

    @Test
    public void test_getSerializedValue_normal() throws IOException {
        String key = getTestName().getMethodName();
        String value = "testValue";
        kvTable.put(key, value);
        Assert.assertEquals(value, KvStateSerializer.deserializeValue(kvTable.getSerializedValue(key),
            StringSerializer.INSTANCE));
    }

    @Test
    public void test_slice_table_put_get_check_remove_success() {
        // 7MB * 3 写入fresh table，触发segment 16MB 写入slice table
        List<String> textDataKeys = new ArrayList<>();
        List<String> textDataElements = NativeUtil.getInstance().getTextDataElements(3);
        for (int i = 0; i < textDataElements.size(); i++) {
            String key = getTestName().getMethodName() + i;
            textDataKeys.add(key);
            kvTable.put(key, textDataElements.get(i));
        }
        // 检查包含key
        for (String key : textDataKeys) {
            Assert.assertTrue(kvTable.contains(key));
        }
        LOGGER.debug("success put data and flush to fresh table");
        // 检查数据一致性
        for (int i = 0; i < textDataElements.size(); i++) {
            String actualValue = kvTable.get(textDataKeys.get(i));
            boolean isEquals = textDataElements.get(i).equals(actualValue);
            String msg;
            if (actualValue == null) {
                msg = String.format(Locale.ROOT, "Data is not consistency, key: %s, expect value length: %d, "
                        + "actual is null", textDataKeys.get(i), textDataElements.get(i).length());
            } else {
                msg = String.format(Locale.ROOT, "Data is not consistency, key: %s, expect value length: %d, "
                                + "actual value length: %d", textDataKeys.get(i), textDataElements.get(i).length(),
                        actualValue.length());
            }
            Assert.assertTrue(msg, isEquals);
        }
        LOGGER.debug("success put data and check data consistency");
        // 移除key
        for (String key : textDataKeys) {
            kvTable.remove(key);
            Assert.assertFalse("failed to remove data, key: " + key, kvTable.contains(key));
        }
        LOGGER.debug("success remove data");
    }

    @Test
    public void test_lsm_put_get_check_remove_success() throws IOException {
        /*
         * 7MB * 10 写入fresh table，触发segment 16MB 写入slice table
         * 水位线 0.01F * 48MB，LSM 32MB一组写盘
         */
        List<String> textDataKeys = new ArrayList<>();
        List<String> textDataElements = NativeUtil.getInstance().getTextDataElements(10);
        for (int i = 0; i < textDataElements.size(); i++) {
            String key = getTestName().getMethodName() + i;
            textDataKeys.add(key);
            kvTable.put(key, textDataElements.get(i));
        }
        // 检查包含key
        for (String key : textDataKeys) {
            Assert.assertTrue(kvTable.contains(key));
        }
        // 检查是否真实写盘
        Assert.assertTrue("failed flush data to disk", nativeUtil.isExistInstanceBasePathFile());
        LOGGER.debug("success put data and flush to disk");
        // 检查数据一致性
        for (int i = 0; i < textDataElements.size(); i++) {
            String actualValue = kvTable.get(textDataKeys.get(i));
            boolean isEquals = textDataElements.get(i).equals(actualValue);
            String msg;
            if (actualValue == null) {
                msg = String.format(Locale.ROOT, "Data is not consistency, key: %s, expect value length: %d, "
                        + "actual is null", textDataKeys.get(i), textDataElements.get(i).length());
            } else {
                msg = String.format(Locale.ROOT, "Data is not consistency, key: %s, expect value length: %d, "
                                + "actual value length: %d", textDataKeys.get(i), textDataElements.get(i).length(),
                        actualValue.length());
            }
            Assert.assertTrue(msg, isEquals);
        }
        LOGGER.debug("success get and check data consistency");
        // 移除key
        for (String key : textDataKeys) {
            kvTable.remove(key);
            Assert.assertFalse("failed to remove data, key: " + key, kvTable.contains(key));
        }
        LOGGER.debug("success remove data");
    }
}
