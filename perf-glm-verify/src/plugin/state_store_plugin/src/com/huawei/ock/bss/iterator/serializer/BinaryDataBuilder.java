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

package com.huawei.ock.bss.iterator.serializer;

import org.apache.flink.api.common.typeutils.TypeSerializer;
import org.apache.flink.core.memory.DataInputDeserializer;
import org.apache.flink.core.memory.DataOutputSerializer;
import org.apache.flink.runtime.state.CompositeKeySerializationUtils;
import org.apache.flink.runtime.state.ListDelimitedSerializer;
import org.apache.flink.runtime.state.SerializedCompositeKeyBuilder;
import org.apache.flink.runtime.state.VoidNamespace;
import org.apache.flink.runtime.state.VoidNamespaceSerializer;
import org.apache.flink.util.FlinkRuntimeException;

import java.io.IOException;
import java.util.ArrayList;
import java.util.List;

/**
 * BinaryDataBuilder
 *
 * @param <K> key
 * @since BeiMing 25.0.T1
 */
public class BinaryDataBuilder<K> {
    private final byte[] voidNamespaceBytes;

    private final DataSerializer reuseSerializerWrapper;

    private final DataOutputSerializer dataOutputView = new DataOutputSerializer(64);

    private final DataInputDeserializer dataInputView = new DataInputDeserializer();

    private final SerializedCompositeKeyBuilder<byte[]> compositeKeyBuilder;

    @SuppressWarnings({"rawtypes", "unchecked"})
    public BinaryDataBuilder(TypeSerializer<K> keySerializer, int totalKeyGroups) {
        int keyGroupPrefixBytes = CompositeKeySerializationUtils.computeRequiredBytesInKeyGroupPrefix(totalKeyGroups);
        this.compositeKeyBuilder =
            new SerializedCompositeKeyBuilder(new DataSerializer(keySerializer), keyGroupPrefixBytes, 32);

        this.reuseSerializerWrapper = new DataSerializer();

        try {
            this.dataOutputView.setPosition(0);
            VoidNamespaceSerializer.INSTANCE.serialize(VoidNamespace.INSTANCE, this.dataOutputView);
            this.voidNamespaceBytes = this.dataOutputView.getCopyOfBuffer();
        } catch (IOException e) {
            throw new FlinkRuntimeException("Failed to serialize void namespace", e);
        }
    }

    /**
     * formatValueAndListKey
     *
     * @param keyGroup keyGroup
     * @param key      key
     * @return byte[]
     * @throws IOException IOException
     */
    public byte[] formatValueAndListKey(int keyGroup, byte[] key) throws IOException {
        return formatValueAndListKey(keyGroup, key, (TypeSerializer<?>) VoidNamespaceSerializer.INSTANCE,
            this.voidNamespaceBytes);
    }

    /**
     * formatValueAndListKey
     *
     * @param <N>                 namespace
     * @param keyGroup            keyGroup
     * @param key                 key
     * @param namespaceSerializer namespaceSerializer
     * @param namespace           namespace
     * @return byte[]
     * @throws IOException IOException
     */
    public <N> byte[] formatValueAndListKey(int keyGroup, byte[] key, TypeSerializer<N> namespaceSerializer,
        byte[] namespace) throws IOException {
        setKeyGroupAndKeyAndNamespace(keyGroup, key, namespaceSerializer, namespace);
        return this.compositeKeyBuilder.build();
    }

    /**
     * formatMapKey
     *
     * @param <UK>              userKey
     * @param keyGroup          keyGroup
     * @param key               key
     * @param userKeySerializer userKeySerializer
     * @param userKey           userKey
     * @return byte[]
     * @throws IOException IOException
     */
    public <UK> byte[] formatMapKey(int keyGroup, byte[] key, TypeSerializer<UK> userKeySerializer, byte[] userKey)
        throws IOException {
        return formatNSMapKey(keyGroup, key, (TypeSerializer<?>) VoidNamespaceSerializer.INSTANCE,
            this.voidNamespaceBytes, userKeySerializer, userKey);
    }

    /**
     * formatNSMapKey
     *
     * @param <N>                 namespace
     * @param <UK>                userKey
     * @param keyGroup            keyGroup
     * @param key                 key
     * @param namespaceSerializer namespaceSerializer
     * @param namespace           namespace
     * @param userKeySerializer   userKeySerializer
     * @param userKey             userKey
     * @return byte[]
     * @throws IOException IOException
     */
    public <N, UK> byte[] formatNSMapKey(int keyGroup, byte[] key, TypeSerializer<N> namespaceSerializer,
        byte[] namespace, TypeSerializer<UK> userKeySerializer, byte[] userKey) throws IOException {
        setKeyGroupAndKeyAndNamespace(keyGroup, key, namespaceSerializer, namespace);
        this.reuseSerializerWrapper.reset(userKeySerializer);
        return this.compositeKeyBuilder.buildCompositeKeyUserKey(userKey, this.reuseSerializerWrapper);
    }

    private <N> void setKeyGroupAndKeyAndNamespace(int keyGroup, byte[] key, TypeSerializer<N> namespaceSerializer,
        byte[] namespace) {
        this.compositeKeyBuilder.setKeyAndKeyGroup(key, keyGroup);
        this.reuseSerializerWrapper.reset(namespaceSerializer);
        this.compositeKeyBuilder.setNamespace(namespace, this.reuseSerializerWrapper);
    }

    /**
     * formatUserValueForMapState
     *
     * @param <UV>                userValueSerializer
     * @param userValueSerializer userValueSerializer
     * @param userValueBytes      userValueBytes
     * @return byte[]
     * @throws IOException IOException
     */
    public <UV> byte[] formatUserValueForMapState(TypeSerializer<UV> userValueSerializer, byte[] userValueBytes)
        throws IOException {
        this.dataInputView.setBuffer(userValueBytes);
        UV userValue = userValueSerializer.deserialize(this.dataInputView);
        this.dataInputView.releaseArrays();

        this.dataOutputView.setPosition(0);
        this.dataOutputView.writeBoolean((userValue == null));
        this.dataOutputView.write(userValueBytes);

        return this.dataOutputView.getCopyOfBuffer();
    }

    /**
     * formatListValue
     *
     * @param elementSerializer elementSerializer
     * @param rawValue          rawValue
     * @return byte[]
     * @throws IOException IOException
     */
    public <E> byte[] formatListValue(TypeSerializer<E> elementSerializer, byte[] rawValue) throws IOException {
        this.dataInputView.setBuffer(rawValue);
        List<E> list = new ArrayList<>();

        while (dataInputView.available() > 0) {
            int subListSize = dataInputView.readInt();
            for (int i = 0; i < subListSize; i++) {
                list.add(elementSerializer.deserialize(dataInputView));
            }
            dataInputView.skipBytes(subListSize * 4);
        }
        this.dataInputView.releaseArrays();
        ListDelimitedSerializer listDelimitedSerializer = new ListDelimitedSerializer();

        return listDelimitedSerializer.serializeList(list, elementSerializer);
    }
}