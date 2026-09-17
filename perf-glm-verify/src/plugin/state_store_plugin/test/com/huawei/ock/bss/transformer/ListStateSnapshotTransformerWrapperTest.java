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

package com.huawei.ock.bss.transformer;

import com.huawei.ock.bss.common.serialize.KVSerializerUtil;

import org.apache.flink.api.common.typeutils.TypeSerializer;
import org.apache.flink.api.common.typeutils.base.StringSerializer;
import org.apache.flink.core.memory.DataInputDeserializer;
import org.apache.flink.runtime.state.StateSnapshotTransformer;
import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;

import java.io.IOException;
import java.util.ArrayList;
import java.util.List;

import javax.annotation.Nullable;

/**
 * ListStateSnapshotTransformerWrapperTest
 *
 * @since BeiMing 25.0.T1
 */
public class ListStateSnapshotTransformerWrapperTest {
    private ListStateSnapshotTransformerWrapper<String> transformerWrapper;

    private final TypeSerializer<String> elementSerializer = new StringSerializer();

    @Before
    public void setUp() {
        transformerWrapper = new ListStateSnapshotTransformerWrapper<>(new TestSnapshotTransformer(),
            elementSerializer);
    }

    private static class TestSnapshotTransformer implements StateSnapshotTransformer<String> {
        @Nullable
        @Override
        public String filterOrTransform(@Nullable String s) {
            return s + "-t";
        }
    }

    @After
    public void tearDown() {
    }

    @Test
    public void test_filterOrTransform() throws IOException {
        List<String> testList = new ArrayList<>();
        testList.add("str1");
        testList.add("str2");
        testList.add("str3");
        List<String> destList = new ArrayList<>();
        destList.add("str1-t");
        destList.add("str2-t");
        destList.add("str3-t");
        byte[] serializedData = KVSerializerUtil.getCopyOfBuffer(KVSerializerUtil.serList(testList, elementSerializer));
        byte[] transformedData = transformerWrapper.filterOrTransform(serializedData);
        Assert.assertNotNull(transformedData);
        List<String> transformedList = toList(transformedData, elementSerializer, new DataInputDeserializer());
        Assert.assertEquals(testList.size(), transformedList.size());
        transformedList.forEach(e -> Assert.assertTrue(destList.contains(e)));
    }

    @Test
    public void test_filterOrTransform_multi_list() throws IOException {
        List<String> testList1 = new ArrayList<>();
        testList1.add("str1");
        List<String> testList2 = new ArrayList<>();
        testList2.add("str2");
        testList2.add("str3");
        List<String> destList = new ArrayList<>();
        destList.add("str1-t");
        destList.add("str2-t");
        destList.add("str3-t");
        byte[] serializedData1 = KVSerializerUtil.getCopyOfBuffer(
            KVSerializerUtil.serList(testList1, elementSerializer));
        byte[] serializedData2 = KVSerializerUtil.getCopyOfBuffer(
            KVSerializerUtil.serList(testList2, elementSerializer));
        byte[] serializedDatas = new byte[serializedData1.length + serializedData2.length];
        System.arraycopy(serializedData1, 0, serializedDatas, 0, serializedData1.length);
        System.arraycopy(serializedData2, 0, serializedDatas, serializedData1.length, serializedData2.length);
        byte[] transformedData = transformerWrapper.filterOrTransform(serializedDatas);
        Assert.assertNotNull(transformedData);
        List<String> transformedList = toList(transformedData, elementSerializer, new DataInputDeserializer());
        Assert.assertEquals(testList1.size() + testList2.size(), transformedList.size());
        transformedList.forEach(e -> Assert.assertTrue(destList.contains(e)));
    }

    private static <E> List<E> toList(byte[] data, TypeSerializer<E> elementSerializer,
        DataInputDeserializer inputView) throws IOException {
        inputView.setBuffer(data);
        List<E> list = new ArrayList<>();

        while (inputView.available() != 0) {
            list.addAll(toSingleList(elementSerializer, inputView));
        }

        return list;
    }

    private static <E> List<E> toSingleList(TypeSerializer<E> elementSerializer,
        DataInputDeserializer inputView) throws IOException {
        int size = inputView.readInt();
        List<E> list = new ArrayList<>(size);

        for (int i = 0; i < size; i++) {
            list.add(elementSerializer.deserialize(inputView));
        }

        inputView.skipBytes(size * 4);
        return list;
    }
}
