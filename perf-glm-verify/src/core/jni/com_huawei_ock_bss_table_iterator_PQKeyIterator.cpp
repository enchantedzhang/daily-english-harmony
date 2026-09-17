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

#include "com_huawei_ock_bss_table_iterator_PQKeyIterator.h"

#include "jni_common.h"
#include "kv_table/pq_table.h"

using namespace ock::bss;

JNIEXPORT jlong JNICALL Java_com_huawei_ock_bss_table_iterator_PQKeyIterator_open(JNIEnv *env, jobject, jlong jtable,
                                                                                  jbyteArray groupId)
{
    if (UNLIKELY(env == nullptr)) {
        LOG_ERROR("Input env is nullptr.");
        return 0;
    }
    auto table = reinterpret_cast<PQTable *>(jtable);
    if (UNLIKELY(table == nullptr)) {
        LOG_ERROR("PQTable is nullptr.");
        return 0;
    }
    jbyte *byte = env->GetByteArrayElements(groupId, nullptr);
    RETURN_FALSE_AS_NULLPTR(byte);
    uint32_t len = static_cast<uint32_t>(env->GetArrayLength(groupId));
    uint8_t *cpdata = static_cast<uint8_t *>(malloc(len));
    RETURN_FALSE_AS_NULLPTR(cpdata);
    if (UNLIKELY(memcpy_s(cpdata, len, byte, len) != EOK)) {
        free(cpdata);
        LOG_ERROR("memcpy groupId failed.");
        return 0;
    }
    env->ReleaseByteArrayElements(groupId, byte, JNI_ABORT);
    BinaryData binaryData(cpdata, len);
    auto iter = table->KeyIterator(binaryData);
    if (UNLIKELY(iter == nullptr)) {
        free(cpdata);
        LOG_ERROR("iter is nullptr.");
        return 0;
    }
    return reinterpret_cast<jlong>(iter);
}

JNIEXPORT jboolean JNICALL Java_com_huawei_ock_bss_table_iterator_PQKeyIterator_hasNext(JNIEnv *env, jobject,
                                                                                        jlong jIter)
{
    if (UNLIKELY(env == nullptr)) {
        LOG_ERROR("Input env is nullptr.");
        return false;
    }
    auto iter = reinterpret_cast<PQKeyIterator *>(jIter);
    if (UNLIKELY(iter == nullptr)) {
        LOG_ERROR("PQKeyIterator is nullptr.");
        return false;
    }
    return iter->HasNext();
}

JNIEXPORT jobject JNICALL Java_com_huawei_ock_bss_table_iterator_PQKeyIterator_next(JNIEnv *env, jobject, jlong jIter,
                                                                                    jobject obj)
{
    if (UNLIKELY(env == nullptr)) {
        LOG_ERROR("Input env is nullptr.");
        return nullptr;
    }
    auto iter = reinterpret_cast<PQKeyIterator *>(jIter);
    if (UNLIKELY(iter == nullptr)) {
        LOG_ERROR("PQKeyIterator is nullptr.");
        return nullptr;
    }
    BinaryData binaryData = iter->Next();
    env->SetLongField(obj, gKeyAddrField, reinterpret_cast<jlong>(binaryData.Data()));
    env->SetIntField(obj, gKeyLenField, binaryData.Length());
    return obj;
}

JNIEXPORT jboolean JNICALL Java_com_huawei_ock_bss_table_iterator_PQKeyIterator_close(JNIEnv *env, jobject, jlong jIter)
{
    if (UNLIKELY(env == nullptr)) {
        LOG_ERROR("Input env is nullptr.");
        return false;
    }
    auto iter = reinterpret_cast<PQKeyIterator *>(jIter);
    if (UNLIKELY(iter == nullptr)) {
        LOG_ERROR("PQKeyIterator is nullptr.");
        return false;
    }
    iter->Close();
    delete iter;
    return true;
}