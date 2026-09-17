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
import static org.mockito.ArgumentMatchers.anyMap;
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.ArgumentMatchers.argThat;
import static org.mockito.ArgumentMatchers.eq;

import com.huawei.ock.bss.state.internal.descriptor.nskeyed.NSKeyedMapStateDescriptor;
import com.huawei.ock.bss.table.namespace.NsKMapTableImpl;
import com.huawei.ock.bss.table.api.NsKMapTable;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.powermock.api.mockito.PowerMockito;
import org.powermock.core.classloader.annotations.PrepareForTest;
import org.powermock.modules.junit4.PowerMockRunner;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.Iterator;
import java.util.List;
import java.util.Map;

/**
 * NSKeyedMapStateTest
 *
 * @since BeiMing 25.0.T1
 */
@RunWith(PowerMockRunner.class)
@PrepareForTest(value = NSKeyedMapStateImpl.class)
public class NSKeyedMapStateTest {
    private NSKeyedMapState<String, String, String, String> keyedState;

    private final String notExistKey = "notExistKey";

    private final String key = "testKey";

    private final String namespace = "testNamespace";

    private final String userKey = "userKey";

    private final String userValue = "userValue";

    private final Map<String, String> value = new HashMap<>();

    private final Map<String, String> emptyValue = new HashMap<>();

    @Before
    public void setUp() throws Exception {
        value.clear();
        value.put(userKey, userValue);
        NSKeyedMapStateDescriptor<String, String, String, String> descriptor =
            PowerMockito.mock(NSKeyedMapStateDescriptor.class);
        NsKMapTable<String, String, String, String, Map<String, String>> table =
            PowerMockito.mock(NsKMapTableImpl.class);
        PowerMockito.doNothing().when(table, "add", any(), anyString(), anyString());
        PowerMockito.doNothing().when(table, "addAll", any(), anyMap());
        PowerMockito.doNothing().when(table, "remove", any(), anyString());
        PowerMockito.doNothing().when(table, "remove", any());
        PowerMockito.when(table, "contains", any()).thenReturn(true);
        PowerMockito.when(table, "contains", any(), anyString()).thenReturn(true);
        PowerMockito.when(table, "iterator", any()).thenReturn(null);
        PowerMockito.when(table.getKeyPair(key, namespace)).thenCallRealMethod();
        PowerMockito.doReturn(value)
            .when(table)
            .get(argThat(keyPair -> key.equals(keyPair.getFirstKey()) && namespace.equals(keyPair.getSecondKey())));
        PowerMockito.doReturn(userValue)
            .when(table)
            .get(argThat(keyPair -> key.equals(keyPair.getFirstKey()) && namespace.equals(keyPair.getSecondKey())),
                eq(userKey));

        keyedState = PowerMockito.spy(new NSKeyedMapStateImpl<>(descriptor, table));
    }

    @After
    public void tearDown() {

    }

    @Test
    public void test_ns_k_map_add_normal() {
        try {
            keyedState.add(key, namespace, userKey, userValue);
        } catch (Exception e) {
            fail("Should not have thrown any exception");
        }
    }

    @Test
    public void test_ns_k_map_add_null_key() {
        try {
            keyedState.add(null, namespace, userKey, userValue);
        } catch (Exception e) {
            fail("Should not have thrown any exception");
        }
    }

    @Test
    public void test_ns_k_map_add_null_namespace() {
        try {
            keyedState.add(key, null, userKey, userValue);
        } catch (Exception e) {
            fail("Should not have thrown any exception");
        }
    }

    @Test
    public void test_ns_k_map_add_null_userKey() {
        try {
            keyedState.add(key, namespace, null, userValue);
        } catch (Exception e) {
            fail("Should not have thrown any exception");
        }
    }

    @Test
    public void test_ns_k_map_add_null_userValue() {
        try {
            keyedState.add(key, namespace, userKey, null);
        } catch (Exception e) {
            fail("Should not have thrown any exception");
        }
    }

    @Test
    public void test_ns_k_map_addAll_normal() {
        try {
            keyedState.addAll(key, namespace, value);
        } catch (Exception e) {
            fail("Should not have thrown any exception");
        }
    }

    @Test
    public void test_ns_k_map_addAll_null_key() {
        try {
            keyedState.addAll(null, namespace, value);
        } catch (Exception e) {
            fail("Should not have thrown any exception");
        }
    }

    @Test
    public void test_ns_k_map_addAll_null_namespace() {
        try {
            keyedState.addAll(key, null, value);
        } catch (Exception e) {
            fail("Should not have thrown any exception");
        }
    }

    @Test
    public void test_ns_k_map_addAll_null_value() {
        try {
            keyedState.addAll(key, namespace, null);
        } catch (Exception e) {
            fail("Should not have thrown any exception");
        }
    }

    @Test
    public void test_ns_k_map_addAll_empty_value() {
        try {
            keyedState.addAll(key, namespace, emptyValue);
        } catch (Exception e) {
            fail("Should not have thrown any exception");
        }
    }

    @Test
    public void test_ns_k_map_remove_normal() {
        try {
            keyedState.remove(key, namespace, userKey);
        } catch (Exception e) {
            fail("Should not have thrown any exception");
        }
    }

    @Test
    public void test_ns_k_map_remove_null_key() {
        try {
            keyedState.remove(null, namespace, userKey);
        } catch (Exception e) {
            fail("Should not have thrown any exception");
        }
    }

    @Test
    public void test_ns_k_map_remove_null_namespace() {
        try {
            keyedState.remove(key, null, userKey);
        } catch (Exception e) {
            fail("Should not have thrown any exception");
        }
    }

    @Test
    public void test_ns_k_map_remove_null_userKey() {
        try {
            keyedState.remove(key, namespace, null);
        } catch (Exception e) {
            fail("Should not have thrown any exception");
        }
    }

    @Test
    public void test_ns_k_map_removeAll_normal() {
        List<String> userKeys = new ArrayList<>();
        userKeys.add(userKey);
        try {
            keyedState.removeAll(key, namespace, userKeys);
        } catch (Exception e) {
            fail("Should not have thrown any exception");
        }
    }

    @Test
    public void test_ns_k_map_removeAll_null_key() {
        List<String> userKeys = new ArrayList<>();
        userKeys.add(userKey);
        try {
            keyedState.removeAll(null, namespace, userKeys);
        } catch (Exception e) {
            fail("Should not have thrown any exception");
        }
    }

    @Test
    public void test_ns_k_map_removeAll_null_namespace() {
        List<String> userKeys = new ArrayList<>();
        userKeys.add(userKey);
        try {
            keyedState.removeAll(key, null, userKeys);
        } catch (Exception e) {
            fail("Should not have thrown any exception");
        }
    }

    @Test
    public void test_ns_k_map_removeAll_null_userKeys() {
        try {
            keyedState.removeAll(key, namespace, null);
        } catch (Exception e) {
            fail("Should not have thrown any exception");
        }
    }

    @Test
    public void test_ns_k_map_removeAll_empty_userKeys() {
        List<String> userKeys = new ArrayList<>();
        try {
            keyedState.removeAll(key, namespace, userKeys);
        } catch (Exception e) {
            fail("Should not have thrown any exception");
        }
    }

    @Test
    public void test_ns_k_map_remove1_normal() {
        try {
            keyedState.remove(key, namespace);
        } catch (Exception e) {
            fail("Should not have thrown any exception");
        }
    }

    @Test
    public void test_ns_k_map_remove1_null_key() {
        try {
            keyedState.remove(null, namespace);
        } catch (Exception e) {
            fail("Should not have thrown any exception");
        }
    }

    @Test
    public void test_ns_k_map_remove1_null_namespace() {
        try {
            keyedState.remove(key, null);
        } catch (Exception e) {
            fail("Should not have thrown any exception");
        }
    }

    @Test
    public void test_ns_k_map_contains_normal() {
        try {
            boolean res = keyedState.contains(key, namespace, userKey);
            Assert.assertTrue(res);
        } catch (Exception e) {
            fail("Should not have thrown any exception");
        }
    }

    @Test
    public void test_ns_k_map_contains_null_key() {
        try {
            boolean res = keyedState.contains(null, namespace, userKey);
            Assert.assertFalse(res);
        } catch (Exception e) {
            fail("Should not have thrown any exception");
        }
    }

    @Test
    public void test_ns_k_map_contains_null_namespace() {
        try {
            boolean res = keyedState.contains(key, null, userKey);
            Assert.assertFalse(res);
        } catch (Exception e) {
            fail("Should not have thrown any exception");
        }
    }

    @Test
    public void test_ns_k_map_contains_null_userKey() {
        try {
            boolean res = keyedState.contains(key, namespace, null);
            Assert.assertFalse(res);
        } catch (Exception e) {
            fail("Should not have thrown any exception");
        }
    }

    @Test
    public void test_ns_k_map_contains1_normal() {
        try {
            boolean res = keyedState.contains(key, namespace);
            Assert.assertTrue(res);
        } catch (Exception e) {
            fail("Should not have thrown any exception");
        }
    }

    @Test
    public void test_ns_k_map_contains1_null_key() {
        try {
            boolean res = keyedState.contains(null, namespace);
            Assert.assertFalse(res);
        } catch (Exception e) {
            fail("Should not have thrown any exception");
        }
    }

    @Test
    public void test_ns_k_map_contains1_null_namespace() {
        try {
            boolean res = keyedState.contains(key, null);
            Assert.assertFalse(res);
        } catch (Exception e) {
            fail("Should not have thrown any exception");
        }
    }

    @Test
    public void test_ns_k_map_get_normal() {
        try {
            String res = keyedState.get(key, namespace, userKey);
            Assert.assertEquals(userValue, res);
        } catch (Exception e) {
            fail("Should not have thrown any exception");
        }
    }

    @Test
    public void test_ns_k_map_get_null_key() {
        try {
            String res = keyedState.get(null, namespace, userKey);
            Assert.assertNull(res);
        } catch (Exception e) {
            fail("Should not have thrown any exception");
        }
    }

    @Test
    public void test_ns_k_map_get_null_namespace() {
        try {
            String res = keyedState.get(key, null, userKey);
            Assert.assertNull(res);
        } catch (Exception e) {
            fail("Should not have thrown any exception");
        }
    }

    @Test
    public void test_ns_k_map_get_null_userKey() {
        try {
            String res = keyedState.get(key, namespace, null);
            Assert.assertNull(res);
        } catch (Exception e) {
            fail("Should not have thrown any exception");
        }
    }

    @Test
    public void test_ns_k_map_get1_normal() {
        try {
            Map<String, String> res = keyedState.get(key, namespace);
            Assert.assertEquals(value, res);
        } catch (Exception e) {
            fail("Should not have thrown any exception");
        }
    }

    @Test
    public void test_ns_k_map_get1_null_key() {
        try {
            Map<String, String> res = keyedState.get(null, namespace);
            Assert.assertNull(res);
        } catch (Exception e) {
            fail("Should not have thrown any exception");
        }
    }

    @Test
    public void test_ns_k_map_get1_null_namespace() {
        try {
            Map<String, String> res = keyedState.get(key, null);
            Assert.assertNull(res);
        } catch (Exception e) {
            fail("Should not have thrown any exception");
        }
    }

    @Test
    public void test_ns_k_map_getAll_normal() {
        List<String> userKeySet = new ArrayList<>();
        userKeySet.add(userKey);
        try {
            Map<String, String> res = keyedState.getAll(key, namespace, userKeySet);
            Assert.assertEquals(value, res);
        } catch (Exception e) {
            fail("Should not have thrown any exception");
        }
    }

    @Test
    public void test_ns_k_map_getAll_null_key() {
        List<String> userKeySet = new ArrayList<>();
        userKeySet.add(userKey);
        try {
            Map<String, String> res = keyedState.getAll(null, namespace, userKeySet);
            Assert.assertTrue(res.isEmpty());
        } catch (Exception e) {
            fail("Should not have thrown any exception");
        }
    }

    @Test
    public void test_ns_k_map_getAll_null_namespace() {
        List<String> userKeySet = new ArrayList<>();
        userKeySet.add(userKey);
        try {
            Map<String, String> res = keyedState.getAll(key, null, userKeySet);
            Assert.assertTrue(res.isEmpty());
        } catch (Exception e) {
            fail("Should not have thrown any exception");
        }
    }

    @Test
    public void test_ns_k_map_getAll_null_userKeySet() {
        try {
            Map<String, String> res = keyedState.getAll(key, namespace, null);
            Assert.assertTrue(res.isEmpty());
        } catch (Exception e) {
            fail("Should not have thrown any exception");
        }
    }

    @Test
    public void test_ns_k_map_getAll_empty_userKeySet() {
        List<String> userKeySet = new ArrayList<>();
        try {
            Map<String, String> res = keyedState.getAll(key, namespace, userKeySet);
            Assert.assertTrue(res.isEmpty());
        } catch (Exception e) {
            fail("Should not have thrown any exception");
        }
    }

    @Test(expected = UnsupportedOperationException.class)
    public void test_ns_k_map_getAll1() {
        keyedState.getAll(key);
    }

    @Test(expected = UnsupportedOperationException.class)
    public void test_ns_k_map_removeAll() {
        keyedState.removeAll(key);
    }

    @Test(expected = UnsupportedOperationException.class)
    public void test_ns_k_map_iterator() {
        keyedState.iterator(key);
    }

    @Test
    public void test_ns_k_map_iterator1() {
        Iterator<Map.Entry<String, String>> iterator = keyedState.iterator(key, namespace);
        Assert.assertFalse(iterator.hasNext());
    }
}
