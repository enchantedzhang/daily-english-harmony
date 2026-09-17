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

import com.huawei.ock.bss.common.BoostStateDB;
import com.huawei.ock.bss.common.exception.BSSRuntimeException;
import com.huawei.ock.bss.table.BoostPQTable;
import com.huawei.ock.bss.table.description.PQTableDescription;

import org.apache.flink.annotation.VisibleForTesting;
import org.apache.flink.api.common.typeutils.TypeSerializer;
import org.apache.flink.api.common.typeutils.TypeSerializerSchemaCompatibility;
import org.apache.flink.core.memory.DataInputDeserializer;
import org.apache.flink.core.memory.DataOutputSerializer;
import org.apache.flink.runtime.state.CompositeKeySerializationUtils;
import org.apache.flink.runtime.state.KeyExtractorFunction;
import org.apache.flink.runtime.state.KeyGroupRange;
import org.apache.flink.runtime.state.KeyGroupedInternalPriorityQueue;
import org.apache.flink.runtime.state.Keyed;
import org.apache.flink.runtime.state.PriorityComparable;
import org.apache.flink.runtime.state.PriorityComparator;
import org.apache.flink.runtime.state.PriorityQueueSetFactory;
import org.apache.flink.runtime.state.RegisteredPriorityQueueStateBackendMetaInfo;
import org.apache.flink.runtime.state.RegisteredStateMetaInfoBase;
import org.apache.flink.runtime.state.heap.HeapPriorityQueueElement;
import org.apache.flink.runtime.state.heap.KeyGroupPartitionedPriorityQueue;
import org.apache.flink.util.StateMigrationException;

import java.util.Map;

import javax.annotation.Nonnull;

/**
 * OckDBPriorityQueueSetFactory PQ Factory
 *
 * @since BeiMing 25.3.0
 */
public class OckDBPriorityQueueSetFactory implements PriorityQueueSetFactory {
    @VisibleForTesting
    static final int DEFAULT_CACHES_SIZE = 20480;

    @Nonnull
    private final DataOutputSerializer sharedElementOutView;

    @Nonnull
    private final DataInputDeserializer sharedElementInView;

    private final Map<String, RegisteredStateMetaInfoBase> kvStateInformation;

    private final KeyGroupRange keyGroupRange;
    private final BoostStateDB db;
    private final int numberOfKeyGroups;
    private final int keyGroupPrefixBytes;

    public OckDBPriorityQueueSetFactory(KeyGroupRange keyGroupRange, BoostStateDB db, int numberOfKeyGroups,
        Map<String, RegisteredStateMetaInfoBase> kvStateInformation) {
        this.keyGroupRange = keyGroupRange;
        this.db = db;
        this.kvStateInformation = kvStateInformation;
        this.numberOfKeyGroups = numberOfKeyGroups;
        this.sharedElementOutView = new DataOutputSerializer(128);
        this.sharedElementInView = new DataInputDeserializer();
        this.keyGroupPrefixBytes =
                CompositeKeySerializationUtils.computeRequiredBytesInKeyGroupPrefix(numberOfKeyGroups);
    }

    KeyGroupRange getKeyGroupRange() {
        return keyGroupRange;
    }
    @Nonnull
    @Override
    public <T extends HeapPriorityQueueElement & PriorityComparable<? super T> & Keyed<?>>
    KeyGroupedInternalPriorityQueue<T> create(
            @Nonnull String stateName,
            @Nonnull TypeSerializer<T> byteOrderedElementSerializer) {
        return create(stateName, byteOrderedElementSerializer, false);
    }

    @Nonnull
    @Override
    public <T extends HeapPriorityQueueElement & PriorityComparable<? super T> & Keyed<?>>
    KeyGroupedInternalPriorityQueue<T> create(
            @Nonnull String stateName,
            @Nonnull TypeSerializer<T> byteOrderedElementSerializer,
            boolean allowFutureMetadataUpdates) {
        PQTableDescription pqTableDescription = new PQTableDescription<>(stateName, byteOrderedElementSerializer);
        BoostPQTable table = pqTableDescription.createTable(db);
        tryRegisterPriorityQueueMetaInfo(
                stateName, byteOrderedElementSerializer, allowFutureMetadataUpdates);
        return new KeyGroupPartitionedPriorityQueue<>(KeyExtractorFunction.forKeyedObjects(),
            PriorityComparator.forPriorityComparableObjects(),
            new KeyGroupPartitionedPriorityQueue.PartitionQueueSetFactory<T, OckDBStateCachingPriorityQueueSet<T>>() {
                    @Nonnull
                    @Override
                    public OckDBStateCachingPriorityQueueSet<T> create(
                            int keyGroupId,
                            int numKeyGroups,
                            @Nonnull KeyExtractorFunction<T> keyExtractor,
                            @Nonnull PriorityComparator<T> elementPriorityComparator) {
                        TreeSetCache bytesSetCache = new TreeSetCache(DEFAULT_CACHES_SIZE);
                        return new OckDBStateCachingPriorityQueueSet<>(
                                keyGroupId,
                                keyGroupPrefixBytes,
                                keyExtractor,
                                stateName,
                                table,
                                byteOrderedElementSerializer,
                                sharedElementOutView,
                                sharedElementInView,
                                bytesSetCache);
                    }
                },
                keyGroupRange,
                numberOfKeyGroups);
    }

    @Nonnull
    private <T> void tryRegisterPriorityQueueMetaInfo(
            @Nonnull String stateName,
            @Nonnull TypeSerializer<T> byteOrderedElementSerializer,
            boolean allowFutureMetadataUpdates) {
        RegisteredStateMetaInfoBase stateInfo = kvStateInformation.get(stateName);
        if (stateInfo == null) {
            RegisteredPriorityQueueStateBackendMetaInfo<T> metaInfo =
                    new RegisteredPriorityQueueStateBackendMetaInfo<>(
                            stateName, byteOrderedElementSerializer);

            metaInfo = allowFutureMetadataUpdates ? metaInfo.withSerializerUpgradesAllowed() : metaInfo;
            kvStateInformation.put(stateName, metaInfo);
        } else {
            RegisteredPriorityQueueStateBackendMetaInfo<T> castedMetaInfo =
                    (RegisteredPriorityQueueStateBackendMetaInfo<T>) stateInfo;

            TypeSerializer<T> previousElementSerializer =
                    castedMetaInfo.getPreviousElementSerializer();

            if (previousElementSerializer != byteOrderedElementSerializer) {
                TypeSerializerSchemaCompatibility<T> compatibilityResult =
                        castedMetaInfo.updateElementSerializer(byteOrderedElementSerializer);
                if (compatibilityResult.isIncompatible()) {
                    throw new BSSRuntimeException(
                            new StateMigrationException(
                                    "The new priority queue serializer must not be incompatible."));
                }

                RegisteredPriorityQueueStateBackendMetaInfo<T> metaInfo =
                        new RegisteredPriorityQueueStateBackendMetaInfo<>(
                                stateName, byteOrderedElementSerializer);

                metaInfo =
                        allowFutureMetadataUpdates
                                ? metaInfo.withSerializerUpgradesAllowed()
                                : metaInfo;
                kvStateInformation.put(stateName, metaInfo);
            }
        }
    }
}