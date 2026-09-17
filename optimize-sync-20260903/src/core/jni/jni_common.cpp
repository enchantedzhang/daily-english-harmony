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

#include "jni_common.h"

#include "com_huawei_ock_bss_table_KListTableImpl.h"
#include "common/jvm_instance.h"

static bool g_initialized = false;

JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM *vm, void *reserved)
{
    if (UNLIKELY(vm == nullptr)) {
        LOG_ERROR("Input vm is nullptr.");
        return JNI_ERR;
    }
    if (g_initialized)
        return JNI_VERSION_1_6;
    JNIEnv *env;

    if (UNLIKELY(vm == nullptr)) {
        LOG_ERROR("Input vm is nullptr.");
        return JNI_ERR;
    }

    if (vm->GetEnv((void **)&env, JNI_VERSION_1_6) != JNI_OK) {
        return JNI_ERR;
    }

    if (!KListTableImplInit(env) || !SubTableEntryInit(env)) {
        return JNI_ERR;
    }

    return JNI_VERSION_1_6;
}

JNIEXPORT void JNICALL JNI_OnUnload(JavaVM *vm, void *reserved)
{
    if (UNLIKELY(vm == nullptr)) {
        LOG_ERROR("Input vm is nullptr.");
        return;
    }
    JNIEnv *env;
    if (vm->GetEnv((void **)&env, JNI_VERSION_1_6) == JNI_OK) {
        KListTableImplExit(env);
        SubTableEntryExit(env);
    }
    ock::bss::JVMInstance::Close();
}