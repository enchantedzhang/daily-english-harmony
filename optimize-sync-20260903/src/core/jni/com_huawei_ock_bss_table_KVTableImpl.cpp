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

#include "com_huawei_ock_bss_table_KVTableImpl.h"

#include "include/boost_state_table.h"
#include "jni_common.h"
#include "kv_table/serialized_data.h"

using namespace ock::bss;

JNIEXPORT void JNICALL Java_com_huawei_ock_bss_table_KVTableImpl_put(JNIEnv *env, jclass, jlong jTableHandle,
                                                                     jint jKeyHash, jlong jKey, jint jKeyLen,
                                                                     jlong jVal, jint jValLen)
{
    if (UNLIKELY(jKey == 0)) {
        LOG_ERROR("Invalid key. jKey is null.");
        return;
    }

    if (UNLIKELY(jVal == 0)) {
        LOG_ERROR("Invalid Value. jVal is null.");
        return;
    }

    if (UNLIKELY((jKeyHash < 0) || (jKeyLen < 0) || (jValLen < 0))) {
        LOG_ERROR("error length. jKeyHash:" << jKeyHash << ", jKeyLen:" << jKeyLen << ", jValLen:" << jValLen);
        return;
    }

    auto *kVTable = reinterpret_cast<KVTable *>(jTableHandle);
    if (UNLIKELY(kVTable == nullptr)) {
        return;
    }

    uint32_t keyHashCode = static_cast<uint32_t>(jKeyHash);
    BinaryData priKey(reinterpret_cast<uint8_t *>(jKey), static_cast<uint32_t>(jKeyLen));
    BinaryData value(reinterpret_cast<uint8_t *>(jVal), static_cast<uint32_t>(jValLen));
    auto ret = kVTable->Put(keyHashCode, priKey, value);
    if (UNLIKELY(ret != BSS_OK)) {
        LOG_ERROR("KValue put failed, ret:" << ret << ", jKeyHash:" << jKeyHash);
    }
}

JNIEXPORT jlong JNICALL Java_com_huawei_ock_bss_table_KVTableImpl_get(JNIEnv *env, jclass, jlong jTableHandle,
                                                                      jint jKeyHash, jlong jKey, jint jKeyLen)
{
    if (UNLIKELY(jKey == 0)) {
        return 0;
    }

    if (UNLIKELY((jKeyHash < 0) || (jKeyLen < 0))) {
        LOG_ERROR("error length. jKeyHash:" << jKeyHash << ", jKeyLen:" << jKeyLen);
        return 0;
    }

    auto *kVTable = reinterpret_cast<KVTable *>(jTableHandle);
    if (UNLIKELY(kVTable == nullptr)) {
        return 0;
    }

    uint32_t keyHashCode = static_cast<uint32_t>(jKeyHash);
    BinaryData key(reinterpret_cast<uint8_t *>(jKey), static_cast<uint32_t>(jKeyLen));
    BinaryData value;
    auto result = kVTable->Get(keyHashCode, key, value);
    if (LIKELY(result == BSS_OK)) {
        return value.IsNull() ?
                   0 :
                   reinterpret_cast<jlong>(new SerializedDataWrapper(value.Data(), value.Length(), value.Buffer()));
    }
    return 0;
}

JNIEXPORT jboolean JNICALL Java_com_huawei_ock_bss_table_KVTableImpl_contains(JNIEnv *env, jclass, jlong jTableHandle,
                                                                              jint jKeyHash, jlong jKey, jint jKeyLen)
{
    if (UNLIKELY(jKey == 0)) {
        return JNI_FALSE;
    }

    if (UNLIKELY((jKeyHash < 0) || (jKeyLen < 0))) {
        LOG_ERROR("error length. jKeyHash:" << jKeyHash << ", jKeyLen:" << jKeyLen);
        return JNI_FALSE;
    }

    auto *kVTable = reinterpret_cast<KVTable *>(jTableHandle);
    if (UNLIKELY(kVTable == nullptr)) {
        return JNI_FALSE;
    }

    uint32_t keyHashCode = static_cast<uint32_t>(jKeyHash);
    BinaryData priKey(reinterpret_cast<uint8_t *>(jKey), static_cast<uint32_t>(jKeyLen));
    bool isContain = kVTable->Contain(keyHashCode, priKey);
    if (isContain) {
        return JNI_TRUE;
    }
    return JNI_FALSE;
}

JNIEXPORT void JNICALL Java_com_huawei_ock_bss_table_KVTableImpl_remove(JNIEnv *env, jclass, jlong jTableHandle,
                                                                        jint jKeyHash, jlong jKey, jint jKeyLen)
{
    if (UNLIKELY(jKey == 0)) {
        return;
    }

    if (UNLIKELY((jKeyHash < 0) || (jKeyLen < 0))) {
        LOG_ERROR("error length. jKeyHash:" << jKeyHash << ", jKeyLen:" << jKeyLen);
        return;
    }

    auto *kVTable = reinterpret_cast<KVTable *>(jTableHandle);
    if (UNLIKELY(kVTable == nullptr)) {
        return;
    }

    uint32_t keyHashCode = static_cast<uint32_t>(jKeyHash);
    BinaryData priKey(reinterpret_cast<uint8_t *>(jKey), static_cast<uint32_t>(jKeyLen));
    kVTable->Remove(keyHashCode, priKey);
}