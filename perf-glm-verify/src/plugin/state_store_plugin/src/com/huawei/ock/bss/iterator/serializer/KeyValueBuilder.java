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

import com.huawei.ock.bss.common.BinaryKeyValueItem;
import com.huawei.ock.bss.common.BoostStateType;
import com.huawei.ock.bss.common.exception.BSSRuntimeException;
import com.huawei.ock.bss.iterator.BoostSortedKeyValueIterator;
import com.huawei.ock.bss.snapshot.FullBoostSnapshotResources;

import org.apache.flink.api.common.typeutils.TypeSerializer;
import org.apache.flink.api.common.typeutils.base.ListSerializer;
import org.apache.flink.api.common.typeutils.base.MapSerializer;
import org.apache.flink.runtime.state.RegisteredKeyValueStateBackendMetaInfo;
import org.apache.flink.runtime.state.RegisteredPriorityQueueStateBackendMetaInfo;
import org.apache.flink.runtime.state.RegisteredStateMetaInfoBase;
import org.apache.flink.runtime.state.StateSnapshotTransformer;
import org.apache.flink.util.Preconditions;

import java.io.IOException;
import java.util.Map;

import javax.annotation.Nullable;

public class KeyValueBuilder<K> {
    private final Map<String, FullBoostSnapshotResources.KvMetaData> kvStateMetaDataMap;

    private final BinaryDataBuilder<K> binaryDataBuilder;

    public KeyValueBuilder(int totalKeyGroups, TypeSerializer<K> keySerializer,
        Map<String, FullBoostSnapshotResources.KvMetaData> kvStateMetaDataMap) {
        this.kvStateMetaDataMap = kvStateMetaDataMap;
        this.binaryDataBuilder = new BinaryDataBuilder<>(keySerializer, totalKeyGroups);
    }

    /**
     * buildKeyValueItem
     *
     * @param binaryKeyValueItem BinaryKeyValueItem
     * @return BoostSortedKeyValueIterator.KeyValueItem
     * @throws IOException IOException
     */
    @Nullable
    public BoostSortedKeyValueIterator.KeyValueItem buildKeyValueItem(BinaryKeyValueItem binaryKeyValueItem)
        throws IOException {
        if (binaryKeyValueItem == null) {
            throw new BSSRuntimeException("binaryKeyValueItem is null.");
        }
        FullBoostSnapshotResources.KvMetaData kvMetaData =
            this.kvStateMetaDataMap.get(binaryKeyValueItem.getStateName());
        if (kvMetaData == null) {
            throw new BSSRuntimeException("kvMetaData is null.");
        }
        Preconditions.checkNotNull(kvMetaData, "Failed to find meta for state %s", binaryKeyValueItem.getStateName());

        byte[] binaryValue = binaryKeyValueItem.getValue();

        StateSnapshotTransformer<byte[]> stateSnapshotTransformer = kvMetaData.stateSnapshotTransformer;
        if (stateSnapshotTransformer != null) {
            binaryValue = stateSnapshotTransformer.filterOrTransform(binaryValue);
            if (binaryValue == null) {
                return null;
            }
        }

        BoostStateType stateType = binaryKeyValueItem.getStateType();
        RegisteredStateMetaInfoBase metaInfo = kvMetaData.metaInfo;

        byte[] key = buildKey(binaryKeyValueItem, stateType, metaInfo);
        binaryValue = buildValue(binaryValue, stateType, metaInfo);

        return new BoostSortedKeyValueIterator.KeyValueItem(binaryKeyValueItem.getKeyGroup(), kvMetaData.stateId, key,
            binaryValue);
    }

    private byte[] buildKey(BinaryKeyValueItem binaryKeyValueItem, BoostStateType stateType,
        RegisteredStateMetaInfoBase metaInfo) throws IOException {
        byte[] binaryKey = binaryKeyValueItem.getKey();
        if (metaInfo instanceof RegisteredPriorityQueueStateBackendMetaInfo) {
            return binaryKey;
        }
        int keyGroup = binaryKeyValueItem.getKeyGroup();
        RegisteredKeyValueStateBackendMetaInfo<?, ?> kvMeta = (RegisteredKeyValueStateBackendMetaInfo<?, ?>) metaInfo;
        switch (stateType) {
            case VALUE:
            case LIST:
                return this.binaryDataBuilder.formatValueAndListKey(keyGroup, binaryKey);
            case MAP:
                return this.binaryDataBuilder.formatMapKey(keyGroup, binaryKey, getUserKeySerializer(kvMeta),
                    binaryKeyValueItem.getMapKey());

            case SUB_VALUE:
            case SUB_LIST:
                return this.binaryDataBuilder.formatValueAndListKey(keyGroup, binaryKey, kvMeta

                    .getNamespaceSerializer(), binaryKeyValueItem.getNamespace());
            case SUB_MAP:
                return this.binaryDataBuilder.formatNSMapKey(keyGroup, binaryKey, kvMeta

                        .getNamespaceSerializer(), binaryKeyValueItem.getNamespace(), getUserKeySerializer(kvMeta),
                    binaryKeyValueItem.getMapKey());
        }

        throw new UnsupportedOperationException("Unsupported state type " + stateType);
    }

    private byte[] buildValue(byte[] value, BoostStateType stateType,
        RegisteredStateMetaInfoBase metaInfo) throws IOException {
        if (!(metaInfo instanceof RegisteredKeyValueStateBackendMetaInfo)) {
            return new byte[0];
        }
        RegisteredKeyValueStateBackendMetaInfo<?, ?> kvMeta = (RegisteredKeyValueStateBackendMetaInfo<?, ?>) metaInfo;
        switch (stateType) {
            case VALUE:
            case SUB_VALUE:
                return value;
            case LIST:
            case SUB_LIST:
                return this.binaryDataBuilder.formatListValue(getElementSerializer(kvMeta), value);
            case MAP:
            case SUB_MAP:
                return this.binaryDataBuilder.formatUserValueForMapState(getUserValueSerializer(kvMeta), value);
        }
        throw new UnsupportedOperationException("Unsupported state type " + stateType);
    }

    @SuppressWarnings({"unchecked", "rawtypes"})
    private TypeSerializer<Object> getUserKeySerializer(RegisteredKeyValueStateBackendMetaInfo<?, ?> metaInfo) {
        return ((MapSerializer) metaInfo.getStateSerializer()).getKeySerializer();
    }

    @SuppressWarnings({"unchecked", "rawtypes"})
    private TypeSerializer<Object> getUserValueSerializer(RegisteredKeyValueStateBackendMetaInfo<?, ?> metaInfo) {
        return ((MapSerializer) metaInfo.getStateSerializer()).getValueSerializer();
    }

    @SuppressWarnings({"unchecked", "rawtypes"})
    private TypeSerializer<Object> getElementSerializer(RegisteredKeyValueStateBackendMetaInfo<?, ?> metaInfo) {
        return ((ListSerializer) metaInfo.getStateSerializer()).getElementSerializer();
    }
}