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
#include "com_huawei_ock_bss_table_iterator_SubTableFullEntryIterator.h"

#include "include/boost_state_table.h"
#include "jni_common.h"
#include "kv_table/serialized_data.h"
#include "kv_table_iterator.h"
using namespace ock::bss;

jclass gEntryClass = nullptr;
jfieldID gKeyAddrField = nullptr;
jfieldID gKeyLenField = nullptr;
jfieldID gValueAddrField = nullptr;
jfieldID gValueLenField = nullptr;
jfieldID gSubKeyAddrField = nullptr;
jfieldID gSubKeyLenField = nullptr;

bool SubTableEntryInit(JNIEnv *env)
{
    if (UNLIKELY(env == nullptr)) {
        LOG_ERROR("Input env is nullptr.");
        return false;
    }
    // 初始化全局引用
    jclass stateListCountLocalClass = env->FindClass("com/huawei/ock/bss/table/result/EntryResult");
    if (stateListCountLocalClass == nullptr) {
        env->ExceptionClear();
        LOG_ERROR("Failed to FindClass for StateListResultClass");
        return false;  // 如果类未找到，返回空对象
    }
    gEntryClass = (jclass)env->NewGlobalRef(stateListCountLocalClass);
    env->DeleteLocalRef(stateListCountLocalClass);
    if (UNLIKELY(gEntryClass == nullptr)) {
        LOG_ERROR("Failed to create global reference for EntryResult");
        return false;
    }
    // 预加载字段ID
    gKeyAddrField = env->GetFieldID(gEntryClass, "keyAddr", "J");
    gKeyLenField = env->GetFieldID(gEntryClass, "keyLen", "I");
    gValueAddrField = env->GetFieldID(gEntryClass, "valueAddr", "J");
    gValueLenField = env->GetFieldID(gEntryClass, "valueLen", "I");
    gSubKeyAddrField = env->GetFieldID(gEntryClass, "subKeyAddr", "J");
    gSubKeyLenField = env->GetFieldID(gEntryClass, "subKeyLen", "I");
    if (UNLIKELY(gKeyLenField == nullptr || gKeyAddrField == nullptr || gValueAddrField == nullptr ||
                 gValueLenField == nullptr || gSubKeyAddrField == nullptr || gSubKeyLenField == nullptr)) {
        LOG_ERROR("Some field not exist!");
        env->DeleteGlobalRef(gEntryClass);
        return false;
    }
    return true;
}

void SubTableEntryExit(JNIEnv *env)
{
    if (env != nullptr) {
        env->DeleteGlobalRef(gEntryClass);
    }
    gKeyAddrField = nullptr;
    gKeyLenField = nullptr;
    gValueAddrField = nullptr;
    gValueLenField = nullptr;
    gSubKeyAddrField = nullptr;
    gSubKeyLenField = nullptr;
}

JNIEXPORT jlong JNICALL Java_com_huawei_ock_bss_table_iterator_SubTableFullEntryIterator_open(JNIEnv *env, jclass,
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
        LOG_ERROR("Create mapIterator failed.");
        return 0;
    }
    return reinterpret_cast<jlong>(iterator);
}

JNIEXPORT jboolean JNICALL Java_com_huawei_ock_bss_table_iterator_SubTableFullEntryIterator_hasNext(
    JNIEnv *env, jclass, jlong jSubTableFullEntryIterator)
{
    auto *SubTableFullEntryIterator = reinterpret_cast<MapIterator *>(jSubTableFullEntryIterator);
    if (UNLIKELY(SubTableFullEntryIterator == nullptr)) {
        LOG_ERROR("Cast to  mapIterator failed.");
        return JNI_FALSE;
    }
    return SubTableFullEntryIterator->HasNext() ? JNI_TRUE : JNI_FALSE;
}
JNIEXPORT jobject JNICALL Java_com_huawei_ock_bss_table_iterator_SubTableFullEntryIterator_next(
    JNIEnv *env, jclass, jlong jSubTableFullEntryIterator, jobject object)
{
    if (UNLIKELY(env == nullptr)) {
        LOG_ERROR("Input env is nullptr.");
        return nullptr;
    }
    auto *SubTableFullEntryIterator = reinterpret_cast<MapIterator *>(jSubTableFullEntryIterator);
    if (UNLIKELY(SubTableFullEntryIterator == nullptr)) {
        LOG_ERROR("Cast to  mapIterator failed.");
        return nullptr;
    }
    KeyValueRef result = SubTableFullEntryIterator->Next();
    if (UNLIKELY(result == nullptr)) {
        LOG_ERROR("Iterator get null data.");
        return nullptr;
    }
    env->SetLongField(object, gKeyAddrField, reinterpret_cast<jlong>(result->key.PriKey().KeyData()));
    env->SetIntField(object, gKeyLenField, result->key.PriKey().KeyLen());
    env->SetLongField(object, gValueAddrField, reinterpret_cast<jlong>(result->value.ValueData()));
    env->SetIntField(object, gValueLenField, result->value.ValueLen());
    env->SetLongField(object, gSubKeyAddrField, reinterpret_cast<jlong>(result->key.SecKey().KeyData()));
    env->SetIntField(object, gSubKeyLenField, result->key.SecKey().KeyLen());
    return object;
}

JNIEXPORT jboolean JNICALL Java_com_huawei_ock_bss_table_iterator_SubTableFullEntryIterator_close(
    JNIEnv *env, jclass, jlong jSubTableFullEntryIterator)
{
    auto *keyIterator = reinterpret_cast<MapIterator *>(jSubTableFullEntryIterator);
    if (UNLIKELY(keyIterator == nullptr)) {
        LOG_WARN("keyIterator is nullptr.");
        return JNI_TRUE;
    }
    keyIterator->Close();
    delete keyIterator;
    keyIterator = nullptr;
    return JNI_TRUE;
}
