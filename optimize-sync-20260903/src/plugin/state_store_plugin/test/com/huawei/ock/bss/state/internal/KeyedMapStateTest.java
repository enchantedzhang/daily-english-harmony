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
import static org.mockito.ArgumentMatchers.anyMap;
import static org.mockito.ArgumentMatchers.anyString;

import com.huawei.ock.bss.state.internal.descriptor.keyed.KeyedMapStateDescriptor;
import com.huawei.ock.bss.table.KMapTableImpl;
import com.huawei.ock.bss.table.api.KMapTable;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.powermock.api.mockito.PowerMockito;
import org.powermock.core.classloader.annotations.PrepareForTest;
import org.powermock.modules.junit4.PowerMockRunner;

import java.util.ArrayList;
import java.util.Collection;
import java.util.HashMap;
import java.util.Iterator;
import java.util.List;
import java.util.Map;

/**
 * KeyedMapStateTest
 *
 * @since BeiMing 25.0.T1
 */
@RunWith(PowerMockRunner.class)
@PrepareForTest(value = KeyedMapStateImpl.class)
public class KeyedMapStateTest {
    private KeyedMapStateImpl<String, String, String> keyedState;

    private final String notExistKey = "notExistKey";

    private final String key = "testKey";

    private final String userKey = "userKey";

    private final String userValue = "userValue";

    private final Map<String, String> value = new HashMap<>();

    @Before
    public void setUp() throws Exception {
        value.clear();
        value.put(userKey, userValue);
        KeyedMapStateDescriptor<String, String, String> descriptor = PowerMockito.mock(KeyedMapStateDescriptor.class);
        KMapTable<String, String, String, Map<String, String>> table = PowerMockito.mock(KMapTableImpl.class);
        PowerMockito.doNothing().when(table, "add", anyString(), anyString(), anyString());
        PowerMockito.doNothing().when(table, "addAll", anyString(), anyMap());
        PowerMockito.doNothing().when(table, "remove", anyString(), anyString());
        PowerMockito.when(table.contains(key, userKey)).thenReturn(true);
        PowerMockito.when(table.contains(key)).thenReturn(true);
        PowerMockito.when(table.get(key)).thenReturn(value);
        PowerMockito.when(table.get(notExistKey)).thenReturn(null);
        PowerMockito.when(table.get(key, userKey)).thenReturn(userValue);
        PowerMockito.when(table.get(key, notExistKey)).thenReturn(null);
        PowerMockito.when(table.iterator(key)).thenReturn(new Iterator<Map.Entry<String, String>>() {
            @Override
            public boolean hasNext() {
                return false;
            }

            @Override
            public Map.Entry<String, String> next() {
                return null;
            }
        });

        keyedState = PowerMockito.spy(new KeyedMapStateImpl<>(descriptor, table));
    }

    @After
    public void tearDown() {

    }

    @Test
    public void test_k_map_add_normal() {
        try {
            keyedState.add(key, userKey, userValue);
        } catch (Exception e) {
            fail("Should not have thrown any exception");
        }
    }

    @Test
    public void test_k_map_add_null_key() {
        try {
            keyedState.add(null, userKey, userValue);
        } catch (Exception e) {
            fail("Should not have thrown any exception");
        }
    }

    @Test
    public void test_k_map_add_null_userKey() {
        try {
            keyedState.add(key, null, userValue);
        } catch (Exception e) {
            fail("Should not have thrown any exception");
        }
    }

    @Test
    public void test_k_map_add_null_userValue() {
        try {
            keyedState.add(key, userKey, userValue);
        } catch (Exception e) {
            fail("Should not have thrown any exception");
        }
    }

    @Test
    public void test_k_map_addAll_normal() {
        try {
            keyedState.addAll(key, value);
        } catch (Exception e) {
            fail("Should not have thrown any exception");
        }
    }

    @Test
    public void test_k_map_addAll_null_key() {
        try {
            keyedState.addAll(null, value);
        } catch (Exception e) {
            fail("Should not have thrown any exception");
        }
    }

    @Test
    public void test_k_map_addAll_null_value() {
        try {
            keyedState.addAll(key, null);
        } catch (Exception e) {
            fail("Should not have thrown any exception");
        }
    }

    @Test
    public void test_k_map_addAll_empty_value() {
        try {
            keyedState.addAll(key, new HashMap<>());
        } catch (Exception e) {
            fail("Should not have thrown any exception");
        }
    }

    @Test
    public void test_k_map_addAll1_normal() {
        Map<String, Map<String, String>> kMaps = new HashMap<>();
        kMaps.put(key, value);
        try {
            keyedState.addAll(kMaps);
        } catch (Exception e) {
            fail("Should not have thrown any exception");
        }
    }

    @Test
    public void test_k_map_addAll1_null_kMap() {
        try {
            keyedState.addAll(null);
        } catch (Exception e) {
            fail("Should not have thrown any exception");
        }
    }

    @Test
    public void test_k_map_addAll1_empty_kMap() {
        Map<String, Map<String, String>> kMaps = new HashMap<>();
        try {
            keyedState.addAll(kMaps);
        } catch (Exception e) {
            fail("Should not have thrown any exception");
        }
    }

    @Test
    public void test_k_map_remove_normal() {
        try {
            keyedState.remove(key);
        } catch (Exception e) {
            fail("Should not have thrown any exception");
        }
    }

    @Test
    public void test_k_map_remove_null_key() {
        try {
            keyedState.remove(null);
        } catch (Exception e) {
            fail("Should not have thrown any exception");
        }
    }

    @Test
    public void test_k_map_removeAll_normal() {
        List<String> keySet = new ArrayList<>();
        keySet.add(key);
        try {
            keyedState.removeAll(keySet);
        } catch (Exception e) {
            fail("Should not have thrown any exception");
        }
    }

    @Test
    public void test_k_map_removeAll_null_keySet() {
        try {
            keyedState.removeAll((Collection<? extends String>) null);
        } catch (Exception e) {
            fail("Should not have thrown any exception");
        }
    }

    @Test
    public void test_k_map_removeAll_empty_keySet() {
        List<String> keySet = new ArrayList<>();
        try {
            keyedState.removeAll(keySet);
        } catch (Exception e) {
            fail("Should not have thrown any exception");
        }
    }

    @Test
    public void test_k_map_removeAll_null_key() {
        List<String> keySet = new ArrayList<>();
        keySet.add(key);
        keySet.add(null);
        try {
            keyedState.removeAll(keySet);
        } catch (Exception e) {
            fail("Should not have thrown any exception");
        }
    }

    @Test
    public void test_k_map_remove1_normal() {
        try {
            keyedState.remove(key, userKey);
        } catch (Exception e) {
            fail("Should not have thrown any exception");
        }
    }

    @Test
    public void test_k_map_remove1_null_key() {
        try {
            keyedState.remove(null, userKey);
        } catch (Exception e) {
            fail("Should not have thrown any exception");
        }
    }

    @Test
    public void test_k_map_remove1_null_userKey() {
        try {
            keyedState.remove(key, null);
        } catch (Exception e) {
            fail("Should not have thrown any exception");
        }
    }

    @Test
    public void test_k_map_removeAll1_normal() {
        List<String> userKeys = new ArrayList<>();
        userKeys.add(userKey);
        try {
            keyedState.removeAll(key, userKeys);
        } catch (Exception e) {
            fail("Should not have thrown any exception");
        }
    }

    @Test
    public void test_k_map_removeAll1_null_key() {
        List<String> userKeys = new ArrayList<>();
        userKeys.add(userKey);
        try {
            keyedState.removeAll(null, userKeys);
        } catch (Exception e) {
            fail("Should not have thrown any exception");
        }
    }

    @Test
    public void test_k_map_removeAll1_null_userKeys() {
        try {
            keyedState.removeAll(key, null);
        } catch (Exception e) {
            fail("Should not have thrown any exception");
        }
    }

    @Test
    public void test_k_map_removeAll1_empty_userKeys() {
        List<String> userKeys = new ArrayList<>();
        try {
            keyedState.removeAll(key, userKeys);
        } catch (Exception e) {
            fail("Should not have thrown any exception");
        }
    }

    @Test
    public void test_k_map_removeAll1_null_userKey() {
        List<String> userKeys = new ArrayList<>();
        userKeys.add(userKey);
        userKeys.add(null);
        try {
            keyedState.removeAll(key, userKeys);
        } catch (Exception e) {
            fail("Should not have thrown any exception");
        }
    }

    @Test
    public void test_k_map_removeAll2_normal() {
        Map<String, List<String>> kMaps = new HashMap<>();
        List<String> userKeys = new ArrayList<>();
        userKeys.add(userKey);
        kMaps.put(key, userKeys);
        try {
            keyedState.removeAll(kMaps);
        } catch (Exception e) {
            fail("Should not have thrown any exception");
        }
    }

    @Test
    public void test_k_map_removeAll2_null_map() {
        try {
            keyedState.removeAll((Map<? extends String, ? extends Collection<? extends String>>) null);
        } catch (Exception e) {
            fail("Should not have thrown any exception");
        }
    }

    @Test
    public void test_k_map_removeAll2_empty_map() {
        Map<String, List<String>> kMaps = new HashMap<>();
        try {
            keyedState.removeAll(kMaps);
        } catch (Exception e) {
            fail("Should not have thrown any exception");
        }
    }

    @Test
    public void test_k_map_contains_normal() {
        try {
            boolean res = keyedState.contains(key);
            Assert.assertTrue(res);
        } catch (Exception e) {
            fail("Should not have thrown any exception");
        }
    }

    @Test
    public void test_k_map_contains_null_key() {
        try {
            boolean res = keyedState.contains(null);
            Assert.assertFalse(res);
        } catch (Exception e) {
            fail("Should not have thrown any exception");
        }
    }

    @Test
    public void test_k_map_contains1_normal() {
        try {
            boolean res = keyedState.contains(key, userKey);
            Assert.assertTrue(res);
        } catch (Exception e) {
            fail("Should not have thrown any exception");
        }
    }

    @Test
    public void test_k_map_contains1_null_key() {
        try {
            boolean res = keyedState.contains(null, userKey);
            Assert.assertFalse(res);
        } catch (Exception e) {
            fail("Should not have thrown any exception");
        }
    }
    @Test
    public void test_k_map_contains1_null_userKey() {
        try {
            boolean res = keyedState.contains(key, null);
            Assert.assertFalse(res);
        } catch (Exception e) {
            fail("Should not have thrown any exception");
        }
    }

    @Test
    public void test_k_map_get_normal() {
        try {
            Map<String, String> res = keyedState.get(key);
            Assert.assertNotNull(res);
            Assert.assertEquals(1, res.size());
            Assert.assertTrue(res.containsKey(userKey));
            Assert.assertEquals(userValue, res.get(userKey));
        } catch (Exception e) {
            fail("Should not have thrown any exception");
        }
    }

    @Test
    public void test_k_map_get_null_key() {
        try {
            Map<String, String> res = keyedState.get(null);
            Assert.assertNotNull(res);
            Assert.assertTrue(res.isEmpty());
        } catch (Exception e) {
            fail("Should not have thrown any exception");
        }
    }

    @Test
    public void test_k_map_get1_normal() {
        try {
            String res = keyedState.get(key, userKey);
            Assert.assertEquals(userValue, res);
        } catch (Exception e) {
            fail("Should not have thrown any exception");
        }
    }

    @Test
    public void test_k_map_get1_null_key() {
        try {
            String res = keyedState.get(null, userKey);
            Assert.assertNull(res);
        } catch (Exception e) {
            fail("Should not have thrown any exception");
        }
    }

    @Test
    public void test_k_map_get1_null_userKey() {
        try {
            String res = keyedState.get(key, null);
            Assert.assertNull(res);
        } catch (Exception e) {
            fail("Should not have thrown any exception");
        }
    }

    @Test
    public void test_k_map_getAll_normal() {
        List<String> keySet = new ArrayList<>();
        keySet.add(key);
        try {
            Map<String, Map<String, String>> res = keyedState.getAll(keySet);
            Assert.assertNotNull(res);
            Assert.assertEquals(1, res.size());
            Assert.assertTrue(res.containsKey(key));
            Assert.assertEquals(1, res.get(key).size());
            Assert.assertTrue(res.get(key).containsKey(userKey));
            Assert.assertEquals(userValue, res.get(key).get(userKey));
        } catch (Exception e) {
            fail("Should not have thrown any exception");
        }
    }

    @Test
    public void test_k_map_getAll_null_keySet() {
        try {
            Map<String, Map<String, String>> res = keyedState.getAll((Collection<? extends String>) null);
            Assert.assertNotNull(res);
            Assert.assertTrue(res.isEmpty());
        } catch (Exception e) {
            fail("Should not have thrown any exception");
        }
    }

    @Test
    public void test_k_map_getAll_empty_keySet() {
        List<String> keySet = new ArrayList<>();
        try {
            Map<String, Map<String, String>> res = keyedState.getAll(keySet);
            Assert.assertNotNull(res);
            Assert.assertTrue(res.isEmpty());
        } catch (Exception e) {
            fail("Should not have thrown any exception");
        }
    }

    @Test
    public void test_k_map_getAll_abnormal() {
        List<String> keySet = new ArrayList<>();
        keySet.add(key);
        keySet.add(null);
        keySet.add(notExistKey);
        try {
            Map<String, Map<String, String>> res = keyedState.getAll(keySet);
            Assert.assertNotNull(res);
            Assert.assertTrue(res.containsKey(key));
            Assert.assertTrue(res.get(key).containsKey(userKey));
            Assert.assertEquals(userValue, res.get(key).get(userKey));
        } catch (Exception e) {
            fail("Should not have thrown any exception");
        }
    }

    @Test
    public void test_k_map_getAll1_normal() {
        List<String> userKeys = new ArrayList<>();
        userKeys.add(userKey);
        try {
            Map<String, String> res = keyedState.getAll(key, userKeys);
            Assert.assertNotNull(res);
            Assert.assertEquals(1, res.size());
            Assert.assertTrue(res.containsKey(userKey));
            Assert.assertEquals(userValue, res.get(userKey));
        } catch (Exception e) {
            fail("Should not have thrown any exception");
        }
    }

    @Test
    public void test_k_map_getAll1_null_key() {
        List<String> userKeys = new ArrayList<>();
        userKeys.add(userKey);
        try {
            Map<String, String> res = keyedState.getAll(null, userKeys);
            Assert.assertNotNull(res);
            Assert.assertTrue(res.isEmpty());
        } catch (Exception e) {
            fail("Should not have thrown any exception");
        }
    }

    @Test
    public void test_k_map_getAll1_null_userKeys() {
        try {
            Map<String, String> res = keyedState.getAll(key, null);
            Assert.assertNotNull(res);
            Assert.assertTrue(res.isEmpty());
        } catch (Exception e) {
            fail("Should not have thrown any exception");
        }
    }

    @Test
    public void test_k_map_getAll1_empty_userKeys() {
        List<String> userKeys = new ArrayList<>();
        try {
            Map<String, String> res = keyedState.getAll(key, userKeys);
            Assert.assertNotNull(res);
            Assert.assertTrue(res.isEmpty());
        } catch (Exception e) {
            fail("Should not have thrown any exception");
        }
    }

    @Test
    public void test_k_map_getAll1_abnormal() {
        List<String> userKeys = new ArrayList<>();
        userKeys.add(userKey);
        userKeys.add(null);
        userKeys.add(notExistKey);
        try {
            Map<String, String> res = keyedState.getAll(key, userKeys);
            Assert.assertNotNull(res);
            Assert.assertEquals(1, res.size());
            Assert.assertTrue(res.containsKey(userKey));
            Assert.assertEquals(userValue, res.get(userKey));
        } catch (Exception e) {
            fail("Should not have thrown any exception");
        }
    }

    @Test
    public void test_k_map_getAll2_normal() {
        Map<String, List<String>> keySet = new HashMap<>();
        List<String> userKeys = new ArrayList<>();
        userKeys.add(userKey);
        keySet.put(key, userKeys);
        try {
            Map<String, Map<String, String>> res = keyedState.getAll(keySet);
            Assert.assertNotNull(res);
            Assert.assertEquals(1, res.size());
            Assert.assertTrue(res.containsKey(key));
            Assert.assertEquals(1, res.get(key).size());
            Assert.assertTrue(res.get(key).containsKey(userKey));
            Assert.assertEquals(userValue, res.get(key).get(userKey));
        } catch (Exception e) {
            fail("Should not have thrown any exception");
        }
    }

    @Test
    public void test_k_map_getAll2_null_keySet() {
        try {
            Map<String, Map<String, String>> res = keyedState.getAll(
                (Map<String, ? extends Collection<? extends String>>) null);
            Assert.assertNotNull(res);
            Assert.assertTrue(res.isEmpty());
        } catch (Exception e) {
            fail("Should not have thrown any exception");
        }
    }

    @Test
    public void test_k_map_getAll2_empty_keySet() {
        Map<String, List<String>> keySet = new HashMap<>();
        try {
            Map<String, Map<String, String>> res = keyedState.getAll(keySet);
            Assert.assertNotNull(res);
            Assert.assertTrue(res.isEmpty());
        } catch (Exception e) {
            fail("Should not have thrown any exception");
        }
    }

    @Test
    public void test_k_map_getAll2_abnormal() {
        Map<String, List<String>> keySet = new HashMap<>();
        List<String> userKeys = new ArrayList<>();
        userKeys.add(userKey);
        userKeys.add(notExistKey);
        userKeys.add(null);
        keySet.put(key, userKeys);
        try {
            Map<String, Map<String, String>> res = keyedState.getAll(keySet);
            Assert.assertNotNull(res);
            Assert.assertEquals(1, res.size());
            Assert.assertTrue(res.containsKey(key));
            Assert.assertEquals(1, res.get(key).size());
            Assert.assertTrue(res.get(key).containsKey(userKey));
            Assert.assertEquals(userValue, res.get(key).get(userKey));
        } catch (Exception e) {
            fail("Should not have thrown any exception");
        }
    }

    @Test
    public void test_k_map_iterator_normal() {
        try {
            Iterator<Map.Entry<String, String>> res = keyedState.iterator(key);
            Assert.assertNotNull(res);
        } catch (Exception e) {
            fail("Should not have thrown any exception");
        }
    }

    @Test
    public void test_k_map_iterator_null_key() {
        try {
            Iterator<Map.Entry<String, String>> res = keyedState.iterator(null);
            Assert.assertNull(res);
        } catch (Exception e) {
            fail("Should not have thrown any exception");
        }
    }
}
