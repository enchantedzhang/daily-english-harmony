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

package com.huawei.ock.bss.snapshot;

import com.huawei.ock.bss.common.BoostStateDB;
import com.huawei.ock.bss.iterator.BoostKeyValueStateIterator;
import com.huawei.ock.bss.iterator.BoostSortedKeyValueIterator;
import com.huawei.ock.bss.iterator.serializer.KeyValueBuilder;

import org.apache.flink.api.common.state.StateDescriptor;
import org.apache.flink.api.common.typeutils.TypeSerializer;
import org.apache.flink.configuration.Configuration;
import org.apache.flink.contrib.streaming.state.iterator.RocksQueueIterator;
import org.apache.flink.contrib.streaming.state.iterator.SingleStateIterator;
import org.apache.flink.runtime.state.FullSnapshotResources;
import org.apache.flink.runtime.state.KeyGroupRange;
import org.apache.flink.runtime.state.KeyValueStateIterator;
import org.apache.flink.runtime.state.RegisteredKeyValueStateBackendMetaInfo;
import org.apache.flink.runtime.state.RegisteredPriorityQueueStateBackendMetaInfo;
import org.apache.flink.runtime.state.RegisteredStateMetaInfoBase;
import org.apache.flink.runtime.state.StateSnapshotTransformer;
import org.apache.flink.runtime.state.StreamCompressionDecorator;
import org.apache.flink.runtime.state.heap.HeapPriorityQueueStateSnapshot;
import org.apache.flink.runtime.state.metainfo.StateMetaInfoSnapshot;
import org.apache.flink.util.IOUtils;
import org.apache.flink.util.ResourceGuard;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import java.io.IOException;
import java.util.ArrayList;
import java.util.List;
import java.util.Map;
import java.util.concurrent.atomic.AtomicBoolean;

import javax.annotation.Nonnegative;

/**
 * savepoint资源，返回给flink上层调用
 *
 * @param <K> key
 * @since BeiMing 25.0.T1
 */
public class FullBoostSnapshotResources<K> implements FullSnapshotResources<K> {
    private static final Logger LOG = LoggerFactory.getLogger(FullBoostSnapshotResources.class);

    private final int toKeyGroupNum;

    @Nonnegative
    private final int keyGroupPrefixBytes;

    private final BoostStateDB db;

    private final KeyGroupRange keyGroupRange;

    private final AtomicBoolean isIteratorCreated;

    private final AtomicBoolean isSavepointDBResultClosed;

    private final Configuration configuration;

    private final SavepointDBResult savepointDBResult;

    private final ResourceGuard.Lease lease;

    private final TypeSerializer<K> keySerializer;

    private final StreamCompressionDecorator streamCompressionDecorator;

    private final List<StateMetaInfoSnapshot> stateMetaInfoSnapshots;

    private final Map<String, KvMetaData> kvStateMetaDataMap;

    private final Map<String, PqMetaData> pqMetaDataMap;

    public FullBoostSnapshotResources(
        ResourceGuard.Lease lease,
        SavepointDBResult savepointDBResult,
        BoostStateDB db,
        int keyGroupPrefixBytes,
        KeyGroupRange keyGroupRange,
        int toKeyGroupNum,
        TypeSerializer<K> keySerializer,
        Configuration configuration,
        StreamCompressionDecorator streamCompressionDecorator,
        List<StateMetaInfoSnapshot> stateMetaInfoSnapshots,
        Map<String, KvMetaData> kvStateMetaDataMap,
        Map<String, PqMetaData> pqMetaDataMap) {
        this.lease = lease;
        this.savepointDBResult = savepointDBResult;
        this.db = db;
        this.keyGroupPrefixBytes = keyGroupPrefixBytes;
        this.keyGroupRange = keyGroupRange;
        this.toKeyGroupNum = toKeyGroupNum;
        this.keySerializer = keySerializer;
        this.streamCompressionDecorator = streamCompressionDecorator;
        this.stateMetaInfoSnapshots = stateMetaInfoSnapshots;
        this.kvStateMetaDataMap = kvStateMetaDataMap;
        this.pqMetaDataMap = pqMetaDataMap;
        this.isIteratorCreated = new AtomicBoolean(false);
        this.isSavepointDBResultClosed = new AtomicBoolean(false);
        this.configuration = configuration;
    }

    @Override
    public KeyValueStateIterator createKVStateIterator() throws IOException {
        if (this.isIteratorCreated.compareAndSet(false, true)) {
            KeyValueBuilder<K> keyValueBuilder =
                new KeyValueBuilder<>(toKeyGroupNum, keySerializer, kvStateMetaDataMap);

            List<SingleStateIterator> iterators = new ArrayList<>(pqMetaDataMap.size() + kvStateMetaDataMap.size());

            BoostSortedKeyValueIterator<K> keyValueIterator =
                new BoostSortedKeyValueIterator<>(savepointDBResult, keyValueBuilder, keyGroupRange);
            iterators.add(keyValueIterator);

            for (PqMetaData metaData : pqMetaDataMap.values()) {
                RocksQueueIterator pqIterator =
                    new RocksQueueIterator(metaData.stateSnapshot, keyGroupRange, keyGroupPrefixBytes,
                        metaData.stateId);
                iterators.add(pqIterator);
            }
            return new BoostKeyValueStateIterator(iterators, keyGroupPrefixBytes);
        }
        throw new IOException("Failed to create iterator repeatedly.");
    }

    @Override
    public List<StateMetaInfoSnapshot> getMetaInfoSnapshots() {
        return this.stateMetaInfoSnapshots;
    }

    @Override
    public KeyGroupRange getKeyGroupRange() {
        return this.keyGroupRange;
    }

    @Override
    public TypeSerializer<K> getKeySerializer() {
        return this.keySerializer;
    }

    @Override
    public StreamCompressionDecorator getStreamCompressionDecorator() {
        return this.streamCompressionDecorator;
    }

    @Override
    public void release() {
        if (this.isSavepointDBResultClosed.compareAndSet(false, true)) {
            try {
                LOG.info("Close data view for savepoint {}", this.savepointDBResult.getSnapshotId());
                this.savepointDBResult.close();
            } catch (Exception e) {
                LOG.error("Failed to close data view for savepoint {}", this.savepointDBResult.getSnapshotId(), e);
            }
        }
        IOUtils.closeQuietly(this.lease);
    }

    /**
     * 封装元数据和stateSnapshotTransformer
     */
    public static class KvMetaData {
        /**
         * stateId
         */
        public final int stateId;

        /**
         * metaInfo
         */
        public final RegisteredStateMetaInfoBase metaInfo;

        /**
         * stateSnapshotTransformer
         */
        public final StateSnapshotTransformer<byte[]> stateSnapshotTransformer;

        final StateDescriptor.Type stateType;

        final StateMetaInfoSnapshot stateMetaInfoSnapshot;

        public KvMetaData(
            int stateId,
            StateDescriptor.Type type,
            StateMetaInfoSnapshot stateMetaInfoSnapshot,
            StateSnapshotTransformer<byte[]> stateSnapshotTransformer) {
            this.metaInfo = type != StateDescriptor.Type.UNKNOWN
                    ? new RegisteredKeyValueStateBackendMetaInfo<>(stateMetaInfoSnapshot)
                    : new RegisteredPriorityQueueStateBackendMetaInfo<>(stateMetaInfoSnapshot);
            this.stateSnapshotTransformer = stateSnapshotTransformer;
            this.stateId = stateId;
            this.stateType = type;
            this.stateMetaInfoSnapshot = stateMetaInfoSnapshot;
        }
    }

    /**
     * PqMetaData
     */
    public static class PqMetaData {
        /**
         * stateId
         */
        public final int stateId;

        /**
         * stateSnapshot
         */
        public final HeapPriorityQueueStateSnapshot<?> stateSnapshot;

        public PqMetaData(int stateId, HeapPriorityQueueStateSnapshot<?> stateSnapshot) {
            this.stateId = stateId;
            this.stateSnapshot = stateSnapshot;
        }
    }
}
