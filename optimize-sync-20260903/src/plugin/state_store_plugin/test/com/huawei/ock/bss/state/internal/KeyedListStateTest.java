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
import static org.mockito.ArgumentMatchers.anyCollection;
import static org.mockito.ArgumentMatchers.anyString;

import com.huawei.ock.bss.state.internal.descriptor.keyed.KeyedListStateDescriptor;
import com.huawei.ock.bss.table.KListTableImpl;
import com.huawei.ock.bss.table.api.KListTable;

import org.apache.flink.api.common.typeutils.base.ListSerializer;
import org.apache.flink.api.common.typeutils.base.StringSerializer;
import org.apache.flink.queryablestate.client.VoidNamespace;
import org.apache.flink.queryablestate.client.VoidNamespaceSerializer;
import org.apache.flink.queryablestate.client.state.serialization.KvStateSerializer;
import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.powermock.api.mockito.PowerMockito;
import org.powermock.core.classloader.annotations.PrepareForTest;
import org.powermock.modules.junit4.PowerMockRunner;

import java.io.IOException;
import java.util.ArrayList;
import java.util.Collection;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/**
 * KeyedListStateTest
 *
 * @since BeiMing 25.0.T1
 */
@RunWith(PowerMockRunner.class)
@PrepareForTest(value = KeyedListStateImpl.class)
public class KeyedListStateTest {
    private KeyedListStateImpl<String, String> keyedState;

    private final String notExistKey = "notExistKey";

    private final String key = "testKey";

    private final String element = "testElement";

    private final List<String> value = new ArrayList<>();

    private final List<String> emptyValue = new ArrayList<>();

    @Before
    public void setUp() throws Exception {
        KeyedListStateDescriptor<String, String> descriptor = PowerMockito.mock(KeyedListStateDescriptor.class);
        PowerMockito.when(descriptor.getElementSerializer()).thenReturn(StringSerializer.INSTANCE);
        KListTable<String, String> table = PowerMockito.mock(KListTableImpl.class);
        PowerMockito.doNothing().when(table, "add", anyString(), anyString());
        PowerMockito.doNothing().when(table, "addAll", anyString(), anyCollection());
        PowerMockito.doNothing().when(table, "put", anyString(), anyCollection());
        PowerMockito.doNothing().when(table, "remove", anyString());
        PowerMockito.when(table.contains(key)).thenReturn(true);
        PowerMockito.when(table.contains(notExistKey)).thenReturn(false);
        value.add(element);
        PowerMockito.when(table.get(key)).thenReturn(value);
        PowerMockito.when(table.get(notExistKey)).thenReturn(null);

        keyedState = PowerMockito.spy(new KeyedListStateImpl<>(descriptor, table));
    }

    @After
    public void tearDown() {

    }

    @Test
    public void test_k_list_add_normal() {
        try {
            keyedState.add(key, element);
        } catch (Exception e) {
            fail("Should not have thrown any exception");
        }
    }

    @Test
    public void test_k_list_add_null_key() {
        try {
            keyedState.add(null, element);
        } catch (Exception e) {
            fail("Should not have thrown any exception");
        }
    }

    @Test
    public void test_k_list_add_null_element() {
        try {
            keyedState.add(key, null);
        } catch (Exception e) {
            fail("Should not have thrown any exception");
        }
    }

    @Test
    public void test_k_list_addAll_normal() {
        try {
            keyedState.addAll(key, value);
        } catch (Exception e) {
            fail("Should not have thrown any exception");
        }
    }

    @Test
    public void test_k_list_addAll_null_key() {
        try {
            keyedState.addAll(null, value);
        } catch (Exception e) {
            fail("Should not have thrown any exception");
        }
    }

    @Test
    public void test_k_list_addAll_null_elements() {
        try {
            keyedState.addAll(key, null);
        } catch (Exception e) {
            fail("Should not have thrown any exception");
        }
    }

    @Test
    public void test_k_list_addAll_empty_elements() {
        try {
            keyedState.addAll(key, emptyValue);
        } catch (Exception e) {
            fail("Should not have thrown any exception");
        }
    }

    @Test
    public void test_k_list_addAll1_normal() {
        Map<String, List<String>> kListMap = new HashMap<>();
        kListMap.put(key, value);
        try {
            keyedState.addAll(kListMap);
        } catch (Exception e) {
            fail("Should not have thrown any exception");
        }
    }

    @Test
    public void test_k_list_addAll1_null_map() {
        try {
            keyedState.addAll(null);
        } catch (Exception e) {
            fail("Should not have thrown any exception");
        }
    }

    @Test
    public void test_k_list_addAll1_empty_map() {
        Map<String, List<String>> kListMap = new HashMap<>();
        try {
            keyedState.addAll(kListMap);
        } catch (Exception e) {
            fail("Should not have thrown any exception");
        }
    }

    @Test
    public void test_k_list_put_normal() {
        try {
            keyedState.put(key, element);
        } catch (Exception e) {
            fail("Should not have thrown any exception");
        }
    }

    @Test
    public void test_k_list_put_null_key() {
        try {
            keyedState.put(null, element);
        } catch (Exception e) {
            fail("Should not have thrown any exception");
        }
    }

    @Test
    public void test_k_list_put_null_element() {
        try {
            keyedState.put(key, null);
        } catch (Exception e) {
            fail("Should not have thrown any exception");
        }
    }

    @Test
    public void test_k_list_putAll_normal() {
        try {
            keyedState.putAll(key, value);
        } catch (Exception e) {
            fail("Should not have thrown any exception");
        }
    }

    @Test
    public void test_k_list_putAll_null_key() {
        try {
            keyedState.putAll(null, value);
        } catch (Exception e) {
            fail("Should not have thrown any exception");
        }
    }

    @Test
    public void test_k_list_putAll_null_elements() {
        try {
            keyedState.putAll(key, null);
        } catch (Exception e) {
            fail("Should not have thrown any exception");
        }
    }

    @Test
    public void test_k_list_putAll_empty_elements() {
        try {
            keyedState.putAll(key, emptyValue);
        } catch (Exception e) {
            fail("Should not have thrown any exception");
        }
    }

    @Test
    public void test_k_list_putAll1_normal() {
        Map<String, List<String>> kListMap = new HashMap<>();
        kListMap.put(key, value);
        try {
            keyedState.putAll(kListMap);
        } catch (Exception e) {
            fail("Should not have thrown any exception");
        }
    }

    @Test
    public void test_k_list_putAll1_null_map() {
        try {
            keyedState.putAll(null);
        } catch (Exception e) {
            fail("Should not have thrown any exception");
        }
    }

    @Test
    public void test_k_list_putAll1_empty_map() {
        Map<String, List<String>> kListMap = new HashMap<>();
        try {
            keyedState.putAll(kListMap);
        } catch (Exception e) {
            fail("Should not have thrown any exception");
        }
    }

    @Test
    public void test_k_list_remove_normal() {
        try {
            keyedState.remove(key);
        } catch (Exception e) {
            fail("Should not have thrown any exception");
        }
    }

    @Test
    public void test_k_list_remove_key_null() {
        try {
            keyedState.remove(null);
        } catch (Exception e) {
            fail("Should not have thrown any exception");
        }
    }

    @Test
    public void test_k_list_removeAll_normal() {
        List<String> keySet = new ArrayList<>();
        keySet.add(key);
        try {
            keyedState.removeAll(keySet);
        } catch (Exception e) {
            fail("Should not have thrown any exception");
        }
    }

    @Test
    public void test_k_list_removeAll_null_keySet() {
        try {
            keyedState.removeAll((Collection<? extends String>) null);
        } catch (Exception e) {
            fail("Should not have thrown any exception");
        }
    }

    @Test
    public void test_k_list_removeAll_empty_keySet() {
        List<String> keySet = new ArrayList<>();
        try {
            keyedState.removeAll(keySet);
        } catch (Exception e) {
            fail("Should not have thrown any exception");
        }
    }

    @Test
    public void test_k_list_remove1_normal() {
        try {
            boolean res = keyedState.remove(key, element);
            Assert.assertTrue(res);
        } catch (Exception e) {
            fail("Should not have thrown any exception");
        }
    }

    @Test
    public void test_k_list_remove1_null_key() {
        try {
            boolean res = keyedState.remove(null, element);
            Assert.assertFalse(res);
        } catch (Exception e) {
            fail("Should not have thrown any exception");
        }
    }

    @Test
    public void test_k_list_remove1_null_element() {
        try {
            boolean res = keyedState.remove(key, null);
            Assert.assertTrue(res);
        } catch (Exception e) {
            fail("Should not have thrown any exception");
        }
    }

    @Test
    public void test_k_list_removeAll1_normal() {
        try {
            boolean res = keyedState.removeAll(key, value);
            Assert.assertTrue(res);
        } catch (Exception e) {
            fail("Should not have thrown any exception");
        }
    }

    @Test
    public void test_k_list_removeAll1_null_key() {
        try {
            boolean res = keyedState.removeAll(null, value);
            Assert.assertFalse(res);
        } catch (Exception e) {
            fail("Should not have thrown any exception");
        }
    }

    @Test
    public void test_k_list_removeAll1_null_elements() {
        try {
            boolean res = keyedState.removeAll(key, null);
            Assert.assertTrue(res);
        } catch (Exception e) {
            fail("Should not have thrown any exception");
        }
    }

    @Test
    public void test_k_list_removeAll1_empty_elements() {
        try {
            boolean res = keyedState.removeAll(key, emptyValue);
            Assert.assertTrue(res);
        } catch (Exception e) {
            fail("Should not have thrown any exception");
        }
    }

    @Test
    public void test_k_list_removeAll2_normal() {
        Map<String, List<String>> kListMap = new HashMap<>();
        kListMap.put(key, value);
        try {
            boolean res = keyedState.removeAll(kListMap);
            Assert.assertTrue(res);
        } catch (Exception e) {
            fail("Should not have thrown any exception");
        }
    }

    @Test
    public void test_k_list_removeAll2_null_map() {
        try {
            boolean res = keyedState.removeAll((Map<? extends String, ? extends Collection<? extends String>>) null);
            Assert.assertFalse(res);
        } catch (Exception e) {
            fail("Should not have thrown any exception");
        }
    }

    @Test
    public void test_k_list_removeAll2_empty_map() {
        Map<String, List<String>> kListMap = new HashMap<>();
        try {
            boolean res = keyedState.removeAll(kListMap);
            Assert.assertFalse(res);
        } catch (Exception e) {
            fail("Should not have thrown any exception");
        }
    }

    @Test
    public void test_k_list_contains_normal() {
        try {
            boolean res = keyedState.contains(key);
            Assert.assertTrue(res);
        } catch (Exception e) {
            fail("Should not have thrown any exception");
        }
    }

    @Test
    public void test_k_list_contains_abnormal() {
        try {
            boolean res = keyedState.contains(notExistKey);
            Assert.assertFalse(res);
        } catch (Exception e) {
            fail("Should not have thrown any exception");
        }
    }

    @Test
    public void test_k_list_get_normal() {
        try {
            List<String> res = keyedState.get(key);
            Assert.assertNotNull(res);
            Assert.assertEquals(value.get(0), res.get(0));
        } catch (Exception e) {
            fail("Should not have thrown any exception");
        }
    }

    @Test
    public void test_k_list_get_null_key() {
        try {
            List<String> res = keyedState.get(null);
            Assert.assertNull(res);
        } catch (Exception e) {
            fail("Should not have thrown any exception");
        }
    }

    @Test
    public void test_k_list_get_not_exist() {
        try {
            List<String> res = keyedState.get(notExistKey);
            Assert.assertNull(res);
        } catch (Exception e) {
            fail("Should not have thrown any exception");
        }
    }

    @Test
    public void test_k_list_getAll_normal() {
        List<String> keySet = new ArrayList<>();
        keySet.add(key);
        try {
            Map<String, List<String>> kListMap = keyedState.getAll(keySet);
            Assert.assertNotNull(kListMap);
            Assert.assertTrue(kListMap.containsKey(key));
            Assert.assertEquals(element, kListMap.get(key).get(0));
        } catch (Exception e) {
            fail("Should not have thrown any exception");
        }
    }

    @Test
    public void test_k_list_getAll_null_keySet() {
        try {
            Map<String, List<String>> kListMap = keyedState.getAll(null);
            Assert.assertNotNull(kListMap);
            Assert.assertTrue(kListMap.isEmpty());
        } catch (Exception e) {
            fail("Should not have thrown any exception");
        }
    }

    @Test
    public void test_k_list_getAll_empty_keySet() {
        List<String> keySet = new ArrayList<>();
        try {
            Map<String, List<String>> kListMap = keyedState.getAll(keySet);
            Assert.assertNotNull(kListMap);
            Assert.assertTrue(kListMap.isEmpty());
        } catch (Exception e) {
            fail("Should not have thrown any exception");
        }
    }

    @Test
    public void test_k_list_getAll_abnormal() {
        List<String> keySet = new ArrayList<>();
        keySet.add(key);
        keySet.add(null);
        keySet.add(notExistKey);
        try {
            Map<String, List<String>> kListMap = keyedState.getAll(keySet);
            Assert.assertNotNull(kListMap);
            Assert.assertTrue(kListMap.containsKey(key));
            Assert.assertFalse(kListMap.containsKey(notExistKey));
            Assert.assertFalse(kListMap.containsKey(null));
            Assert.assertEquals(1, kListMap.size());
            Assert.assertEquals(element, kListMap.get(key).get(0));
        } catch (Exception e) {
            fail("Should not have thrown any exception");
        }
    }

    @Test
    public void test_k_list_getSerializedValue_normal() throws IOException {
        byte[] serializeKeyAndNamespace =
            KvStateSerializer.serializeKeyAndNamespace(key, StringSerializer.INSTANCE, VoidNamespace.INSTANCE,
                VoidNamespaceSerializer.INSTANCE);
        byte[] serializedValue = keyedState.getSerializedValue(serializeKeyAndNamespace, StringSerializer.INSTANCE,
            new ListSerializer<>(StringSerializer.INSTANCE));
        List<String> list = KvStateSerializer.deserializeList(serializedValue, StringSerializer.INSTANCE);
        Assert.assertNotNull(serializedValue);
        Assert.assertNotNull(list);
        Assert.assertEquals(1, list.size());
        Assert.assertTrue(list.contains(value.get(0)));
    }

    @Test
    public void test_k_list_getSerializedValue_value_null() throws IOException {
        byte[] serializeKeyAndNamespace =
            KvStateSerializer.serializeKeyAndNamespace(notExistKey, StringSerializer.INSTANCE, VoidNamespace.INSTANCE,
                VoidNamespaceSerializer.INSTANCE);
        byte[] serializedValue = keyedState.getSerializedValue(serializeKeyAndNamespace, StringSerializer.INSTANCE,
            new ListSerializer<>(StringSerializer.INSTANCE));
        Assert.assertNull(serializedValue);
    }
}
