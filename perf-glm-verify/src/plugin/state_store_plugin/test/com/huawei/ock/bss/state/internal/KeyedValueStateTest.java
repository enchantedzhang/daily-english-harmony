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

package com.huawei.ock.bss.state.internal;

import static org.junit.Assert.fail;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.anyString;

import com.huawei.ock.bss.state.internal.descriptor.keyed.KeyedValueStateDescriptor;
import com.huawei.ock.bss.table.KVTableImpl;
import com.huawei.ock.bss.table.api.KVTable;

import org.apache.flink.api.common.functions.AggregateFunction;
import org.apache.flink.api.common.functions.ReduceFunction;
import org.apache.flink.api.common.typeutils.base.StringSerializer;
import org.apache.flink.queryablestate.client.state.serialization.KvStateSerializer;
import org.apache.flink.runtime.state.VoidNamespaceSerializer;
import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.powermock.api.mockito.PowerMockito;
import org.powermock.core.classloader.annotations.PrepareForTest;
import org.powermock.modules.junit4.PowerMockRunner;

/**
 * KeyedValueState测试类
 *
 * @since BeiMing 25.0.T1
 */
@RunWith(PowerMockRunner.class)
@PrepareForTest(value = KeyedValueStateImpl.class)
public class KeyedValueStateTest {
    private KeyedValueStateImpl<String, String> keyedValueState;

    @Before
    public void setUp() throws Exception {
        String testKey = "testKey";
        String testValue = "testValue";
        String notExist = "notExist";
        String testReduceKey = "testReduceKey";
        String reduceOldValue = "reduceOldValue";
        // aggregate累加器不为空
        String testAggregateKey = "testAggregateKey";
        String testAggregateACC = "testAggregateValue";

        KeyedValueStateDescriptor<String, String> descriptor = PowerMockito.mock(KeyedValueStateDescriptor.class);
        PowerMockito.when(descriptor.getValueSerializer()).thenReturn(StringSerializer.INSTANCE);
        KVTable<String, String> table = PowerMockito.mock(KVTableImpl.class);
        // test_put_normal
        PowerMockito.doNothing().when(table, "put", testKey, testValue);
        // test_get_normal
        PowerMockito.when(table.get(testKey)).thenReturn(testValue);
        // test_getSerializedValue_normal
        PowerMockito.when(table.getSerializedValue(testKey))
            .thenReturn(KvStateSerializer.serializeValue(testValue, StringSerializer.INSTANCE));
        // test_contain_true
        PowerMockito.when(table.contains(testKey)).thenReturn(true);
        // test_contain_false
        PowerMockito.when(table.get(notExist)).thenReturn(null);
        // test_remove
        PowerMockito.doNothing().when(table, "remove", any());
        // test_reduce_normal
        PowerMockito.when(table.get(testReduceKey)).thenReturn(reduceOldValue);
        // test_aggregate_normal
        PowerMockito.when(table.get(testAggregateKey)).thenReturn(testAggregateACC);

        keyedValueState = PowerMockito.spy(new KeyedValueStateImpl<>(descriptor, table));
    }

    @After
    public void tearDown() {

    }

    @Test
    public void test_put_normal() {
        String testKey = "testKey";
        String testValue = "testValue";
        try {
            keyedValueState.put(testKey, testValue);
        } catch (Exception e) {
            fail("Should not have thrown any exception");
        }
    }

    @Test(expected = NullPointerException.class)
    public void test_put_key_null() {
        String testValue = "testValue";
        keyedValueState.put(null, testValue);
    }

    @Test
    public void test_get_normal() {
        String testKey = "testKey";
        String testValue = "testValue";
        try {
            String res = keyedValueState.get(testKey);
            Assert.assertEquals(testValue, res);
        } catch (Exception e) {
            fail("Should not have thrown any exception");
        }
    }

    @Test
    public void test_get_key_null() {
        String testKey = null;
        try {
            String res = keyedValueState.get(testKey);
            Assert.assertNull(res);
        } catch (Exception e) {
            fail("Should not have thrown any exception");
        }
    }

    @Test
    public void test_contain_true() {
        String testKey = "testKey";
        try {
            boolean res = keyedValueState.contains(testKey);
            Assert.assertTrue(res);
        } catch (Exception e) {
            fail("Should not have thrown any exception");
        }
    }

    @Test
    public void test_contain_false() {
        String notExist = "notExist";
        try {
            boolean res = keyedValueState.contains(notExist);
            Assert.assertFalse(res);
        } catch (Exception e) {
            fail("Should not have thrown any exception");
        }
    }

    @Test
    public void test_remove_normal() {
        String testKey = "testKey";
        try {
            keyedValueState.remove(testKey);
        } catch (Exception e) {
            fail("Should not have thrown any exception");
        }
    }

    @Test
    public void test_remove_key_null() {
        String testKey = null;
        try {
            keyedValueState.remove(testKey);
        } catch (Exception e) {
            fail("Should not have thrown any exception");
        }
    }

    @Test
    public void test_getSerializedValue_normal() {
        String testKey = "testKey";
        String testValue = "testValue";
        try {
            byte[] serializedKey = KvStateSerializer.serializeKeyAndNamespace(testKey, StringSerializer.INSTANCE, null,
                VoidNamespaceSerializer.INSTANCE);
            byte[] res =
                keyedValueState.getSerializedValue(
                    serializedKey, StringSerializer.INSTANCE, StringSerializer.INSTANCE);
            String value = KvStateSerializer.deserializeValue(res, StringSerializer.INSTANCE);
            Assert.assertEquals(testValue, value);
        } catch (Exception e) {
            fail("Should not have thrown any exception");
        }
    }

    @Test
    public void test_reduce_normal() {
        String testReduceKey = "testReduceKey";
        // 需要reduce的新值
        String reduceNewValue = "reduceNewValue";
        // reduce的结果
        String testReduceValue = "testReduceValue";
        try {
            ReduceFunction<String> reduceFunction = PowerMockito.mock(ReduceFunction.class);
            PowerMockito.when(reduceFunction, "reduce", anyString(), anyString()).thenReturn(testReduceValue);
            keyedValueState.reduce(testReduceKey, reduceNewValue, reduceFunction);
        } catch (Exception e) {
            fail("Should not have thrown any exception");
        }
    }

    @Test
    public void test_reduce_oldValue_null() {
        String notExist = "notExist";
        // 需要reduce的新值
        String reduceNewValue = "reduceNewValue";
        // reduce的结果
        String testReduceValue = "testReduceValue";
        try {
            ReduceFunction<String> reduceFunction = PowerMockito.mock(ReduceFunction.class);
            PowerMockito.when(reduceFunction, "reduce", anyString(), anyString()).thenReturn(testReduceValue);
            keyedValueState.reduce(notExist, reduceNewValue, reduceFunction);
        } catch (Exception e) {
            fail("Should not have thrown any exception");
        }
    }

    @Test
    public void test_aggregate_normal() {
        String testAggregateKey = "testAggregateKey";
        String testAggregateACC = "testAggregateValue";
        Integer testAggregateIN = 1234;
        try {
            AggregateFunction<Integer, String, Character> aggregateFunction =
                PowerMockito.mock(AggregateFunction.class);
            PowerMockito.when(aggregateFunction, "add", anyInt(), anyString()).thenReturn(testAggregateACC);
            keyedValueState.aggregate(testAggregateKey, testAggregateIN, aggregateFunction);
        } catch (Exception e) {
            fail("Should not have thrown any exception");
        }
    }

    @Test
    public void test_aggregate_acc_null() {
        String notExist = "notExist";
        String testAggregateACC = "testAggregateValue";
        Integer testAggregateIN = 1234;
        try {
            AggregateFunction<Integer, String, Character> aggregateFunction =
                PowerMockito.mock(AggregateFunction.class);
            PowerMockito.when(aggregateFunction.createAccumulator()).thenReturn(testAggregateACC);
            PowerMockito.when(aggregateFunction, "add", anyInt(), anyString()).thenReturn(testAggregateACC);
            keyedValueState.aggregate(notExist, testAggregateIN, aggregateFunction);
        } catch (Exception e) {
            fail("Should not have thrown any exception");
        }
    }

    @Test
    public void test_getSerializedValue_value_null() {
        String notExist = "notExist";
        try {
            byte[] serializedKey = KvStateSerializer.serializeKeyAndNamespace(notExist, StringSerializer.INSTANCE, null,
                VoidNamespaceSerializer.INSTANCE);
            byte[] res =
                keyedValueState.getSerializedValue(
                    serializedKey, StringSerializer.INSTANCE, StringSerializer.INSTANCE);
            Assert.assertNull(res);
        } catch (Exception e) {
            fail("Should not have thrown any exception");
        }
    }
}
