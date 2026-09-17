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

import org.apache.flink.api.java.tuple.Tuple3;
import org.apache.flink.util.CloseableIterator;
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
import java.util.AbstractMap;
import java.util.ArrayList;
import java.util.Iterator;
import java.util.List;
import java.util.Locale;
import java.util.Map;
import java.util.Random;
import java.util.regex.Pattern;

/**
 * KMapTableImplNativeTest
 *
 * @since BeiMing 25.0.T1
 */
public class KMapTableImplNativeTest {
    private static final Logger LOGGER = LoggerFactory.getLogger(KMapTableImplNativeTest.class);
    private KMapTableImpl<String, String, String> kMapTable;
    private final TestName testName = new TestName();

    private NativeUtil nativeUtil;

    @Rule
    public TestName getTestName() {
        return testName;
    }


    @Before
    public void setUp() throws IOException {
        LOGGER.info("[{}] KMapTable start to test", getTestName().getMethodName());
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
        kMapTable = nativeUtil.createKMapTableImpl(tableName, sliceMemWaterMark);
    }

    @AfterClass
    public static void clear() {
        NativeUtil.removeInstancePathData();
    }


    @After
    public void tearDown() {
        kMapTable.db.close();
        LOGGER.info("[{}] KMapTable test done", getTestName().getMethodName());
    }

    @Test
    public void test_add_success() {
        String key = getTestName().getMethodName();
        String subKey = getTestName().getMethodName() + "SubKey";
        String value = "testValue";
        kMapTable.add(key, subKey, value);
        Assert.assertTrue(kMapTable.contains(key, subKey));
        Assert.assertTrue(kMapTable.contains(key));
    }

    @Test
    public void test_add_key_subKey_not_null_value_null() {
        String key = getTestName().getMethodName();
        String subKey = getTestName().getMethodName() + "SubKey";
        kMapTable.add(key, subKey, null);
        Assert.assertTrue(kMapTable.contains(key, subKey));
        Assert.assertTrue(kMapTable.contains(key));
    }

    @Test(expected = NullPointerException.class)
    public void test_add_key_not_null_subKey_value_null() {
        String key = getTestName().getMethodName();
        kMapTable.add(key, null, null);
    }

    @Test(expected = NullPointerException.class)
    public void test_add_key_subKey_value_null() {
        kMapTable.add(null, null, null);
    }


    @Test
    public void test_get_success() {
        String key = getTestName().getMethodName();
        String subKey = getTestName().getMethodName() + "SubKey";
        String value = "testValue";
        kMapTable.add(key, subKey, value);
        Map<String, String> actualMap = kMapTable.get(key);
        Assert.assertNotNull(actualMap);
        Assert.assertEquals(value, actualMap.get(subKey));
    }


    @Test(expected = NullPointerException.class)
    public void test_get_key_null() {
        kMapTable.get(null);
    }

    @Test
    public void test_get_value_null() {
        String key = getTestName().getMethodName();
        String subKey = getTestName().getMethodName() + "SubKey";
        kMapTable.add(key, subKey, null);
        Map<String, String> actualMap = kMapTable.get(key);
        Assert.assertNotNull(actualMap);
        Assert.assertNull(actualMap.get(subKey));
    }

    @Test
    public void test_get_key_subKey_null() {
        String key = getTestName().getMethodName();
        String subKey = getTestName().getMethodName() + "SubKey";
        String value = "testValue";
        kMapTable.add(key, subKey, value);
        Map<String, String> actualMap = kMapTable.get(key);
        Assert.assertNotNull(actualMap);
        Assert.assertNull(actualMap.get(null));
    }

    @Test
    public void test_get_default_success() {
        String key = getTestName().getMethodName();
        String subKey = getTestName().getMethodName() + "SubKey";
        String value = "testValue";
        Assert.assertNull(kMapTable.get(key, subKey));
        Assert.assertEquals(value, kMapTable.getOrDefault(key, subKey, value));
    }

    @Test(expected = NullPointerException.class)
    public void test_get_default_key_null() {
        String subKey = getTestName().getMethodName() + "SubKey";
        String value = "testValue";
        kMapTable.getOrDefault(null, subKey, value);
    }

    @Test(expected = NullPointerException.class)
    public void test_get_default_key_not_null_subKey_null() {
        String key = getTestName().getMethodName();
        String value = "testValue";
        kMapTable.getOrDefault(key, null, value);
    }

    @Test
    public void test_getSerializedValue_normal() throws IOException {
        String key = getTestName().getMethodName();
        String subKey = getTestName().getMethodName() + "SubKey";
        String value = "testValue";
        kMapTable.add(key, subKey, value);
        byte[] res = kMapTable.getSerializedValue(key);
        Map<String, String> actualMap =
            KvStateSerializer.deserializeMap(res, StringSerializer.INSTANCE, StringSerializer.INSTANCE);
        Assert.assertNotNull(actualMap);
        Assert.assertEquals(value, actualMap.get(subKey));
    }

    @Test
    public void test_contains_key_subKey_is_exist() {
        String key = getTestName().getMethodName();
        String subKey = getTestName().getMethodName() + "SubKey";
        String value = "testValue";
        kMapTable.add(key, subKey, value);
        Assert.assertTrue(kMapTable.contains(key));
        Assert.assertTrue(kMapTable.contains(key, subKey));
    }

    @Test
    public void test_contains_key_is_exist_subKey_not_exist() {
        String key = getTestName().getMethodName();
        String subKey = getTestName().getMethodName() + "SubKey";
        String value = "testValue";
        kMapTable.add(key, subKey, value);
        Assert.assertTrue(kMapTable.contains(key));
        Assert.assertFalse(kMapTable.contains(key, subKey + 1));
    }

    @Test
    public void test_contains_key_not_exist() {
        String key = getTestName().getMethodName();
        Assert.assertFalse(kMapTable.contains(key));
    }

    @Test
    public void test_remove_success() {
        String key = getTestName().getMethodName();
        String subKey = getTestName().getMethodName() + "SubKey";
        String value = "testValue";
        kMapTable.add(key, subKey, value);
        kMapTable.remove(key, subKey);
        Assert.assertFalse(kMapTable.contains(key, subKey));
        kMapTable.remove(key);
        Assert.assertFalse(kMapTable.contains(key));
    }

    @Test(expected = NullPointerException.class)
    public void test_remove_key_null() {
        kMapTable.remove(null);
    }

    @Test
    public void test_iterator_normal() {
        String key = getTestName().getMethodName();
        String subKey1 = getTestName().getMethodName() + "SubKey1";
        String value1 = "testValue1";
        String subKey2 = getTestName().getMethodName() + "SubKey2";
        String value2 = "testValue2";
        Map.Entry<String, String> entry1 = new AbstractMap.SimpleEntry<>(subKey1, value1);
        Map.Entry<String, String> entry2 = new AbstractMap.SimpleEntry<>(subKey2, value2);

        kMapTable.add(key, subKey1, value1);
        kMapTable.add(key, subKey2, value2);
        Iterator<Map.Entry<String, String>> iterator = kMapTable.iterator(key);
        for (int i = 0; i < 2; i++) {
            Assert.assertTrue(iterator.hasNext());
            Map.Entry<String, String> entry = iterator.next();
            Assert.assertTrue(entry.equals(entry1) || entry.equals(entry2));
        }
    }

    @Test
    public void test_entry_iterator_normal() {
        String key = getTestName().getMethodName();
        String subKey1 = getTestName().getMethodName() + "SubKey1";
        String value1 = "testValue1";
        String subKey2 = getTestName().getMethodName() + "SubKey2";
        String value2 = "testValue2";
        Tuple3<String, String, String> entry1 = new Tuple3<>(key, subKey1, value1);
        Tuple3<String, String, String> entry2 = new Tuple3<>(key, subKey2, value2);

        kMapTable.add(key, subKey1, value1);
        kMapTable.add(key, subKey2, value2);
        CloseableIterator<Tuple3<String, String, String>> iterator = kMapTable.entryIterator();
        for (int i = 0; i < 2; i++) {
            Assert.assertTrue(iterator.hasNext());
            Tuple3<String, String, String> next = iterator.next();
            Assert.assertTrue(next.equals(entry1) || next.equals(entry2));
        }
    }

    @Test
    public void test_iterator_null() {
        Iterator<Map.Entry<String, String>> iterator = kMapTable.iterator(getTestName().getMethodName());
        Assert.assertNull(iterator);
    }

    @Test
    public void test_slice_table_add_get_check_remove_success() {
        // 7MB * 3 写入fresh table，触发segment 16MB 写入slice table
        String pKey = getTestName().getMethodName();
        List<String> textDataKeys = new ArrayList<>();
        List<String> textDataElements = NativeUtil.getInstance().getTextDataElements(3);
        for (int i = 0; i < textDataElements.size(); i++) {
            String subKey = getTestName().getMethodName() + i;
            textDataKeys.add(subKey);
            kMapTable.add(pKey, subKey, textDataElements.get(i));
        }
        // 检查包含key
        Assert.assertTrue(kMapTable.contains(pKey));
        for (String subKey : textDataKeys) {
            Assert.assertTrue(kMapTable.contains(pKey, subKey));
        }
        LOGGER.debug("success put data and flush to slice table");
        // 检查数据一致性
        for (int i = 0; i < textDataElements.size(); i++) {
            String actualValue = kMapTable.get(pKey, textDataKeys.get(i));
            boolean isEquals = textDataElements.get(i).equals(actualValue);
            String msg;
            if (actualValue == null) {
                msg = String.format(Locale.ROOT, "Data is not consistency, key: %s, subKey: %s, expect value length: "
                        + "%d, actual is null", pKey, textDataKeys.get(i), textDataElements.get(i).length());
            } else {
                msg = String.format(Locale.ROOT, "Data is not consistency, key: %s, subKey: %s, expect value length: "
                                + "%d, actual value length: %d", pKey, textDataKeys.get(i),
                        textDataElements.get(i).length(), actualValue.length());
            }
            Assert.assertTrue(msg, isEquals);
        }
        LOGGER.debug("success get and check data consistency");

        // 移除key
        for (String subKey : textDataKeys) {
            kMapTable.remove(pKey, subKey);
            Assert.assertFalse(String.format(Locale.ROOT, "failed to remove data, key: %s, sbuKey: %s", pKey, subKey),
                    kMapTable.contains(pKey, subKey));
        }
        LOGGER.debug("success remove data");
    }

    @Test
    public void test_lsm_add_get_check_remove_success() throws IOException {
        // 7MB * 10 写入fresh table，触发segment 16MB 写入slice table
        String pKey = getTestName().getMethodName();
        List<String> textDataKeys = new ArrayList<>();
        List<String> textDataElements = NativeUtil.getInstance().getTextDataElements(10);
        for (int i = 0; i < textDataElements.size(); i++) {
            String subKey = getTestName().getMethodName() + i;
            textDataKeys.add(subKey);
            kMapTable.add(pKey, subKey, textDataElements.get(i));
        }
        // 检查包含key
        Assert.assertTrue(kMapTable.contains(pKey));
        for (String subKey : textDataKeys) {
            Assert.assertTrue(kMapTable.contains(pKey, subKey));
        }
        // 检查是否真实写盘
        Assert.assertTrue("failed flush data to disk", nativeUtil.isExistInstanceBasePathFile());
        LOGGER.debug("success add data and flush to slice table");
        // 检查数据一致性
        for (int i = 0; i < textDataElements.size(); i++) {
            String actualValue = kMapTable.get(pKey, textDataKeys.get(i));
            boolean isEquals = textDataElements.get(i).equals(actualValue);
            String msg;
            if (actualValue == null) {
                msg = String.format(Locale.ROOT, "Data is not consistency, key: %s, subKey: %s, expect value length: "
                        + "%d, actual is null", pKey, textDataKeys.get(i), textDataElements.get(i).length());
            } else {
                msg = String.format(Locale.ROOT, "Data is not consistency, key: %s, subKey: %s, expect value length: "
                                + "%d, actual value length: %d", pKey, textDataKeys.get(i),
                        textDataElements.get(i).length(), actualValue.length());
            }
            Assert.assertTrue(msg, isEquals);
        }
        LOGGER.debug("success get and check data consistency");

        // 移除key
        for (String subKey : textDataKeys) {
            kMapTable.remove(pKey, subKey);
            Assert.assertFalse(String.format(Locale.ROOT, "failed to remove data, key: %s, sbuKey: %s", pKey, subKey),
                    kMapTable.contains(pKey, subKey));
        }
        LOGGER.debug("success remove data");
    }
}