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
import static org.mockito.Mockito.when;

import com.huawei.ock.bss.common.BoostStateDB;
import com.huawei.ock.bss.common.BoostStateType;
import com.huawei.ock.bss.common.conf.BoostConfig;
import com.huawei.ock.bss.common.exception.BSSRuntimeException;
import com.huawei.ock.bss.common.memory.DirectBuffer;
import com.huawei.ock.bss.common.memory.DirectDataOutputSerializer;
import com.huawei.ock.bss.common.serialize.KVSerializerUtil;
import com.huawei.ock.bss.common.serialize.SubTableSerializer;
import com.huawei.ock.bss.jni.AbstractNativeHandleReference;
import com.huawei.ock.bss.table.description.KMapTableDescription;
import com.huawei.ock.bss.table.iterator.SubTableEntryIterator;
import com.huawei.ock.bss.table.result.EntryResult;

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
import java.util.Map;

/**
 * KVTableImplTest
 *
 * @since BeiMing 25.0.T1
 */
@RunWith(PowerMockRunner.class)
@PrepareForTest(value = {
    KMapTableImpl.class, AbstractKMapTable.class, KVSerializerUtil.class, SubTableEntryIterator.class,
    DirectBuffer.class, AbstractTable.class
})
public class KMapTableImplTest {
    private KMapTableImpl<String, String, String> kMapTable;

    private KMapTableDescription<String, String, String> kMapTableDescription;

    @Before
    public void setUp() {
        PowerMockito.mockStatic(KMapTableImpl.class);
        PowerMockito.mockStatic(AbstractKMapTable.class);
        PowerMockito.mockStatic(AbstractTable.class);
        PowerMockito.when(AbstractTable.open(anyLong(), Mockito.nullable(String.class), any())).thenReturn(1L);
        PowerMockito.mockStatic(SubTableEntryIterator.class);
        PowerMockito.when(SubTableEntryIterator.open(anyLong(), anyInt(), anyLong(), anyInt())).thenReturn(1L);
        PowerMockito.mockStatic(AbstractNativeHandleReference.class);
        PowerMockito.when(AbstractNativeHandleReference.close(anyLong())).thenReturn(true);
        PowerMockito.mockStatic(DirectBuffer.class);
        PowerMockito.when(DirectBuffer.nativeAcquireDirectBuffer(anyLong(), anyInt())).thenReturn(1L);
        BoostConfig config = PowerMockito.mock(BoostConfig.class);
        BoostStateDB db = PowerMockito.mock(BoostStateDB.class);
        PowerMockito.when(config.isEnableBloomFilter()).thenReturn(true);
        PowerMockito.when(config.getExpectedKeyCount()).thenReturn(10000);
        PowerMockito.when(db.getConfig()).thenReturn(config);
        PowerMockito.when(db.getNativeHandle()).thenReturn(1L);
        TypeSerializer<String> keySerializer = new StringSerializer();
        TypeSerializer<String> subKeySerializer = new StringSerializer();
        TypeSerializer<String> valueSerializer = new StringSerializer();
        SubTableSerializer<String, String, String> subTableSerializer = new SubTableSerializer<>(keySerializer,
            subKeySerializer, valueSerializer);
        kMapTableDescription = new KMapTableDescription<>("testTable", 10, subTableSerializer);
        Assert.assertEquals(kMapTableDescription.getStateType(), BoostStateType.MAP);
        kMapTable = PowerMockito.spy(new KMapTableImpl<>(db, kMapTableDescription));
        PowerMockito.when(kMapTable.testKeyHash(anyInt())).thenReturn(true);
        PowerMockito.when(kMapTable.testKeyHash(anyInt(), anyInt())).thenReturn(true);
    }

    @After
    public void tearDown() {
    }

    @PrepareForTest(value = {
        KMapTableImpl.class, AbstractKMapTable.class, KVSerializerUtil.class, SubTableEntryIterator.class,
        DirectBuffer.class
    })
    @Test
    public void test_contains_key_when_exist() {
        String key = "testKey";

        // mock native method
        PowerMockito.when(kMapTable.contains(anyLong(), anyInt(), anyLong(), anyInt())).thenReturn(true);
        boolean result = kMapTable.contains(key);
        Assert.assertTrue(result);
    }

    @PrepareForTest(value = {
        KMapTableImpl.class, AbstractKMapTable.class, KVSerializerUtil.class, SubTableEntryIterator.class,
        DirectBuffer.class
    })
    @Test
    public void test_contains_key_when_not_exist() {
        String key = "testKey";

        // mock native method
        PowerMockito.when(kMapTable.contains(anyLong(), anyInt(), anyLong(), anyInt())).thenReturn(false);
        boolean result = kMapTable.contains(key);
        Assert.assertFalse(result);
    }

    @PrepareForTest(value = {
        KMapTableImpl.class, AbstractKMapTable.class, KVSerializerUtil.class, SubTableEntryIterator.class,
        DirectBuffer.class
    })
    @Test
    public void test_contains_key_filtered() {
        String key = "testKey";
        BoostStateDB db = PowerMockito.mock(BoostStateDB.class);
        BoostConfig config = PowerMockito.mock(BoostConfig.class);
        PowerMockito.when(config.isEnableBloomFilter()).thenReturn(true);
        PowerMockito.when(config.getExpectedKeyCount()).thenReturn(10000);
        PowerMockito.when(db.getConfig()).thenReturn(config);
        kMapTable = PowerMockito.spy(new KMapTableImpl<>(db, kMapTableDescription));
        PowerMockito.when(kMapTable.testKeyHash(anyInt())).thenReturn(false);

        // mock native method
        boolean result = kMapTable.contains(key);
        Assert.assertFalse(result);
    }

    @PrepareForTest(value = {
        KMapTableImpl.class, AbstractKMapTable.class, KVSerializerUtil.class, SubTableEntryIterator.class,
        DirectBuffer.class
    })
    @Test(expected = BSSRuntimeException.class)
    public void test_contains_key_abnormal() throws IOException {
        // setup
        String key = "testKey";

        // run the test
        PowerMockito.mockStatic(KVSerializerUtil.class);
        PowerMockito.when(KVSerializerUtil.serKey(any(), any())).thenThrow(new IOException());
        kMapTable.contains(key);
    }

    @PrepareForTest(value = {
        KMapTableImpl.class, AbstractKMapTable.class, KVSerializerUtil.class, SubTableEntryIterator.class,
        DirectBuffer.class
    })
    @Test
    public void test_contains_sub_key_when_exist() {
        String key = "testKey";
        String subKey = "testSubKey";

        // mock native method
        PowerMockito.when(kMapTable.contains(anyLong(), anyInt(), anyLong(), anyInt(), anyLong(), anyInt()))
            .thenReturn(true);
        boolean result = kMapTable.contains(key, subKey);
        Assert.assertTrue(result);
    }

    @PrepareForTest(value = {
        KMapTableImpl.class, AbstractKMapTable.class, KVSerializerUtil.class, SubTableEntryIterator.class,
        DirectBuffer.class
    })
    @Test
    public void test_contains_sub_key_when_not_exist() {
        String key = "testKey";
        String subKey = "testSubKey";

        // mock native method
        PowerMockito.when(kMapTable.contains(anyLong(), anyInt(), anyLong(), anyInt(), anyLong(), anyInt()))
            .thenReturn(false);
        boolean result = kMapTable.contains(key, subKey);
        Assert.assertFalse(result);
    }

    @PrepareForTest(value = {
        KMapTableImpl.class, AbstractKMapTable.class, KVSerializerUtil.class, SubTableEntryIterator.class,
        DirectBuffer.class
    })
    @Test
    public void test_contains_sub_key_filtered() {
        String key = "testKey";
        String subKey = "testSubKey";
        BoostStateDB db = PowerMockito.mock(BoostStateDB.class);
        BoostConfig config = PowerMockito.mock(BoostConfig.class);
        PowerMockito.when(config.isEnableBloomFilter()).thenReturn(true);
        PowerMockito.when(config.getExpectedKeyCount()).thenReturn(10000);
        PowerMockito.when(db.getConfig()).thenReturn(config);
        kMapTable = PowerMockito.spy(new KMapTableImpl<>(db, kMapTableDescription));
        PowerMockito.when(kMapTable.testKeyHash(anyInt(), anyInt())).thenReturn(false);

        boolean result = kMapTable.contains(key, subKey);
        Assert.assertFalse(result);
    }

    @PrepareForTest(value = {
        KMapTableImpl.class, AbstractKMapTable.class, KVSerializerUtil.class, SubTableEntryIterator.class,
        DirectBuffer.class
    })
    @Test(expected = BSSRuntimeException.class)
    public void test_contains_sub_key_abnormal() throws IOException {
        // setup
        String key = "testKey";
        String subKey = "testSubKey";

        // run the test
        PowerMockito.mockStatic(KVSerializerUtil.class);
        PowerMockito.when(KVSerializerUtil.serKey(any(), any())).thenThrow(new IOException());
        kMapTable.contains(key, subKey);
    }

    @PrepareForTest(value = {
        KMapTableImpl.class, AbstractKMapTable.class, KVSerializerUtil.class, SubTableEntryIterator.class,
        DirectBuffer.class
    })
    @Test
    @SuppressWarnings("unchecked")
    public void test_get_sub_key_when_exist() {
        String key = "testKey";
        String subKey = "testSubKey";
        String value = "testValue";
        DirectDataOutputSerializer outputView = new DirectDataOutputSerializer(16);

        try {
            kMapTable.getTableDescription().getTableSerializer().getValueSerializer().serialize(value, outputView);
            DirectBuffer valueBytes = outputView.wrapDirectData();

            // mock native method
            PowerMockito.when(kMapTable.get(anyLong(), anyInt(), anyLong(), anyInt(), anyLong(), anyInt()))
                .thenReturn(valueBytes.data());
            PowerMockito.when(DirectBuffer.acquireDirectBuffer(anyLong())).thenReturn(valueBytes);
            String result = kMapTable.get(key, subKey);
            Assert.assertEquals(result, value);
        } catch (Exception e) {
            fail("Should not have thrown any exception: " + e.getMessage());
        }
    }

    @PrepareForTest(value = {
        KMapTableImpl.class, AbstractKMapTable.class, KVSerializerUtil.class, SubTableEntryIterator.class,
        DirectBuffer.class
    })
    @Test
    public void test_get_sub_key_when_not_exist() {
        String key = "testKey";
        String subKey = "testSubKey";
        String defaultValue = "testDefault";

        try {
            DirectDataOutputSerializer valueBuffer = KVSerializerUtil.serValue(defaultValue,
                    kMapTable.uvSerializer);
            // mock native method
            PowerMockito.when(kMapTable.get(anyLong(), anyInt(), anyLong(), anyInt(), anyLong(), anyInt()))
                .thenReturn(0L);
            String result = kMapTable.getOrDefault(key, subKey, defaultValue);
            Assert.assertEquals(defaultValue, result);
        } catch (Exception e) {
            fail("Should not have thrown any exception: " + e.getMessage());
        }
    }

    @PrepareForTest(value = {
        KMapTableImpl.class, AbstractKMapTable.class, KVSerializerUtil.class, SubTableEntryIterator.class,
        DirectBuffer.class
    })
    @Test
    public void test_get_sub_key_when_filtered() {
        String key = "testKey";
        String subKey = "testSubKey";
        String defaultValue = "testDefault";
        BoostStateDB db = PowerMockito.mock(BoostStateDB.class);
        BoostConfig config = PowerMockito.mock(BoostConfig.class);
        PowerMockito.when(config.isEnableBloomFilter()).thenReturn(true);
        PowerMockito.when(config.getExpectedKeyCount()).thenReturn(10000);
        PowerMockito.when(db.getConfig()).thenReturn(config);
        kMapTable = PowerMockito.spy(new KMapTableImpl<>(db, kMapTableDescription));
        PowerMockito.when(kMapTable.testKeyHash(anyInt(), anyInt())).thenReturn(false);

        try {
            String result = kMapTable.getOrDefault(key, subKey, defaultValue);
            Assert.assertEquals(defaultValue, result);
        } catch (Exception e) {
            fail("Should not have thrown any exception: " + e.getMessage());
        }
    }

    @PrepareForTest(value = {
        KMapTableImpl.class, AbstractKMapTable.class, KVSerializerUtil.class, SubTableEntryIterator.class,
        DirectBuffer.class
    })
    @Test(expected = BSSRuntimeException.class)
    public void test_get_sub_key_abnormal() throws IOException {
        // setup
        String key = "testKey";
        String subKey = "testSubKey";

        // run the test
        PowerMockito.mockStatic(KVSerializerUtil.class);
        PowerMockito.when(KVSerializerUtil.serKey(any(), any())).thenThrow(new IOException());
        kMapTable.get(key, subKey);
    }

    @PrepareForTest(value = {
        KMapTableImpl.class, AbstractKMapTable.class, KVSerializerUtil.class, SubTableEntryIterator.class,
        DirectBuffer.class
    })
    @Test
    public void test_get_normal() {
        String key = "testKey";
        String subKey1 = "testSubKey1";
        String subValue1 = "testSubValue1";
        String subKey2 = "testSubKey2";
        String subValue2 = "testSubValue2";

        try {
            DirectBuffer k1Buffer = KVSerializerUtil.serSubKey(subKey1, kMapTable.ukSerializer)
                    .wrapDirectData().getCopy();
            DirectBuffer k2Buffer = KVSerializerUtil.serSubKey(subKey2, kMapTable.ukSerializer)
                    .wrapDirectData().getCopy();
            DirectBuffer v1Buffer = KVSerializerUtil.serValue(subValue1, kMapTable.uvSerializer)
                    .wrapDirectData().getCopy();
            DirectBuffer v2Buffer = KVSerializerUtil.serValue(subValue2, kMapTable.uvSerializer)
                    .wrapDirectData().getCopy();
            List<DirectBuffer> e1BufferList = new ArrayList<>();
            e1BufferList.add(k1Buffer);
            e1BufferList.add(v1Buffer);
            List<DirectBuffer> e2BufferList = new ArrayList<>();
            e2BufferList.add(k2Buffer);
            e2BufferList.add(v2Buffer);
            // mock native method
            PowerMockito.when(SubTableEntryIterator.hasNext(anyLong())).thenReturn(true, true, false);
            when(SubTableEntryIterator.next(anyLong(), any(EntryResult.class)))
                    .thenAnswer(invocation -> {
                        // Get the arguments passed to the method
                        Object[] args = invocation.getArguments();
                        if (args[1] instanceof EntryResult) {
                            EntryResult slc = (EntryResult) args[1];
                            slc.setKeyAddr(k1Buffer.data());
                            slc.setKeyLen(k1Buffer.length());
                            slc.setValueAddr(v1Buffer.data());
                            slc.setValueLen(v1Buffer.length());
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

            Map<String, String> retMap = kMapTable.get(key);
            Assert.assertEquals(2, retMap.size());
            Assert.assertTrue(retMap.containsKey(subKey1) && retMap.get(subKey1).equals(subValue1));
            Assert.assertTrue(retMap.containsKey(subKey2) && retMap.get(subKey2).equals(subValue2));
        } catch (Exception e) {
            e.printStackTrace();
            fail("Should not have thrown any exception: " + e.getMessage());
        }
    }

    @PrepareForTest(value = {
        KMapTableImpl.class, AbstractKMapTable.class, KVSerializerUtil.class, SubTableEntryIterator.class,
        DirectBuffer.class
    })
    @Test
    public void test_get_return_empty() {
        String key = "testKey";

        try {
            PowerMockito.when(SubTableEntryIterator.hasNext(anyLong())).thenReturn(false);
            Assert.assertNull(kMapTable.get(key));
        } catch (Exception e) {
            fail("Should not have thrown any exception: " + e.getMessage());
        }
    }

    @PrepareForTest(value = {
        KMapTableImpl.class, AbstractKMapTable.class, KVSerializerUtil.class, SubTableEntryIterator.class,
        DirectBuffer.class
    })
    @Test
    public void test_add_normal() {
        String key = "testKey";
        String subKey = "testSubKey";
        String value = "testValue";

        try {
            // mock native method
            PowerMockito.doNothing()
                .when(AbstractKMapTable.class, "add", anyLong(), anyInt(), anyLong(), anyInt(), anyLong(), anyInt(),
                    anyLong(), anyInt());
            kMapTable.add(key, subKey, value);
        } catch (Exception e) {
            fail("Should not have thrown any exception: " + e.getMessage());
        }
    }

    @PrepareForTest(value = {
        KMapTableImpl.class, AbstractKMapTable.class, KVSerializerUtil.class, SubTableEntryIterator.class,
        DirectBuffer.class
    })
    @Test(expected = BSSRuntimeException.class)
    public void test_add_abnormal() throws IOException {
        String key = "testKey";
        String subKey = "testSubKey";
        String value = "testValue";

        PowerMockito.mockStatic(KVSerializerUtil.class);
        PowerMockito.when(KVSerializerUtil.serKey(any(), any())).thenThrow(new IOException());

        kMapTable.add(key, subKey, value);
    }

    @PrepareForTest(value = {
        KMapTableImpl.class, AbstractKMapTable.class, KVSerializerUtil.class, SubTableEntryIterator.class,
        DirectBuffer.class
    })
    @Test
    public void test_remove_key_normal() {
        String key = "testKey";

        try {
            // mock native method
            PowerMockito.doNothing()
                .when(AbstractKMapTable.class, "removeAll", anyLong(), anyInt(), anyLong(), anyInt());
            kMapTable.remove(key);
        } catch (Exception e) {
            fail("Should not have thrown any exception: " + e.getMessage());
        }
    }

    @PrepareForTest(value = {
        KMapTableImpl.class, AbstractKMapTable.class, KVSerializerUtil.class, SubTableEntryIterator.class,
        DirectBuffer.class
    })
    @Test(expected = BSSRuntimeException.class)
    public void test_remove_key_abnormal() throws IOException {
        String key = "testKey";

        PowerMockito.mockStatic(KVSerializerUtil.class);
        PowerMockito.when(KVSerializerUtil.serKey(any(), any())).thenThrow(new IOException());

        kMapTable.remove(key);
    }

    @PrepareForTest(value = {
        KMapTableImpl.class, AbstractKMapTable.class, KVSerializerUtil.class, SubTableEntryIterator.class,
        DirectBuffer.class
    })
    @Test
    public void test_remove_sub_key_normal() {
        String key = "testKey";
        String subKey = "testSubKey";

        try {
            // mock native method
            PowerMockito.doNothing()
                .when(AbstractKMapTable.class, "remove", anyLong(), anyInt(), anyLong(), anyInt(), anyLong(), anyInt());
            kMapTable.remove(key, subKey);
        } catch (Exception e) {
            fail("Should not have thrown any exception: " + e.getMessage());
        }
    }

    @PrepareForTest(value = {
        KMapTableImpl.class, AbstractKMapTable.class, KVSerializerUtil.class, SubTableEntryIterator.class,
        DirectBuffer.class
    })
    @Test(expected = BSSRuntimeException.class)
    public void test_remove_sub_key_abnormal() throws IOException {
        String key = "testKey";
        String subKey = "testSubKey";

        PowerMockito.mockStatic(KVSerializerUtil.class);
        PowerMockito.when(KVSerializerUtil.serKey(any(), any())).thenThrow(new IOException());

        kMapTable.remove(key, subKey);
    }
}