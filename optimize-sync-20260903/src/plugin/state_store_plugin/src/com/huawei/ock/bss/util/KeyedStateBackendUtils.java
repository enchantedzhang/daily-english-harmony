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

package com.huawei.ock.bss.util;

import com.huawei.ock.bss.common.BoostStateDB;
import com.huawei.ock.bss.state.internal.descriptor.InternalStateType;
import com.huawei.ock.bss.state.internal.descriptor.keyed.KeyedStateDescriptor;
import com.huawei.ock.bss.state.internal.descriptor.nskeyed.NSKeyedStateDescriptor;
import com.huawei.ock.bss.table.api.Table;
import com.huawei.ock.bss.table.api.TableDescription;

import org.apache.flink.runtime.state.RegisteredKeyValueStateBackendMetaInfo;
import org.apache.flink.runtime.state.RegisteredStateMetaInfoBase;
import org.apache.flink.runtime.state.StateSnapshotTransformer;
import org.apache.flink.runtime.state.VoidNamespaceSerializer;

import java.util.Map;

/**
 * KeyedStateBackendUtils
 *
 * @since BeiMing 25.0.T1
 */
public class KeyedStateBackendUtils {
    /**
     * getOrCreateTable
     *
     * @param db               BoostStateDB
     * @param tables           tables
     * @param tableDescription TableDescription
     * @return Table
     */
    @SuppressWarnings({"rawtypes"})
    public static Table getOrCreateTable(BoostStateDB db, Map<String, Table> tables,
        TableDescription tableDescription) {
        String tableName = tableDescription.getTableName();
        Table table = tables.get(tableName);
        if (table == null) {
            table = db.getTableOrCreate(tableDescription);
            tables.put(tableName, table);
        }
        return table;
    }

    /**
     * createKeyedStateMetaInfo
     *
     * @param descriptor                 KeyedStateDescriptor
     * @param registeredKVStateMetaInfos registeredKVStateMetaInfos
     */
    @SuppressWarnings({"rawtypes", "unchecked"})
    public static void createKeyedStateMetaInfo(KeyedStateDescriptor descriptor,
        Map<String, RegisteredStateMetaInfoBase> registeredKVStateMetaInfos) {
        String name = descriptor.getName();
        RegisteredKeyValueStateBackendMetaInfo<?, ?> stateMetaInfo =
                (RegisteredKeyValueStateBackendMetaInfo<?, ?>) registeredKVStateMetaInfos.get(name);
        if (stateMetaInfo != null) {
            return;
        }

        RegisteredKeyValueStateBackendMetaInfo<?, ?> newStateMetaInfo =
            new RegisteredKeyValueStateBackendMetaInfo<>(
                InternalStateType.getOriginStateType(descriptor.getStateType()), name,
                VoidNamespaceSerializer.INSTANCE, descriptor.getValueSerializer(),
                StateSnapshotTransformer.StateSnapshotTransformFactory.noTransform());
        stateMetaInfo = new RegisteredKeyValueStateBackendMetaInfo<>(newStateMetaInfo.snapshot());
        registeredKVStateMetaInfos.put(name, stateMetaInfo);
    }

    /**
     * createNSKeyedStateMetaInfo
     *
     * @param descriptor                 NSKeyedStateDescriptor
     * @param registeredKVStateMetaInfos Map<String, RegisteredKeyValueStateBackendMetaInfo<?, ?>>
     */
    @SuppressWarnings({"rawtypes", "unchecked"})
    public static void createNSKeyedStateMetaInfo(NSKeyedStateDescriptor descriptor,
        Map<String, RegisteredStateMetaInfoBase> registeredKVStateMetaInfos) {
        String name = descriptor.getName();
        RegisteredKeyValueStateBackendMetaInfo<?, ?> stateMetaInfo =
                (RegisteredKeyValueStateBackendMetaInfo<?, ?>) registeredKVStateMetaInfos.get(name);
        if (stateMetaInfo != null) {
            return;
        }

        RegisteredKeyValueStateBackendMetaInfo<?, ?> newStateMetaInfo =
            new RegisteredKeyValueStateBackendMetaInfo<>(
                InternalStateType.getOriginStateType(descriptor.getStateType()), name,
                descriptor.getNamespaceSerializer(), descriptor.getValueSerializer(),
                StateSnapshotTransformer.StateSnapshotTransformFactory.noTransform());
        stateMetaInfo = new RegisteredKeyValueStateBackendMetaInfo<>(newStateMetaInfo.snapshot());
        registeredKVStateMetaInfos.put(name, stateMetaInfo);
    }
}
