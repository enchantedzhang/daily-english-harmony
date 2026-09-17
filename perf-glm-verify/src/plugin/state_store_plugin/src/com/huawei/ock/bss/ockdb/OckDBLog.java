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

package com.huawei.ock.bss.ockdb;

import com.huawei.ock.bss.jni.AbstractNativeHandleReference;

/**
 * 下层日志类
 *
 * @since 2025年1月17日19:23:16
 */
public class OckDBLog extends AbstractNativeHandleReference {
    /**
     * 初始化ock日志
     *
     * @param logPath 日志路径
     * @param logLevel 日志等级
     * @param logSize 日志文件大小
     * @param logNum  日志最大数量
     */
    public static void initialOckLog(String logPath, int logLevel, int logSize, int logNum) {
        initial(logPath, logLevel, logSize, logNum);
    }

    private static native long initial(String logFilePath, int logLevel, int logSize, int logNum);
}
