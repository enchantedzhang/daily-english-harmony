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

package com.huawei.ock.bss.common.consistency;

import com.huawei.ock.bss.common.memory.DirectBuffer;
import com.huawei.ock.bss.common.serialize.KVSerializerUtil;

import org.apache.flink.api.common.typeutils.TypeSerializer;

import java.util.Arrays;
import java.util.Collection;
import java.util.List;

/**
 * debugMap ListTable的参数builder
 *
 * @param <K> ListTable的 key
 * @param <E> ListTable的 value
 * @since BeiMing 25.0.T1
 */
public class ListTableUtilParam<K, E> {
    private boolean actualContains;
    private int keyHashCode;
    private K key;
    private E element;
    private DirectBuffer keyBytes;
    private List<E> valueList;
    private List<DirectBuffer> directBuffers;
    private Collection<? extends E> elements;
    private TypeSerializer<E> elementSerializer;

    private ListTableUtilParam() {
    }

    public boolean getActualContains() {
        return actualContains;
    }

    public int getKeyHashCode() {
        return keyHashCode;
    }

    public K getKey() {
        return key;
    }

    public E getElement() {
        return element;
    }

    /**
     * getKeyStr 获取key字符串
     *
     * @return key string
     */
    public String getKeyStr() {
        return Arrays.toString(KVSerializerUtil.toUnsignedArray(KVSerializerUtil.getCopyOfBuffer(keyBytes)));
    }

    public List<E> getValueList() {
        return valueList;
    }

    public List<DirectBuffer> getDirectBuffers() {
        return directBuffers;
    }

    public Collection<? extends E> getElements() {
        return elements;
    }

    public TypeSerializer<E> getElementSerializer() {
        return elementSerializer;
    }

    /**
     * Builder
     *
     * @param <K> ListTable的 key
     * @param <E> ListTable的 value
     */
    public static class Builder<K, E> {
        private boolean actualContains;
        private int keyHashCode;
        private K key;
        private E element;
        private DirectBuffer keyBytes;
        private List<E> valueList;
        private List<DirectBuffer> directBuffers;
        private Collection<? extends E> elements;
        private TypeSerializer<E> elementSerializer;


        /**
         * build 构建参数
         *
         * @return debugMap参数builder
         */
        public ListTableUtilParam<K, E> build() {
            ListTableUtilParam<K, E> builder = new ListTableUtilParam<>();
            builder.actualContains = actualContains;
            builder.keyHashCode = keyHashCode;
            builder.key = key;
            builder.element = element;
            builder.keyBytes = keyBytes;
            builder.valueList = valueList;
            builder.directBuffers = directBuffers;
            builder.elements = elements;
            builder.elementSerializer = elementSerializer;
            return builder;
        }

        public Builder<K, E> setActualContains(boolean actualContains) {
            this.actualContains = actualContains;
            return this;
        }

        public Builder<K, E> setKeyHashCode(int keyHashCode) {
            this.keyHashCode = keyHashCode;
            return this;
        }

        public Builder<K, E> setKey(K key) {
            this.key = key;
            return this;
        }

        public Builder<K, E> setElement(E element) {
            this.element = element;
            return this;
        }

        public Builder<K, E> setKeyBytes(DirectBuffer keyBytes) {
            this.keyBytes = keyBytes;
            return this;
        }

        public Builder<K, E> setValueList(List<E> valueList) {
            this.valueList = valueList;
            return this;
        }

        public Builder<K, E> setDirectBuffers(List<DirectBuffer> directBuffers) {
            this.directBuffers = directBuffers;
            return this;
        }

        public Builder<K, E> setElements(Collection<? extends E> elements) {
            this.elements = elements;
            return this;
        }

        public Builder<K, E> setElementSerializer(TypeSerializer<E> elementSerializer) {
            this.elementSerializer = elementSerializer;
            return this;
        }
    }
}
