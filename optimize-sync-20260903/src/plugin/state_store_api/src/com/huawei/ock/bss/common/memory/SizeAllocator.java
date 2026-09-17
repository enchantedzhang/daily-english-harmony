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

package com.huawei.ock.bss.common.memory;

import java.util.Objects;

/**
 * SizeAllocator
 *
 * @since BeiMing 25.0.T1
 */
public class SizeAllocator implements AutoCloseable {
    private final String identifier;

    private final long memorySize;

    public SizeAllocator(String identifier, long memorySize) {
        this.identifier = identifier;
        this.memorySize = memorySize;
    }

    public long getMemorySize() {
        return this.memorySize;
    }

    public void close() {
    }

    public int hashCode() {
        return Objects.hash(this.identifier, this.memorySize);
    }

    public boolean equals(Object o) {
        if (this == o) {
            return true;
        }
        if (o == null || getClass() != o.getClass()) {
            return false;
        }
        SizeAllocator that = (SizeAllocator) o;
        return (this.memorySize == that.memorySize && Objects.equals(this.identifier, that.identifier));
    }

    public String toString() {
        return "SizeAllocator{identifier='" + this.identifier + '\'' + ", memorySize=" + this.memorySize + '}';
    }
}
