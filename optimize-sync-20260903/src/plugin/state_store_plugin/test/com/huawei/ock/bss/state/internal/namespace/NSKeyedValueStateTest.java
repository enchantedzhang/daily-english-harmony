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

package com.huawei.ock.bss.state.internal.namespace;

import static org.junit.Assert.fail;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.anyString;

import com.huawei.ock.bss.state.internal.descriptor.nskeyed.NSKeyedValueStateDescriptor;
import com.huawei.ock.bss.table.api.NsKVTable;
import com.huawei.ock.bss.table.namespace.NsKVTableImpl;

import org.apache.flink.api.common.functions.AggregateFunction;
import org.apache.flink.api.common.functions.ReduceFunction;
import org.apache.flink.api.common.typeutils.base.StringSerializer;
import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.powermock.api.mockito.PowerMockito;
import org.powermock.core.classloader.annotations.PrepareForTest;
import org.powermock.modules.junit4.PowerMockRunner;

/**
 * NSKeyedValueStateTest
 *
 * @since BeiMing 25.0.T1
 */
@RunWith(PowerMockRunner.class)
@PrepareForTest(value = {NSKeyedValueState.class})
public class NSKeyedValueStateTest {
    private NSKeyedValueState<String, String, String> keyedState;

    private final String key = "testKey";

    private final String namespace = "testNamespace";

    private final String value = "testValue";

    private final String nullValueKey = "nullValueKey";

    private final String nullValueNamespace = "nullValueNamespace";

    @Before
    public void setUp() throws Exception {
        String defaultValue = "defaultValue";

        NSKeyedValueStateDescriptor<String, String, String> descriptor =
            PowerMockito.mock(NSKeyedValueStateDescriptor.class);
        PowerMockito.when(descriptor.getValueSerializer()).thenReturn(StringSerializer.INSTANCE);
        NsKVTable<String, String, String> table = PowerMockito.mock(NsKVTableImpl.class);
        // test_ns_k_value_put
        PowerMockito.doNothing().when(table, "add", anyString(), anyString(), anyString());
        // test_ns_k_value_get
        PowerMockito.when(table.get(key, namespace)).thenReturn(value);
        // test_ns_k_value_contains
        PowerMockito.when(table.contains(key, namespace)).thenReturn(true);
        // test_ns_k_value_remove
        PowerMockito.doNothing().when(table, "remove", anyString(), anyString());
        // test_ns_k_value_reduce
        PowerMockito.when(table.get(nullValueKey, nullValueNamespace)).thenReturn(null);

        keyedState = PowerMockito.spy(new NSKeyedValueStateImpl<>(descriptor, table));
    }

    @After
    public void tearDown() {

    }

    @Test
    public void test_ns_k_value_put_normal() {
        try {
            keyedState.put(key, namespace, value);
        } catch (Exception e) {
            fail("Should not have thrown any exception");
        }
    }

    @Test(expected = NullPointerException.class)
    public void test_ns_k_value_put_null_key() {
        try {
            keyedState.put(null, namespace, value);
        } catch (NullPointerException e) {
            Assert.assertEquals("Failed to put a null key/namespace into table.", e.getMessage());
            throw e;
        }
    }

    @Test(expected = NullPointerException.class)
    public void test_ns_k_value_put_null_namespace() {
        try {
            keyedState.put(key, null, value);
        } catch (NullPointerException e) {
            Assert.assertEquals("Failed to put a null key/namespace into table.", e.getMessage());
            throw e;
        }
    }

    @Test
    public void test_ns_k_value_get_normal() {
        try {
            String res = keyedState.get(key, namespace);
            Assert.assertEquals(value, res);
        } catch (Exception e) {
            fail("Should not have thrown any exception");
        }
    }

    @Test
    public void test_ns_k_value_get_null_key() {
        try {
            String res = keyedState.get(null, namespace);
            Assert.assertNull(res);
        } catch (Exception e) {
            fail("Should not have thrown any exception");
        }
    }

    @Test
    public void test_ns_k_value_get_null_namespace() {
        try {
            String res = keyedState.get(key, null);
            Assert.assertNull(res);
        } catch (Exception e) {
            fail("Should not have thrown any exception");
        }
    }

    @Test
    public void test_ns_k_value_contains_normal() {
        try {
            boolean res = keyedState.contains(key, namespace);
            Assert.assertTrue(res);
        } catch (Exception e) {
            fail("Should not have thrown any exception");
        }
    }

    @Test
    public void test_ns_k_value_contains_null_key() {
        try {
            boolean res = keyedState.contains(null, namespace);
            Assert.assertFalse(res);
        } catch (Exception e) {
            fail("Should not have thrown any exception");
        }
    }

    @Test
    public void test_ns_k_value_contains_null_namespace() {
        try {
            boolean res = keyedState.contains(key, null);
            Assert.assertFalse(res);
        } catch (Exception e) {
            fail("Should not have thrown any exception");
        }
    }

    @Test
    public void test_ns_k_value_remove_normal() {
        try {
            keyedState.remove(key, namespace);
        } catch (Exception e) {
            fail("Should not have thrown any exception");
        }
    }

    @Test
    public void test_ns_k_value_remove_null_key() {
        try {
            keyedState.remove(null, namespace);
        } catch (Exception e) {
            fail("Should not have thrown any exception");
        }
    }

    @Test
    public void test_ns_k_value_remove_null_namespace() {
        try {
            keyedState.remove(key, null);
        } catch (Exception e) {
            fail("Should not have thrown any exception");
        }
    }

    @Test
    public void test_ns_k_value_getAndRemove_normal() {
        try {
            String res = keyedState.getAndRemove(key, namespace);
            Assert.assertEquals(value, res);
        } catch (Exception e) {
            fail("Should not have thrown any exception");
        }
    }

    @Test
    public void test_ns_k_value_getAndRemove_key_null() {
        try {
            String res = keyedState.getAndRemove(null, namespace);
            Assert.assertNull(res);
        } catch (Exception e) {
            fail("Should not have thrown any exception");
        }
    }

    @Test
    public void test_ns_k_value_getAndRemove_namespace_null() {
        try {
            String res = keyedState.getAndRemove(key, null);
            Assert.assertNull(res);
        } catch (Exception e) {
            fail("Should not have thrown any exception");
        }
    }

    @Test
    public void test_ns_k_value_reduce_normal() {
        String newValue = "newValue";
        try {
            ReduceFunction<String> reduceFunction = PowerMockito.mock(ReduceFunction.class);
            PowerMockito.when(reduceFunction, "reduce", anyString(), anyString()).thenReturn(value);
            keyedState.reduce(key, namespace, newValue, reduceFunction);
        } catch (Exception e) {
            fail("Should not have thrown any exception");
        }
    }

    @Test
    public void test_ns_k_value_reduce_null_oldValue() {
        String newValue = "newValue";
        try {
            ReduceFunction<String> reduceFunction = PowerMockito.mock(ReduceFunction.class);
            PowerMockito.when(reduceFunction, "reduce", anyString(), anyString()).thenReturn(value);
            keyedState.reduce(nullValueKey, nullValueNamespace, newValue, reduceFunction);
        } catch (Exception e) {
            fail("Should not have thrown any exception");
        }
    }

    @Test
    public void test_ns_k_value_aggregate_normal() {
        Integer in = 123;
        try {
            AggregateFunction<Integer, String, Character> aggregateFunction =
                PowerMockito.mock(AggregateFunction.class);
            PowerMockito.when(aggregateFunction.createAccumulator()).thenReturn(value);
            PowerMockito.when(aggregateFunction, "add", anyInt(), anyString()).thenReturn(value);
            keyedState.aggregate(key, namespace, in, aggregateFunction);
        } catch (Exception e) {
            fail("Should not have thrown any exception");
        }
    }

    @Test
    public void test_ns_k_value_aggregate_null_acc() {
        Integer in = 123;
        try {
            AggregateFunction<Integer, String, Character> aggregateFunction =
                PowerMockito.mock(AggregateFunction.class);
            PowerMockito.when(aggregateFunction.createAccumulator()).thenReturn(value);
            PowerMockito.when(aggregateFunction, "add", anyInt(), anyString()).thenReturn(value);
            keyedState.aggregate(nullValueKey, nullValueNamespace, in, aggregateFunction);
        } catch (Exception e) {
            fail("Should not have thrown any exception");
        }
    }
}
