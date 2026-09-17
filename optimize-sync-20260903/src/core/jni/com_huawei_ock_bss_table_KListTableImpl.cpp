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

#include "com_huawei_ock_bss_table_KListTableImpl.h"

#include "include/boost_state_table.h"
#include "jni_common.h"
#include "kv_helper.h"

using namespace ock::bss;

jclass gStateListCountClass = nullptr;
jfieldID gResIdField = nullptr;
jfieldID gReadSectionIdField = nullptr;
jfieldID gSizeField = nullptr;
jfieldID gAddressesField = nullptr;
jfieldID gLengthsField = nullptr;

bool KListTableImplInit(JNIEnv *env)
{
    if (UNLIKELY(env == nullptr)) {
        LOG_ERROR("env is nullptr.");
        return false;
    }
    // 初始化全局引用
    jclass stateListCountLocalClass = env->FindClass("com/huawei/ock/bss/table/result/StateListResult");
    if (stateListCountLocalClass == nullptr) {
        env->ExceptionClear();
        LOG_ERROR("Failed to FindClass for StateListResultClass");
        return false;  // 如果类未找到，返回空对象
    }
    gStateListCountClass = (jclass)env->NewGlobalRef(stateListCountLocalClass);
    env->DeleteLocalRef(stateListCountLocalClass);
    if (UNLIKELY(gStateListCountClass == nullptr)) {
        LOG_ERROR("Failed to create global reference for StateListResult");
        return false;
    }

    // 预加载字段ID
    gResIdField = env->GetFieldID(gStateListCountClass, "resId", "I");
    gReadSectionIdField = env->GetFieldID(gStateListCountClass, "readSectionId", "I");
    gSizeField = env->GetFieldID(gStateListCountClass, "size", "I");
    gAddressesField = env->GetFieldID(gStateListCountClass, "addresses", "[J");
    gLengthsField = env->GetFieldID(gStateListCountClass, "lengths", "[I");
    if (UNLIKELY(gResIdField == nullptr || gReadSectionIdField == nullptr || gSizeField == nullptr ||
                 gAddressesField == nullptr || gLengthsField == nullptr)) {
        LOG_ERROR("Some field not exist!");
        env->DeleteGlobalRef(gStateListCountClass);
        return false;
    }
    return true;
}

void KListTableImplExit(JNIEnv *env)
{
    if (UNLIKELY(env == nullptr)) {
        LOG_ERROR("Input env is nullptr.");
        return;
    }
    env->DeleteGlobalRef(gStateListCountClass);
    gResIdField = nullptr;
    gReadSectionIdField = nullptr;
    gSizeField = nullptr;
    gAddressesField = nullptr;
    gLengthsField = nullptr;
}

JNIEXPORT void JNICALL Java_com_huawei_ock_bss_table_KListTableImpl_put(JNIEnv *env, jclass, jlong jTableHandle,
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

    auto *abstractKListTable = reinterpret_cast<AbstractKListTable *>(jTableHandle);
    if (UNLIKELY(abstractKListTable == nullptr)) {
        LOG_ERROR("Input param table handler is invalid, tableHandle is null.");
        return;
    }

    BinaryData key(reinterpret_cast<uint8_t *>(jKey), static_cast<uint32_t>(jKeyLen));
    BinaryData value(reinterpret_cast<uint8_t *>(jVal), static_cast<uint32_t>(jValLen));

    BResult ret = abstractKListTable->Put(static_cast<uint32_t>(jKeyHash), key, value);
    if (UNLIKELY(ret != BSS_OK)) {
        LOG_ERROR("Put KList state failed, ret:" << ret << ", keyHash:" << jKeyHash << ", keyLen:" << jKeyLen
                                                 << ", valueLen:" << jValLen);
    }
}

JNIEXPORT void JNICALL Java_com_huawei_ock_bss_table_KListTableImpl_get(JNIEnv *env, jclass, jlong jTableHandle,
                                                                        jint jKeyHash, jlong jKey, jint jKeyLen,
                                                                        jobject object)
{
    if (UNLIKELY(env == nullptr)) {
        LOG_ERROR("Input env is nullptr.");
        return;
    }

    if (UNLIKELY(jKey == 0)) {
        LOG_ERROR("Input param key is invalid, jKey is null.");
        return;
    }

    if (UNLIKELY((jKeyHash < 0) || (jKeyLen < 0))) {
        LOG_ERROR("error length. jKeyHash:" << jKeyHash << ", jKeyLen:" << jKeyLen);
        return;
    }

    if (UNLIKELY(object == nullptr)) {
        LOG_ERROR("object is nullptr.");
        return;
    }

    auto *abstractKListTable = reinterpret_cast<AbstractKListTable *>(jTableHandle);
    if (UNLIKELY(abstractKListTable == nullptr)) {
        LOG_ERROR("Input param table handler is invalid, tableHandle is null.");
        return;
    }

    BinaryData key(reinterpret_cast<uint8_t *>(jKey), static_cast<uint32_t>(jKeyLen));

    ListResult listCount = abstractKListTable->Get(static_cast<uint32_t>(jKeyHash), key);
    if (listCount.size == 0 || listCount.addresses.empty() || listCount.lengths.empty()) {
        return;
    }
    if (UNLIKELY(listCount.size > static_cast<uint32_t>(INT32_MAX) || listCount.addresses.size() < listCount.size ||
                 listCount.lengths.size() < listCount.size)) {
        LOG_ERROR("Invalid list result size: " << listCount.size);
        return;
    }
    env->SetIntField(object, gResIdField, listCount.resId);
    env->SetIntField(object, gSizeField, static_cast<jsize>(listCount.size));
    jlongArray newDatas = env->NewLongArray(static_cast<jsize>(listCount.size));
    if (UNLIKELY(newDatas == nullptr)) {
        LOG_ERROR("new long array failed！");
        return;
    }
    env->SetLongArrayRegion(newDatas, 0, static_cast<jsize>(listCount.size), listCount.addresses.data());
    env->SetObjectField(object, gAddressesField, newDatas);
    jintArray newLengths = env->NewIntArray(static_cast<jsize>(listCount.size));
    if (UNLIKELY(newLengths == nullptr)) {
        env->DeleteLocalRef(newDatas);
        LOG_ERROR("new int array failed！");
        return;
    }
    env->SetIntArrayRegion(newLengths, 0, static_cast<jsize>(listCount.size), listCount.lengths.data());
    env->SetObjectField(object, gLengthsField, newLengths);
    env->SetIntField(object, gReadSectionIdField, listCount.sectionReadId);
    env->DeleteLocalRef(newDatas);
    env->DeleteLocalRef(newLengths);
}

JNIEXPORT jboolean JNICALL Java_com_huawei_ock_bss_table_KListTableImpl_contains(JNIEnv *env, jclass,
                                                                                 jlong jTableHandle, jint jKeyHash,
                                                                                 jlong jKey, jint jKeyLen)
{
    if (UNLIKELY(jKey == 0)) {
        LOG_ERROR("Input param key is invalid, jKey is null");
        return JNI_FALSE;
    }

    if (UNLIKELY((jKeyHash < 0) || (jKeyLen < 0))) {
        LOG_ERROR("error length. jKeyHash:" << jKeyHash << ", jKeyLen:" << jKeyLen);
        return JNI_FALSE;
    }

    auto *abstractKListTable = reinterpret_cast<AbstractKListTable *>(jTableHandle);
    if (UNLIKELY(abstractKListTable == nullptr)) {
        LOG_ERROR("Input param table handler is invalid, tableHandle is null");
        return JNI_FALSE;
    }

    BinaryData key(reinterpret_cast<uint8_t *>(jKey), static_cast<uint32_t>(jKeyLen));
    bool isContain = abstractKListTable->Contain(static_cast<uint32_t>(jKeyHash), key);
    if (isContain) {
        return JNI_TRUE;
    }
    return JNI_FALSE;
}

JNIEXPORT void JNICALL Java_com_huawei_ock_bss_table_KListTableImpl_remove(JNIEnv *env, jclass, jlong jTableHandle,
                                                                           jint jKeyHash, jlong jKey, jint jKeyLen)
{
    if (UNLIKELY(jKey == 0)) {
        return;
    }

    if (UNLIKELY((jKeyHash < 0) || (jKeyLen < 0))) {
        LOG_ERROR("error length. jKeyHash:" << jKeyHash << ", jKeyLen:" << jKeyLen);
        return;
    }

    auto *abstractKListTable = reinterpret_cast<AbstractKListTable *>(jTableHandle);
    if (UNLIKELY(abstractKListTable == nullptr)) {
        return;
    }

    BinaryData key(reinterpret_cast<uint8_t *>(jKey), static_cast<uint32_t>(jKeyLen));
    abstractKListTable->Remove(static_cast<uint32_t>(jKeyHash), key);
}

JNIEXPORT void JNICALL Java_com_huawei_ock_bss_table_KListTableImpl_add(JNIEnv *env, jclass, jlong jTableHandle,
                                                                        jint jKeyHash, jlong jKey, jint jKeyLen,
                                                                        jlong jVal, jint jValLen)
{
    if (UNLIKELY(jKey == 0 || jVal == 0)) {
        return;
    }

    if (UNLIKELY((jKeyHash < 0) || (jKeyLen < 0) || (jValLen < 0))) {
        LOG_ERROR("error length. jKeyHash:" << jKeyHash << ", jKeyLen:" << jKeyLen << ", jValLen:" << jValLen);
        return;
    }

    auto *abstractKListTable = reinterpret_cast<AbstractKListTable *>(jTableHandle);
    if (UNLIKELY(abstractKListTable == nullptr)) {
        return;
    }

    BinaryData key(reinterpret_cast<uint8_t *>(jKey), static_cast<uint32_t>(jKeyLen));
    BinaryData value(reinterpret_cast<uint8_t *>(jVal), static_cast<uint32_t>(jValLen));

    auto ret = abstractKListTable->Add(static_cast<uint32_t>(jKeyHash), key, value);
    if (UNLIKELY(ret != BSS_OK)) {
        LOG_ERROR("KList add failed, ret:" << ret << ", jKeyHash:" << jKeyHash);
    }
}

JNIEXPORT void JNICALL Java_com_huawei_ock_bss_table_KListTableImpl_addAll(JNIEnv *env, jclass, jlong jTableHandle,
                                                                           jint jKeyHash, jlong jKey, jint jKeyLen,
                                                                           jlong jVal, jint jValLen)
{
    if (UNLIKELY(jKey == 0)) {
        return;
    }

    if (UNLIKELY(jVal == 0)) {
        return;
    }

    if (UNLIKELY((jKeyHash < 0) || (jKeyLen < 0) || (jValLen < 0))) {
        LOG_ERROR("error length. jKeyHash:" << jKeyHash << ", jKeyLen:" << jKeyLen << ", jValLen:" << jValLen);
        return;
    }

    auto *abstractKListTable = reinterpret_cast<AbstractKListTable *>(jTableHandle);
    if (UNLIKELY(abstractKListTable == nullptr)) {
        LOG_ERROR("Failed to get abstractKListTable.");
        return;
    }

    BinaryData key(reinterpret_cast<uint8_t *>(jKey), static_cast<uint32_t>(jKeyLen));
    BinaryData value(reinterpret_cast<uint8_t *>(jVal), static_cast<uint32_t>(jValLen));
    abstractKListTable->Add(static_cast<uint32_t>(jKeyHash), key, value);
}

JNIEXPORT void JNICALL Java_com_huawei_ock_bss_table_KListTableImpl_releaseAllBuffer(JNIEnv *env, jclass,
                                                                                     jlong jTableHandle, jint resId)
{
    auto *abstractKListTable = reinterpret_cast<AbstractKListTable *>(jTableHandle);
    if (UNLIKELY(abstractKListTable == nullptr)) {
        LOG_ERROR("Input param table handler is invalid, tableHandle is null.");
        return;
    }
    if (UNLIKELY(resId < 0)) {
        LOG_ERROR("resId less than zero.");
        return;
    }
    abstractKListTable->CleanResource(resId);
}

JNIEXPORT jobject JNICALL Java_com_huawei_ock_bss_table_KListTableImpl_doNativeSectionRead(JNIEnv *env, jclass,
                                                                                           jlong jTableHandle,
                                                                                           jobject obj)
{
    auto *abstractKListTable = reinterpret_cast<AbstractKListTable *>(jTableHandle);
    if (UNLIKELY(abstractKListTable == nullptr)) {
        LOG_ERROR("Input param table handler is invalid, tableHandle is null.");
        return obj;
    }
    int32_t resId = env->GetIntField(obj, gResIdField);
    if (resId > 0) {
        abstractKListTable->CleanResource(resId);
    }

    int32_t gReadSectionId = env->GetIntField(obj, gReadSectionIdField);
    if (UNLIKELY(gReadSectionId == 0)) {
        LOG_ERROR("gReadSectionId is 0.");
        return obj;
    }

    ListResult listCount = abstractKListTable->SectionRead(gReadSectionId);
    if (UNLIKELY(listCount.size > static_cast<uint32_t>(INT32_MAX) || listCount.addresses.size() < listCount.size ||
                 listCount.lengths.size() < listCount.size)) {
        LOG_ERROR("Invalid section read result size: " << listCount.size);
        return obj;
    }
    env->SetIntField(obj, gResIdField, listCount.resId);
    env->SetIntField(obj, gSizeField, static_cast<jsize>(listCount.size));
    jlongArray newDatas = env->NewLongArray(static_cast<jsize>(listCount.size));
    if (UNLIKELY(newDatas == nullptr)) {
        LOG_ERROR("new long array failed！");
        return obj;
    }
    env->SetLongArrayRegion(newDatas, 0, static_cast<jsize>(listCount.size), listCount.addresses.data());
    env->SetObjectField(obj, gAddressesField, newDatas);
    jintArray newLengths = env->NewIntArray(static_cast<jsize>(listCount.size));
    if (UNLIKELY(newLengths == nullptr)) {
        env->DeleteLocalRef(newDatas);
        LOG_ERROR("new int array failed！");
        return obj;
    }
    env->SetIntArrayRegion(newLengths, 0, static_cast<jsize>(listCount.size), listCount.lengths.data());
    env->SetObjectField(obj, gLengthsField, newLengths);
    env->SetIntField(obj, gReadSectionIdField, listCount.sectionReadId);
    env->DeleteLocalRef(newDatas);
    env->DeleteLocalRef(newLengths);
    return obj;
}
