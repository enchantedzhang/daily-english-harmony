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

#include "com_huawei_ock_bss_table_KMapTableImpl.h"

#include "bss_log.h"
#include "include/boost_state_table.h"

using namespace ock::bss;

JNIEXPORT jboolean JNICALL Java_com_huawei_ock_bss_table_KMapTableImpl_contains(JNIEnv *env, jclass, jlong jTableHandle,
                                                                                jint jKeyHash, jlong jKey, jint jKeyLen)
{
    if (UNLIKELY((jKeyHash < 0) || (jKeyLen < 0))) {
        LOG_ERROR("error length. jKeyHash:" << jKeyHash << ", jKeyLen:" << jKeyLen);
        return JNI_FALSE;
    }

    auto *abstractKMapTable = reinterpret_cast<AbstractKMapTable *>(jTableHandle);
    if (UNLIKELY(abstractKMapTable == nullptr)) {
        LOG_ERROR("Input param table handler is invalid.");
        return JNI_FALSE;
    }

    if (UNLIKELY(jKey == 0)) {
        LOG_ERROR("Input param is invalid, jKey is null.");
        return JNI_FALSE;
    }

    uint32_t keyHashCode = static_cast<uint32_t>(jKeyHash);
    BinaryData key(reinterpret_cast<uint8_t *>(jKey), static_cast<uint32_t>(jKeyLen));
    bool isContain = abstractKMapTable->Contain(keyHashCode, key);
    if (isContain) {
        return JNI_TRUE;
    }
    return JNI_FALSE;
}