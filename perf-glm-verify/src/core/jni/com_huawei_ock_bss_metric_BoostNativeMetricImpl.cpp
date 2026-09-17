/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#include "com_huawei_ock_bss_metric_BoostNativeMetricImpl.h"

#include "common/bss_metric.h"

using namespace ock::bss;

JNIEXPORT jlong JNICALL Java_com_huawei_ock_bss_metric_BoostNativeMetricImpl_open(JNIEnv *, jclass, jint jSwitchMap)
{
    auto *boostNativeMetric = new (std::nothrow) BoostNativeMetric(static_cast<uint32_t>(jSwitchMap));
    if (UNLIKELY(boostNativeMetric == nullptr)) {
        LOG_ERROR("boostNativeMetric is nullptr.");
        return 0;
    }
    boostNativeMetric->Init();
    return static_cast<jlong>(reinterpret_cast<size_t>(boostNativeMetric));
}

JNIEXPORT jlong JNICALL Java_com_huawei_ock_bss_metric_BoostNativeMetricImpl_getMetric(JNIEnv *, jclass,
                                                                                       jlong jMetricHandle, jint jType)
{
    auto *boostNativeMetric = reinterpret_cast<BoostNativeMetric *>(jMetricHandle);
    if (UNLIKELY(boostNativeMetric == nullptr)) {
        LOG_ERROR("boostNativeMetric is nullptr.");
        return INT64_MAX;
    }
    int64_t metricValue = boostNativeMetric->GetMetrics(static_cast<MetricType>(jType));
    return static_cast<jlong>(metricValue);
}

JNIEXPORT void JNICALL Java_com_huawei_ock_bss_metric_BoostNativeMetricImpl_close(JNIEnv *, jclass, jlong jMetricHandle)
{
    if (UNLIKELY(jMetricHandle == 0)) {
        LOG_ERROR("BoostNativeMetric already closed.");
        return;
    }
    auto *boostNativeMetric = reinterpret_cast<BoostNativeMetric *>(jMetricHandle);
    if (UNLIKELY(boostNativeMetric == nullptr)) {
        LOG_ERROR("boostNativeMetric is nullptr.");
        return;
    }
    boostNativeMetric->Close();
    delete boostNativeMetric;
    boostNativeMetric = nullptr;
}