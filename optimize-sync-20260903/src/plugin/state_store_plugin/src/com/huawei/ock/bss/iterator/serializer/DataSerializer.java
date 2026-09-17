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
import org.apache.flink.api.common.typeutils.TypeSerializerSnapshot;
import org.apache.flink.api.common.typeutils.base.TypeSerializerSingleton;
import org.apache.flink.core.memory.DataInputView;
import org.apache.flink.core.memory.DataOutputView;

import java.io.IOException;

/**
 * DataSerializer
 *
 * @since BeiMing 25.0.T1
 */
@SuppressWarnings("rawtypes")
public class DataSerializer extends TypeSerializerSingleton<byte[]> {
    private static final long serialVersionUID = 1L;

    private TypeSerializer rawSerializer;

    public DataSerializer() {
    }

    public DataSerializer(TypeSerializer rawSerializer) {
        this.rawSerializer = rawSerializer;
    }

    /**
     * getLength
     *
     * @return int
     */
    public int getLength() {
        return this.rawSerializer.getLength();
    }

    /**
     * serialize
     *
     * @param record The record to serialize.
     * @param target The output view to write the serialized data to.
     * @throws IOException IOException
     */
    public void serialize(byte[] record, DataOutputView target) throws IOException {
        if (record == null) {
            throw new IllegalArgumentException("The record must not be null.");
        }

        target.write(record);
    }

    /**
     * reset
     *
     * @param newSerializer newSerializer
     */
    public void reset(TypeSerializer newSerializer) {
        this.rawSerializer = newSerializer;
    }

    /**
     * isImmutableType
     *
     * @return boolean
     */
    @Override
    public boolean isImmutableType() {
        throw new UnsupportedOperationException();
    }

    /**
     * createInstance
     *
     * @return byte[]
     */
    @Override
    public byte[] createInstance() {
        throw new UnsupportedOperationException();
    }

    /**
     * copy
     *
     * @param bytes The element reuse be copied.
     * @return byte[]
     */
    @Override
    public byte[] copy(byte[] bytes) {
        throw new UnsupportedOperationException();
    }

    /**
     * byte[]
     *
     * @param bytes The element to be copied.
     * @param t1 The element to be reused. May or may not be used.
     * @return byte[]
     */
    @Override
    public byte[] copy(byte[] bytes, byte[] t1) {
        throw new UnsupportedOperationException();
    }

    /**
     * deserialize
     *
     * @param dataInputView The input view from which to read the data.
     * @return byte[]
     * @throws IOException IOException
     */
    @Override
    public byte[] deserialize(DataInputView dataInputView) throws IOException {
        throw new UnsupportedOperationException();
    }

    /**
     * deserialize
     *
     * @param bytes The record instance into which to de-serialize the data.
     * @param dataInputView The input view from which to read the data.
     * @return byte[]
     * @throws IOException IOException
     */
    @Override
    public byte[] deserialize(byte[] bytes, DataInputView dataInputView) throws IOException {
        throw new UnsupportedOperationException();
    }

    /**
     * copy
     *
     * @param dataInputView The input view from which to read the record.
     * @param dataOutputView The target output view to which to write the record.
     * @throws IOException IOException
     */
    @Override
    public void copy(DataInputView dataInputView, DataOutputView dataOutputView) throws IOException {
        throw new UnsupportedOperationException();
    }

    /**
     * snapshotConfiguration
     *
     * @return TypeSerializerSnapshot<byte[]>
     */
    @Override
    public TypeSerializerSnapshot<byte[]> snapshotConfiguration() {
        throw new UnsupportedOperationException();
    }
}
