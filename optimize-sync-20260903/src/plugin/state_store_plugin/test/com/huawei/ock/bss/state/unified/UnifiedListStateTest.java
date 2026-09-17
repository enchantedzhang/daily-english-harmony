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
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyCollection;
import static org.mockito.ArgumentMatchers.anyString;

import com.huawei.ock.bss.state.internal.KeyedListState;
import com.huawei.ock.bss.state.internal.KeyedListStateImpl;

import org.apache.flink.api.common.typeutils.base.ListSerializer;
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

import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.List;

/**
 * UnifiedListStateTest
 *
 * @since BeiMing 25.0.T1
 */
@RunWith(PowerMockRunner.class)
@PrepareForTest(value = {UnifiedListState.class})
public class UnifiedListStateTest {
    private UnifiedListState<String, String> listState;

    private final String key = "testKey";

    private final String element = "testElement";

    private final List<String> value = new ArrayList<>();

    private final List<String> emptyValue = new ArrayList<>();

    @Before
    public void setUp() throws Exception {
        InternalKeyContext<String> keyContext = PowerMockito.mock(InternalKeyContextImpl.class);
        PowerMockito.when(keyContext.getCurrentKey()).thenReturn(key);

        KeyedListState<String, String> keyedState = PowerMockito.mock(KeyedListStateImpl.class);
        // test_list_add
        PowerMockito.doNothing().when(keyedState, "add", anyString(), anyString());
        // test_list_addAll
        PowerMockito.doNothing().when(keyedState, "addAll", anyString(), anyCollection());
        // test_list_update / test_list_updateInternal
        PowerMockito.doNothing().when(keyedState, "putAll", anyString(), anyCollection());
        // test_list_clear
        PowerMockito.doNothing().when(keyedState, "remove", anyString());
        // test_list_getSerializedValue
        PowerMockito.when(keyedState, "getSerializedValue", any(), any(), any()).thenReturn(element.getBytes(
            StandardCharsets.UTF_8));
        // test_list_get / test_list_getInternal
        value.add(element);
        PowerMockito.when(keyedState.get(key)).thenReturn(value);

        listState = PowerMockito.spy(new UnifiedListState<>(keyContext, keyedState));
    }

    @After
    public void tearDown() {

    }

    @Test
    public void test_list_add_normal() {
        try {
            listState.add(element);
        } catch (Exception e) {
            fail("Should not have thrown any exception");
        }
    }

    @Test
    public void test_list_add_null_element() {
        try {
            listState.add(null);
        } catch (Exception e) {
            fail("Should not have thrown any exception");
        }
    }

    @Test
    public void test_list_update_normal() {
        try {
            listState.update(value);
        } catch (Exception e) {
            fail("Should not have thrown any exception");
        }
    }

    @Test
    public void test_list_updateInternal_normal() {
        try {
            listState.updateInternal(value);
        } catch (Exception e) {
            fail("Should not have thrown any exception");
        }
    }

    @Test
    public void test_list_updateInternal_null_elements() {
        try {
            listState.updateInternal(null);
        } catch (Exception e) {
            fail("Should not have thrown any exception");
        }
    }

    @Test
    public void test_list_updateInternal_empty_elements() {
        try {
            listState.updateInternal(emptyValue);
        } catch (Exception e) {
            fail("Should not have thrown any exception");
        }
    }

    @Test
    public void test_list_addAll_normal() {
        try {
            listState.addAll(value);
        } catch (Exception e) {
            fail("Should not have thrown any exception");
        }
    }

    @Test
    public void test_list_addAll_null_elements() {
        try {
            listState.addAll(null);
        } catch (Exception e) {
            fail("Should not have thrown any exception");
        }
    }

    @Test
    public void test_list_addAll_empty_elements() {
        try {
            listState.addAll(emptyValue);
        } catch (Exception e) {
            fail("Should not have thrown any exception");
        }
    }

    @Test
    public void test_list_get_normal() {
        try {
            Iterable<String> res = listState.get();
            Assert.assertEquals(value.get(0), res.iterator().next());
        } catch (Exception e) {
            fail("Should not have thrown any exception");
        }
    }

    @Test
    public void test_list_getInternal_normal() {
        try {
            List<String> res = listState.getInternal();
            Assert.assertEquals(value.get(0), res.get(0));
        } catch (Exception e) {
            fail("Should not have thrown any exception");
        }
    }

    @Test
    public void test_list_clear_normal() {
        try {
            listState.clear();
        } catch (Exception e) {
            fail("Should not have thrown any exception");
        }
    }

    @Test
    public void test_list_getSerializedValue_normal() {
        try {
            byte[] res =
                listState.getSerializedValue(new byte[1], StringSerializer.INSTANCE, VoidNamespaceSerializer.INSTANCE,
                    new ListSerializer<>(StringSerializer.INSTANCE));
            Assert.assertNotNull(res);
            Assert.assertEquals(element, new String(res, StandardCharsets.UTF_8));
        } catch (Exception e) {
            fail("Should not have thrown any exception");
        }
    }

    @Test
    public void test_list_getSerializedValue_null_array() {
        try {
            byte[] res = listState.getSerializedValue(null, StringSerializer.INSTANCE, VoidNamespaceSerializer.INSTANCE,
                new ListSerializer<>(StringSerializer.INSTANCE));
            Assert.assertNull(res);
        } catch (Exception e) {
            fail("Should not have thrown any exception");
        }
    }

    @Test
    public void test_list_getSerializedValue_empty_array() {
        try {
            byte[] res =
                listState.getSerializedValue(new byte[0], StringSerializer.INSTANCE, VoidNamespaceSerializer.INSTANCE,
                    new ListSerializer<>(StringSerializer.INSTANCE));
            Assert.assertNull(res);
        } catch (Exception e) {
            fail("Should not have thrown any exception");
        }
    }

    @Test
    public void test_list_getSerializedValue_null_keySerializer() {
        try {
            byte[] res = listState.getSerializedValue(new byte[1], null, VoidNamespaceSerializer.INSTANCE,
                new ListSerializer<>(StringSerializer.INSTANCE));
            Assert.assertNull(res);
        } catch (Exception e) {
            fail("Should not have thrown any exception");
        }
    }

    @Test
    public void test_list_getSerializedValue_null_namespaceSerializer() {
        try {
            byte[] res = listState.getSerializedValue(new byte[1], StringSerializer.INSTANCE, null,
                new ListSerializer<>(StringSerializer.INSTANCE));
            Assert.assertNull(res);
        } catch (Exception e) {
            fail("Should not have thrown any exception");
        }
    }

    @Test
    public void test_list_getSerializedValue_null_valueSerializer() {
        try {
            byte[] res =
                listState.getSerializedValue(new byte[1], StringSerializer.INSTANCE, VoidNamespaceSerializer.INSTANCE,
                    null);
            Assert.assertNull(res);
        } catch (Exception e) {
            fail("Should not have thrown any exception");
        }
    }

    @Test(expected = UnsupportedOperationException.class)
    public void test_list_mergeNamespaces() throws Exception {
        listState.mergeNamespaces(null, null);
    }
}
