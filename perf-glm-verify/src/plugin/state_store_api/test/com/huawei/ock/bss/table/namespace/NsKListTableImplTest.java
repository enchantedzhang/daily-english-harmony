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

import static org.junit.Assert.fail;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.Mockito.when;

import com.huawei.ock.bss.common.BoostStateDB;
import com.huawei.ock.bss.common.BoostStateType;
import com.huawei.ock.bss.common.conf.BoostConfig;
import com.huawei.ock.bss.common.memory.DirectBuffer;
import com.huawei.ock.bss.common.serialize.KVSerializerUtil;
import com.huawei.ock.bss.common.serialize.KeyPairSerializer;
import com.huawei.ock.bss.common.serialize.TableSerializer;
import com.huawei.ock.bss.jni.AbstractNativeHandleReference;
import com.huawei.ock.bss.table.AbstractTable;
import com.huawei.ock.bss.table.KeyPair;
import com.huawei.ock.bss.table.description.NsKListTableDescription;
import com.huawei.ock.bss.table.iterator.SubTableKeyIterator;
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

import java.util.Iterator;
import java.util.stream.Stream;

/**
 * NsKListTableImplTest
 *
 * @since BeiMing 25.0.T1
 */
@RunWith(PowerMockRunner.class)
@PrepareForTest(
    value = {NsKListTableImpl.class, SubTableKeyIterator.class, DirectBuffer.class})
public class NsKListTableImplTest {
    private NsKListTableImpl<String, String, String> nsKListTable;

    private final TypeSerializer<String> keySerializer = new StringSerializer();

    private final TypeSerializer<String> nsSerializer = new StringSerializer();

    private final TypeSerializer<String> valueSerializer = new StringSerializer();

    @Before
    public void setUp() {
        PowerMockito.mockStatic(NsKListTableImpl.class);
        PowerMockito.mockStatic(AbstractTable.class);
        PowerMockito.when(AbstractTable.open(anyLong(), Mockito.nullable(String.class), any())).thenReturn(1L);
        PowerMockito.mockStatic(SubTableKeyIterator.class);
        PowerMockito.when(SubTableKeyIterator.open(anyLong(), anyLong(), anyInt())).thenReturn(1L);
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
        TableSerializer<KeyPair<String, String>, String> tableSerializer = new TableSerializer<>(
            new KeyPairSerializer<>(keySerializer, nsSerializer), valueSerializer);
        NsKListTableDescription<String, String, String> kListTableDescription = new NsKListTableDescription<>(
            "testTable", 10, tableSerializer);
        Assert.assertEquals(kListTableDescription.getStateType(), BoostStateType.SUB_LIST);
        nsKListTable = PowerMockito.spy(new NsKListTableImpl<>(db, kListTableDescription));
        PowerMockito.when(nsKListTable.testKeyHash(anyInt())).thenReturn(true);
        PowerMockito.when(nsKListTable.testKeyHash(anyInt(), anyInt())).thenReturn(true);
    }

    @After
    public void tearDown() {
    }

    @Test
    public void test_get_keys() {
        String key1 = "testSubKListKey1";
        String key2 = "testSubKListKey2";

        try {
            // mock native method
            DirectBuffer key1Bytes = KVSerializerUtil.serKey(key1, keySerializer).wrapDirectData();
            DirectBuffer key2Bytes = KVSerializerUtil.serSubKey(key2, keySerializer).wrapDirectData();

            PowerMockito.when(SubTableKeyIterator.hasNext(anyLong())).thenReturn(true, true, false);
            when(SubTableKeyIterator.next(anyLong(), any(EntryResult.class)))
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

            Stream<String> keyStream = nsKListTable.getKeys("testSubKListNs");
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
}