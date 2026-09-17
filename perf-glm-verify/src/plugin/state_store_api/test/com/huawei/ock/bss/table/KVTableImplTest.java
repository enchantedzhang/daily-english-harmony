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
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.Mockito.when;

import com.huawei.ock.bss.common.BoostStateDB;
import com.huawei.ock.bss.common.BoostStateType;
import com.huawei.ock.bss.common.conf.BoostConfig;
import com.huawei.ock.bss.common.conf.TableConfig;
import com.huawei.ock.bss.common.exception.BSSRuntimeException;
import com.huawei.ock.bss.common.memory.DirectBuffer;
import com.huawei.ock.bss.common.memory.DirectDataOutputSerializer;
import com.huawei.ock.bss.common.serialize.KVSerializerUtil;
import com.huawei.ock.bss.common.serialize.TableSerializer;
import com.huawei.ock.bss.jni.AbstractNativeHandleReference;
import com.huawei.ock.bss.table.description.KVTableDescription;
import com.huawei.ock.bss.table.iterator.TableEntryIterator;
import com.huawei.ock.bss.table.iterator.TableKeyIterator;
import com.huawei.ock.bss.table.result.EntryResult;

import org.apache.flink.api.common.typeutils.TypeSerializer;
import org.apache.flink.api.common.typeutils.base.StringSerializer;
import org.apache.flink.util.CloseableIterator;
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
import java.util.AbstractMap;
import java.util.ArrayList;
import java.util.Iterator;
import java.util.List;
import java.util.Map;
import java.util.stream.Stream;

/**
 * KVTableImplTest
 *
 * @since BeiMing 25.0.T1
 */
@RunWith(PowerMockRunner.class)
public class KVTableImplTest {
    private KVTableImpl<String, String> kvTable;

    private KVTableDescription<String, String> description;

    @Before
    public void setUp() {
        PowerMockito.mockStatic(KVTableImpl.class);
        PowerMockito.mockStatic(AbstractTable.class);
        PowerMockito.when(AbstractTable.open(anyLong(), Mockito.nullable(String.class), any())).thenReturn(1L);
        PowerMockito.mockStatic(TableEntryIterator.class);
        PowerMockito.when(TableEntryIterator.open(anyLong())).thenReturn(1L);
        PowerMockito.when(TableEntryIterator.close(anyLong())).thenReturn(true);
        PowerMockito.mockStatic(TableKeyIterator.class);
        PowerMockito.when(TableKeyIterator.open(anyLong())).thenReturn(1L);
        PowerMockito.when(TableKeyIterator.close(anyLong())).thenReturn(true);
        PowerMockito.mockStatic(AbstractNativeHandleReference.class);
        PowerMockito.when(AbstractNativeHandleReference.close(anyLong())).thenReturn(true);
        PowerMockito.mockStatic(DirectBuffer.class);
        PowerMockito.when(DirectBuffer.nativeAcquireDirectBuffer(anyLong(), anyInt())).thenReturn(1L);
        BoostStateDB db = PowerMockito.mock(BoostStateDB.class);
        BoostConfig config = PowerMockito.mock(BoostConfig.class);
        PowerMockito.when(config.isEnableBloomFilter()).thenReturn(true);
        PowerMockito.when(config.getExpectedKeyCount()).thenReturn(10000);
        PowerMockito.when(db.getConfig()).thenReturn(config);
        PowerMockito.when(db.getNativeHandle()).thenReturn(1L);
        TypeSerializer<String> keySerializer = new StringSerializer();
        TypeSerializer<String> valueSerializer = new StringSerializer();
        TableSerializer<String, String> tableSerializer = new TableSerializer<>(keySerializer, valueSerializer);
        description = new KVTableDescription<>("testTable", 10, tableSerializer);
        Assert.assertEquals(description.getStateType(), BoostStateType.VALUE);
        kvTable = PowerMockito.spy(new KVTableImpl<>(db, description));
        PowerMockito.when(kvTable.testKeyHash(anyInt())).thenReturn(true);
        PowerMockito.when(kvTable.testKeyHash(anyInt(), anyInt())).thenReturn(true);
    }

    @After
    public void tearDown() {
    }

    @Test
    @PrepareForTest(value = {
        KVTableImpl.class, KVSerializerUtil.class, TableEntryIterator.class, TableKeyIterator.class, DirectBuffer.class
    })
    public void test_update_config_normal() {
        TableConfig.Builder builder = new TableConfig.Builder();
        builder.setTableTtl(1).setIsEnableKVSeparate(true);

        try {
            PowerMockito.doNothing()
                    .when(AbstractTable.class, "updateTtl", anyLong(), anyString(), any());

            kvTable.updateTableConfig(builder.build());
        } catch (Exception e) {
            fail("Should not have thrown any exception: " + e.getMessage());
        }
    }

    @Test
    @PrepareForTest(value = {
        KVTableImpl.class, KVSerializerUtil.class, TableEntryIterator.class, TableKeyIterator.class, DirectBuffer.class
    })
    public void test_put_normal() {
        // setup
        String key = "testKey";
        String value = "testValue";

        // run the test
        try {
            // mock native method
            PowerMockito.doNothing()
                .when(KVTableImpl.class, "put", anyLong(), anyInt(), anyLong(), anyInt(), anyLong(), anyInt());

            kvTable.put(key, value);
        } catch (Exception e) {
            fail("Should not have thrown any exception");
        }
    }

    @Test(expected = BSSRuntimeException.class)
    @PrepareForTest(value = {
        KVTableImpl.class, KVSerializerUtil.class, TableEntryIterator.class, TableKeyIterator.class, DirectBuffer.class
    })
    public void test_put_abnormal() throws Exception {
        // setup
        String key = "testKey";
        String value = "testValue";

        // run the test
        PowerMockito.mockStatic(KVSerializerUtil.class);
        PowerMockito.when(KVSerializerUtil.serValue(any(), any())).thenThrow(new IOException());
        kvTable.put(key, value);
    }

    @Test
    @PrepareForTest(value = {
        KVTableImpl.class, KVSerializerUtil.class, TableEntryIterator.class, TableKeyIterator.class, DirectBuffer.class
    })
    @SuppressWarnings("unchecked")
    public void test_get_normal() {
        // setup
        String key = "testKey";
        String value = "testValue";
        DirectDataOutputSerializer outputView = new DirectDataOutputSerializer(16);

        // run the test
        try {
            kvTable.getTableDescription().getTableSerializer().getValueSerializer().serialize(value, outputView);
            DirectBuffer valueBytes = outputView.wrapDirectData();
            // mock native method
            PowerMockito.when(kvTable.get(anyLong(), anyInt(), anyLong(), anyInt()))
                .thenReturn(valueBytes.data());
            PowerMockito.when(DirectBuffer.acquireDirectBuffer(anyLong())).thenReturn(valueBytes);
            String result = kvTable.get(key);
            Assert.assertEquals(result, value);
        } catch (Exception e) {
            e.printStackTrace();
            fail("Should not have thrown any exception: " + e.getMessage());
        }
    }

    @Test
    @PrepareForTest(value = {
        KVTableImpl.class, KVSerializerUtil.class, TableEntryIterator.class, TableKeyIterator.class, DirectBuffer.class
    })
    public void test_get_key_filtered() {
        String key = "testKey";
        BoostStateDB db = PowerMockito.mock(BoostStateDB.class);
        BoostConfig config = PowerMockito.mock(BoostConfig.class);
        PowerMockito.when(config.isEnableBloomFilter()).thenReturn(true);
        PowerMockito.when(config.getExpectedKeyCount()).thenReturn(10000);
        PowerMockito.when(db.getConfig()).thenReturn(config);
        kvTable = PowerMockito.spy(new KVTableImpl<>(db, description));
        PowerMockito.when(kvTable.testKeyHash(anyInt())).thenReturn(false);

        // run the test
        try {
            String result = kvTable.get(key);
            Assert.assertNull(result);
        } catch (Exception e) {
            e.printStackTrace();
            fail("Should not have thrown any exception: " + e.getMessage());
        }
    }

    @Test
    @PrepareForTest(value = {
        KVTableImpl.class, KVSerializerUtil.class, TableEntryIterator.class, TableKeyIterator.class, DirectBuffer.class
    })
    public void test_contains_key_filtered() {
        String key = "testKey";
        BoostStateDB db = PowerMockito.mock(BoostStateDB.class);
        BoostConfig config = PowerMockito.mock(BoostConfig.class);
        PowerMockito.when(config.isEnableBloomFilter()).thenReturn(true);
        PowerMockito.when(config.getExpectedKeyCount()).thenReturn(10000);
        PowerMockito.when(db.getConfig()).thenReturn(config);
        kvTable = PowerMockito.spy(new KVTableImpl<>(db, description));
        PowerMockito.when(kvTable.testKeyHash(anyInt())).thenReturn(false);

        // run the test
        try {
            boolean result = kvTable.contains(key);
            Assert.assertFalse(result);
        } catch (Exception e) {
            e.printStackTrace();
            fail("Should not have thrown any exception: " + e.getMessage());
        }
    }

    @Test
    @PrepareForTest(value = {
        KVTableImpl.class, KVSerializerUtil.class, TableEntryIterator.class, TableKeyIterator.class, DirectBuffer.class
    })
    public void test_get_when_key_not_exist() {
        // setup
        String key = "testKey";

        // mock native method
        PowerMockito.when(kvTable.get(anyLong(), anyInt(), anyLong(), anyInt())).thenReturn(0L);

        // run the test
        try {
            String result = kvTable.get(key);
            Assert.assertNull(result);
        } catch (Exception e) {
            fail("Should not have thrown any exception");
        }
    }

    @Test(expected = BSSRuntimeException.class)
    @PrepareForTest(value = {
        KVTableImpl.class, KVSerializerUtil.class, TableEntryIterator.class, TableKeyIterator.class, DirectBuffer.class
    })
    public void test_get_abnormal() throws IOException {
        // setup
        String key = "testKey";

        // run the test
        PowerMockito.mockStatic(KVSerializerUtil.class);
        PowerMockito.when(KVSerializerUtil.serKey(any(), any())).thenThrow(new IOException());
        kvTable.get(key);
    }

    @Test
    @PrepareForTest(value = {
        KVTableImpl.class, KVSerializerUtil.class, TableEntryIterator.class, TableKeyIterator.class, DirectBuffer.class
    })
    public void test_contains_return_exist() {
        // setup
        String key = "testKey";

        // run the test
        try {
            // mock native method
            PowerMockito.when(kvTable.contains(anyLong(), anyInt(), anyLong(), anyInt())).thenReturn(true);
            boolean result = kvTable.contains(key);
            Assert.assertTrue(result);
        } catch (Exception e) {
            fail("Should not have thrown any exception: " + e.getMessage());
        }
    }

    @Test
    @PrepareForTest(value = {
        KVTableImpl.class, KVSerializerUtil.class, TableEntryIterator.class, TableKeyIterator.class, DirectBuffer.class
    })
    public void test_contains_not_exist() {
        // setup
        String key = "testKey";

        // run the test
        try {
            // mock native method
            PowerMockito.when(kvTable.contains(anyLong(), anyInt(), anyLong(), anyInt())).thenReturn(false);
            boolean result = kvTable.contains(key);
            Assert.assertFalse(result);
        } catch (Exception e) {
            fail("Should not have thrown any exception: " + e.getMessage());
        }
    }

    @Test(expected = BSSRuntimeException.class)
    @PrepareForTest(value = {
        KVTableImpl.class, KVSerializerUtil.class, TableEntryIterator.class, TableKeyIterator.class, DirectBuffer.class
    })
    public void test_contains_abnormal() throws IOException {
        // setup
        String key = "testKey";

        // run the test
        PowerMockito.mockStatic(KVSerializerUtil.class);
        PowerMockito.when(KVSerializerUtil.serKey(any(), any())).thenThrow(new IOException());
        kvTable.contains(key);
    }

    @Test
    @PrepareForTest(value = {
        KVTableImpl.class, KVSerializerUtil.class, TableEntryIterator.class, TableKeyIterator.class, DirectBuffer.class
    })
    public void test_remove_normal() {
        // setup
        String key = "testKey";

        // run the test
        try {
            // mock native method
            PowerMockito.doNothing().when(KVTableImpl.class, "remove", anyLong(), anyInt(), anyLong(), anyInt());
            kvTable.remove(key);
        } catch (Exception e) {
            fail("Should not have thrown any exception: " + e.getMessage());
        }
    }

    @Test(expected = BSSRuntimeException.class)
    @PrepareForTest(value = {
        KVTableImpl.class, KVSerializerUtil.class, TableEntryIterator.class, TableKeyIterator.class, DirectBuffer.class
    })
    public void test_remove_abnormal() throws IOException {
        // setup
        String key = "testKey";

        // run the test
        PowerMockito.mockStatic(KVSerializerUtil.class);
        PowerMockito.when(KVSerializerUtil.serKey(any(), any())).thenThrow(new IOException());
        kvTable.remove(key);
    }

    @PrepareForTest(value = {
        KVTableImpl.class, KVSerializerUtil.class, TableEntryIterator.class, TableKeyIterator.class, DirectBuffer.class
    })
    @Test
    public void test_iterator_normal() throws Exception {
        String key1 = "testKey1";
        String value1 = "testValue1";
        Map.Entry<String, String> entry1 = new AbstractMap.SimpleEntry<>(key1, value1);
        String key2 = "testKey2";
        String value2 = "testValue2";
        Map.Entry<String, String> entry2 = new AbstractMap.SimpleEntry<>(key2, value2);
        CloseableIterator<Map.Entry<String, String>> iterator = null;
        try {
            // mock native method
            DirectBuffer k1Buffer = KVSerializerUtil.serKey(key1, kvTable.keySerializer).wrapDirectData();
            DirectBuffer k2Buffer = KVSerializerUtil.serSubKey(key2, kvTable.keySerializer).wrapDirectData();
            DirectBuffer v1Buffer = KVSerializerUtil.serValue(value1, kvTable.valueSerializer).wrapDirectData();
            DirectBuffer v2Buffer = KVSerializerUtil.serValue(value2, kvTable.valueSerializer).wrapDirectData();
            List<DirectBuffer> e1BufferList = new ArrayList<>();
            e1BufferList.add(k1Buffer);
            e1BufferList.add(v1Buffer);
            List<DirectBuffer> e2BufferList = new ArrayList<>();
            e2BufferList.add(k2Buffer);
            e2BufferList.add(v2Buffer);

            PowerMockito.when(TableEntryIterator.hasNext(anyLong())).thenReturn(true, true, false);
            when(TableEntryIterator.next(anyLong(), any(EntryResult.class)))
                    .thenAnswer(invocation -> {
                        // Get the arguments passed to the method
                        Object[] args = invocation.getArguments();
                        if (args[1] instanceof EntryResult) {
                            EntryResult slc = (EntryResult) args[1];
                            slc.setKeyAddr(k1Buffer.data());
                            slc.setKeyLen(k1Buffer.length());
                            slc.setValueAddr(v1Buffer.data());
                            slc.setValueLen(v1Buffer.length());
                            Assert.assertNotNull(slc.toString());
                            return slc;
                        }

                        return null; // Must return null for void methods
                    }).thenAnswer(invocation -> {
                        // Get the arguments passed to the method
                        Object[] args = invocation.getArguments();
                        if (args[1] instanceof EntryResult) {
                            EntryResult slc = (EntryResult) args[1];
                            slc.setKeyAddr(k2Buffer.data());
                            slc.setKeyLen(k2Buffer.length());
                            slc.setValueAddr(v2Buffer.data());
                            slc.setValueLen(v2Buffer.length());
                            return slc;
                        }
                        return null; // Must return null for void methods
                    });

            iterator = kvTable.iterator();
            for (int i = 0; i < 2; i++) {
                Assert.assertTrue(iterator.hasNext());
                Map.Entry<String, String> entry = iterator.next();
                Assert.assertTrue(entry.equals(entry1) || entry.equals(entry2));
            }
        } catch (Exception e) {
            e.printStackTrace();
            fail("Should not have thrown any exception: " + e.getMessage());
        } finally {
            if (iterator != null) {
                iterator.close();
            }
        }
    }

    @Test
    @PrepareForTest(value = {
        KVTableImpl.class, KVSerializerUtil.class, TableEntryIterator.class, TableKeyIterator.class, DirectBuffer.class
    })
    public void test_get_keys() {
        String key1 = "testKey1";
        String key2 = "testKey2";
        try {
            // mock native method
            DirectBuffer key1Bytes = KVSerializerUtil.serKey(key1, kvTable.keySerializer).wrapDirectData();
            DirectBuffer key2Bytes = KVSerializerUtil.serKey(key2, kvTable.keySerializer).wrapDirectData();
            PowerMockito.when(TableKeyIterator.hasNext(anyLong())).thenReturn(true, true, false);
            when(TableKeyIterator.next(anyLong(), any(EntryResult.class)))
                    .thenAnswer(invocation -> {
                        Object[] args = invocation.getArguments();
                        if (args[1] instanceof EntryResult) {
                            EntryResult slc = (EntryResult) args[1];
                            slc.setKeyAddr(key1Bytes.data());
                            slc.setKeyLen(key1Bytes.length());
                            return slc;
                        }
                        return null;
                    }).thenAnswer(invocation -> {
                        Object[] args = invocation.getArguments();
                        if (args[1] instanceof EntryResult) {
                            EntryResult slc = (EntryResult) args[1];
                            slc.setKeyAddr(key2Bytes.data());
                            slc.setKeyLen(key2Bytes.length());
                            return slc;
                        }
                        return null;
                    });
            PowerMockito.when(DirectBuffer.acquireDirectBuffer(anyLong())).thenReturn(key1Bytes, key2Bytes);

            Stream<String> keyStream = kvTable.getKeys("testNamespace");
            Iterator<String> iterator = keyStream.iterator();

            for (int i = 0; i < 2; i++) {
                Assert.assertTrue(iterator.hasNext());
                String key = iterator.next();
                Assert.assertTrue(key.equals(key1) || key.equals(key2));
            }
        } catch (Exception e) {
            e.printStackTrace();
            fail("Should not have thrown any exception: " + e.getMessage());
        }
    }

    @Test
    @PrepareForTest(value = {
        KVTableImpl.class, KVSerializerUtil.class, TableEntryIterator.class, TableKeyIterator.class, DirectBuffer.class
    })
    public void test_close_tableKeyIterator() {
        try {
            TableKeyIterator iterator = new TableKeyIterator<>(1, kvTable.keySerializer);
            iterator.close();
        } catch (Exception e) {
            e.printStackTrace();
        }
    }
}