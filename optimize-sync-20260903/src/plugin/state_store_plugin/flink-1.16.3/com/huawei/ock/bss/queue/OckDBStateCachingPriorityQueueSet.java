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

package com.huawei.ock.bss.queue;

import com.huawei.ock.bss.common.exception.BSSRuntimeException;
import com.huawei.ock.bss.common.serialize.KVSerializerUtil;
import com.huawei.ock.bss.table.BoostPQTable;
import com.huawei.ock.bss.table.iterator.PQKeyIterator;
import com.huawei.ock.bss.table.result.EntryResult;

import org.apache.flink.api.common.typeutils.TypeSerializer;
import org.apache.flink.core.memory.DataInputDeserializer;
import org.apache.flink.core.memory.DataOutputSerializer;
import org.apache.flink.runtime.state.CompositeKeySerializationUtils;
import org.apache.flink.runtime.state.InternalPriorityQueue;
import org.apache.flink.runtime.state.KeyExtractorFunction;
import org.apache.flink.runtime.state.heap.HeapPriorityQueueElement;
import org.apache.flink.shaded.guava30.com.google.common.primitives.UnsignedBytes;
import org.apache.flink.util.CloseableIterator;
import org.apache.flink.util.MathUtils;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import java.io.IOException;
import java.util.Collection;
import java.util.NoSuchElementException;

import javax.annotation.Nonnegative;
import javax.annotation.Nonnull;
import javax.annotation.Nullable;

/**
 * OckDBStateCachingPriorityQueueSet PQ Set
 *
 * @since BeiMing 25.3.0
 */
public class OckDBStateCachingPriorityQueueSet<E extends HeapPriorityQueueElement>
        implements InternalPriorityQueue<E>, HeapPriorityQueueElement {
    private static final Logger LOG = LoggerFactory.getLogger(OckDBStateCachingPriorityQueueSet.class);

    @Nonnull
    private final byte[] seekHint;

    @Nonnull
    private final byte[] groupPrefixBytes;

    private final BoostPQTable pqTable;

    @Nonnull
    private final TreeSetCache bytesSetCache;

    @Nonnull
    private final DataOutputSerializer outputView;

    @Nonnull
    private final DataInputDeserializer inputView;

    private final KeyExtractorFunction<E> keyExtractor;

    @Nonnull
    private final TypeSerializer<E> typeSerializer;

    private int internalIndex;

    @Nullable
    private E peekCache;

    private boolean isAllElementsInCache;

    public OckDBStateCachingPriorityQueueSet(
            @Nonnegative int keyGroupId,
            @Nonnegative int keyGroupPrefixBytes,
            KeyExtractorFunction<E> keyExtractor,
            String stateName,
            @Nonnegative BoostPQTable pqTable,
            @Nonnull TypeSerializer<E> typeSerializer,
            @Nonnull DataOutputSerializer outputStream,
            @Nonnull DataInputDeserializer inputStream,
            @Nonnull TreeSetCache bytesSetCache) {
        this.keyExtractor = keyExtractor;
        this.typeSerializer = typeSerializer;
        this.outputView = outputStream;
        this.inputView = inputStream;
        this.bytesSetCache = bytesSetCache;
        this.isAllElementsInCache = false;
        this.groupPrefixBytes = createGroupIdBytes(keyGroupId, keyGroupPrefixBytes);
        this.seekHint = groupPrefixBytes;
        this.internalIndex = HeapPriorityQueueElement.NOT_CONTAINED;
        this.pqTable = pqTable;
    }

    @Nullable
    @Override
    public E peek() {
        buildCacheFromPQTable();

        if (peekCache != null) {
            return peekCache;
        }

        byte[] firstBytes = bytesSetCache.peekFirst();
        if (firstBytes == null) {
            return null;
        }

        peekCache = deserialize(firstBytes);
        return peekCache;
    }

    @Nullable
    @Override
    public E poll() {
        buildCacheFromPQTable();

        final byte[] firstBytes = bytesSetCache.pollFirst();
        if (firstBytes == null) {
            return null;
        }

        E element = peekCache == null ? deserialize(firstBytes) : peekCache;
        Object key = keyExtractor.extractKeyFromElement(element);
        removeFromPQTable(firstBytes, MathUtils.murmurHash(key.hashCode()));
        peekCache = null;
        return element;
    }

    @Override
    public boolean add(@Nonnull E toAdd) {
        buildCacheFromPQTable();

        Object key = keyExtractor.extractKeyFromElement(toAdd);
        final byte[] toAddBytes = serialize(toAdd);

        final boolean isCacheFull = bytesSetCache.isFull();

        if (bytesSetCache.isEmpty() || (!isCacheFull && isAllElementsInCache)
            || UnsignedBytes.lexicographicalComparator().compare(toAddBytes, bytesSetCache.peekLast()) < 0) {
            if (isCacheFull) {
                bytesSetCache.pollLast();
                isAllElementsInCache = false;
            }

            if (bytesSetCache.add(toAddBytes)) {
                addToPQTable(toAddBytes, MathUtils.murmurHash(key.hashCode()));
                if (toAddBytes == bytesSetCache.peekFirst()) {
                    peekCache = null;
                    return true;
                }
            }
        } else {
            addToPQTable(toAddBytes, MathUtils.murmurHash(key.hashCode()));
            isAllElementsInCache = false;
        }
        return false;
    }

    @Override
    public boolean remove(@Nonnull E toRemove) {
        buildCacheFromPQTable();

        final byte[] oldHead = bytesSetCache.peekFirst();
        if (oldHead == null) {
            return false;
        }

        final byte[] toRemoveBytes = serialize(toRemove);
        Object key = keyExtractor.extractKeyFromElement(toRemove);
        removeFromPQTable(toRemoveBytes, MathUtils.murmurHash(key.hashCode()));
        bytesSetCache.remove(toRemoveBytes);

        if (bytesSetCache.isEmpty() || oldHead != bytesSetCache.peekFirst()) {
            peekCache = null;
            return true;
        }

        return false;
    }

    @Override
    public void addAll(@Nullable Collection<? extends E> toAdds) {
        if (toAdds == null) {
            return;
        }

        toAdds.forEach(this::add);
    }

    @Override
    public boolean isEmpty() {
        buildCacheFromPQTable();
        return bytesSetCache.isEmpty();
    }

    @Nonnull
    @Override
    public CloseableIterator<E> iterator() {
        return new DeserializedIteratorWrapper(createPQTableBytesIterator());
    }

    @Override
    public int size() {
        if (isAllElementsInCache) {
            return bytesSetCache.getSize();
        }

        int count = 0;
        try (final PQTableBytesIterator iterator = createPQTableBytesIterator()) {
            while (iterator.hasNext()) {
                iterator.next();
                ++count;
            }
        }
        return count;
    }

    @Override
    public int getInternalIndex() {
        return internalIndex;
    }

    @Override
    public void setInternalIndex(int internalIndex) {
        this.internalIndex = internalIndex;
    }

    @Nonnull
    private PQTableBytesIterator createPQTableBytesIterator() {
        return new PQTableBytesIterator(pqTable.newIterator(seekHint));
    }

    private void addToPQTable(@Nonnull byte[] toAddBytes, int hashCode) {
        pqTable.add(toAddBytes, hashCode);
    }

    private void removeFromPQTable(@Nonnull byte[] toRemoveBytes, int hashCode) {
        pqTable.remove(toRemoveBytes, hashCode);
    }

    private void buildCacheFromPQTable() {
        if (isAllElementsInCache || !bytesSetCache.isEmpty()) {
            LOG.debug("pq cache is not empty.");
            return;
        }
        try (final PQTableBytesIterator iterator = createPQTableBytesIterator()) {
            bytesSetCache.loadFromIterator(iterator);
            isAllElementsInCache = !iterator.hasNext();
        } catch (Exception e) {
            LOG.error("Exception while build cache from pq iterator.");
            throw new BSSRuntimeException("Exception while build cache from pq iterator.", e);
        }
    }

    @Nonnull
    private byte[] serialize(@Nonnull E element) {
        try {
            outputView.clear();
            outputView.write(groupPrefixBytes);
            typeSerializer.serialize(element, outputView);
            return outputView.getCopyOfBuffer();
        } catch (IOException e) {
            LOG.error("Error while serializing the element.");
            throw new BSSRuntimeException("Error while serializing the element.", e);
        }
    }

    @Nonnull
    private E deserialize(@Nonnull byte[] bytes) {
        try {
            final int numPrefixBytes = groupPrefixBytes.length;
            inputView.setBuffer(bytes, numPrefixBytes, bytes.length - numPrefixBytes);
            return typeSerializer.deserialize(inputView);
        } catch (IOException e) {
            LOG.error("Error while deserializing the element.");
            throw new BSSRuntimeException("Error while deserializing the element.", e);
        }
    }

    @Nonnull
    private byte[] createGroupIdBytes(int keyGroupId, int numPrefixBytes) {
        outputView.clear();
        try {
            CompositeKeySerializationUtils.writeKeyGroup(keyGroupId, numPrefixBytes, outputView);
        } catch (IOException e) {
            LOG.error("failed to write keyGroupIr.");
            throw new BSSRuntimeException("failed to write keyGroupIr", e);
        }

        return outputView.getCopyOfBuffer();
    }

    private class DeserializedIteratorWrapper implements CloseableIterator<E> {
        @Nonnull
        private final CloseableIterator<byte[]> iterator;

        private DeserializedIteratorWrapper(@Nonnull CloseableIterator<byte[]> bytesIterator) {
            this.iterator = bytesIterator;
        }

        @Override
        public boolean hasNext() {
            return iterator.hasNext();
        }

        @Override
        public E next() {
            return deserialize(iterator.next());
        }

        @Override
        public void close() throws Exception {
            iterator.close();
        }
    }

    private static class PQTableBytesIterator implements CloseableIterator<byte[]> {
        @Nonnull
        private final PQKeyIterator<EntryResult> iterator;

        @Nullable
        private byte[] current;

        private PQTableBytesIterator(@Nonnull PQKeyIterator<EntryResult> iterator) {
            this.iterator = iterator;
            try {
                EntryResult result = iterator.hasNext() ? iterator.next() : null;
                current = result == null ? null
                    : KVSerializerUtil.getCopyOfBuffer(result.getKeyAddr(), result.getKeyLen());
            } catch (Exception ex) {
                iterator.close();
                LOG.error("Could not initialize bytes iterator.");
                throw new BSSRuntimeException("Could not initialize bytes iterator.", ex);
            }
        }

        @Override
        public boolean hasNext() {
            return current != null;
        }

        @Override
        public byte[] next() {
            if (this.current == null) {
                throw new NoSuchElementException("Iterator has no more elements!");
            }
            final byte[] ret = this.current;
            EntryResult nextElement = iterator.hasNext() ? iterator.next() : null;
            current = nextElement == null ? null
                : KVSerializerUtil.getCopyOfBuffer(nextElement.getKeyAddr(), nextElement.getKeyLen());
            return ret;
        }

        @Override
        public void close() {
            iterator.close();
        }
    }
}