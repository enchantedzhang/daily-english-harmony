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

import static org.junit.Assert.fail;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.anyLong;

import com.huawei.ock.bss.common.BoostStateDB;
import com.huawei.ock.bss.common.BoostStateType;
import com.huawei.ock.bss.common.conf.BoostConfig;
import com.huawei.ock.bss.common.exception.BSSRuntimeException;
import com.huawei.ock.bss.common.memory.DirectBuffer;
import com.huawei.ock.bss.common.serialize.KVSerializerUtil;
import com.huawei.ock.bss.common.serialize.TableSerializer;
import com.huawei.ock.bss.table.description.KListTableDescription;
import com.huawei.ock.bss.table.result.StateListResult;

import org.apache.flink.api.common.typeutils.TypeSerializer;
import org.apache.flink.api.common.typeutils.base.StringSerializer;
import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mockito;
import org.powermock.api.mockito.PowerMockito;
import org.powermock.core.classloader.annotations.PrepareForTest;
import org.powermock.modules.junit4.PowerMockRunner;

import java.io.IOException;
import java.util.ArrayList;
import java.util.List;

/**
 * KListTableImplTest
 *
 * @since BeiMing 25.0.T1
 */
@RunWith(PowerMockRunner.class)
public class KListTableImplTest {
    private KListTableImpl<String, String> kListTable;

    private KListTableDescription<String, String> kListTableDescription;

    @Before
    public void setUp() {
        PowerMockito.mockStatic(KListTableImpl.class);
        PowerMockito.mockStatic(AbstractTable.class);
        PowerMockito.when(AbstractTable.open(anyLong(), Mockito.nullable(String.class), any())).thenReturn(1L);
        BoostConfig config = PowerMockito.mock(BoostConfig.class);
        BoostStateDB db = PowerMockito.mock(BoostStateDB.class);
        PowerMockito.when(config.isEnableBloomFilter()).thenReturn(true);
        PowerMockito.when(config.getExpectedKeyCount()).thenReturn(10000);
        PowerMockito.when(db.getConfig()).thenReturn(config);
        PowerMockito.when(db.getNativeHandle()).thenReturn(1L);
        TypeSerializer<String> keySerializer = new StringSerializer();
        TypeSerializer<String> valueSerializer = new StringSerializer();
        TableSerializer<String, String> tableSerializer = new TableSerializer<>(keySerializer, valueSerializer);
        kListTableDescription = new KListTableDescription<>("testTable", 10, tableSerializer);
        Assert.assertEquals(kListTableDescription.getStateType(), BoostStateType.LIST);
        kListTable = PowerMockito.spy(new KListTableImpl<>(db, kListTableDescription));
        PowerMockito.mockStatic(DirectBuffer.class);
        PowerMockito.when(DirectBuffer.nativeAcquireDirectBuffer(anyLong(), anyInt())).thenReturn(1L);
        PowerMockito.when(kListTable.testKeyHash(anyInt())).thenReturn(true);
        PowerMockito.when(kListTable.testKeyHash(anyInt(), anyInt())).thenReturn(true);
    }

    @After
    public void tearDown() {
    }

    @PrepareForTest(value = {KListTableImpl.class, KVSerializerUtil.class, DirectBuffer.class})
    @Test
    public void test_put_normal() {
        // setup
        String key = "testKey";
        List<String> value = new ArrayList<>();
        value.add("testValue");

        // run the test
        try {
            // mock native method
            PowerMockito.doNothing()
                .when(KListTableImpl.class, "put", anyLong(), anyInt(), anyLong(), anyInt(), anyLong(), anyInt());

            kListTable.put(key, value);
        } catch (Exception e) {
            fail("Should not have thrown any exception");
        }
    }

    @PrepareForTest(value = {KListTableImpl.class, KVSerializerUtil.class, DirectBuffer.class})
    @Test(expected = BSSRuntimeException.class)
    public void test_put_abnormal() throws Exception {
        // setup
        String key = "testKey";
        List<String> value = new ArrayList<>();
        value.add("testValue");

        // run the test
        PowerMockito.mockStatic(KVSerializerUtil.class);
        PowerMockito.when(KVSerializerUtil.serList(any(), any())).thenThrow(new IOException());

        kListTable.put(key, value);
    }

    @PrepareForTest(value = {KListTableImpl.class, KVSerializerUtil.class, DirectBuffer.class})
    @Test
    public void test_get_when_key_exist() {
        // setup
        String key = "testKey";
        String val = "testValue";
        List<String> value = new ArrayList<>();
        value.add(val);

        // run the test
        try {
            DirectBuffer valueBytes = KVSerializerUtil.serList(value, kListTable.elementSerializer).getCopy();

            // mock native method
            PowerMockito.mockStatic(KListTableImpl.class);
            PowerMockito.doAnswer(invocation -> {
                // Get the arguments passed to the method
                Object[] args = invocation.getArguments();
                if (args[4] instanceof StateListResult) {
                    StateListResult slc = (StateListResult) args[4];
                    // Simulate what the native code would do
                    slc.setResId(0);
                    slc.setSize(1);
                    long[] address = new long[1];
                    address[0] = valueBytes.data();
                    int[] length = new int[1];
                    length[0] = valueBytes.length();
                    slc.setAddresses(address);
                    slc.setLengths(length);
                }

                return null; // Must return null for void methods
            }).when(KListTableImpl.class);
            kListTable.get(anyLong(), anyInt(), anyLong(), anyInt(), any(StateListResult.class));
            List<DirectBuffer> buffers = new ArrayList<>();
            buffers.add(valueBytes);
            List<String> result = kListTable.get(key);
            Assert.assertEquals(1, result.size());
            Assert.assertEquals(value.get(0), result.get(0));
        } catch (Exception e) {
            e.printStackTrace();
            fail("Should not have thrown any exception: " + e.getMessage());
        }
    }

    @PrepareForTest(value = {KListTableImpl.class, KVSerializerUtil.class, DirectBuffer.class})
    @Test
    public void test_get_when_key_not_exist() {
        // setup
        String key = "testKey";
        String defVal = "default";
        List<String> defaultValue = new ArrayList<>();
        defaultValue.add(defVal);

        // run the test
        try {
            // mock native method
            PowerMockito.mockStatic(KListTableImpl.class);
            PowerMockito.doNothing().when(KListTableImpl.class);
            kListTable.get(anyLong(), anyInt(), anyLong(), anyInt(), any());
            List<String> result = kListTable.getOrDefault(key, defaultValue);
            Assert.assertEquals(result.get(0), defaultValue.get(0));
        } catch (Exception e) {
            fail("Should not have thrown any exception: " + e.getMessage());
        }
    }

    @PrepareForTest(value = {KListTableImpl.class, KVSerializerUtil.class, DirectBuffer.class})
    @Test
    public void test_get_when_key_filtered() {
        // setup
        String key = "testKey";
        String defVal = "default";
        List<String> defaultValue = new ArrayList<>();
        defaultValue.add(defVal);
        BoostStateDB db = PowerMockito.mock(BoostStateDB.class);
        BoostConfig config = PowerMockito.mock(BoostConfig.class);
        PowerMockito.when(config.isEnableBloomFilter()).thenReturn(true);
        PowerMockito.when(config.getExpectedKeyCount()).thenReturn(10000);
        PowerMockito.when(db.getConfig()).thenReturn(config);
        kListTable = PowerMockito.spy(new KListTableImpl<>(db, kListTableDescription));
        PowerMockito.when(kListTable.testKeyHash(anyInt())).thenReturn(false);

        // run the test
        try {
            List<String> result = kListTable.getOrDefault(key, defaultValue);
            Assert.assertEquals(result.get(0), defaultValue.get(0));
        } catch (Exception e) {
            fail("Should not have thrown any exception: " + e.getMessage());
        }
    }

    @PrepareForTest(value = {KListTableImpl.class, KVSerializerUtil.class, DirectBuffer.class})
    @Test(expected = BSSRuntimeException.class)
    public void test_get_abnormal() throws IOException {
        // setup
        String key = "testKey";

        // run the test
        PowerMockito.mockStatic(KVSerializerUtil.class);
        PowerMockito.when(KVSerializerUtil.serKey(any(), any())).thenThrow(new IOException());
        kListTable.get(key);
    }

    @PrepareForTest(value = {KListTableImpl.class, KVSerializerUtil.class, DirectBuffer.class})
    @Test
    public void test_remove_normal() {
        // setup
        String key = "testKey";

        // run the test
        try {
            // mock native method
            PowerMockito.doNothing().when(KListTableImpl.class, "remove", anyLong(), anyInt(), anyLong(), anyInt());
            kListTable.remove(key);
        } catch (Exception e) {
            fail("Should not have thrown any exception: " + e.getMessage());
        }
    }

    @PrepareForTest(value = {KListTableImpl.class, KVSerializerUtil.class, DirectBuffer.class})
    @Test(expected = BSSRuntimeException.class)
    public void test_remove_abnormal() throws IOException {
        // setup
        String key = "testKey";

        // run the test
        PowerMockito.mockStatic(KVSerializerUtil.class);
        PowerMockito.when(KVSerializerUtil.serKey(any(), any())).thenThrow(new IOException());
        kListTable.remove(key);
    }

    @PrepareForTest(value = {KListTableImpl.class, KVSerializerUtil.class, DirectBuffer.class})
    @Test
    public void test_contains_normal() {
        // setup
        String key = "testKey";

        // run the test
        try {
            // mock native method
            PowerMockito.when(kListTable.contains(anyLong(), anyInt(), anyLong(), anyInt())).thenReturn(true);
            Assert.assertTrue(kListTable.contains(key));
        } catch (Exception e) {
            fail("Should not have thrown any exception: " + e.getMessage());
        }
    }

    @PrepareForTest(value = {KListTableImpl.class, KVSerializerUtil.class, DirectBuffer.class})
    @Test(expected = BSSRuntimeException.class)
    public void test_contains_abnormal() throws IOException {
        // setup
        String key = "testKey";

        // run the test
        PowerMockito.mockStatic(KVSerializerUtil.class);
        PowerMockito.when(KVSerializerUtil.serKey(any(), any())).thenThrow(new IOException());
        kListTable.contains(key);
    }

    @PrepareForTest(value = {KListTableImpl.class, KVSerializerUtil.class, DirectBuffer.class})
    @Test
    public void test_contains_key_filtered() throws IOException {
        // setup
        String key = "testKey";
        BoostStateDB db = PowerMockito.mock(BoostStateDB.class);
        BoostConfig config = PowerMockito.mock(BoostConfig.class);
        PowerMockito.when(config.isEnableBloomFilter()).thenReturn(true);
        PowerMockito.when(config.getExpectedKeyCount()).thenReturn(10000);
        PowerMockito.when(db.getConfig()).thenReturn(config);
        kListTable = PowerMockito.spy(new KListTableImpl<>(db, kListTableDescription));
        PowerMockito.when(kListTable.testKeyHash(anyInt())).thenReturn(false);

        // run the test
        try {
            boolean result = kListTable.contains(key);
            Assert.assertFalse(result);
        } catch (Exception e) {
            e.printStackTrace();
            fail("Should not have thrown any exception: " + e.getMessage());
        }
    }

    @PrepareForTest(value = {KListTableImpl.class, KVSerializerUtil.class, DirectBuffer.class})
    @Test
    public void test_add_normal() {
        // setup
        String key = "testKey";
        String value = "testValue";

        // run the test
        try {
            // mock native method
            PowerMockito.doNothing()
                .when(KListTableImpl.class, "add", anyLong(), anyInt(), anyLong(), anyInt(), anyLong(), anyInt());
            kListTable.add(key, value);
        } catch (Exception e) {
            fail("Should not have thrown any exception");
        }
    }

    @PrepareForTest(value = {KListTableImpl.class, KVSerializerUtil.class, DirectBuffer.class})
    @Test(expected = BSSRuntimeException.class)
    public void test_add_abnormal() throws IOException {
        // setup
        String key = "testKey";
        String value = "testValue";

        // run the test
        PowerMockito.mockStatic(KVSerializerUtil.class);
        PowerMockito.when(KVSerializerUtil.serKey(any(), any())).thenThrow(new IOException());
        kListTable.add(key, value);
    }

    @PrepareForTest(value = {KListTableImpl.class, KVSerializerUtil.class, DirectBuffer.class})
    @Test
    public void test_add_all_normal() {
        // setup
        String key = "testKey";
        List<String> elements = new ArrayList<>();
        elements.add("testValue1");
        elements.add("testValue2");

        // run the test
        try {
            // mock native method
            PowerMockito.doNothing()
                .when(KListTableImpl.class, "addAll", anyLong(), anyInt(), anyLong(), anyInt(), anyLong(), anyInt());
            kListTable.addAll(key, elements);
        } catch (Exception e) {
            fail("Should not have thrown any exception");
        }
    }

    @PrepareForTest(value = {KListTableImpl.class, KVSerializerUtil.class, DirectBuffer.class})
    @Test(expected = BSSRuntimeException.class)
    public void test_add_all_abnormal() throws IOException {
        // setup
        String key = "testKey";
        List<String> elements = new ArrayList<>();
        elements.add("testValue1");
        elements.add("testValue2");

        // run the test
        PowerMockito.mockStatic(KVSerializerUtil.class);
        PowerMockito.when(KVSerializerUtil.serKey(any(), any())).thenThrow(new IOException());
        kListTable.addAll(key, elements);
    }
}
