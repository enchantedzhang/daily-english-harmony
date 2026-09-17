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

package com.huawei.ock.bss.common.serialize;

import com.huawei.ock.bss.common.exception.BSSRuntimeException;
import com.huawei.ock.bss.common.memory.DirectDataInputDeserializer;
import com.huawei.ock.bss.common.memory.DirectDataOutputSerializer;
import com.huawei.ock.bss.table.KeyPair;

import org.apache.flink.api.common.typeutils.TypeSerializer;
import org.apache.flink.api.common.typeutils.TypeSerializerSnapshot;
import org.apache.flink.core.memory.DataInputView;
import org.apache.flink.core.memory.DataOutputView;

import java.io.IOException;
import java.util.Objects;

/**
 * KeyPairSerializer
 *
 * @param <K1> key
 * @param <K2> namespace
 * @since BeiMing 25.0.T1
 */
public class KeyPairSerializer<K1, K2> extends TypeSerializer<KeyPair<K1, K2>> {
    private final TypeSerializer<K1> k1Serializer;

    private final TypeSerializer<K2> k2Serializer;

    public KeyPairSerializer(TypeSerializer<K1> k1Serializer, TypeSerializer<K2> k2Serializer) {
        this.k1Serializer = k1Serializer;
        this.k2Serializer = k2Serializer;
    }

    public TypeSerializer<K1> getK1Serializer() {
        return k1Serializer;
    }

    public TypeSerializer<K2> getK2Serializer() {
        return k2Serializer;
    }

    @Override
    public boolean isImmutableType() {
        return k1Serializer.isImmutableType() && k2Serializer.isImmutableType();
    }

    @Override
    public TypeSerializer<KeyPair<K1, K2>> duplicate() {
        throw new BSSRuntimeException("Unsupported.");
    }

    @Override
    public KeyPair<K1, K2> createInstance() {
        return new KeyPair<>(k1Serializer.createInstance(), k2Serializer.createInstance());
    }

    @Override
    public KeyPair<K1, K2> copy(KeyPair<K1, K2> from) {
        return new KeyPair<>(k1Serializer.copy(from.getFirstKey()), k2Serializer.copy(from.getSecondKey()));
    }

    @Override
    public KeyPair<K1, K2> copy(KeyPair<K1, K2> from, KeyPair<K1, K2> reuse) {
        throw new BSSRuntimeException("Unsupported.");
    }

    @Override
    public int getLength() {
        return k1Serializer.getLength() + k2Serializer.getLength();
    }

    @Override
    public void serialize(KeyPair<K1, K2> keyPair, DataOutputView dataOutputView) throws IOException {
        if (!(dataOutputView instanceof DirectDataOutputSerializer)) {
            throw new BSSRuntimeException("Unexpected type: " + dataOutputView.getClass());
        }

        DirectDataOutputSerializer target = (DirectDataOutputSerializer) dataOutputView;
        target.writeIntByLittleEndian(keyPair.getSecondKey().hashCode());
        int mPos = target.length();
        this.k1Serializer.serialize(keyPair.getFirstKey(), target);
        int len = target.length() - mPos;
        this.k2Serializer.serialize(keyPair.getSecondKey(), target);
        target.writeIntByLittleEndian(len);
    }

    @Override
    public KeyPair<K1, K2> deserialize(DataInputView dataInputView) throws IOException {
        if (!(dataInputView instanceof DirectDataInputDeserializer)) {
            throw new BSSRuntimeException("Unexpected type: " + dataInputView.getClass());
        }

        DirectDataInputDeserializer source = (DirectDataInputDeserializer) dataInputView;
        source.readIntByLittleEndian();
        KeyPair<K1, K2> keyPair = new KeyPair<>(k1Serializer.deserialize(source), k2Serializer.deserialize(source));
        source.readIntByLittleEndian();
        return keyPair;
    }

    @Override
    public KeyPair<K1, K2> deserialize(KeyPair<K1, K2> keyPair, DataInputView dataInputView) {
        throw new BSSRuntimeException("Unsupported.");
    }

    @Override
    public void copy(DataInputView dataInputView, DataOutputView dataOutputView) throws IOException {
        if (!(dataInputView instanceof DirectDataInputDeserializer)) {
            throw new BSSRuntimeException("Unexpected type: " + dataInputView.getClass());
        }

        if (!(dataOutputView instanceof DirectDataOutputSerializer)) {
            throw new BSSRuntimeException("Unexpected type: " + dataOutputView.getClass());
        }

        DirectDataInputDeserializer source = (DirectDataInputDeserializer) dataInputView;
        DirectDataOutputSerializer target = (DirectDataOutputSerializer) dataOutputView;

        while (source.available() != 0) {
            target.writeByte(source.readByte());
        }
    }

    @Override
    public boolean equals(Object o) {
        if (this == o) {
            return true;
        }

        if (o == null || getClass() != o.getClass()) {
            return false;
        }

        KeyPairSerializer<?, ?> that = (KeyPairSerializer<?, ?>) o;
        return Objects.equals(k1Serializer, that.k1Serializer) && Objects.equals(k2Serializer,
            that.k2Serializer);
    }

    @Override
    public int hashCode() {
        return Objects.hash(k1Serializer, k2Serializer);
    }

    @Override
    public TypeSerializerSnapshot<KeyPair<K1, K2>> snapshotConfiguration() {
        throw new BSSRuntimeException("Unsupported.");
    }
}
