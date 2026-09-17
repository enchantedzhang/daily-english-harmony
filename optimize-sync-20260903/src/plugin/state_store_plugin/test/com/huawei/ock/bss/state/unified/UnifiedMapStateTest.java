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
import static org.mockito.ArgumentMatchers.anyMap;
import static org.mockito.ArgumentMatchers.anyString;

import com.huawei.ock.bss.state.internal.KeyedMapState;
import com.huawei.ock.bss.state.internal.KeyedMapStateImpl;

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

import java.util.HashMap;
import java.util.Iterator;
import java.util.Map;

/**
 * UnifiedMapStateTest
 *
 * @since BeiMing 25.0.T1
 */
@RunWith(PowerMockRunner.class)
@PrepareForTest(value = {UnifiedMapState.class})
public class UnifiedMapStateTest {
    private UnifiedMapState<String, String, String> mapState;

    private final String key = "testKey";

    private final String userKey = "userKey";

    private final String userValue = "userValue";

    private final Map<String, String> value = new HashMap<>();

    private final Map<String, String> emptyValue = new HashMap<>();

    @Before
    public void setUp() throws Exception {
        value.clear();
        value.put(userKey, userValue);
        InternalKeyContext<String> keyContext = PowerMockito.mock(InternalKeyContextImpl.class);
        PowerMockito.when(keyContext.getCurrentKey()).thenReturn(key);
        KeyedMapState<String, String, String> keyedState = PowerMockito.mock(KeyedMapStateImpl.class);
        PowerMockito.doNothing().when(keyedState, "add", anyString(), anyString(), anyString());
        PowerMockito.doNothing().when(keyedState, "addAll", anyString(), anyMap());
        PowerMockito.doNothing().when(keyedState, "remove", anyString());
        PowerMockito.doNothing().when(keyedState, "remove", anyString(), anyString());
        PowerMockito.when(keyedState.contains(key, userKey)).thenReturn(true);
        PowerMockito.when(keyedState.contains(key)).thenReturn(false);
        PowerMockito.when(keyedState.get(key, userKey)).thenReturn(userValue);
        PowerMockito.when(keyedState.iterator(key)).thenReturn(new Iterator<Map.Entry<String, String>>() {
            @Override
            public boolean hasNext() {
                return false;
            }

            @Override
            public Map.Entry<String, String> next() {
                return null;
            }
        });

        mapState = PowerMockito.spy(new UnifiedMapState<>(keyContext, keyedState));
    }

    @After
    public void tearDown() {

    }

    @Test
    public void test_map_put_normal() {
        try {
            mapState.put(userKey, userValue);
        } catch (Exception e) {
            fail("Should not have thrown any exception");
        }
    }

    @Test
    public void test_map_put_null_userKey() {
        try {
            mapState.put(null, userValue);
        } catch (Exception e) {
            fail("Should not have thrown any exception");
        }
    }

    @Test
    public void test_map_put_null_userValue() {
        try {
            mapState.put(userKey, null);
        } catch (Exception e) {
            fail("Should not have thrown any exception");
        }
    }

    @Test
    public void test_map_putAll_normal() {
        try {
            mapState.putAll(value);
        } catch (Exception e) {
            fail("Should not have thrown any exception");
        }
    }

    @Test
    public void test_map_putAll_null_value() {
        try {
            mapState.putAll(null);
        } catch (Exception e) {
            fail("Should not have thrown any exception");
        }
    }

    @Test
    public void test_map_putAll_empty_value() {
        try {
            mapState.putAll(emptyValue);
        } catch (Exception e) {
            fail("Should not have thrown any exception");
        }
    }

    @Test
    public void test_map_remove_normal() {
        try {
            mapState.remove(userKey);
        } catch (Exception e) {
            fail("Should not have thrown any exception");
        }
    }

    @Test
    public void test_map_remove_null_userKey() {
        try {
            mapState.remove(null);
        } catch (Exception e) {
            fail("Should not have thrown any exception");
        }
    }

    @Test
    public void test_map_contains_normal() {
        try {
            boolean res = mapState.contains(userKey);
            Assert.assertTrue(res);
        } catch (Exception e) {
            fail("Should not have thrown any exception");
        }
    }

    @Test
    public void test_map_contains_null_userKey() {
        try {
            boolean res = mapState.contains(null);
            Assert.assertFalse(res);
        } catch (Exception e) {
            fail("Should not have thrown any exception");
        }
    }

    @Test
    public void test_map_get_normal() {
        try {
            String res = mapState.get(userKey);
            Assert.assertEquals(userValue, res);
        } catch (Exception e) {
            fail("Should not have thrown any exception");
        }
    }

    @Test
    public void test_map_get_null_userKey() {
        try {
            String res = mapState.get(null);
            Assert.assertNull(res);
        } catch (Exception e) {
            fail("Should not have thrown any exception");
        }
    }

    @Test
    public void test_map_isEmpty_normal() {
        try {
            boolean res = mapState.isEmpty();
            Assert.assertTrue(res);
        } catch (Exception e) {
            fail("Should not have thrown any exception");
        }
    }

    @Test
    public void test_map_iterator_normal() {
        try {
            Iterator<Map.Entry<String, String>> it = mapState.iterator();
            Assert.assertNotNull(it);
        } catch (Exception e) {
            fail("Should not have thrown any exception");
        }
    }

    @Test
    public void test_map_clear_normal() {
        try {
            mapState.clear();
        } catch (Exception e) {
            fail("Should not have thrown any exception");
        }
    }
}
