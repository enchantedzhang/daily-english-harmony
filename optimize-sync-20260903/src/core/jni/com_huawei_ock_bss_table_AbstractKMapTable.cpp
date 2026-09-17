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
#include "com_huawei_ock_bss_table_AbstractKMapTable.h"

#include "include/boost_state_table.h"
#include "jni_common.h"
#include "kv_table/serialized_data.h"

using namespace ock::bss;

JNIEXPORT jboolean JNICALL Java_com_huawei_ock_bss_table_AbstractKMapTable_contains(JNIEnv *env, jclass,
                                                                                    jlong jTableHandle, jint jKeyHash,
                                                                                    jlong jKey, jint jKeyLen,
                                                                                    jlong jMapKey, jint jMapKeyLen)
{
    if (UNLIKELY(jKey == 0)) {
        LOG_ERROR("Input param is invalid, jKey is null");
        return JNI_FALSE;
    }

    if (UNLIKELY(jMapKey == 0)) {
        LOG_ERROR("Input param is invalid, jMapKey is null");
        return JNI_FALSE;
    }

    auto *abstractKMapTable = reinterpret_cast<AbstractKMapTable *>(jTableHandle);
    if (UNLIKELY(abstractKMapTable == nullptr)) {
        LOG_ERROR("Input param table handler is invalid, tableHandle is null");
        return JNI_FALSE;
    }

    if (UNLIKELY((jKeyHash < 0) || (jMapKeyLen < 0) || (jKeyLen < 0))) {
        LOG_ERROR("error length. jKeyHash:" << jKeyHash << ", jMapKeyLen:" << jMapKeyLen << ", jKeyLen:" << jKeyLen);
        return JNI_FALSE;
    }

    uint32_t keyHashCode = static_cast<uint32_t>(jKeyHash);
    BinaryData priKey(reinterpret_cast<uint8_t *>(jKey), static_cast<uint32_t>(jKeyLen));
    BinaryData secKey(reinterpret_cast<uint8_t *>(jMapKey), static_cast<uint32_t>(jMapKeyLen));

    bool isContain = abstractKMapTable->Contain(keyHashCode, priKey, secKey);
    if (isContain) {
        return JNI_TRUE;
    }
    return JNI_FALSE;
}

JNIEXPORT jlong JNICALL Java_com_huawei_ock_bss_table_AbstractKMapTable_get(JNIEnv *env, jclass, jlong jTableHandle,
                                                                            jint jKeyHash, jlong jKey, jint jKeyLen,
                                                                            jlong jMapKey, jint jMapKeyLen)
{
    if (UNLIKELY(jKey == 0)) {
        LOG_ERROR("invalid key. jKey is null.");
        return 0;
    }

    if (UNLIKELY(jMapKey == 0)) {
        LOG_ERROR("invalid key. jMapKey is null.");
        return 0;
    }

    auto *mapTable = reinterpret_cast<AbstractKMapTable *>(jTableHandle);
    if (UNLIKELY(mapTable == nullptr)) {
        LOG_ERROR("Input param table handler is invalid, tableHandle is null");
        return 0;
    }

    if (UNLIKELY((jKeyHash < 0) || (jMapKeyLen < 0) || (jKeyLen < 0))) {
        LOG_ERROR("error length. jKeyHash:" << jKeyHash << ", jMapKeyLen:" << jMapKeyLen << ", jKeyLen:" << jKeyLen);
        return 0;
    }

    uint32_t keyHashCode = static_cast<uint32_t>(jKeyHash);
    BinaryData priKey(reinterpret_cast<uint8_t *>(jKey), static_cast<uint32_t>(jKeyLen));
    BinaryData secKey(reinterpret_cast<uint8_t *>(jMapKey), static_cast<uint32_t>(jMapKeyLen));
    BinaryData value = {};
    BResult result = mapTable->Get(keyHashCode, priKey, secKey, value);
    if (result == BSS_OK) {
        return value.IsNull() ?
                   0 :
                   reinterpret_cast<jlong>(new SerializedDataWrapper(value.Data(), value.Length(), value.Buffer()));
    }
    return 0;
}

JNIEXPORT void JNICALL Java_com_huawei_ock_bss_table_AbstractKMapTable_add(JNIEnv *env, jclass, jlong jTableHandle,
                                                                           jint jKeyHash, jlong jKey, jint jKeyLen,
                                                                           jlong jMapKey, jint jMapKeyLen, jlong jVal,
                                                                           jint jValLen)
{
    if (UNLIKELY(jKey == 0) || (jMapKey == 0) || (jVal == 0)) {
        LOG_ERROR("invalid key or value. Key or value is null.");
        return;
    }
    if (UNLIKELY((jKeyHash < 0) || (jValLen < 0) || (jMapKeyLen < 0) || (jKeyLen < 0))) {
        LOG_ERROR("error length. jKeyHash:" << jKeyHash << ", jValLen:" << jValLen << ", jMapKeyLen:" << jMapKeyLen
                                            << ", jKeyLen:" << jKeyLen);
        return;
    }

    auto *abstractKMapTable = reinterpret_cast<AbstractKMapTable *>(jTableHandle);
    if (UNLIKELY(abstractKMapTable == nullptr)) {
        LOG_ERROR("Input param table handler is invalid, tableHandle:" << jTableHandle);
        return;
    }

    uint32_t keyHashCode = static_cast<uint32_t>(jKeyHash);
    BinaryData priKey(reinterpret_cast<uint8_t *>(jKey), static_cast<uint32_t>(jKeyLen));
    BinaryData secKey(reinterpret_cast<uint8_t *>(jMapKey), static_cast<uint32_t>(jMapKeyLen));
    BinaryData value(reinterpret_cast<uint8_t *>(jVal), static_cast<uint32_t>(jValLen));

    auto ret = abstractKMapTable->Put(keyHashCode, priKey, secKey, value);
    if (UNLIKELY(ret != BSS_OK)) {
        LOG_WARN("KMap add failed, ret:" << ret << ", jKeyHash:" << jKeyHash);
    }
}

JNIEXPORT void JNICALL Java_com_huawei_ock_bss_table_AbstractKMapTable_remove(JNIEnv *env, jclass, jlong jTableHandle,
                                                                              jint jKeyHash, jlong jKey, jint jKeyLen,
                                                                              jlong jMapKey, jint jMapKeyLen)
{
    if (UNLIKELY(jKey == 0)) {
        LOG_ERROR("Invalid key. jKey is null.");
        return;
    }

    if (UNLIKELY(jMapKey == 0)) {
        LOG_ERROR("Invalid key. jMapKey is null.");
        return;
    }

    if (UNLIKELY((jKeyHash < 0) || (jMapKeyLen < 0) || (jKeyLen < 0))) {
        LOG_ERROR("error length. jKeyHash:" << jKeyHash << ", jMapKeyLen:" << jMapKeyLen << ", jKeyLen:" << jKeyLen);
        return;
    }

    auto *abstractKMapTable = reinterpret_cast<AbstractKMapTable *>(jTableHandle);
    if (UNLIKELY(abstractKMapTable == nullptr)) {
        LOG_ERROR("Input param table handler is invalid, tableHandle is null.");
        return;
    }

    uint32_t keyHashCode = static_cast<uint32_t>(jKeyHash);
    BinaryData priKey(reinterpret_cast<uint8_t *>(jKey), static_cast<uint32_t>(jKeyLen));
    BinaryData secKey(reinterpret_cast<uint8_t *>(jMapKey), static_cast<uint32_t>(jMapKeyLen));
    auto ret = abstractKMapTable->Remove(keyHashCode, priKey, secKey);
    if (UNLIKELY(ret != BSS_OK)) {
        LOG_ERROR("KMap remove failed, ret:" << ret << ", jKeyHash:" << jKeyHash);
    }
}

JNIEXPORT void JNICALL Java_com_huawei_ock_bss_table_AbstractKMapTable_removeAll(JNIEnv *env, jclass,
                                                                                 jlong jTableHandle, jint jKeyHash,
                                                                                 jlong jKey, jint jKeyLen)
{
    if (UNLIKELY(jKey == 0)) {
        LOG_ERROR("Input param is invalid, jKey is null.");
        return;
    }

    if (UNLIKELY((jKeyHash < 0) || (jKeyLen < 0))) {
        LOG_ERROR("error length. jKeyHash:" << jKeyHash << ", jKeyLen:" << jKeyLen);
        return;
    }

    auto *abstractKMapTable = reinterpret_cast<AbstractKMapTable *>(jTableHandle);
    if (UNLIKELY(abstractKMapTable == nullptr)) {
        LOG_ERROR("Input param table handler is invalid, tableHandle is null.");
        return;
    }

    uint32_t keyHashCode = static_cast<uint32_t>(jKeyHash);
    BinaryData priKey(reinterpret_cast<uint8_t *>(jKey), static_cast<uint32_t>(jKeyLen));
    abstractKMapTable->Remove(keyHashCode, priKey);
}