package com.huawei.ock.bss.iterator;

import com.huawei.ock.bss.common.BinaryKeyValueItem;
import com.huawei.ock.bss.common.exception.BSSRuntimeException;
import com.huawei.ock.bss.iterator.serializer.KeyValueBuilder;
import com.huawei.ock.bss.snapshot.SavepointDBResult;

import org.apache.flink.contrib.streaming.state.iterator.SingleStateIterator;
import org.apache.flink.core.memory.DataInputView;
import org.apache.flink.core.memory.DataOutputView;
import org.apache.flink.runtime.state.KeyGroupRange;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import java.io.IOException;

import javax.annotation.Nullable;

public final class BoostSortedKeyValueIterator<K> implements SingleStateIterator {
    private static final Logger LOG = LoggerFactory.getLogger(BoostSortedKeyValueIterator.class);

    private CloseableIterator<BinaryKeyValueItem> iterator;

    private KeyValueItem currentItem;

    private KeyValueBuilder<K> keyValueBuilder;

    private boolean isValid;

    private KeyGroupRange keyGroupRange;

    public BoostSortedKeyValueIterator(SavepointDBResult savepointDBResult, KeyValueBuilder<K> keyValueBuilder,
        KeyGroupRange keyGroupRange) throws IOException {
        this.keyValueBuilder = keyValueBuilder;
        this.isValid = false;
        this.iterator = savepointDBResult.iterator();

        this.keyGroupRange = keyGroupRange;
        next();
    }

    @Override
    public void next() {
        currentItem = null;
        BinaryKeyValueItem binaryKeyValueItem = null;
        try {
            while (currentItem == null && iterator.hasNext()) {
                binaryKeyValueItem = iterator.next();
                currentItem = keyValueBuilder.buildKeyValueItem(binaryKeyValueItem);
            }
        } catch (IOException e) {
            throw new BSSRuntimeException("Failed to get next kv item.", e);
        }
        isValid = currentItem != null;
    }

    private boolean checkKeyGroup(int keyGroup) {
        return this.keyGroupRange.contains(keyGroup);
    }

    @Override
    public boolean isValid() {
        return isValid;
    }

    @Override
    public byte[] key() {
        return currentItem.getKey();
    }

    @Override
    public byte[] value() {
        return currentItem.getValue();
    }

    @Override
    public int getKvStateId() {
        return currentItem.getStateId();
    }

    @Override
    public void close() {
        try {
            iterator.close();
        } catch (Exception e) {
            throw new BSSRuntimeException("Failed to close iterator.", e);
        }
    }

    public static class KeyValueItem {
        private final int keyGroup;

        private final int stateId;

        private final byte[] key;

        private final byte[] value;

        public KeyValueItem(int keyGroup, int stateId, byte[] key, byte[] value) {
            this.keyGroup = keyGroup;
            this.stateId = stateId;
            this.key = key;
            this.value = value;
        }

        /**
         * getKeyGroup
         *
         * @return int
         */
        public int getKeyGroup() {
            return this.keyGroup;
        }

        /**
         * getStateId
         *
         * @return int
         */
        public int getStateId() {
            return this.stateId;
        }

        /**
         * getKey
         *
         * @return byte[]
         */
        public byte[] getKey() {
            return this.key;
        }

        /**
         * getValue
         *
         * @return byte[]
         */
        public byte[] getValue() {
            return this.value;
        }
    }
}