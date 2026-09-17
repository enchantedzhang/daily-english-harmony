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

import org.apache.flink.core.memory.DataInputView;
import org.apache.flink.core.memory.MemoryUtils;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import java.io.EOFException;
import java.io.IOException;
import java.io.UTFDataFormatException;
import java.nio.ByteOrder;

import javax.annotation.Nonnull;
import javax.annotation.Nullable;

/**
 * A simple and efficient deserializer for the {@link java.io.DataInput} interface.
 */
public class DirectDataInputDeserializer implements DataInputView, java.io.Serializable {
    private static final long serialVersionUID = 1L;

    private static final Logger LOG = LoggerFactory.getLogger(DirectDataInputDeserializer.class);

    // ------------------------------------------------------------------------

    private long address;

    private int end;

    private int position;

    // ------------------------------------------------------------------------

    public DirectDataInputDeserializer() {
    }

    public void setBuffer(DirectBuffer buffer) {
        this.address = buffer.data();
        this.end = buffer.length();
        this.position = 0;
    }

    public void setBuffer(long address, int length) {
        this.address = address;
        this.end = length;
        this.position = 0;
    }

    // ----------------------------------------------------------------------------------------
    //                               Data Input
    // ----------------------------------------------------------------------------------------

    public int available() {
        if (position < end) {
            return end - position;
        } else {
            return 0;
        }
    }

    @Override
    public boolean readBoolean() throws IOException {
        if (this.position < this.end) {
            return UNSAFE.getBoolean(null, this.address + this.position++);
        } else {
            throw new EOFException();
        }
    }

    @Override
    public byte readByte() throws IOException {
        if (this.position < this.end) {
            return UNSAFE.getByte(null, this.address + this.position++);
        } else {
            throw new EOFException();
        }
    }

    @Override
    public char readChar() throws IOException {
        if (this.position < this.end - 1) {
            return (char) (((UNSAFE.getByte(null, this.address + this.position++) & 0xff) << 8) | (
                UNSAFE.getByte(null, this.address + this.position++) & 0xff));
        } else {
            throw new EOFException();
        }
    }

    @Override
    public double readDouble() throws IOException {
        return Double.longBitsToDouble(readLong());
    }

    @Override
    public float readFloat() throws IOException {
        return Float.intBitsToFloat(readInt());
    }

    @Override
    public void readFully(@Nonnull byte[] b) throws IOException {
        readFully(b, 0, b.length);
    }

    private void unsafeCopy(byte[] b, int off, int len) {
        long pos = BYTE_ARRAY_BASE_OFFSET + off;
        UNSAFE.copyMemory(null, this.address + this.position, b, pos, len);
        this.position += len;
    }

    @Override
    public void readFully(@Nonnull byte[] b, int off, int len) throws IOException {
        if (len >= 0) {
            if (off <= b.length - len) {
                if (this.position <= this.end - len) {
                    unsafeCopy(b, off, len);
                } else {
                    throw new EOFException();
                }
            } else {
                throw new ArrayIndexOutOfBoundsException();
            }
        } else {
            throw new IllegalArgumentException("Length may not be negative.");
        }
    }

    @Override
    public int readInt() throws IOException {
        if (this.position >= 0 && this.position < this.end - 3) {
            @SuppressWarnings("restriction") int value = UNSAFE.getInt(this.address + this.position);
            if (LITTLE_ENDIAN) {
                value = Integer.reverseBytes(value);
            }

            this.position += 4;
            return value;
        } else {
            LOG.error("early EOF, current position: {}, end: {}", position, end);
            throw new EOFException();
        }
    }

    /**
     * readIntByLittleEndian 小端读取
     *
     * @return 读取的数值
     * @throws IOException IOException
     */
    public int readIntByLittleEndian() throws IOException {
        if (this.position >= 0 && this.position < this.end - 3) {
            @SuppressWarnings("restriction") int value = UNSAFE.getInt(this.address + this.position);
            this.position += 4;
            return value;
        } else {
            LOG.error("early EOF, current position: {}, end: {}", position, end);
            throw new EOFException();
        }
    }

    @Nullable
    @Override
    public String readLine() throws IOException {
        if (this.position < this.end) {
            // read until a newline is found
            StringBuilder bld = new StringBuilder();
            char curr = (char) readUnsignedByte();
            while (position < this.end && curr != '\n') {
                bld.append(curr);
                curr = (char) readUnsignedByte();
            }
            // trim a trailing carriage return
            int len = bld.length();
            if (len > 0 && bld.charAt(len - 1) == '\r') {
                bld.setLength(len - 1);
            }
            String s = bld.toString();
            bld.setLength(0);
            return s;
        } else {
            return null;
        }
    }

    @Override
    public long readLong() throws IOException {
        if (position >= 0 && position < this.end - 7) {
            @SuppressWarnings("restriction") long value = UNSAFE.getLong(this.address + this.position);
            if (LITTLE_ENDIAN) {
                value = Long.reverseBytes(value);
            }
            this.position += 8;
            return value;
        } else {
            throw new EOFException();
        }
    }

    @Override
    public short readShort() throws IOException {
        if (position >= 0 && position < this.end - 1) {
            return (short) ((((UNSAFE.getByte(this.address + this.position++)) & 0xff) << 8) | (
                (UNSAFE.getByte(this.address + this.position++)) & 0xff));
        } else {
            throw new EOFException();
        }
    }

    @Nonnull
    @Override
    public String readUTF() throws IOException {
        int utflen = readUnsignedShort();
        byte[] bytearr = new byte[utflen];
        char[] chararr = new char[utflen];

        int c;
        int char2;
        int char3;
        int count = 0;
        int chararrCount = 0;

        readFully(bytearr, 0, utflen);

        while (count < utflen) {
            c = (int) bytearr[count] & 0xff;
            if (c > 127) {
                break;
            }
            count++;
            chararr[chararrCount++] = (char) c;
        }

        while (count < utflen) {
            c = (int) bytearr[count] & 0xff;
            switch (c >> 4) {
                case 0:
                case 1:
                case 2:
                case 3:
                case 4:
                case 5:
                case 6:
                case 7:
                    /* 0xxxxxxx */
                    count++;
                    chararr[chararrCount++] = (char) c;
                    break;
                case 12:
                case 13:
                    /* 110x xxxx 10xx xxxx */
                    count += 2;
                    if (count > utflen) {
                        throw new UTFDataFormatException("malformed input: partial character at end");
                    }
                    char2 = (int) bytearr[count - 1];
                    if ((char2 & 0xC0) != 0x80) {
                        throw new UTFDataFormatException("malformed input around byte " + count);
                    }
                    chararr[chararrCount++] = (char) (((c & 0x1F) << 6) | (char2 & 0x3F));
                    break;
                case 14:
                    /* 1110 xxxx 10xx xxxx 10xx xxxx */
                    count += 3;
                    if (count > utflen) {
                        throw new UTFDataFormatException("malformed input: partial character at end");
                    }
                    char2 = (int) bytearr[count - 2];
                    char3 = (int) bytearr[count - 1];
                    if (((char2 & 0xC0) != 0x80) || ((char3 & 0xC0) != 0x80)) {
                        throw new UTFDataFormatException("malformed input around byte " + (count - 1));
                    }
                    chararr[chararrCount++] = (char) (((c & 0x0F) << 12) | ((char2 & 0x3F) << 6) | (char3 & 0x3F));
                    break;
                default:
                    /* 10xx xxxx, 1111 xxxx */
                    throw new UTFDataFormatException("malformed input around byte " + count);
            }
        }
        // The number of chars produced may be less than utflen
        return new String(chararr, 0, chararrCount);
    }

    @Override
    public int readUnsignedByte() throws IOException {
        if (this.position < this.end) {
            return (UNSAFE.getByte(this.address + this.position++) & 0xff);
        } else {
            throw new EOFException();
        }
    }

    @Override
    public int readUnsignedShort() throws IOException {
        if (this.position < this.end - 1) {
            return ((UNSAFE.getByte(this.address + this.position++) & 0xff) << 8) | (
                UNSAFE.getByte(this.address + this.position++) & 0xff);
        } else {
            throw new EOFException();
        }
    }

    @Override
    public int skipBytes(int n) {
        int skipBytes = n;
        if (this.position <= this.end - n) {
            this.position += n;
            return skipBytes;
        } else {
            skipBytes = this.end - this.position;
            this.position = this.end;
            return skipBytes;
        }
    }

    @Override
    public void skipBytesToRead(int numBytes) throws IOException {
        int skippedBytes = skipBytes(numBytes);

        if (skippedBytes < numBytes) {
            throw new EOFException("Could not skip " + numBytes + " bytes.");
        }
    }

    @Override
    public int read(@Nonnull byte[] b, int off, int len) throws IOException {
        if (off < 0) {
            throw new IndexOutOfBoundsException("Offset cannot be negative.");
        }

        if (len < 0) {
            throw new IndexOutOfBoundsException("Length cannot be negative.");
        }

        if (b.length - off < len) {
            throw new IndexOutOfBoundsException(
                "Byte array does not provide enough space to store requested data" + ".");
        }

        if (this.position >= this.end) {
            return -1;
        } else {
            int toRead = Math.min(this.end - this.position, len);
            unsafeCopy(b, off, toRead);

            return toRead;
        }
    }

    @Override
    public int read(@Nonnull byte[] b) throws IOException {
        return read(b, 0, b.length);
    }

    @SuppressWarnings("restriction")
    private static final sun.misc.Unsafe UNSAFE = MemoryUtils.UNSAFE;

    public void readFullyUnsafe(long dest, int len) {
        UNSAFE.copyMemory(address + this.position, dest, len);
        this.position += len;
    }

    public int getPosition() {
        return position;
    }

    @SuppressWarnings("restriction")
    private static final long BYTE_ARRAY_BASE_OFFSET = UNSAFE.arrayBaseOffset(byte[].class);

    private static final boolean LITTLE_ENDIAN = (MemoryUtils.NATIVE_BYTE_ORDER == ByteOrder.LITTLE_ENDIAN);
}