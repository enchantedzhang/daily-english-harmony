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
#include "com_huawei_ock_bss_table_iterator_TableEntryIterator.h"

#include "include/boost_state_table.h"
#include "jni_common.h"
#include "kv_table/serialized_data.h"
#include "kv_table_iterator.h"
using namespace ock::bss;

JNIEXPORT jlong JNICALL Java_com_huawei_ock_bss_table_iterator_TableEntryIterator_open(JNIEnv *env, jclass,
                                                                                       jlong jTablePath)
{
    if (UNLIKELY(env == nullptr)) {
        LOG_ERROR("Input env is nullptr.");
        return 0;
    }
    auto *table = reinterpret_cast<AbstractTable *>(jTablePath);
    if (UNLIKELY(table == nullptr)) {
        return 0;
    }

    MapIterator *iterator = table->FullEntryIterator();
    if (UNLIKELY(iterator == nullptr)) {
        return 0;
    }

    return reinterpret_cast<jlong>(iterator);
}

JNIEXPORT jboolean JNICALL Java_com_huawei_ock_bss_table_iterator_TableEntryIterator_hasNext(JNIEnv *env, jclass,
                                                                                             jlong jTableEntryIterator)
{
    auto *tableEntryIterator = reinterpret_cast<MapIterator *>(jTableEntryIterator);
    if (UNLIKELY(tableEntryIterator == nullptr)) {
        return JNI_FALSE;
    }
    return tableEntryIterator->HasNext() ? JNI_TRUE : JNI_FALSE;
}
JNIEXPORT jobject JNICALL Java_com_huawei_ock_bss_table_iterator_TableEntryIterator_next(JNIEnv *env, jclass,
                                                                                         jlong jTableEntryIterator,
                                                                                         jobject object)
{
    if (UNLIKELY(env == nullptr)) {
        LOG_ERROR("Input env is nullptr.");
        return nullptr;
    }

    if (UNLIKELY(object == nullptr)) {
        LOG_ERROR("Input object is nullptr.");
        return nullptr;
    }

    auto *tableEntryIterator = reinterpret_cast<MapIterator *>(jTableEntryIterator);
    if (UNLIKELY(tableEntryIterator == nullptr)) {
        return nullptr;
    }

    KeyValueRef result = tableEntryIterator->Next();
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

JNIEXPORT jboolean JNICALL Java_com_huawei_ock_bss_table_iterator_TableEntryIterator_close(JNIEnv *env, jclass,
                                                                                           jlong jTableEntryIterator)
{
    auto *keyIterator = reinterpret_cast<MapIterator *>(jTableEntryIterator);
    if (UNLIKELY(keyIterator == nullptr)) {
        LOG_WARN("keyIterator is nullptr.");
        return JNI_TRUE;
    }
    keyIterator->Close();
    delete keyIterator;
    keyIterator = nullptr;
    return JNI_TRUE;
}