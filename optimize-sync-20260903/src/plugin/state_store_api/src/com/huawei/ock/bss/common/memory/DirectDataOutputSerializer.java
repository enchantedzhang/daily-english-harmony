/*
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * We modify this part of the code based on Apache Flink to adapt to our customized BoostStateStore state backend.
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
 */
package com.huawei.ock.bss.common.memory;

import com.huawei.ock.bss.common.exception.BSSRuntimeException;

import org.apache.flink.core.memory.DataInputView;
import org.apache.flink.core.memory.DataOutputView;
import org.apache.flink.core.memory.MemorySegment;
import org.apache.flink.core.memory.MemorySegmentWritable;
import org.apache.flink.core.memory.MemoryUtils;
import org.apache.flink.util.Preconditions;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import java.io.EOFException;
import java.io.IOException;
import java.io.UTFDataFormatException;
import java.lang.reflect.Field;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.util.Locale;

import javax.annotation.Nonnull;

/**
 * A simple and efficient deserializer for the {@link java.io.DataOutput} interface.
 */
public class DirectDataOutputSerializer implements DataOutputView, MemorySegmentWritable {
    private static final Logger LOG = LoggerFactory.getLogger(DirectDataOutputSerializer.class);

    private long address;

    private int capacity;

    private int position;

    private ByteBuffer buffer;

    private boolean fromNative = false;

    public DirectDataOutputSerializer(int startSize) {
        if (startSize < 1) {
            throw new IllegalArgumentException();
        }

        try {
            this.buffer = ByteBuffer.allocateDirect(startSize);
        } catch (OutOfMemoryError e) {
            LOG.info("Off-heap memory is not enough.");
            this.buffer = DirectBuffer.nativeAllocDirectBuffer(startSize);
            if (this.buffer == null) {
                throw new BSSRuntimeException("create native buffer failed.");
            }
            this.fromNative = true;
        }

        this.address = getDirectAddress(this.buffer);
        this.capacity = startSize;
    }

    public DirectBuffer wrapDirectData() {
        return new DirectBuffer(this.address, this.position);
    }

    public long data() {
        return address;
    }

    public void clear() {
        this.position = 0;
    }

    public int length() {
        return this.position;
    }

    @Override
    public String toString() {
        return String.format(Locale.ROOT, "[pos=%d cap=%d]", this.position, this.capacity);
    }

    @Override
    public void write(int b) {
        if (this.position >= this.capacity) {
            resize(1);
        }
        writeUnsafeByte((byte) (b & 0xff));
    }

    @Override
    public void write(@Nonnull byte[] b) {
        write(b, 0, b.length);
    }

    @Override
    public void write(@Nonnull byte[] b, int off, int len) {
        if (len < 0 || off > b.length - len) {
            throw new ArrayIndexOutOfBoundsException();
        }
        if (this.position > this.capacity - len) {
            resize(len);
        }
        unsafeCopy(b, off, len);
    }

    @Override
    public void write(MemorySegment segment, int off, int len) {
        if (len < 0 || off < 0 || off > segment.size() - len) {
            throw new IndexOutOfBoundsException(
                String.format(Locale.ROOT, "offset: %d, length: %d, size: %d", off, len, segment.size()));
        }
        if (this.position > this.capacity - len) {
            resize(len);
        }
        copyFromSegment(segment, off, len);
    }

    @Override
    public void writeBoolean(boolean v) {
        write(v ? 1 : 0);
    }

    @Override
    public void writeByte(int v) {
        write(v);
    }

    @Override
    public void writeBytes(@Nonnull String s) {
        final int sLen = s.length();
        if (this.position >= this.capacity - sLen) {
            resize(sLen);
        }

        for (int i = 0; i < sLen; i++) {
            writeByte(s.charAt(i));
        }
        this.position += sLen;
    }

    @Override
    public void writeChar(int v) {
        if (this.position >= this.capacity - 1) {
            resize(2);
        }
        writeUnsafeByte((byte) (v >> 8));
        writeUnsafeByte((byte) (v));
    }

    @Override
    public void writeChars(@Nonnull String s) {
        final int sLen = s.length();
        if (this.position >= this.capacity - 2 * sLen) {
            resize(2 * sLen);
        }
        for (int i = 0; i < sLen; i++) {
            writeChar(s.charAt(i));
        }
    }

    @Override
    public void writeDouble(double v) {
        writeLong(Double.doubleToLongBits(v));
    }

    @Override
    public void writeFloat(float v) {
        writeInt(Float.floatToIntBits(v));
    }

    @SuppressWarnings("restriction")
    @Override
    public void writeInt(int v) {
        int b = v;
        if (this.position >= this.capacity - 3) {
            resize(4);
        }
        if (LITTLE_ENDIAN) {
            b = Integer.reverseBytes(v);
        }
        UNSAFE.putInt(this.address + this.position, b);
        this.position += 4;
    }

    /**
     * writeIntByLittleEndian 小端写入
     *
     * @param v v
     * @throws IOException IOException
     */
    public void writeIntByLittleEndian(int v) {
        int b = v;
        if (this.position >= this.capacity - 3) {
            resize(4);
        }
        UNSAFE.putInt(this.address + this.position, b);
        this.position += 4;
    }

    @SuppressWarnings("restriction")
    @Override
    public void writeLong(long v) {
        long b = v;
        if (this.position >= this.capacity - 7) {
            resize(8);
        }
        if (LITTLE_ENDIAN) {
            b = Long.reverseBytes(v);
        }
        UNSAFE.putLong(this.address + this.position, b);
        this.position += 8;
    }

    @Override
    public void writeShort(int v) {
        if (this.position >= this.capacity - 1) {
            resize(2);
        }
        UNSAFE.putByte(this.address + this.position++, (byte) ((v >>> 8) & 0xff));
        UNSAFE.putByte(this.address + this.position++, (byte) (v & 0xff));
    }

    @Override
    public void writeUTF(@Nonnull String str) throws IOException {
        int strlen = str.length();
        int utflen = 0;
        int c;

        /* use charAt instead of copying String to char array */
        for (int i = 0; i < strlen; i++) {
            c = str.charAt(i);
            if ((c >= 0x0001) && (c <= 0x007F)) {
                utflen++;
            } else if (c > 0x07FF) {
                utflen += 3;
            } else {
                utflen += 2;
            }
        }

        if (utflen > 65535) {
            throw new UTFDataFormatException("Encoded string is too long: " + utflen);
        }
        if (this.position > this.capacity - utflen - 2) {
            resize(utflen + 2);
        }

        writeUnsafeByte((byte) ((utflen >>> 8) & 0xFF));
        writeUnsafeByte((byte) (utflen & 0xFF));

        int i;
        for (i = 0; i < strlen; i++) {
            c = str.charAt(i);
            if (!((c >= 0x0001) && (c <= 0x007F))) {
                break;
            }
            writeUnsafeByte((byte) c);
        }
        for (; i < strlen; i++) {
            c = str.charAt(i);
            if ((c >= 0x0001) && (c <= 0x007F)) {
                writeUnsafeByte((byte) c);
            } else if (c > 0x07FF) {
                writeUnsafeByte((byte) (0xE0 | ((c >> 12) & 0x0F)));
                writeUnsafeByte((byte) (0x80 | ((c >> 6) & 0x3F)));
                writeUnsafeByte((byte) (0x80 | (c & 0x3F)));
            } else {
                writeUnsafeByte((byte) (0xC0 | ((c >> 6) & 0x1F)));
                writeUnsafeByte((byte) (0x80 | (c & 0x3F)));
            }
        }
    }

    @Override
    public void skipBytesToWrite(int numBytes) throws IOException {
        if (this.capacity - this.position < numBytes) {
            throw new EOFException("Could not skip " + numBytes + " bytes.");
        }

        this.position += numBytes;
    }

    @Override
    public void write(@Nonnull DataInputView source, int numBytes) throws IOException {
        if (this.capacity - this.position < numBytes) {
            throw new EOFException("Could not write " + numBytes + " bytes. Buffer overflow.");
        }
        if (source instanceof DirectDataInputDeserializer) {
            DirectDataInputDeserializer directSource = (DirectDataInputDeserializer) source;
            directSource.readFullyUnsafe(this.address + this.position, numBytes);
            this.position += numBytes;
        } else {
            byte[] tmp = new byte[numBytes];
            source.readFully(tmp, 0, numBytes);
            unsafeCopy(tmp, 0, numBytes);
        }
    }

    private void copyFromSegment(@Nonnull MemorySegment segment, int offset, int length) {
        // check the byte array this.position and length and the status
        if ((this.position | length | (this.position + length) | (this.capacity - (this.position + length))) < 0) {
            throw new IndexOutOfBoundsException();
        }

        final long baseAddress = segment.isOffHeap() ? segment.getAddress() : BYTE_ARRAY_BASE_OFFSET;
        final long pos = baseAddress + offset;
        final long addressLimit = baseAddress + segment.size();
        if (offset >= 0 && pos <= addressLimit - length) {
            UNSAFE.copyMemory(segment.getHeapMemory(), pos, null, this.address + this.position, length);
            this.position += length;
        } else {
            throw new IndexOutOfBoundsException(
                String.format(Locale.ROOT, "pos: %d, length: %d, offset: %d, this.position: %d", pos, length, offset,
                    this.position));
        }
    }

    private void resize(int minCapacityAdd) {
        int newLen = Math.max(this.capacity * 2, this.capacity + minCapacityAdd);
        long newAddress;
        ByteBuffer newBuffer = null;
        boolean flag = false;
        try {
            newBuffer = ByteBuffer.allocateDirect(newLen);
            newAddress = getDirectAddress(newBuffer);
        } catch (BSSRuntimeException e) {
            throw new BSSRuntimeException(e);
        } catch (OutOfMemoryError e) {
            LOG.info("Off-heap memory is not enough");
            newBuffer = DirectBuffer.nativeAllocDirectBuffer(newLen);
            if (newBuffer == null) {
                throw new BSSRuntimeException("create native buffer failed.");
            }
            newAddress = getDirectAddress(newBuffer);
            flag = true;
        }

        UNSAFE.copyMemory(null, this.address, null, newAddress, this.position);
        releaseDirectBuffer();
        this.fromNative = flag;
        this.capacity = newLen;
        this.address = newAddress;
        this.buffer = newBuffer;
    }

    private void unsafeCopy(@Nonnull byte[] b, int off, int len) {
        long pos = BYTE_ARRAY_BASE_OFFSET + off;
        UNSAFE.copyMemory(b, pos, null, this.address + this.position, len);
        this.position += len;
    }

    public void writeIntUnsafe(int v, int pos) throws IOException {
        int b = v;
        if (LITTLE_ENDIAN) {
            b = Integer.reverseBytes(v);
        }
        UNSAFE.putInt(this.address + pos, b);
    }

    private void writeUnsafeByte(byte b) {
        UNSAFE.putByte(this.address + this.position++, b);
    }

    private long getDirectAddress(ByteBuffer buffer) {
        try {
            Field addressField = buffer.getClass()
                .getSuperclass()
                .getSuperclass()
                .getSuperclass()
                .getDeclaredField("address");
            addressField.setAccessible(true);
            return (long) addressField.get(buffer);
        } catch (NoSuchFieldException | IllegalAccessException e) {
            throw new BSSRuntimeException(e);
        }
    }

    private void releaseDirectBuffer() {
        if (this.buffer.isDirect() && !fromNative) {
            LOG.info("start to release buffer {}", this.buffer);
            try {
                Field cleanerField = this.buffer.getClass().getDeclaredField("cleaner");
                cleanerField.setAccessible(true);
                Object cleaner = cleanerField.get(this.buffer);
                if (cleaner != null) {
                    cleaner.getClass().getMethod("clean").invoke(cleaner);
                }
            } catch (Exception e) {
                LOG.info(e.getMessage());
            }
            return;
        }

        if (fromNative) {
            DirectBuffer.freeDirectBuffer(getDirectAddress(buffer));
        }
    }

    public void setPosition(int position) {
        Preconditions.checkArgument(position >= 0 && position <= this.position, "Position out of bounds.");
        this.position = position;
    }

    public void setPositionUnsafe(int position) {
        this.position = position;
    }

    public byte[] getCopyOfBuffer() {
        byte[] data = new byte[position];
        UNSAFE.copyMemory(null, address, data, BYTE_ARRAY_BASE_OFFSET, position);
        return data;
    }
    // ------------------------------------------------------------------------
    //  Utilities
    // ------------------------------------------------------------------------

    @SuppressWarnings("restriction")
    private static final sun.misc.Unsafe UNSAFE = MemoryUtils.UNSAFE;

    @SuppressWarnings("restriction")
    private static final int BYTE_ARRAY_BASE_OFFSET = UNSAFE.arrayBaseOffset(byte[].class);

    private static final boolean LITTLE_ENDIAN = (MemoryUtils.NATIVE_BYTE_ORDER == ByteOrder.LITTLE_ENDIAN);
}