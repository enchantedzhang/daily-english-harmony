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

package com.huawei.ock.bss;

import org.apache.flink.configuration.IllegalConfigurationException;
import org.apache.flink.configuration.ReadableConfig;
import org.apache.flink.runtime.state.StateBackendFactory;

import java.io.IOException;

/**
 * OckDBStateBackendFactory类，状态后端入口类
 *
 * @since 2025年1月12日16:26:41
 */
public class OckDBStateBackendFactory implements StateBackendFactory<EmbeddedOckStateBackend> {
    @Override
    public EmbeddedOckStateBackend createFromConfig(ReadableConfig config, ClassLoader classLoader)
        throws IllegalConfigurationException, IOException {
        return new EmbeddedOckStateBackend().configure(config, classLoader);
    }
}
