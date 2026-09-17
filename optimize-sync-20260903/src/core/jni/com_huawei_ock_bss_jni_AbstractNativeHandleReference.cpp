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
#include "com_huawei_ock_bss_jni_AbstractNativeHandleReference.h"

#include "common/bss_log.h"
#include "include/boost_state_db.h"
using namespace ock::bss;

JNIEXPORT jboolean JNICALL Java_com_huawei_ock_bss_jni_AbstractNativeHandleReference_close(JNIEnv *env, jclass,
                                                                                           jlong jCloseable)
{
    auto *autoCloseable = reinterpret_cast<AutoCloseable *>(jCloseable);
    if (UNLIKELY(autoCloseable == nullptr)) {
        LOG_WARN("AutoCloseable is nullptr.");
        return JNI_TRUE;
    }
    autoCloseable->Close();
    delete autoCloseable;
    return JNI_TRUE;
}