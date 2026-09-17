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

package com.huawei.ock.bss.transformer;

import org.apache.flink.runtime.state.StateSnapshotTransformer;
import org.apache.flink.runtime.state.StateSnapshotTransformer.StateSnapshotTransformFactory;
import org.apache.flink.api.common.state.ListStateDescriptor;
import org.apache.flink.api.common.state.StateDescriptor;
import org.apache.flink.api.common.typeutils.TypeSerializer;
import org.apache.flink.api.common.typeutils.base.ListSerializer;

import java.util.Optional;

/**
 * OckDBSnapshotTransformFactoryAdaptor
 *
 * @param <SV>  sv
 * @param <SEV> sev
 * @since 2025年1月17日21:00:42
 */
public abstract class OckDBSnapshotTransformFactoryAdaptor<SV, SEV>
    implements StateSnapshotTransformFactory<SV> {
    final StateSnapshotTransformFactory<SEV> transformFactory;

    OckDBSnapshotTransformFactoryAdaptor(
        StateSnapshotTransformFactory<SEV> transformFactory) {
        this.transformFactory = transformFactory;
    }

    /**
     * createForDeserializedState
     *
     * @return 暂不支持
     */
    public Optional<StateSnapshotTransformer<SV>> createForDeserializedState() {
        throw new UnsupportedOperationException("Only serialized state filtering is supported in OckDB backend");
    }

    /**
     * 打桩
     *
     * @param stateDesc                状态描述符
     * @param snapshotTransformFactory snapshotTransformFactory
     * @param stateSerializer          stateSerializer
     * @param <SV>                     SV
     * @param <SEV>                    SEV
     * @return StateSnapshotTransformer.StateSnapshotTransformFactory<SV>
     */
    public static <SV, SEV> StateSnapshotTransformFactory<SV> wrapStateSnapshotTransformFactory(
        StateDescriptor<?, SV> stateDesc, StateSnapshotTransformFactory<SEV> snapshotTransformFactory,
        TypeSerializer<SV> stateSerializer) {
        if (stateDesc instanceof ListStateDescriptor) {
            TypeSerializer<SEV> elementSerializer = ((ListSerializer<SEV>) stateSerializer).getElementSerializer();
            return new OckDBListStateSnapshotTransformFactory<>(snapshotTransformFactory, elementSerializer);
        }
        return new OckDBStateSnapshotTransformFactory<>(snapshotTransformFactory);
    }

    private static class OckDBStateSnapshotTransformFactory<SV, SEV>
        extends OckDBSnapshotTransformFactoryAdaptor<SV, SEV> {
        private OckDBStateSnapshotTransformFactory(StateSnapshotTransformFactory<SEV> snapshotTransformFactory) {
            super(snapshotTransformFactory);
        }

        @Override
        public Optional<StateSnapshotTransformer<byte[]>> createForSerializedState() {
            return this.transformFactory.createForSerializedState();
        }
    }

    private static class OckDBListStateSnapshotTransformFactory<SV, SEV>
        extends OckDBSnapshotTransformFactoryAdaptor<SV, SEV> {
        private final TypeSerializer<SEV> elementSerializer;

        private OckDBListStateSnapshotTransformFactory(StateSnapshotTransformFactory<SEV> transformFactory,
            TypeSerializer<SEV> elementSerializer) {
            super(transformFactory);
            this.elementSerializer = elementSerializer;
        }

        @Override
        public Optional<StateSnapshotTransformer<byte[]>> createForSerializedState() {
            return this.transformFactory.createForDeserializedState()
                .map(est -> new ListStateSnapshotTransformerWrapper<>(est, elementSerializer.duplicate()));
        }
    }
}
