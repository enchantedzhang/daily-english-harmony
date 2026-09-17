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

package com.huawei.ock.bss.util;

import com.huawei.ock.bss.common.serialize.KeyPairSerializer;
import com.huawei.ock.bss.common.serialize.SubTableSerializer;
import com.huawei.ock.bss.common.serialize.TableSerializer;
import com.huawei.ock.bss.state.internal.descriptor.keyed.KeyedListStateDescriptor;
import com.huawei.ock.bss.state.internal.descriptor.keyed.KeyedMapStateDescriptor;
import com.huawei.ock.bss.state.internal.descriptor.keyed.KeyedStateDescriptor;
import com.huawei.ock.bss.state.internal.descriptor.keyed.KeyedValueStateDescriptor;
import com.huawei.ock.bss.state.internal.descriptor.nskeyed.NSKeyedListStateDescriptor;
import com.huawei.ock.bss.state.internal.descriptor.nskeyed.NSKeyedMapStateDescriptor;
import com.huawei.ock.bss.state.internal.descriptor.nskeyed.NSKeyedStateDescriptor;
import com.huawei.ock.bss.state.internal.descriptor.nskeyed.NSKeyedValueStateDescriptor;
import com.huawei.ock.bss.table.api.TableDescription;
import com.huawei.ock.bss.table.description.KListTableDescription;
import com.huawei.ock.bss.table.description.KMapTableDescription;
import com.huawei.ock.bss.table.description.KVTableDescription;
import com.huawei.ock.bss.table.description.NsKListTableDescription;
import com.huawei.ock.bss.table.description.NsKMapTableDescription;
import com.huawei.ock.bss.table.description.NsKVTableDescription;

import org.apache.flink.api.common.typeutils.TypeSerializer;

/**
 * 创建tableDescription
 *
 * @since 2025年1月17日20:57:35
 */
public class DescriptionUtils {
    /**
     * 创建KVTableDescription
     *
     * @param keyGroupNum keyGroupNum
     * @param descriptor  KeyedStateDescriptor
     * @return 对应Type的TableDescription
     */
    @SuppressWarnings({"rawtypes"})
    public static TableDescription createTableDescription(int keyGroupNum, KeyedStateDescriptor descriptor) {
        if (descriptor instanceof KeyedValueStateDescriptor) {
            return createKVTableDescription(keyGroupNum, (KeyedValueStateDescriptor) descriptor);
        } else if (descriptor instanceof KeyedListStateDescriptor) {
            return createKListTableDescription(keyGroupNum, (KeyedListStateDescriptor) descriptor);
        } else if (descriptor instanceof KeyedMapStateDescriptor) {
            return createKMapTableDescription(keyGroupNum, (KeyedMapStateDescriptor) descriptor);
        } else {
            throw new UnsupportedOperationException(
                "Only valueState/listState/mapState/aggregatingState/reducingState supported.");
        }
    }

    /**
     * 创建SubKVTableDescription
     *
     * @param keyGroupNum keyGroupNum
     * @param descriptor  KeyedStateDescriptor
     * @return 对应Type的SubKVTableDescription
     */
    @SuppressWarnings({"rawtypes"})
    public static TableDescription createTableDescription(int keyGroupNum, NSKeyedStateDescriptor descriptor) {
        if (descriptor instanceof NSKeyedValueStateDescriptor) {
            return createNSKVTableDescription(keyGroupNum, (NSKeyedValueStateDescriptor) descriptor);
        } else if (descriptor instanceof NSKeyedListStateDescriptor) {
            return createNSKListTableDescription(keyGroupNum, (NSKeyedListStateDescriptor) descriptor);
        } else if (descriptor instanceof NSKeyedMapStateDescriptor) {
            return createNSKMapTableDescription(keyGroupNum, (NSKeyedMapStateDescriptor) descriptor);
        } else {
            throw new UnsupportedOperationException(
                "Only valueState/listState/mapState/aggregatingState/reducingState supported.");
        }
    }

    @SuppressWarnings({"rawtypes", "unchecked"})
    private static KVTableDescription createKVTableDescription(int keyGroupNum, KeyedValueStateDescriptor descriptor) {
        TypeSerializer keySerializer = descriptor.getKeySerializer();
        TypeSerializer valueSerializer = descriptor.getValueSerializer();
        TableSerializer tableSerializer = new TableSerializer<>(keySerializer, valueSerializer);
        if (descriptor.isEnableStateTTL()) {
            return new KVTableDescription<>(descriptor.getName(), keyGroupNum, tableSerializer,
                descriptor.getStateTTL());
        }
        return new KVTableDescription<>(descriptor.getName(), keyGroupNum, tableSerializer);
    }

    @SuppressWarnings({"rawtypes", "unchecked"})
    private static KListTableDescription createKListTableDescription(int keyGroupNum,
        KeyedListStateDescriptor descriptor) {
        TypeSerializer keySerializer = descriptor.getKeySerializer();
        TypeSerializer elementSerializer = descriptor.getElementSerializer();
        TableSerializer tableSerializer = new TableSerializer<>(keySerializer, elementSerializer);
        if (descriptor.isEnableStateTTL()) {
            return new KListTableDescription(descriptor.getName(), keyGroupNum, tableSerializer,
                descriptor.getStateTTL());
        }
        return new KListTableDescription(descriptor.getName(), keyGroupNum, tableSerializer);
    }

    @SuppressWarnings({"rawtypes", "unchecked"})
    private static KMapTableDescription createKMapTableDescription(int keyGroupNum,
        KeyedMapStateDescriptor descriptor) {
        TypeSerializer keySerializer = descriptor.getKeySerializer();
        TypeSerializer userKeySerializer = descriptor.getUserKeySerializer();
        TypeSerializer userValueSerializer = descriptor.getUserValueSerializer();
        SubTableSerializer tableSerializer = new SubTableSerializer(keySerializer, userKeySerializer,
            userValueSerializer);
        if (descriptor.isEnableStateTTL()) {
            return new KMapTableDescription(descriptor.getName(), keyGroupNum, tableSerializer,
                descriptor.getStateTTL());
        }
        return new KMapTableDescription(descriptor.getName(), keyGroupNum, tableSerializer);
    }

    @SuppressWarnings({"rawtypes", "unchecked"})
    private static NsKVTableDescription createNSKVTableDescription(int keyGroupNum,
        NSKeyedValueStateDescriptor descriptor) {
        TypeSerializer keySerializer = descriptor.getKeySerializer();
        TypeSerializer nsSerializer = descriptor.getNamespaceSerializer();
        TypeSerializer valueSerializer = descriptor.getValueSerializer();
        SubTableSerializer tableSerializer = new SubTableSerializer<>(keySerializer, nsSerializer, valueSerializer);
        return new NsKVTableDescription<>(descriptor.getName(), keyGroupNum, tableSerializer);
    }

    @SuppressWarnings({"rawtypes", "unchecked"})
    private static NsKListTableDescription createNSKListTableDescription(int keyGroupNum,
        NSKeyedListStateDescriptor descriptor) {
        TypeSerializer keySerializer = descriptor.getKeySerializer();
        TypeSerializer namespaceSerializer = descriptor.getNamespaceSerializer();
        TypeSerializer elementSerializer = descriptor.getElementSerializer();
        KeyPairSerializer keyPairSerializer = new KeyPairSerializer(keySerializer, namespaceSerializer);
        TableSerializer tableSerializer = new TableSerializer<>(keyPairSerializer, elementSerializer);
        return new NsKListTableDescription(descriptor.getName(), keyGroupNum, tableSerializer);
    }

    @SuppressWarnings({"rawtypes", "unchecked"})
    private static NsKMapTableDescription createNSKMapTableDescription(int keyGroupNum,
        NSKeyedMapStateDescriptor descriptor) {
        TypeSerializer keySerializer = descriptor.getKeySerializer();
        TypeSerializer namespaceSerializer = descriptor.getNamespaceSerializer();
        TypeSerializer userKeySerializer = descriptor.getUserKeySerializer();
        TypeSerializer userValueSerializer = descriptor.getUserValueSerializer();
        SubTableSerializer tableSerializer = new SubTableSerializer<>(
            new KeyPairSerializer<>(keySerializer, namespaceSerializer), userKeySerializer, userValueSerializer);
        return new NsKMapTableDescription(descriptor.getName(), keyGroupNum, tableSerializer);
    }
}
