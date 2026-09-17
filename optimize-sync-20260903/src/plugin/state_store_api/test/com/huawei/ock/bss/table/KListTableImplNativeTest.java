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
import java.util.ArrayList;
import java.util.HashSet;
import java.util.List;
import java.util.Locale;
import java.util.Map;
import java.util.Random;
import java.util.Set;
import java.util.regex.Pattern;

/**
 * KListTableImplNativeTest
 *
 * @since BeiMing 25.0.T1
 */
public class KListTableImplNativeTest {
    private static final Logger LOGGER = LoggerFactory.getLogger(KListTableImplNativeTest.class);

    private KListTableImpl<String, String> kListTable;

    private final TestName testName = new TestName();

    private NativeUtil nativeUtil;

    @Rule
    public TestName getTestName() {
        return testName;
    }

    @Before
    public void setUp() throws IOException {
        LOGGER.info("[{}] KListTable start to test", getTestName().getMethodName());
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
        kListTable = nativeUtil.createKListTableImpl(tableName, sliceMemWaterMark);
    }

    @AfterClass
    public static void clear() {
        NativeUtil.removeInstancePathData();
    }

    @After
    public void tearDown() {
        kListTable.db.close();
        LOGGER.info("[{}] KListTable test done", getTestName().getMethodName());
    }

    @Test
    public void test_put_success() {
        String key = getTestName().getMethodName();
        List<String> value = new ArrayList<>();
        value.add("testValue");
        kListTable.put(key, value);
        Assert.assertTrue(kListTable.contains(key));
    }

    @Test
    public void test_put_and_iterator_success() {
        String key = getTestName().getMethodName();
        List<String> value = new ArrayList<>();
        value.add("testValue");
        value.add("testValue2");
        kListTable.put(key, value);
        CloseableIterator<Map.Entry<String, List<String>>> iterator = kListTable.iterator();
        Assert.assertNotNull(iterator);
        Assert.assertTrue(iterator.hasNext());
        Map.Entry<String, List<String>> next = iterator.next();
        Assert.assertEquals(key, next.getKey());
        List<String> valueList = next.getValue();
        Assert.assertEquals(valueList.size(), 2);
        Assert.assertTrue(valueList.contains("testValue"));
        Assert.assertTrue(valueList.contains("testValue2"));
    }

    @Test(expected = NullPointerException.class)
    public void test_put_key_null() {
        List<String> value = new ArrayList<>();
        value.add("testValue");
        kListTable.put(null, value);
    }

    @Test(expected = NullPointerException.class)
    public void test_put_list_null() {
        String key = getTestName().getMethodName();
        kListTable.put(key, null);
    }

    @Test
    public void test_put_list_empty() {
        String key = getTestName().getMethodName();
        List<String> value = new ArrayList<>();
        kListTable.put(key, value);
        Assert.assertTrue(kListTable.contains(key));
    }

    @Test
    public void test_put_list_value_null() {
        String key = getTestName().getMethodName();
        List<String> value = new ArrayList<>();
        value.add(null);
        kListTable.put(key, value);
        Assert.assertTrue(kListTable.contains(key));
    }

    @Test
    public void test_add_success() {
        String key = getTestName().getMethodName();
        String value = "testValue";
        kListTable.add(key, value);
        Assert.assertTrue(kListTable.contains(key));
    }

    @Test
    public void test_add_list_value_null() {
        String key = getTestName().getMethodName();
        kListTable.add(key, null);
        Assert.assertTrue(kListTable.contains(key));
    }

    @Test
    public void test_get_success() {
        String key = getTestName().getMethodName();
        List<String> value = new ArrayList<>();
        value.add("testValue");
        kListTable.put(key, value);
        Assert.assertTrue(kListTable.contains(key));
        List<String> res = kListTable.get(key);
        Assert.assertEquals(value, res);
    }

    @Test(expected = NullPointerException.class)
    public void test_get_key_null() {
        kListTable.get(null);
    }

    @Test
    public void test_get_list_empty() {
        String key = getTestName().getMethodName();
        List<String> value = new ArrayList<>();
        kListTable.put(key, value);
        Assert.assertTrue(kListTable.contains(key));
        List<String> res = kListTable.get(key);
        Assert.assertTrue(res.isEmpty());
    }

    @Test
    public void test_get_list_value_null() {
        String key = getTestName().getMethodName();
        List<String> value = new ArrayList<>();
        value.add(null);
        kListTable.put(key, value);
        Assert.assertTrue(kListTable.contains(key));
        List<String> res = kListTable.get(key);
        Assert.assertNull(res.get(0));
    }

    @Test
    public void test_get_default_success() {
        String key = getTestName().getMethodName();
        List<String> defaultValue = new ArrayList<>();
        defaultValue.add("testValue");
        List<String> res = kListTable.getOrDefault(key, defaultValue);
        Assert.assertEquals(defaultValue, res);
    }

    @Test(expected = NullPointerException.class)
    public void test_get_default_key_null() {
        List<String> defaultValue = new ArrayList<>();
        kListTable.getOrDefault(null, defaultValue);
    }

    @Test
    public void test_addAll_success() {
        String key = getTestName().getMethodName();
        Set<String> value = new HashSet<>();
        value.add("testValue1");
        value.add("testValue2");
        kListTable.addAll(key, value);
        Assert.assertTrue(kListTable.contains(key));
        Assert.assertEquals(value, new HashSet<>(kListTable.get(key)));
    }

    @Test(expected = NullPointerException.class)
    public void test_addAll_key_null() {
        Set<String> value = new HashSet<>();
        value.add("testValue1");
        value.add("testValue2");
        kListTable.addAll(null, value);
    }

    @Test(expected = NullPointerException.class)
    public void test_addAll_collection_null() {
        String key = getTestName().getMethodName();
        kListTable.addAll(key, null);
    }

    @Test
    public void test_addAll_collection_empty() {
        String key = getTestName().getMethodName();
        Set<String> value = new HashSet<>();
        kListTable.addAll(key, value);
        Assert.assertTrue(kListTable.contains(key));
        Assert.assertTrue(kListTable.get(key).isEmpty());
    }

    @Test
    public void test_addAll_collection_value_null() {
        String key = getTestName().getMethodName();
        Set<String> value = new HashSet<>();
        value.add(null);
        kListTable.addAll(key, value);
        Assert.assertTrue(kListTable.contains(key));
        Assert.assertEquals(value, new HashSet<>(kListTable.get(key)));
    }

    @Test
    public void test_contains_is_exist() {
        String key = getTestName().getMethodName();
        List<String> value = new ArrayList<>();
        value.add("testValue");
        kListTable.put(key, value);
        Assert.assertTrue(kListTable.contains(key));
    }

    @Test
    public void test_contains_not_exist() {
        String key = getTestName().getMethodName();
        Assert.assertFalse(kListTable.contains(key + 1));
    }

    @Test
    public void test_remove_success() {
        String key = getTestName().getMethodName();
        List<String> value = new ArrayList<>();
        value.add("testValue");
        kListTable.put(key, value);
        Assert.assertTrue(kListTable.contains(key));
        kListTable.remove(key);
        Assert.assertFalse(kListTable.contains(key));
    }

    @Test
    public void test_remove_not_exist_key() {
        String key = getTestName().getMethodName();
        Assert.assertFalse(kListTable.contains(key));
        kListTable.remove(key);
        Assert.assertFalse(kListTable.contains(key));
    }

    @Test
    public void test_slice_table_add_get_check_remove_success() {
        // 7MB * 3 写入fresh table，触发segment 16MB 写入slice table
        List<String> textDataKeys = new ArrayList<>();
        List<String> textDataElements = NativeUtil.getInstance().getTextDataElements(3);
        for (int i = 0; i < textDataElements.size(); i++) {
            String key = getTestName().getMethodName() + i;
            textDataKeys.add(key);
            kListTable.add(key, textDataElements.get(i));
        }
        // 检查包含key
        for (String key : textDataKeys) {
            Assert.assertTrue(kListTable.contains(key));
        }
        LOGGER.debug("success add data and flush to slice table");
        // 检查数据一致性
        for (int i = 0; i < textDataElements.size(); i++) {
            String actualValue = kListTable.get(textDataKeys.get(i)).get(0);
            boolean isEquals = textDataElements.get(i).equals(actualValue);
            String msg;
            if (actualValue == null) {
                msg = String.format(Locale.ROOT, "Data is not consistency, key: %s, expect value length: %d, "
                    + "actual value is null", textDataKeys.get(i), textDataElements.get(i).length());
            } else {
                msg = String.format(Locale.ROOT, "Data is not consistency, key: %s, expect value length: %d, "
                        + "actual value length: %d", textDataKeys.get(i), textDataElements.get(i).length(),
                    actualValue.length());
            }
            Assert.assertTrue(msg, isEquals);
        }
        LOGGER.debug("success get data and check data consistency");
        // 移除key
        for (String key : textDataKeys) {
            kListTable.remove(key);
            Assert.assertFalse("failed to remove data, key: " + key, kListTable.contains(key));
        }
        LOGGER.debug("success remove data");
    }

    @Test
    public void test_lsm_add_get_check_remove_success() throws IOException {
        // 7MB * 10 写入fresh table，触发segment 16MB 写入slice table
        List<String> textDataKeys = new ArrayList<>();
        List<String> textDataElements = NativeUtil.getInstance().getTextDataElements(10);
        for (int i = 0; i < textDataElements.size(); i++) {
            String key = getTestName().getMethodName() + i;
            textDataKeys.add(key);
            kListTable.add(key, textDataElements.get(i));
        }
        // 检查包含key
        for (String key : textDataKeys) {
            Assert.assertTrue(kListTable.contains(key));
        }
        // 检查是否真实写盘
        Assert.assertTrue("failed flush data to disk", nativeUtil.isExistInstanceBasePathFile());
        LOGGER.debug("success add data and flush to disk");
        // 检查数据一致性
        for (int i = 0; i < textDataElements.size(); i++) {
            String actualValue = kListTable.get(textDataKeys.get(i)).get(0);
            boolean isEquals = textDataElements.get(i).equals(actualValue);
            String msg;
            if (actualValue == null) {
                msg = String.format(Locale.ROOT, "Data is not consistency, key: %s, expect value length: %d, "
                    + "actual value is null", textDataKeys.get(i), textDataElements.get(i).length());
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
            kListTable.remove(key);
            Assert.assertFalse("failed to remove data, key: " + key, kListTable.contains(key));
        }
        LOGGER.debug("success remove data");
    }

    @Test
    public void test_slice_table_put_get_check_remove_success() {
        // 7MB * 3 写入fresh table，触发segment 16MB 写入slice table
        List<String> textDataKeys = new ArrayList<>();
        List<String> textDataElements = NativeUtil.getInstance().getTextDataElements(3);
        for (int i = 0; i < textDataElements.size(); i++) {
            String key = getTestName().getMethodName() + i;
            textDataKeys.add(key);
            List<String> v = new ArrayList<>();
            v.add(textDataElements.get(i));
            kListTable.put(key, v);
        }
        // 检查包含key
        for (String key : textDataKeys) {
            Assert.assertTrue(kListTable.contains(key));
        }
        LOGGER.debug("success put data and flush to slice table");
        // 检查数据一致性
        for (int i = 0; i < textDataElements.size(); i++) {
            String actualValue = kListTable.get(textDataKeys.get(i)).get(0);
            boolean isEquals = textDataElements.get(i).equals(actualValue);
            String msg;
            if (actualValue == null) {
                msg = String.format(Locale.ROOT, "Data is not consistency, key: %s, expect value length: %d, "
                    + "actual value is null", textDataKeys.get(i), textDataElements.get(i).length());
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
            kListTable.remove(key);
            Assert.assertFalse("failed to remove data, key: " + key, kListTable.contains(key));
        }
        LOGGER.debug("success remove data");
    }

    @Test
    public void test_lsm_put_get_check_remove_success() throws IOException {
        // 7MB * 10 写入fresh table，触发segment 16MB 写入slice table
        List<String> textDataKeys = new ArrayList<>();
        List<String> textDataElements = NativeUtil.getInstance().getTextDataElements(10);
        for (int i = 0; i < textDataElements.size(); i++) {
            String key = getTestName().getMethodName() + i;
            textDataKeys.add(key);
            List<String> v = new ArrayList<>();
            v.add(textDataElements.get(i));
            kListTable.put(key, v);
        }
        // 检查包含key
        for (String key : textDataKeys) {
            Assert.assertTrue(kListTable.contains(key));
        }
        // 检查是否真实写盘
        Assert.assertTrue("failed flush data to disk", nativeUtil.isExistInstanceBasePathFile());
        LOGGER.debug("success put data and flush to disk");
        // 检查数据一致性
        for (int i = 0; i < textDataElements.size(); i++) {
            String actualValue = kListTable.get(textDataKeys.get(i)).get(0);
            boolean isEquals = textDataElements.get(i).equals(actualValue);
            String msg;
            if (actualValue == null) {
                msg = String.format(Locale.ROOT, "Data is not consistency, key: %s, expect value length: %d, "
                    + "actual value is null", textDataKeys.get(i), textDataElements.get(i).length());
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
            kListTable.remove(key);
            Assert.assertFalse("failed to remove data, key: " + key, kListTable.contains(key));
        }
    }
}
