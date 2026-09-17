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
#include "com_huawei_ock_bss_table_iterator_TableListEntryIterator.h"

#include "include/boost_state_table.h"
#include "jni_common.h"
#include "kv_table_iterator.h"
using namespace ock::bss;

JNIEXPORT jlong JNICALL Java_com_huawei_ock_bss_table_iterator_TableListEntryIterator_open(JNIEnv *env, jclass,
                                                                                           jlong jTablePath)
{
    if (UNLIKELY(env == nullptr)) {
        LOG_ERROR("Input env is nullptr.");
    }
    auto *table = reinterpret_cast<AbstractTable *>(jTablePath);
    if (UNLIKELY(table == nullptr)) {
        LOG_ERROR("Cast to AbstractTable failed.");
        return 0;
    }

    MapIterator *iterator = table->FullEntryIterator();
    if (UNLIKELY(iterator == nullptr)) {
        LOG_ERROR("Create MapIterator failed.");
        return 0;
    }

    return reinterpret_cast<jlong>(iterator);
}

JNIEXPORT jboolean JNICALL Java_com_huawei_ock_bss_table_iterator_TableListEntryIterator_hasNext(
    JNIEnv *env, jclass, jlong jTableListEntryIterator)
{
    if (UNLIKELY(env == nullptr)) {
        LOG_ERROR("Input env is nullptr.");
    }
    auto *tableListEntryIterator = reinterpret_cast<MapIterator *>(jTableListEntryIterator);
    if (UNLIKELY(tableListEntryIterator == nullptr)) {
        LOG_ERROR("Cast to MapIterator failed.");
        return JNI_FALSE;
    }
    return tableListEntryIterator->HasNext() ? JNI_TRUE : JNI_FALSE;
}
JNIEXPORT jobject JNICALL Java_com_huawei_ock_bss_table_iterator_TableListEntryIterator_next(
    JNIEnv *env, jclass, jlong jTableListEntryIterator, jobject object)
{
    if (UNLIKELY(env == nullptr)) {
        LOG_ERROR("Input env is nullptr.");
        return nullptr;
    }

    if (UNLIKELY(object == nullptr)) {
        LOG_ERROR("Input object is nullptr.");
        return nullptr;
    }

    auto *tableListEntryIterator = reinterpret_cast<MapIterator *>(jTableListEntryIterator);
    if (UNLIKELY(tableListEntryIterator == nullptr)) {
        LOG_ERROR("Cast to MapIterator failed.");
        return nullptr;
    }

    KeyValueRef result = tableListEntryIterator->Next();
    if (UNLIKELY(result == nullptr)) {
        LOG_ERROR("Iterator get null data.");
        return nullptr;
    }
    env->SetLongField(object, gKeyAddrField, reinterpret_cast<jlong>(result->key.PriKey().KeyData()));
    env->SetIntField(object, gKeyLenField, result->key.PriKey().KeyLen());
    env->SetLongField(object, gValueAddrField, reinterpret_cast<jlong>(result->value.ValueData()));
    env->SetIntField(object, gValueLenField, result->value.ValueLen());
    return object;
}

JNIEXPORT jboolean JNICALL
Java_com_huawei_ock_bss_table_iterator_TableListEntryIterator_close(JNIEnv *env, jclass, jlong jTableListEntryIterator)
{
    auto *keyIterator = reinterpret_cast<MapIterator *>(jTableListEntryIterator);
    if (UNLIKELY(keyIterator == nullptr)) {
        LOG_WARN("keyIterator is nullptr.");
        return JNI_TRUE;
    }
    keyIterator->Close();
    delete keyIterator;
    keyIterator = nullptr;
    return JNI_TRUE;
}