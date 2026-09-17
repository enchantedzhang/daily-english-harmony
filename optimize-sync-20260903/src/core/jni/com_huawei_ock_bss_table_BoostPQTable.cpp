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

#include "com_huawei_ock_bss_table_BoostPQTable.h"

#include "include/boost_state_db.h"
#include "include/config.h"
#include "jvm_instance.h"
#include "pq_table.h"
#include "scope_guard.h"

using namespace ock::bss;

JNIEXPORT jlong JNICALL Java_com_huawei_ock_bss_table_BoostPQTable_open(JNIEnv *env, jobject, jlong jDBHandle,
                                                                        jstring jStateName)
{
    if (UNLIKELY(env == nullptr)) {
        LOG_ERROR("Input env is nullptr.");
        return 0;
    }
    auto *boostStateDB = reinterpret_cast<BoostStateDB *>(jDBHandle);
    if (UNLIKELY(boostStateDB == nullptr)) {
        LOG_ERROR("boostStateDB is nullptr.");
        return 0;
    }
    const char *tableType = env->GetStringUTFChars(jStateName, nullptr);
    if (tableType == nullptr) {
        return 0;
    }
    SCOPE_EXIT({ env->ReleaseStringUTFChars(jStateName, tableType); });
    PQTableRef table = boostStateDB->CreatePQTable(tableType);
    if (UNLIKELY(table == nullptr)) {
        return 0;
    }
    auto address = static_cast<jlong>(reinterpret_cast<size_t>(table.get()));
    return address;
}

JNIEXPORT jboolean JNICALL Java_com_huawei_ock_bss_table_BoostPQTable_add(JNIEnv *env, jobject, jlong jpqTable,
                                                                          jbyteArray add, jint hashCode)
{
    if (UNLIKELY(env == nullptr)) {
        LOG_ERROR("Input env is nullptr.");
        return false;
    }
    auto table = reinterpret_cast<PQTable *>(jpqTable);
    if (UNLIKELY(table == nullptr)) {
        LOG_ERROR("PQTable is nullptr.");
        return false;
    }
    jbyte *byte = env->GetByteArrayElements(add, nullptr);
    RETURN_FALSE_AS_NULLPTR(byte);
    auto len = env->GetArrayLength(add);
    if (UNLIKELY(len < 0)) {
        LOG_ERROR("len is less than zero.");
        return false;
    }
    BinaryData binaryData(reinterpret_cast<uint8_t *>(byte), len);
    if (UNLIKELY(table->AddKey(binaryData, hashCode) != BSS_OK)) {
        env->ReleaseByteArrayElements(add, byte, JNI_ABORT);
        return false;
    }
    env->ReleaseByteArrayElements(add, byte, JNI_ABORT);
    return true;
}

JNIEXPORT jboolean JNICALL Java_com_huawei_ock_bss_table_BoostPQTable_remove(JNIEnv *env, jobject, jlong jpqTable,
                                                                             jbyteArray remove, jint hashCode)
{
    if (UNLIKELY(env == nullptr)) {
        LOG_ERROR("Input env is nullptr.");
        return false;
    }
    auto table = reinterpret_cast<PQTable *>(jpqTable);
    if (UNLIKELY(table == nullptr)) {
        LOG_ERROR("PQTable is nullptr.");
        return false;
    }
    jbyte *byte = env->GetByteArrayElements(remove, nullptr);
    RETURN_FALSE_AS_NULLPTR(byte);
    auto len = env->GetArrayLength(remove);
    if (UNLIKELY(len < 0)) {
        LOG_ERROR("len is less than zero.");
        return false;
    }
    BinaryData binaryData(reinterpret_cast<uint8_t *>(byte), len);
    if (UNLIKELY(table->RemoveKey(binaryData, hashCode) != BSS_OK)) {
        env->ReleaseByteArrayElements(remove, byte, JNI_ABORT);
        return false;
    }
    env->ReleaseByteArrayElements(remove, byte, JNI_ABORT);
    return true;
}