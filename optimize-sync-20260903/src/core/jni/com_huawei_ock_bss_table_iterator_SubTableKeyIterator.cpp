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
#include "com_huawei_ock_bss_table_iterator_SubTableKeyIterator.h"

#include "include/boost_state_table.h"
#include "jni_common.h"
#include "kv_table/serialized_data.h"
#include "kv_table_iterator.h"
using namespace ock::bss;

JNIEXPORT jlong JNICALL Java_com_huawei_ock_bss_table_iterator_SubTableKeyIterator_open(JNIEnv *env, jclass,
                                                                                        jlong jTablePath, jlong jNs,
                                                                                        jint jNsLen)
{
    if (UNLIKELY(jNs == 0)) {
        LOG_ERROR("jNs cannot be zero.");
        return 0;
    }
    auto *abstractTable = reinterpret_cast<AbstractTable *>(jTablePath);
    if (UNLIKELY(abstractTable == nullptr)) {
        return 0;
    }

    if (UNLIKELY(jNsLen < 0)) {
        LOG_ERROR("jNsLen less than zero.");
        return 0;
    }

    BinaryData key(reinterpret_cast<uint8_t *>(jNs), static_cast<uint32_t>(jNsLen));
    KeyIterator *iterator = abstractTable->KeysIterator(key);
    if (UNLIKELY(iterator == nullptr)) {
        return 0;
    }
    return reinterpret_cast<jlong>(iterator);
}

JNIEXPORT jboolean JNICALL Java_com_huawei_ock_bss_table_iterator_SubTableKeyIterator_hasNext(JNIEnv *env, jclass,
                                                                                              jlong jTableEntryIterator)
{
    auto *keyIterator = reinterpret_cast<KeyIterator *>(jTableEntryIterator);
    if (UNLIKELY(keyIterator == nullptr)) {
        return JNI_FALSE;
    }
    return keyIterator->HasNext() ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jobject JNICALL Java_com_huawei_ock_bss_table_iterator_SubTableKeyIterator_next(JNIEnv *env, jclass,
                                                                                          jlong jTableEntryIterator,
                                                                                          jobject object)
{
    if (UNLIKELY(env == nullptr)) {
        LOG_ERROR("Input env is nullptr.");
        return nullptr;
    }

    auto *keyIterator = reinterpret_cast<KeyIterator *>(jTableEntryIterator);
    if (UNLIKELY(keyIterator == nullptr)) {
        return nullptr;
    }

    KeyValueRef result = keyIterator->Next();
    if (UNLIKELY(result == nullptr)) {
        return nullptr;
    }
    EntryIteratorType iterType = keyIterator->GetIteratorType();
    if (iterType == EntryIteratorType::KSUBMAP_ITERATOR || iterType == EntryIteratorType::KSUBLIST_ITERATOR) {
        env->SetLongField(object, gKeyAddrField, reinterpret_cast<jlong>(result->key.PriKey().RealKeyData()));
        env->SetIntField(object, gKeyLenField, result->key.PriKey().RealKeyLen());
    } else {
        env->SetLongField(object, gKeyAddrField, reinterpret_cast<jlong>(result->key.PriKey().KeyData()));
        env->SetIntField(object, gKeyLenField, result->key.PriKey().KeyLen());
    }
    return object;
}

JNIEXPORT jboolean JNICALL Java_com_huawei_ock_bss_table_iterator_SubTableKeyIterator_close(JNIEnv *env, jclass,
                                                                                            jlong jTableEntryIterator)
{
    auto *keyIterator = reinterpret_cast<KeyIterator *>(jTableEntryIterator);
    if (UNLIKELY(keyIterator == nullptr)) {
        LOG_WARN("keyIterator is nullptr.");
        return JNI_TRUE;
    }
    keyIterator->Close();
    delete keyIterator;
    return JNI_TRUE;
}