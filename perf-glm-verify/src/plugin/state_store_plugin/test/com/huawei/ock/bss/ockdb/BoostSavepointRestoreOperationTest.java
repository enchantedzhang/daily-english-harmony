/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of the Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 */

package com.huawei.ock.bss.ockdb;

import com.huawei.ock.bss.EmbeddedOckStateBackend;
import com.huawei.ock.bss.OckDBKeyedStateBackend;
import com.huawei.ock.bss.common.BoostStateDB;
import com.huawei.ock.bss.resource.ResourceContainer;
import com.huawei.ock.bss.restore.BoostRestoreOperation;
import com.huawei.ock.bss.restore.BoostSavepointRestoreOperation;

import org.apache.flink.api.common.ExecutionConfig;
import org.apache.flink.api.common.typeutils.base.StringSerializer;
import org.apache.flink.configuration.Configuration;
import org.apache.flink.core.fs.CloseableRegistry;
import org.apache.flink.metrics.MetricGroup;
import org.apache.flink.runtime.query.TaskKvStateRegistry;
import org.apache.flink.runtime.state.KeyGroupRange;
import org.apache.flink.runtime.state.KeyedStateHandle;
import org.apache.flink.runtime.state.LocalRecoveryConfig;
import org.apache.flink.runtime.state.PriorityQueueSetFactory;
import org.apache.flink.runtime.state.SavepointKeyedStateHandle;
import org.apache.flink.runtime.state.StreamCompressionDecorator;
import org.apache.flink.runtime.state.metrics.LatencyTrackingStateConfig;
import org.apache.flink.runtime.state.ttl.TtlTimeProvider;
import org.apache.flink.util.UserCodeClassLoader;
import org.junit.Assert;
import org.junit.Test;
import org.powermock.api.mockito.PowerMockito;
import org.powermock.reflect.Whitebox;

import java.io.File;
import java.util.Collection;
import java.util.Collections;
import java.util.HashMap;

/**
 * Savepoint restore tests.
 */
public class BoostSavepointRestoreOperationTest {
    @Test
    public void shouldUseTotalKeyGroupsPrefixForSavepointRestore() throws Exception {
        TestBuilder builder = new TestBuilder();

        BoostRestoreOperation<String> restoreOperation = builder.createSavepointRestoreOperation();

        Assert.assertTrue(restoreOperation instanceof BoostSavepointRestoreOperation);
        Assert.assertEquals(2,
            ((Integer) Whitebox.getInternalState(restoreOperation, "keyGroupPrefixBytes")).intValue());
    }

    private static final class TestBuilder extends AbstractOckDBKeyedStateBackendBuilder<String> {
        private TestBuilder() {
            super(1024, new KeyGroupRange(40, 48), userCodeClassLoader(), new File("test"),
                PowerMockito.mock(LocalRecoveryConfig.class), PowerMockito.mock(TaskKvStateRegistry.class), "test",
                new ExecutionConfig(), StringSerializer.INSTANCE, TtlTimeProvider.DEFAULT, latencyTrackingConfig(),
                new CloseableRegistry(), PowerMockito.mock(StreamCompressionDecorator.class), savepointHandle(), false,
                PowerMockito.mock(ResourceContainer.class), EmbeddedOckStateBackend.PriorityQueueStateType.HEAP,
                new Configuration());
        }

        private static UserCodeClassLoader userCodeClassLoader() {
            UserCodeClassLoader userCodeClassLoader = PowerMockito.mock(UserCodeClassLoader.class);
            PowerMockito.when(userCodeClassLoader.asClassLoader())
                .thenReturn(BoostSavepointRestoreOperationTest.class.getClassLoader());
            return userCodeClassLoader;
        }

        private static LatencyTrackingStateConfig latencyTrackingConfig() {
            LatencyTrackingStateConfig latencyTrackingConfig = PowerMockito.mock(LatencyTrackingStateConfig.class);
            PowerMockito.when(latencyTrackingConfig.getMetricGroup()).thenReturn(PowerMockito.mock(MetricGroup.class));
            return latencyTrackingConfig;
        }

        private static Collection<KeyedStateHandle> savepointHandle() {
            return Collections.<KeyedStateHandle>singleton(PowerMockito.mock(SavepointKeyedStateHandle.class));
        }

        private BoostRestoreOperation<String> createSavepointRestoreOperation() throws Exception {
            return initRestoreOperation("test", PowerMockito.mock(BoostStateDB.class), 2, new HashMap<>(),
                new HashMap<>(), PowerMockito.mock(PriorityQueueSetFactory.class), new HashMap<>(), new HashMap<>(),
                new HashMap<>());
        }

        @Override
        public OckDBKeyedStateBackend<String> build() {
            return null;
        }
    }
}
