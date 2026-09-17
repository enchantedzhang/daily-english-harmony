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

package com.huawei.ock.bss.state.unified.namespace;

import static org.junit.Assert.fail;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyString;

import com.huawei.ock.bss.state.internal.namespace.NSKeyedValueState;
import com.huawei.ock.bss.state.internal.namespace.NSKeyedValueStateImpl;

import org.apache.flink.api.common.typeutils.base.StringSerializer;
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
 * UnifiedNSValueStateTest
 *
 * @since BeiMing 25.0.T1
 */
@RunWith(PowerMockRunner.class)
@PrepareForTest(value = {UnifiedNSValueState.class})
public class UnifiedNSValueStateTest {
    private UnifiedNSValueState<String, String, String> valueState;

    private final String key = "testKey";

    private final String namespace = "testNamespace";

    private final String value = "testValue";

    private final String nullValueKey = "nullValueKey";

    private final String nullValueNamespace = "nullValueNamespace";

    private final String defaultValue = "defaultValue";

    @Before
    public void setUp() throws Exception {
        InternalKeyContext<String> keyContext = PowerMockito.mock(InternalKeyContextImpl.class);
        PowerMockito.when(keyContext.getCurrentKey()).thenReturn(key);
        NSKeyedValueState<String, String, String> keyedState = PowerMockito.mock(NSKeyedValueStateImpl.class);
        // test_update
        PowerMockito.doNothing().when(keyedState, "put", anyString(), anyString(), anyString());
        // test_value
        PowerMockito.when(keyedState.get(key, namespace)).thenReturn(value);
        // test_remove
        PowerMockito.doNothing().when(keyedState, "remove", anyString(), anyString());
        // test_getSerializedValue
        PowerMockito.when(keyedState, "getSerializedValue", any(), any(), any(), any()).thenReturn(new byte[0]);

        valueState = PowerMockito.spy(new UnifiedNSValueState<>(keyContext, keyedState, defaultValue));
        // 必须手动设置namespace
        valueState.setCurrentNamespace(namespace);
    }

    @After
    public void tearDown() {

    }

    @Test
    public void test_update_normal() {
        try {
            valueState.update(value);
        } catch (Exception e) {
            fail("Should not have thrown any exception");
        }
    }

    @Test
    public void test_value_normal() {
        try {
            String res = valueState.value();
            Assert.assertEquals(value, res);
        } catch (Exception e) {
            fail("Should not have thrown any exception");
        }
    }

    @Test
    public void test_value_null() {
        UnifiedNSValueState<String, String, String> testValueNull;
        try {
            InternalKeyContext<String> keyContext = PowerMockito.mock(InternalKeyContextImpl.class);
            PowerMockito.when(keyContext.getCurrentKey()).thenReturn(nullValueKey);
            NSKeyedValueState<String, String, String> keyedState = PowerMockito.mock(NSKeyedValueStateImpl.class);
            PowerMockito.when(keyedState.get(nullValueKey, nullValueNamespace)).thenReturn(null);
            testValueNull = PowerMockito.spy(new UnifiedNSValueState<>(keyContext, keyedState, defaultValue));
            testValueNull.setCurrentNamespace(nullValueNamespace);
            String res = testValueNull.value();
            Assert.assertEquals(defaultValue, res);
        } catch (Exception e) {
            fail("Should not have thrown any exception");
        }
    }

    @Test
    public void test_clear_normal() {
        try {
            valueState.clear();
        } catch (Exception e) {
            fail("Should not have thrown any exception");
        }
    }

    @Test
    public void test_getSerializedValue_normal() {
        try {
            byte[] res = valueState.getSerializedValue(new byte[1], StringSerializer.INSTANCE,
                StringSerializer.INSTANCE, StringSerializer.INSTANCE);
            Assert.assertEquals(0, res.length);
        } catch (Exception e) {
            fail("Should not have thrown any exception");
        }
    }
}
