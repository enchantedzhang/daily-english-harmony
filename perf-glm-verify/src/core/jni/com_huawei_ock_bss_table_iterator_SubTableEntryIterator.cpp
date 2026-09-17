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
#include "com_huawei_ock_bss_table_iterator_SubTableEntryIterator.h"

#include "include/boost_state_table.h"
#include "jni_common.h"
#include "kv_table/kv_table_iterator.h"

using namespace ock::bss;

JNIEXPORT jlong JNICALL Java_com_huawei_ock_bss_table_iterator_SubTableEntryIterator_open(JNIEnv *env, jclass,
                                                                                          jlong jTablePath,
                                                                                          jint jKeyHash, jlong jKey,
                                                                                          jint jKeyLen)
{
    auto *abstractKMapTable = reinterpret_cast<AbstractKMapTable *>(jTablePath);
    if (UNLIKELY(abstractKMapTable == nullptr)) {
        return 0;
    }

    if (UNLIKELY(jKey == 0)) {
        return 0;
    }

    if (UNLIKELY((jKeyHash < 0) || (jKeyLen < 0))) {
        LOG_ERROR("error length. jKeyHash:" << jKeyHash << ", jKeyLen:" << jKeyLen);
        return 0;
    }

    uint32_t keyHashCode = static_cast<uint32_t>(jKeyHash);
    BinaryData key(reinterpret_cast<uint8_t *>(jKey), static_cast<uint32_t>(jKeyLen), nullptr);
    MapIterator *iterator = abstractKMapTable->EntryIterator(keyHashCode, key);
    if (UNLIKELY(iterator == nullptr)) {
        return 0;
    }

    return reinterpret_cast<jlong>(iterator);
}

JNIEXPORT jboolean JNICALL
Java_com_huawei_ock_bss_table_iterator_SubTableEntryIterator_hasNext(JNIEnv *env, jclass, jlong jTableEntryIterator)
{
    auto *tableEntryIterator = reinterpret_cast<MapIterator *>(jTableEntryIterator);
    if (UNLIKELY(tableEntryIterator == nullptr)) {
        return JNI_FALSE;
    }
    auto result = tableEntryIterator->HasNext() ? JNI_TRUE : JNI_FALSE;
    return result;
}

JNIEXPORT jobject JNICALL Java_com_huawei_ock_bss_table_iterator_SubTableEntryIterator_next(JNIEnv *env, jclass,
                                                                                            jlong jTableEntryIterator,
                                                                                            jobject object)
{
    if (UNLIKELY(env == nullptr || object == nullptr)) {
        LOG_ERROR("Input has nullptr.");
        return nullptr;
    }

    auto *tableEntryIterator = reinterpret_cast<MapIterator *>(jTableEntryIterator);
    if (UNLIKELY(tableEntryIterator == nullptr)) {
        return nullptr;
    }

    KeyValueRef result = tableEntryIterator->Next();
    if (UNLIKELY(result == nullptr)) {
        return nullptr;
    }

    env->SetLongField(object, gKeyAddrField, reinterpret_cast<jlong>(result->key.SecKey().KeyData()));
    env->SetIntField(object, gKeyLenField, result->key.SecKey().KeyLen());
    env->SetLongField(object, gValueAddrField, reinterpret_cast<jlong>(result->value.ValueData()));
    env->SetIntField(object, gValueLenField, result->value.ValueLen());
    return object;
}

JNIEXPORT jboolean JNICALL Java_com_huawei_ock_bss_table_iterator_SubTableEntryIterator_close(JNIEnv *env, jclass,
                                                                                              jlong jTableEntryIterator)
{
    auto *mapIterator = reinterpret_cast<MapIterator *>(jTableEntryIterator);
    if (UNLIKELY(mapIterator == nullptr)) {
        LOG_WARN("mapIterator is nullptr.");
        return JNI_TRUE;
    }
    mapIterator->Close();
    delete mapIterator;
    return JNI_TRUE;
}