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

package com.huawei.ock.bss.restore;

import com.huawei.ock.bss.common.BoostStateDB;
import com.huawei.ock.bss.common.exception.BSSRuntimeException;
import com.huawei.ock.bss.state.internal.KeyedListState;
import com.huawei.ock.bss.state.internal.KeyedListStateImpl;
import com.huawei.ock.bss.state.internal.KeyedMapState;
import com.huawei.ock.bss.state.internal.KeyedMapStateImpl;
import com.huawei.ock.bss.state.internal.KeyedState;
import com.huawei.ock.bss.state.internal.KeyedValueState;
import com.huawei.ock.bss.state.internal.KeyedValueStateImpl;
import com.huawei.ock.bss.state.internal.descriptor.InternalStateType;
import com.huawei.ock.bss.state.internal.descriptor.keyed.KeyedListStateDescriptor;
import com.huawei.ock.bss.state.internal.descriptor.keyed.KeyedMapStateDescriptor;
import com.huawei.ock.bss.state.internal.descriptor.keyed.KeyedStateDescriptor;
import com.huawei.ock.bss.state.internal.descriptor.keyed.KeyedValueStateDescriptor;
import com.huawei.ock.bss.state.internal.descriptor.nskeyed.NSKeyedListStateDescriptor;
import com.huawei.ock.bss.state.internal.descriptor.nskeyed.NSKeyedMapStateDescriptor;
import com.huawei.ock.bss.state.internal.descriptor.nskeyed.NSKeyedStateDescriptor;
import com.huawei.ock.bss.state.internal.descriptor.nskeyed.NSKeyedValueStateDescriptor;
import com.huawei.ock.bss.state.internal.namespace.NSKeyedListState;
import com.huawei.ock.bss.state.internal.namespace.NSKeyedListStateImpl;
import com.huawei.ock.bss.state.internal.namespace.NSKeyedMapState;
import com.huawei.ock.bss.state.internal.namespace.NSKeyedMapStateImpl;
import com.huawei.ock.bss.state.internal.namespace.NSKeyedState;
import com.huawei.ock.bss.state.internal.namespace.NSKeyedValueState;
import com.huawei.ock.bss.state.internal.namespace.NSKeyedValueStateImpl;
import com.huawei.ock.bss.table.api.KListTable;
import com.huawei.ock.bss.table.api.KMapTable;
import com.huawei.ock.bss.table.api.KVTable;
import com.huawei.ock.bss.table.api.Table;
import com.huawei.ock.bss.table.api.NsKListTable;
import com.huawei.ock.bss.table.api.NsKMapTable;
import com.huawei.ock.bss.table.api.NsKVTable;
import com.huawei.ock.bss.table.api.TableDescription;
import com.huawei.ock.bss.util.DescriptionUtils;

import org.apache.flink.api.common.state.StateDescriptor;
import org.apache.flink.api.common.typeutils.TypeSerializer;
import org.apache.flink.api.common.typeutils.base.ListSerializer;
import org.apache.flink.api.common.typeutils.base.MapSerializer;
import org.apache.flink.core.memory.DataInputDeserializer;
import org.apache.flink.runtime.state.CompositeKeySerializationUtils;
import org.apache.flink.runtime.state.KeyExtractorFunction;
import org.apache.flink.runtime.state.KeyGroupRange;
import org.apache.flink.runtime.state.KeyGroupedInternalPriorityQueue;
import org.apache.flink.runtime.state.KeyedStateHandle;
import org.apache.flink.runtime.state.ListDelimitedSerializer;
import org.apache.flink.runtime.state.PriorityQueueSetFactory;
import org.apache.flink.runtime.state.RegisteredKeyValueStateBackendMetaInfo;
import org.apache.flink.runtime.state.RegisteredPriorityQueueStateBackendMetaInfo;
import org.apache.flink.runtime.state.RegisteredStateMetaInfoBase;
import org.apache.flink.runtime.state.StateSerializerProvider;
import org.apache.flink.runtime.state.VoidNamespace;
import org.apache.flink.runtime.state.VoidNamespaceSerializer;
import org.apache.flink.runtime.state.heap.HeapPriorityQueueElement;
import org.apache.flink.runtime.state.heap.HeapPriorityQueueSet;
import org.apache.flink.runtime.state.heap.HeapPriorityQueueSetFactory;
import org.apache.flink.runtime.state.heap.HeapPriorityQueueSnapshotRestoreWrapper;
import org.apache.flink.runtime.state.metainfo.StateMetaInfoSnapshot;
import org.apache.flink.runtime.state.restore.FullSnapshotRestoreOperation;
import org.apache.flink.runtime.state.restore.KeyGroup;
import org.apache.flink.runtime.state.restore.KeyGroupEntry;
import org.apache.flink.runtime.state.restore.SavepointRestoreResult;
import org.apache.flink.runtime.state.restore.ThrowingIterator;
import org.apache.flink.util.Preconditions;
import org.apache.flink.util.StateMigrationException;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import java.io.IOException;
import java.io.UnsupportedEncodingException;
import java.util.Collection;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

import javax.annotation.Nonnull;

/**
 * savepoint restore
 *
 * @param <K> key
 * @since BeiMing 25.0.T1
 */
@SuppressWarnings("rawtypes")
public class BoostSavepointRestoreOperation<K> extends AbstractBoostRestoreOperation<K> {
    private static final Logger LOG = LoggerFactory.getLogger(BoostSavepointRestoreOperation.class);

    private final int keyGroupPrefixBytes;

    private final BoostStateDB db;

    private final KeyGroupRange keyGroupRange;

    private final FullSnapshotRestoreOperation<K> restoreOperation;

    private final StateSerializerProvider<K> serializerProvider;

    private final DataInputDeserializer keyDeserializer;

    private final DataInputDeserializer valueDeserializer;

    private final ListDelimitedSerializer listDelimitedSerializer;

    private final PriorityQueueSetFactory priorityQueueSetFactory;

    private final Map<String, Table> tables;

    private final Map<String, KeyedState> keyedStateMap;

    private final Map<String, NSKeyedState> nsKeyedStateMap;

    private final Map<String, KeyedStateDescriptor> keyedStateDescriptorMap;

    private final Map<String, NSKeyedStateDescriptor> nsKeyedStateDescriptorMap;

    private final Map<String, RegisteredStateMetaInfoBase> registeredKVStateMetas;

    private final Map<String, HeapPriorityQueueSnapshotRestoreWrapper<?>> registeredPQStates;

    public BoostSavepointRestoreOperation(
        KeyGroupRange keyGroupRange,
        int keyGroupPrefixBytes,
        ClassLoader userCodeClassLoader,
        StateSerializerProvider<K> serializerProvider,
        BoostStateDB db,
        @Nonnull Collection<KeyedStateHandle> restoreStateHandles,
        Map<String, Table> tables,
        Map<String, RegisteredStateMetaInfoBase> registeredKVStateMetas,
        Map<String, KeyedStateDescriptor> keyedStateDescriptorMap,
        Map<String, NSKeyedStateDescriptor> nsKeyedStateDescriptorMap,
        Map<String, HeapPriorityQueueSnapshotRestoreWrapper<?>> registeredPQStates,
        PriorityQueueSetFactory priorityQueueFactory) {
        this.restoreOperation =
            new FullSnapshotRestoreOperation<>(keyGroupRange, userCodeClassLoader, restoreStateHandles,
                serializerProvider);
        this.keyedStateMap = new HashMap<>();
        this.nsKeyedStateMap = new HashMap<>();
        this.keyGroupPrefixBytes = keyGroupPrefixBytes;
        this.serializerProvider = serializerProvider;
        this.keyDeserializer = new DataInputDeserializer();
        this.valueDeserializer = new DataInputDeserializer();
        this.listDelimitedSerializer = new ListDelimitedSerializer();
        this.priorityQueueSetFactory = priorityQueueFactory;
        this.keyGroupRange = keyGroupRange;
        this.db = db;
        this.keyedStateDescriptorMap = keyedStateDescriptorMap;
        this.nsKeyedStateDescriptorMap = nsKeyedStateDescriptorMap;
        this.tables = tables;
        this.registeredKVStateMetas = registeredKVStateMetas;
        this.registeredPQStates = registeredPQStates;
        LOG.info("Finished new BoostSavepointRestoreOperation");
    }

    @Override
    public BoostRestoreResult restore() throws Exception {
        LOG.info("Begin savepoint restore.");
        try (ThrowingIterator<SavepointRestoreResult> restore = this.restoreOperation.restore()) {
            while (restore.hasNext()) {
                Map<Integer, RegisteredStateMetaInfoBase> restoredKvStates = new HashMap<>();
                Map<Integer, HeapPriorityQueueSnapshotRestoreWrapper<?>> restoredPQStates = new HashMap<>();
                Map<Integer, KeyGroupedInternalPriorityQueue<?>> pqStates = new HashMap<>();
                SavepointRestoreResult restoreResult = restore.next();
                List<StateMetaInfoSnapshot> restoredMetaInfos = restoreResult.getStateMetaInfoSnapshots();
                restoreStateMeta(restoredMetaInfos, restoredKvStates, restoredPQStates, pqStates);
                restoreTable();
                try (ThrowingIterator<KeyGroup> keyGroups = restoreResult.getRestoredKeyGroups()) {
                    restoreStateData(keyGroups, restoredKvStates, restoredPQStates, pqStates);
                }
            }
        }
        LOG.info("Finish savepoint restore.");
        return BoostRestoreResult.EMPTY_RESULT;
    }

    @SuppressWarnings("unchecked")
    private <T extends org.apache.flink.runtime.state.heap.HeapPriorityQueueElement
        & org.apache.flink.runtime.state.PriorityComparable<? super T>> void restoreStateMeta(
        List<StateMetaInfoSnapshot> stateMetaInfoSnapshots,
        Map<Integer, RegisteredStateMetaInfoBase> restoredKVStates,
        Map<Integer, HeapPriorityQueueSnapshotRestoreWrapper<?>> restoredPQStates,
        Map<Integer, KeyGroupedInternalPriorityQueue<?>> pqStates) {
        LOG.info("Begin savepoint restore restoreStateMeta.");
        for (int i = 0; i < stateMetaInfoSnapshots.size(); i++) {
            RegisteredStateMetaInfoBase metaInfo;
            HeapPriorityQueueSnapshotRestoreWrapper<?> pqInfo;
            StateMetaInfoSnapshot stateMetaInfoSnapshot = stateMetaInfoSnapshots.get(i);
            String name = stateMetaInfoSnapshot.getName();
            switch (stateMetaInfoSnapshot.getBackendStateType()) {
                case KEY_VALUE:
                    metaInfo = registeredKVStateMetas.computeIfAbsent(name,
                        key -> new RegisteredKeyValueStateBackendMetaInfo<>(stateMetaInfoSnapshot));
                    restoredKVStates.put(i, metaInfo);
                    break;
                case PRIORITY_QUEUE:
                    if (this.priorityQueueSetFactory instanceof HeapPriorityQueueSetFactory) {
                        RegisteredPriorityQueueStateBackendMetaInfo pqMetaInfo =
                                new RegisteredPriorityQueueStateBackendMetaInfo<>(stateMetaInfoSnapshot);
                        HeapPriorityQueueSet<T> priorityQueueSet =
                            ((HeapPriorityQueueSetFactory) this.priorityQueueSetFactory).create(pqMetaInfo.getName(),
                            pqMetaInfo.getElementSerializer());
                        pqInfo = registeredPQStates.computeIfAbsent(name,
                                key -> new HeapPriorityQueueSnapshotRestoreWrapper(priorityQueueSet, pqMetaInfo,
                                        KeyExtractorFunction.forKeyedObjects(), this.keyGroupRange,
                                        this.db.getConfig().getMaxParallelism()));
                        restoredPQStates.put(i, pqInfo);
                        break;
                    }
                    Object object = registeredKVStateMetas.computeIfAbsent(name,
                            key -> new RegisteredPriorityQueueStateBackendMetaInfo<>(stateMetaInfoSnapshot));
                    RegisteredPriorityQueueStateBackendMetaInfo pqMetaInfo;
                    if (object instanceof RegisteredPriorityQueueStateBackendMetaInfo) {
                        pqMetaInfo = (RegisteredPriorityQueueStateBackendMetaInfo) object;
                    } else {
                        throw new IllegalStateException("Failed to restore Meta info, invalid pqMetaInfo type: "
                                + object.getClass().getSimpleName() + ".");
                    }
                    KeyGroupedInternalPriorityQueue<?> queue =
                            priorityQueueSetFactory.create(pqMetaInfo.getName(),
                                    pqMetaInfo.getPreviousElementSerializer());
                    restoredKVStates.put(i, pqMetaInfo);
                    pqStates.put(i, queue);
                    break;
                default:
                    throw new IllegalStateException("Failed to restore Meta info because of invalid state type: "
                        + stateMetaInfoSnapshot.getBackendStateType() + ".");
            }
        }
        LOG.info("Finish savepoint restore restoreStateMeta.");
    }

    @SuppressWarnings("unchecked")
    private void restoreStateData(ThrowingIterator<KeyGroup> keyGroups,
        Map<Integer, RegisteredStateMetaInfoBase> restoredKVStates,
        Map<Integer, HeapPriorityQueueSnapshotRestoreWrapper<?>> restoredPQStates,
        Map<Integer, KeyGroupedInternalPriorityQueue<?>> pqStates)
        throws StateMigrationException, IOException {
        RegisteredKeyValueStateBackendMetaInfo<?, ?> kvMetaInfo = null;
        RegisteredPriorityQueueStateBackendMetaInfo<?> pqNativeMetaInfo = null;
        HeapPriorityQueueSnapshotRestoreWrapper<HeapPriorityQueueElement> pqMetaInfo = null;
        KeyGroupedInternalPriorityQueue<?> queue = null;
        while (keyGroups.hasNext()) {
            KeyGroup keyGroup = keyGroups.next();
            if (keyGroup == null) {
                LOG.error("keyGroup is null while trying to restoreStateData.");
                continue;
            }
            try (ThrowingIterator<KeyGroupEntry> entries = keyGroup.getKeyGroupEntries()) {
                int prevStateId = -1;
                while (entries.hasNext()) {
                    KeyGroupEntry entry = entries.next();
                    if (entry == null) {
                        LOG.error("KeyGroupEntry is null while trying to restoreStateData.");
                        continue;
                    }
                    int kvStateId = entry.getKvStateId();
                    if (kvStateId != prevStateId) {
                        prevStateId = kvStateId;
                        RegisteredStateMetaInfoBase base = restoredKVStates.get(kvStateId);
                        if (base instanceof RegisteredKeyValueStateBackendMetaInfo) {
                            kvMetaInfo = (RegisteredKeyValueStateBackendMetaInfo<?, ?>) base;
                            pqNativeMetaInfo = null;
                            queue = null;
                        } else if (base instanceof RegisteredPriorityQueueStateBackendMetaInfo) {
                            pqNativeMetaInfo = (RegisteredPriorityQueueStateBackendMetaInfo) base;
                            queue = pqStates.get(kvStateId);
                            kvMetaInfo = null;
                        } else if (base == null) {
                            pqNativeMetaInfo = null;
                            queue = pqStates.get(kvStateId);
                            kvMetaInfo = null;
                        } else {
                            LOG.error("RegisteredStateMetaInfoBase type not match.");
                            continue;
                        }
                        pqMetaInfo =
                            (HeapPriorityQueueSnapshotRestoreWrapper<HeapPriorityQueueElement>)
                                restoredPQStates.get(kvStateId);
                    }

                    if (pqMetaInfo != null) {
                        restoreQueueElement(pqMetaInfo, entry);
                        continue;
                    }
                    if (pqNativeMetaInfo != null && queue != null) {
                        restoreQueueElement(pqNativeMetaInfo, entry, queue);
                        continue;
                    }

                    if (kvMetaInfo != null) {
                        // 实际无作用，仅根据name check了map中对应state是否为null
                        readKvStateData(kvMetaInfo, entry);
                        continue;
                    }

                    throw new IllegalStateException("Unknown state id: " + kvStateId);
                }
            }
        }
    }

    @SuppressWarnings("unchecked")
    private void restoreTable() {
        for (RegisteredStateMetaInfoBase backendMetaInfo : this.registeredKVStateMetas.values()) {
            if (backendMetaInfo == null) {
                LOG.error("backendMetaInfo is null while trying to restoreTable.");
                continue;
            }
            if (!(backendMetaInfo instanceof RegisteredKeyValueStateBackendMetaInfo)) {
                continue;
            }
            switch (((RegisteredKeyValueStateBackendMetaInfo) backendMetaInfo).getStateType()) {
                case LIST:
                    restoreKeyedListStateTable((RegisteredKeyValueStateBackendMetaInfo) backendMetaInfo);
                    continue;
                case MAP:
                    restoreKeyedMapStateTable((RegisteredKeyValueStateBackendMetaInfo) backendMetaInfo);
                    continue;
                case VALUE:
                case REDUCING:
                case AGGREGATING:
                    restoreKeyedValueStateTable((RegisteredKeyValueStateBackendMetaInfo) backendMetaInfo);
                    continue;
                default:
                    throw new UnsupportedOperationException("Restore table failed because of invalid state type "
                            + ((RegisteredKeyValueStateBackendMetaInfo) backendMetaInfo).getStateType());
            }
        }
    }

    @SuppressWarnings("unchecked")
    private <N, V> void restoreKeyedValueStateTable(RegisteredKeyValueStateBackendMetaInfo<N, V> backendMetaInfo) {
        String name = backendMetaInfo.getName();
        InternalStateType internalStateType = getInternalValueStateType(backendMetaInfo);
        if (internalStateType.isKeyedState()) {
            KeyedValueStateDescriptor<K, V> descriptor =
                new KeyedValueStateDescriptor<>(name, this.serializerProvider.previousSchemaSerializer(),
                    backendMetaInfo.getPreviousStateSerializer(), internalStateType);
            TableDescription tableDescription =
                DescriptionUtils.createTableDescription(this.db.getConfig().getMaxParallelism(), descriptor);
            KVTable<K, V> table = (KVTable<K, V>) this.db.getTableOrCreate(tableDescription);
            KeyedValueState<K, V> valueState = new KeyedValueStateImpl<>(descriptor, table);

            this.keyedStateDescriptorMap.put(name, descriptor);
            this.tables.put(name, table);
            this.keyedStateMap.put(name, valueState);
            return;
        }

        NSKeyedValueStateDescriptor<K, N, V> descriptor =
            new NSKeyedValueStateDescriptor<>(name, this.serializerProvider.previousSchemaSerializer(),
                backendMetaInfo.getPreviousNamespaceSerializer(), backendMetaInfo.getPreviousStateSerializer(),
                internalStateType);
        TableDescription tableDescription =
            DescriptionUtils.createTableDescription(this.db.getConfig().getMaxParallelism(), descriptor);
        NsKVTable<K, N, V> table = (NsKVTable<K, N, V>) this.db.getTableOrCreate(tableDescription);
        NSKeyedValueState<K, N, V> nsValueState = new NSKeyedValueStateImpl<>(descriptor, table);

        this.nsKeyedStateDescriptorMap.put(name, descriptor);
        this.tables.put(name, table);
        this.nsKeyedStateMap.put(name, nsValueState);
    }

    @SuppressWarnings("unchecked")
    private <N, E> void restoreKeyedListStateTable(RegisteredKeyValueStateBackendMetaInfo<N, List<E>> backendMetaInfo) {
        String name = backendMetaInfo.getName();
        if (backendMetaInfo.getPreviousNamespaceSerializer() instanceof VoidNamespaceSerializer) {
            KeyedListStateDescriptor<K, E> descriptor =
                new KeyedListStateDescriptor<>(name, this.serializerProvider.previousSchemaSerializer(),
                    ((ListSerializer<E>) backendMetaInfo.getPreviousStateSerializer()).getElementSerializer());
            TableDescription tableDescription =
                DescriptionUtils.createTableDescription(this.db.getConfig().getMaxParallelism(), descriptor);
            KListTable<K, E> table = (KListTable<K, E>) this.db.getTableOrCreate(tableDescription);
            KeyedListState<K, E> listState = new KeyedListStateImpl<>(descriptor, table);

            this.keyedStateDescriptorMap.put(name, descriptor);
            this.tables.put(name, table);
            this.keyedStateMap.put(name, listState);
            return;
        }

        NSKeyedListStateDescriptor<K, N, E> descriptor =
            new NSKeyedListStateDescriptor<>(name, this.serializerProvider.previousSchemaSerializer(),
                backendMetaInfo.getPreviousNamespaceSerializer(),
                ((ListSerializer<E>) backendMetaInfo.getPreviousStateSerializer()).getElementSerializer());
        TableDescription tableDescription =
            DescriptionUtils.createTableDescription(this.db.getConfig().getMaxParallelism(), descriptor);
        NsKListTable<K, N, E> table = (NsKListTable<K, N, E>) this.db.getTableOrCreate(tableDescription);
        NSKeyedListState<K, N, E> nsListState = new NSKeyedListStateImpl<>(descriptor, table);

        this.nsKeyedStateDescriptorMap.put(name, descriptor);
        this.tables.put(name, table);
        this.nsKeyedStateMap.put(name, nsListState);
    }

    @SuppressWarnings("unchecked")
    private <N, UK, UV> void restoreKeyedMapStateTable(
        RegisteredKeyValueStateBackendMetaInfo<N, Map<UK, UV>> backendMetaInfo) {
        String name = backendMetaInfo.getName();
        if (backendMetaInfo.getPreviousNamespaceSerializer() instanceof VoidNamespaceSerializer) {
            KeyedMapStateDescriptor<K, UK, UV> descriptor =
                new KeyedMapStateDescriptor<>(name, this.serializerProvider.previousSchemaSerializer(),
                    backendMetaInfo.getPreviousStateSerializer());
            TableDescription tableDescription =
                DescriptionUtils.createTableDescription(this.db.getConfig().getMaxParallelism(), descriptor);
            KMapTable<K, UK, UV, Map<UK, UV>> table =
                (KMapTable<K, UK, UV, Map<UK, UV>>) this.db.getTableOrCreate(tableDescription);
            KeyedMapState<K, UK, UV> mapState = new KeyedMapStateImpl<>(descriptor, table);

            this.keyedStateDescriptorMap.put(name, descriptor);
            this.tables.put(name, table);
            this.keyedStateMap.put(name, mapState);
            return;
        }

        NSKeyedMapStateDescriptor<K, N, UK, UV> descriptor =
            new NSKeyedMapStateDescriptor<>(name, this.serializerProvider.previousSchemaSerializer(),
                backendMetaInfo.getPreviousNamespaceSerializer(), backendMetaInfo.getPreviousStateSerializer());
        TableDescription tableDescription =
            DescriptionUtils.createTableDescription(this.db.getConfig().getMaxParallelism(), descriptor);
        NsKMapTable<K, N, UK, UV, Map<UK, UV>> table =
            (NsKMapTable<K, N, UK, UV, Map<UK, UV>>) this.db.getTableOrCreate(tableDescription);
        NSKeyedMapState<K, N, UK, UV> nsMapState = new NSKeyedMapStateImpl<>(descriptor, table);
        this.nsKeyedStateDescriptorMap.put(name, descriptor);
        this.tables.put(name, table);
        this.nsKeyedStateMap.put(name, nsMapState);
    }

    private void restoreQueueElement(HeapPriorityQueueSnapshotRestoreWrapper<HeapPriorityQueueElement> restoredPQ,
        KeyGroupEntry groupEntry) throws IOException {
        this.keyDeserializer.setBuffer(groupEntry.getKey(), this.keyGroupPrefixBytes,
                groupEntry.getKey().length - this.keyGroupPrefixBytes);
        HeapPriorityQueueElement timer =
            restoredPQ.getMetaInfo().getElementSerializer().deserialize(this.keyDeserializer);
        restoredPQ.getPriorityQueue().add(timer);
    }

    private void restoreQueueElement(RegisteredPriorityQueueStateBackendMetaInfo<?> metaInfo,
        KeyGroupEntry groupEntry, KeyGroupedInternalPriorityQueue<?> queue)
            throws IOException {
        this.keyDeserializer.setBuffer(groupEntry.getKey());
        this.keyDeserializer.skipBytesToRead(this.keyGroupPrefixBytes);
        Object ele = metaInfo.getElementSerializer().deserialize(keyDeserializer);
        KeyGroupedInternalPriorityQueue<Object> typedQueue = (KeyGroupedInternalPriorityQueue<Object>) queue;
        typedQueue.add(ele);
    }

    private <N> void readKvStateData(RegisteredKeyValueStateBackendMetaInfo<N, ?> backendMetaInfo,
        KeyGroupEntry groupEntry) throws IOException {
        TypeSerializer<K> keySerializer = this.serializerProvider.previousSchemaSerializer();
        String stateName = backendMetaInfo.getName();
        TypeSerializer<N> namespaceSerializer = backendMetaInfo.getPreviousNamespaceSerializer();
        TypeSerializer<?> stateSerializer = backendMetaInfo.getPreviousStateSerializer();
        Preconditions.checkNotNull(namespaceSerializer,
            "PreviousNamespaceSerializer should not be null when readKvStateData.");
        Preconditions.checkNotNull(stateSerializer,
            "PreviousStateSerializer should not be null when readKvStateData.");

        boolean ambiguousKeyPossible = (keySerializer.getLength() < 0 && namespaceSerializer.getLength() < 0);
        this.keyDeserializer.setBuffer(groupEntry.getKey());
        this.valueDeserializer.setBuffer(groupEntry.getValue());
        // 此处不读取会导致偏移量无法正确增加，后续key和namespace无法正确读取
        CompositeKeySerializationUtils.readKeyGroup(this.keyGroupPrefixBytes, this.keyDeserializer);
        K key = CompositeKeySerializationUtils.readKey(keySerializer, this.keyDeserializer, ambiguousKeyPossible);
        N namespace = CompositeKeySerializationUtils.readNamespace(namespaceSerializer, this.keyDeserializer,
            ambiguousKeyPossible);
        Preconditions.checkNotNull(key, "key should not be null when readKvStateData.");
        Preconditions.checkNotNull(namespace, "namespace should not be null when readKvStateData.");

        StateDescriptor.Type stateType = backendMetaInfo.getStateType();
        switch (stateType) {
            case LIST:
                restoreListState(stateName, key, namespace, groupEntry.getValue(), (ListSerializer<?>) stateSerializer);
                return;
            case MAP:
                restoreMapState(stateName, key, namespace, (MapSerializer<?, ?>) stateSerializer);
                return;
            case VALUE:
            case REDUCING:
            case AGGREGATING:
                restoreKeyedAndNSKeyedState(stateName, key, namespace, stateSerializer);
                return;
            default:
                throw new UnsupportedEncodingException("Invalid state type " + stateType);
        }
    }

    @SuppressWarnings("unchecked")
    private <N, V> void restoreKeyedAndNSKeyedState(String stateName, K key, N namespace,
        TypeSerializer<V> valueSerializer) throws IOException {
        V value = valueSerializer.deserialize(this.valueDeserializer);
        if (namespace instanceof VoidNamespace) {
            KeyedValueState<K, V> keyedValueState = (KeyedValueState<K, V>) getKeyedStateWithCheck(stateName);
            keyedValueState.put(key, value);
            return;
        }

        NSKeyedValueState<K, N, V> nsKeyedValueState =
            (NSKeyedValueState<K, N, V>) getNSKeyedStateWithCheck(stateName);
        nsKeyedValueState.put(key, namespace, value);
    }

    @SuppressWarnings("unchecked")
    private <N, E> void restoreListState(String stateName, K key, N namespace, byte[] valueBytes,
        TypeSerializer<List<E>> stateSerializer) throws IOException {
        if (valueBytes == null) {
            LOG.warn("input valueBytes is null.");
            return;
        }

        LOG.info("restore list state:{} value size:{}", stateName, valueBytes.length);
        List<E> values = this.listDelimitedSerializer.deserializeList(valueBytes,
            ((ListSerializer<E>) stateSerializer).getElementSerializer());

        if (namespace instanceof VoidNamespace) {
            KeyedListState<K, E> keyedListState = (KeyedListState<K, E>) getKeyedStateWithCheck(stateName);
            values.forEach(e -> keyedListState.add(key, e));
            return;
        }

        NSKeyedListState<K, N, E> nsKeyedListState =
            (NSKeyedListState<K, N, E>) getNSKeyedStateWithCheck(stateName);
        values.forEach(e -> nsKeyedListState.add(key, namespace, e));
    }

    @SuppressWarnings("unchecked")
    private <N, UK, UV> void restoreMapState(String stateName, K key, N namespace,
        MapSerializer<UK, UV> stateSerializer) throws IOException {
        UK mapEntryKey = stateSerializer.getKeySerializer().deserialize(this.keyDeserializer);
        UV mapEntryValue = null;
        if (!this.valueDeserializer.readBoolean()) {
            mapEntryValue = stateSerializer.getValueSerializer().deserialize(this.valueDeserializer);
        }

        if (namespace instanceof VoidNamespace) {
            KeyedMapState<K, UK, UV> keyedMapState = (KeyedMapState<K, UK, UV>) getKeyedStateWithCheck(stateName);
            keyedMapState.add(key, mapEntryKey, mapEntryValue);
            return;
        }

        NSKeyedMapState<K, N, UK, UV> nsKeyedMapState =
            (NSKeyedMapState<K, N, UK, UV>) getNSKeyedStateWithCheck(stateName);
        nsKeyedMapState.add(key, namespace, mapEntryKey, mapEntryValue);
    }

    private KeyedState getKeyedStateWithCheck(String stateName) {
        KeyedState keyedState = this.keyedStateMap.get(stateName);
        if (keyedState == null) {
            throw new BSSRuntimeException("Failed to get KeyedState, stateName: " + stateName);
        }
        return keyedState;
    }

    private NSKeyedState getNSKeyedStateWithCheck(String stateName) {
        NSKeyedState nsKeyedState = this.nsKeyedStateMap.get(stateName);
        if (nsKeyedState == null) {
            throw new BSSRuntimeException("Failed to get NSKeyedState, stateName: " + stateName);
        }
        return nsKeyedState;
    }

    private <N, V> InternalStateType getInternalValueStateType(
        RegisteredKeyValueStateBackendMetaInfo<N, V> backendMetaInfo) {
        if (backendMetaInfo.getNamespaceSerializer() instanceof VoidNamespaceSerializer) {
            switch (backendMetaInfo.getStateType()) {
                case VALUE:
                    return InternalStateType.KEYED_VALUE;
                case REDUCING:
                    return InternalStateType.KEYED_REDUCING;
                case AGGREGATING:
                    return InternalStateType.KEYED_AGGREGATING;
            }
            throw new UnsupportedOperationException("Invalid state type: " + backendMetaInfo.getStateType());
        }

        switch (backendMetaInfo.getStateType()) {
            case VALUE:
                return InternalStateType.NSKEYED_VALUE;
            case REDUCING:
                return InternalStateType.NSKEYED_REDUCING;
            case AGGREGATING:
                return InternalStateType.NSKEYED_AGGREGATING;
        }
        throw new UnsupportedOperationException("Invalid state type: " + backendMetaInfo.getStateType());
    }

    @Override
    public void close() throws Exception {
        LOG.info("Savepoint restore closed.");
    }
}
