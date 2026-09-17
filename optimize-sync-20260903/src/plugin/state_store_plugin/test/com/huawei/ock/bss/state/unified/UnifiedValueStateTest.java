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

package com.huawei.ock.bss.state.unified;

import static org.junit.Assert.fail;
import static org.mockito.ArgumentMatchers.anyString;

import com.huawei.ock.bss.state.internal.KeyedValueState;
import com.huawei.ock.bss.state.internal.KeyedValueStateImpl;

import org.apache.flink.api.common.typeutils.base.StringSerializer;
import org.apache.flink.runtime.state.VoidNamespaceSerializer;
import org.apache.flink.runtime.state.heap.InternalKeyContext;
import org.apache.flink.runtime.state.heap.InternalKeyContextImpl;
import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.powermock.api.mockito.PowerMockito;
import org.powermock.core.classloader.annotations.PrepareForTest;
import org.powermock.modules.junit4.PowerMockRunner;

/**
 * UnifiedValueStateTest
 *
 * @since BeiMing 25.0.T1
 */
@RunWith(PowerMockRunner.class)
@PrepareForTest(value = {UnifiedValueState.class})
public class UnifiedValueStateTest {
    private UnifiedValueState<String, String> unifiedValueState;

    @Before
    public void setUp() throws Exception {
        String key = "testKey";
        String value = "testValue";
        String defaultValue = "defaultValue";
        // mock keyContext
        InternalKeyContext<String> keyContext = PowerMockito.mock(InternalKeyContextImpl.class);
        PowerMockito.when(keyContext.getCurrentKey()).thenReturn(key);
        // mock keyedState
        KeyedValueState<String, String> keyedState = PowerMockito.mock(KeyedValueStateImpl.class);
        // test_update_normal
        PowerMockito.doNothing().when(keyedState, "put", anyString(), anyString());
        // test_value_normal
        PowerMockito.when(keyedState.get(key)).thenReturn(value);
        // test_update_null
        PowerMockito.doNothing().when(keyedState, "remove", key);
        // test_getSerializedValue_normal
        PowerMockito.when(
                keyedState.getSerializedValue(new byte[1], StringSerializer.INSTANCE, StringSerializer.INSTANCE))
            .thenReturn(new byte[0]);

        unifiedValueState = PowerMockito.spy(new UnifiedValueState<>(keyContext, keyedState, defaultValue));
    }

    @After
    public void tearDown() {

    }

    @Test
    public void test_update_normal() {
        // setUp
        String value = "testValue";
        // run the test
        try {
            unifiedValueState.update(value);
        } catch (Exception e) {
            fail("Should not have thrown any exception");
        }
    }

    @Test
    public void test_value_normal() {
        String value = "testValue";
        try {
            String res = unifiedValueState.value();
            Assert.assertEquals(value, res);
        } catch (Exception e) {
            fail("Should not have thrown any exception: " + e.getMessage());
        }
    }

    @Test
    public void test_update_null() {
        String value = null;
        try {
            unifiedValueState.update(value);
        } catch (Exception e) {
            fail("Should not have thrown any exception: " + e.getMessage());
        }
    }

    @Test
    public void test_value_value_null() {
        String testKey = "testKey";
        String defaultValue = "defaultValue";

        UnifiedValueState<String, String> testValueIsNull;
        InternalKeyContext<String> keyContext = PowerMockito.mock(InternalKeyContextImpl.class);
        PowerMockito.when(keyContext.getCurrentKey()).thenReturn(testKey);
        KeyedValueState<String, String> keyedState = PowerMockito.mock(KeyedValueStateImpl.class);
        PowerMockito.when(keyedState.get(testKey)).thenReturn(null);
        testValueIsNull = PowerMockito.spy(new UnifiedValueState<>(keyContext, keyedState, defaultValue));
        try {
            String res = testValueIsNull.value();
            Assert.assertEquals(defaultValue, res);
        } catch (Exception e) {
            fail("Should not have thrown any exception: " + e.getMessage());
        }
    }

    @Test
    public void test_clear_normal() {
        try {
            unifiedValueState.clear();
        } catch (Exception e) {
            fail("Should not have thrown any exception: " + e.getMessage());
        }
    }

    @Test
    public void test_getSerializedValue_normal() {
        try {
            byte[] serializedValue = unifiedValueState.getSerializedValue(new byte[1], StringSerializer.INSTANCE,
                VoidNamespaceSerializer.INSTANCE, StringSerializer.INSTANCE);
            Assert.assertEquals(0, serializedValue.length);
        } catch (Exception e) {
            fail("Should not have thrown any exception: " + e.getMessage());
        }
    }
}
