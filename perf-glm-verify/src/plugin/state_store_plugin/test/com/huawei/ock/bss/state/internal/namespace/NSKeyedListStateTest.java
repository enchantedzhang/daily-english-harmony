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
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyCollection;
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.ArgumentMatchers.argThat;

import com.huawei.ock.bss.state.internal.descriptor.nskeyed.NSKeyedListStateDescriptor;
import com.huawei.ock.bss.table.KeyPair;
import com.huawei.ock.bss.table.namespace.NsKListTableImpl;
import com.huawei.ock.bss.table.api.NsKListTable;

import org.apache.flink.api.common.typeutils.base.ListSerializer;
import org.apache.flink.api.common.typeutils.base.StringSerializer;
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
import java.util.Arrays;
import java.util.List;

/**
 * NSKeyedListStateTest
 *
 * @since BeiMing 25.0.T1
 */
@RunWith(PowerMockRunner.class)
@PrepareForTest(value = NSKeyedListStateImpl.class)
public class NSKeyedListStateTest {
    private NSKeyedListStateImpl<String, String, String> keyedState;

    private final String notExistKey = "notExistKey";

    private final String notExistNamespace = "notExistNamespace";

    private final KeyPair<String, String> notExistKeyPair = new KeyPair<>(notExistKey, notExistNamespace);

    private final String key = "testKey";

    private final String namespace = "testNamespace";

    private final KeyPair<String, String> keyPair = new KeyPair<>(key, namespace);

    private final String element = "testElement";

    private final List<String> value = new ArrayList<>();

    private final List<String> nullValue = null;

    private final List<String> emptyValue = new ArrayList<>();

    @Before
    public void setUp() throws Exception {
        NSKeyedListStateDescriptor<String, String, String> descriptor =
            PowerMockito.mock(NSKeyedListStateDescriptor.class);
        PowerMockito.when(descriptor.getElementSerializer()).thenReturn(StringSerializer.INSTANCE);
        NsKListTable<String, String, String> table = PowerMockito.mock(NsKListTableImpl.class);
        PowerMockito.doNothing().when(table, "add", any(), anyString());
        PowerMockito.doNothing().when(table, "addAll", any(), anyCollection());
        PowerMockito.doNothing().when(table, "put", any(), anyCollection());
        PowerMockito.doNothing().when(table, "remove", any());
        PowerMockito.when(table.contains(keyPair)).thenReturn(true);
        PowerMockito.when(table.getKeyPair(key, namespace)).thenReturn(keyPair);
        PowerMockito.when(table.getKeyPair(notExistKey, notExistNamespace)).thenReturn(notExistKeyPair);
        value.clear();
        value.add(element);
        PowerMockito.doReturn(value)
            .when(table)
            .get(argThat(keyPair -> (key.equals(keyPair.getFirstKey()) && namespace.equals(keyPair.getSecondKey()))));
        PowerMockito.doReturn(null)
            .when(table)
            .get(argThat(
                notExistKeyPair -> (notExistKey.equals(notExistKeyPair.getFirstKey()) && notExistNamespace.equals(
                    notExistKeyPair.getSecondKey()))));

        keyedState = PowerMockito.spy(new NSKeyedListStateImpl<>(descriptor, table));
    }

    @After
    public void tearDown() {

    }

    @Test
    public void test_ns_k_list_add_normal() {
        try {
            keyedState.add(key, namespace, element);
        } catch (Exception e) {
            fail("Should not have thrown any exception");
        }
    }

    @Test
    public void test_ns_k_list_add_null_key() {
        try {
            keyedState.add(null, namespace, element);
        } catch (Exception e) {
            fail("Should not have thrown any exception");
        }
    }

    @Test
    public void test_ns_k_list_add_null_namespace() {
        try {
            keyedState.add(key, null, element);
        } catch (Exception e) {
            fail("Should not have thrown any exception");
        }
    }

    @Test
    public void test_ns_k_list_add_null_element() {
        try {
            keyedState.add(key, namespace, null);
        } catch (Exception e) {
            fail("Should not have thrown any exception");
        }
    }

    @Test
    public void test_ns_k_list_addAll_normal() {
        try {
            keyedState.addAll(key, namespace, value);
        } catch (Exception e) {
            fail("Should not have thrown any exception");
        }
    }

    @Test
    public void test_ns_k_list_addAll_null_key() {
        try {
            keyedState.addAll(null, namespace, value);
        } catch (Exception e) {
            fail("Should not have thrown any exception");
        }
    }

    @Test
    public void test_ns_k_list_addAll_null_namespace() {
        try {
            keyedState.addAll(key, null, value);
        } catch (Exception e) {
            fail("Should not have thrown any exception");
        }
    }

    @Test
    public void test_ns_k_list_addAll_null_elements() {
        try {
            keyedState.addAll(key, namespace, null);
        } catch (Exception e) {
            fail("Should not have thrown any exception");
        }
    }

    @Test
    public void test_ns_k_list_addAll_empty_elements() {
        try {
            keyedState.addAll(key, namespace, emptyValue);
        } catch (Exception e) {
            fail("Should not have thrown any exception");
        }
    }

    @Test
    public void test_ns_k_list_put_normal() {
        try {
            keyedState.put(key, namespace, element);
        } catch (Exception e) {
            fail("Should not have thrown any exception");
        }
    }

    @Test
    public void test_ns_k_list_put_null_key() {
        try {
            keyedState.put(null, namespace, element);
        } catch (Exception e) {
            fail("Should not have thrown any exception");
        }
    }

    @Test
    public void test_ns_k_list_put_null_namespace() {
        try {
            keyedState.put(key, null, element);
        } catch (Exception e) {
            fail("Should not have thrown any exception");
        }
    }

    @Test
    public void test_ns_k_list_put_null_element() {
        try {
            keyedState.put(key, namespace, null);
        } catch (Exception e) {
            fail("Should not have thrown any exception");
        }
    }

    @Test
    public void test_ns_k_list_putAll_normal() {
        try {
            keyedState.putAll(key, namespace, value);
        } catch (Exception e) {
            fail("Should not have thrown any exception");
        }
    }

    @Test
    public void test_ns_k_list_putAll_null_key() {
        try {
            keyedState.putAll(null, namespace, value);
        } catch (Exception e) {
            fail("Should not have thrown any exception");
        }
    }

    @Test
    public void test_ns_k_list_putAll_null_namespace() {
        try {
            keyedState.putAll(key, null, value);
        } catch (Exception e) {
            fail("Should not have thrown any exception");
        }
    }

    @Test
    public void test_ns_k_list_putAll_null_elements() {
        try {
            keyedState.putAll(key, namespace, value);
        } catch (Exception e) {
            fail("Should not have thrown any exception");
        }
    }

    @Test
    public void test_ns_k_list_putAll_empty_elements() {
        try {
            keyedState.putAll(key, namespace, emptyValue);
        } catch (Exception e) {
            fail("Should not have thrown any exception");
        }
    }

    @Test
    public void test_ns_k_list_remove_normal() {
        try {
            keyedState.remove(key, namespace);
        } catch (Exception e) {
            fail("Should not have thrown any exception");
        }
    }

    @Test
    public void test_ns_k_list_remove_null_key() {
        try {
            keyedState.remove(null, namespace);
        } catch (Exception e) {
            fail("Should not have thrown any exception");
        }
    }

    @Test
    public void test_ns_k_list_remove_null_namespace() {
        try {
            keyedState.remove(key, null);
        } catch (Exception e) {
            fail("Should not have thrown any exception");
        }
    }

    @Test
    public void test_ns_k_list_remove1_normal() {
        String testRemove1 = "testRemove1";
        value.add(testRemove1);
        try {
            Assert.assertEquals(2, value.size());
            boolean res = keyedState.remove(key, namespace, element);
            Assert.assertTrue(res);
            Assert.assertEquals(1, value.size());
            Assert.assertEquals(testRemove1, value.get(0));
        } catch (Exception e) {
            fail("Should not have thrown any exception");
        }
        value.clear();
        value.add(element);
    }

    @Test
    public void test_ns_k_list_remove1_null_key() {
        try {
            boolean res = keyedState.remove(null, namespace, element);
            Assert.assertFalse(res);
        } catch (Exception e) {
            fail("Should not have thrown any exception");
        }
    }

    @Test
    public void test_ns_k_list_remove1_null_namespace() {
        try {
            boolean res = keyedState.remove(key, null, element);
            Assert.assertFalse(res);
        } catch (Exception e) {
            fail("Should not have thrown any exception");
        }
    }

    @Test
    public void test_ns_k_list_remove1_null_element() {
        try {
            boolean res = keyedState.remove(key, namespace, null);
            Assert.assertTrue(res);
        } catch (Exception e) {
            fail("Should not have thrown any exception");
        }
    }

    @Test
    public void test_ns_k_list_remove1_null_value() {
        try {
            boolean res = keyedState.remove(notExistKey, notExistNamespace, element);
            Assert.assertTrue(res);
        } catch (Exception e) {
            fail("Should not have thrown any exception");
        }
    }

    @Test
    public void test_ns_k_list_remove1_empty_after_remove() {
        try {
            Assert.assertEquals(1, value.size());
            boolean res = keyedState.remove(key, namespace, element);
            Assert.assertTrue(res);
            Assert.assertTrue(value.isEmpty());
        } catch (Exception e) {
            fail("Should not have thrown any exception");
        }
    }

    @Test
    public void test_ns_k_list_removeAll_normal() {
        String testRemoveAll = "testRemoveAll";
        value.add(testRemoveAll);
        try {
            Assert.assertEquals(2, value.size());
            boolean res = keyedState.removeAll(key, namespace, Arrays.asList(element));
            Assert.assertTrue(res);
            Assert.assertEquals(1, value.size());
            Assert.assertEquals(testRemoveAll, value.get(0));
        } catch (Exception e) {
            fail("Should not have thrown any exception");
        }
    }

    @Test
    public void test_ns_k_list_removeAll_null_key() {
        try {
            boolean res = keyedState.removeAll(null, namespace, Arrays.asList(element));
            Assert.assertFalse(res);
            Assert.assertEquals(1, value.size());
        } catch (Exception e) {
            fail("Should not have thrown any exception");
        }
    }

    @Test
    public void test_ns_k_list_removeAll_null_namespace() {
        try {
            boolean res = keyedState.removeAll(key, null, Arrays.asList(element));
            Assert.assertFalse(res);
            Assert.assertEquals(1, value.size());
        } catch (Exception e) {
            fail("Should not have thrown any exception");
        }
    }

    @Test
    public void test_ns_k_list_removeAll_null_elements() {
        try {
            boolean res = keyedState.removeAll(key, namespace, null);
            Assert.assertFalse(res);
            Assert.assertEquals(1, value.size());
        } catch (Exception e) {
            fail("Should not have thrown any exception");
        }
    }

    @Test
    public void test_ns_k_list_removeAll_empty_elements() {
        try {
            boolean res = keyedState.removeAll(key, namespace, new ArrayList<>());
            Assert.assertFalse(res);
            Assert.assertEquals(1, value.size());
        } catch (Exception e) {
            fail("Should not have thrown any exception");
        }
    }

    @Test
    public void test_ns_k_list_removeAll_value_null() {
        try {
            boolean res = keyedState.removeAll(notExistKey, notExistNamespace, Arrays.asList(element));
            Assert.assertTrue(res);
            Assert.assertEquals(1, value.size());
        } catch (Exception e) {
            fail("Should not have thrown any exception");
        }
    }

    @Test
    public void test_ns_k_list_removeAll_empty_after_removeAll() {
        try {
            boolean res = keyedState.removeAll(key, namespace, Arrays.asList(element));
            Assert.assertTrue(res);
            Assert.assertEquals(0, value.size());
        } catch (Exception e) {
            fail("Should not have thrown any exception");
        }
    }

    @Test
    public void test_ns_k_list_contains_normal() {
        try {
            boolean res = keyedState.contains(key, namespace);
            Assert.assertTrue(res);
        } catch (Exception e) {
            fail("Should not have thrown any exception");
        }
    }

    @Test
    public void test_ns_k_list_contains_null_key() {
        try {
            boolean res = keyedState.contains(null, namespace);
            Assert.assertFalse(res);
        } catch (Exception e) {
            fail("Should not have thrown any exception");
        }
    }

    @Test
    public void test_ns_k_list_contains_null_namespace() {
        try {
            boolean res = keyedState.contains(key, null);
            Assert.assertFalse(res);
        } catch (Exception e) {
            fail("Should not have thrown any exception");
        }
    }

    @Test
    public void test_ns_k_list_get_normal() {
        try {
            List<String> res = keyedState.get(key, namespace);
            Assert.assertEquals(1, res.size());
            Assert.assertEquals(element, res.get(0));
        } catch (Exception e) {
            fail("Should not have thrown any exception");
        }
    }

    @Test
    public void test_ns_k_list_get_null_key() {
        try {
            List<String> res = keyedState.get(null, namespace);
            Assert.assertNull(res);
        } catch (Exception e) {
            fail("Should not have thrown any exception");
        }
    }

    @Test
    public void test_ns_k_list_get_null_namespace() {
        try {
            List<String> res = keyedState.get(key, null);
            Assert.assertNull(res);
        } catch (Exception e) {
            fail("Should not have thrown any exception");
        }
    }

    @Test(expected = UnsupportedOperationException.class)
    public void test_ns_k_list_getAll() {
        keyedState.getAll(key);
    }

    @Test(expected = UnsupportedOperationException.class)
    public void test_ns_k_list_removeAll() {
        keyedState.removeAll(key);
    }

    @Test(expected = UnsupportedOperationException.class)
    public void test_ns_k_list_poll() {
        keyedState.poll(key, namespace);
    }

    @Test(expected = UnsupportedOperationException.class)
    public void test_ns_k_list_peek() {
        keyedState.peek(key, namespace);
    }

    @Test(expected = UnsupportedOperationException.class)
    public void test_ns_k_list_iterator() {
        keyedState.iterator(key);
    }

    @Test
    public void test_ns_k_list_getDescriptor_normal() {
        Assert.assertNotNull(keyedState.getDescriptor());
    }

    @Test
    public void test_k_list_getSerializedValue_normal() throws IOException {
        byte[] serializeKeyAndNamespace =
            KvStateSerializer.serializeKeyAndNamespace(key, StringSerializer.INSTANCE, namespace,
                StringSerializer.INSTANCE);
        byte[] serializedValue = keyedState.getSerializedValue(serializeKeyAndNamespace, StringSerializer.INSTANCE,
            StringSerializer.INSTANCE, new ListSerializer<>(StringSerializer.INSTANCE));
        List<String> list = KvStateSerializer.deserializeList(serializedValue, StringSerializer.INSTANCE);
        Assert.assertNotNull(serializedValue);
        Assert.assertNotNull(list);
        Assert.assertEquals(1, list.size());
        Assert.assertEquals(value.get(0), list.get(0));
    }

    @Test
    public void test_k_list_getSerializedValue_value_null() throws IOException {
        byte[] serializeKeyAndNamespace =
            KvStateSerializer.serializeKeyAndNamespace(notExistKey, StringSerializer.INSTANCE, notExistNamespace,
                StringSerializer.INSTANCE);
        byte[] serializedValue = keyedState.getSerializedValue(serializeKeyAndNamespace, StringSerializer.INSTANCE,
            StringSerializer.INSTANCE, new ListSerializer<>(StringSerializer.INSTANCE));
        Assert.assertNull(serializedValue);
    }
}
