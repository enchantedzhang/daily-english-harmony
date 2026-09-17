/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#include "com_huawei_ock_bss_common_BoostStateDB.h"

#include "bss_metric.h"
#include "include/boost_state_db.h"
#include "include/bss_err.h"
#include "include/config.h"
#include "jvm_instance.h"
#include "kv_helper.h"

using namespace ock::bss;

JNIEXPORT jlong JNICALL Java_com_huawei_ock_bss_common_BoostStateDB_open(JNIEnv *env, jclass, jobject jBoostConfig)
{
    if (UNLIKELY(env == nullptr)) {
        LOG_ERROR("Input env is nullptr.");
        return 0;
    }

    if (UNLIKELY(jBoostConfig == nullptr)) {
        LOG_ERROR("BoostConfig is nullptr.");
        return 0;
    }

    BoostStateDB *boostStateDB = BoostStateDBFactory::Create();
    if (UNLIKELY(boostStateDB == nullptr)) {
        LOG_ERROR("New boostStateDB instance failed.");
        return 0;
    }

    ConfigRef config = CreateConfig(env, jBoostConfig);
    if (UNLIKELY(config == nullptr)) {
        LOG_ERROR("Create config instance failed.");
        delete boostStateDB;
        return 0;
    }

    BResult result = boostStateDB->Open(config);
    if (UNLIKELY(result != BSS_OK)) {
        LOG_ERROR("Startup boostStateDB failed, ret:" << result);
        delete boostStateDB;
        return 0;
    }
    result = JVMInstance::Init(env);
    if (UNLIKELY(result != BSS_OK)) {
        LOG_ERROR("Get JVMInstance for lazy restore failed, ret:" << result);
        delete boostStateDB;
        return 0;
    }
    LOG_INFO("Open boostStateDB success.");
    return static_cast<jlong>(reinterpret_cast<size_t>(boostStateDB));
}

JNIEXPORT jboolean JNICALL Java_com_huawei_ock_bss_common_BoostStateDB_restore(JNIEnv *env, jobject, jlong jDBHandle,
                                                                               jobject jRestorePaths,
                                                                               jobject jRemotePaths,
                                                                               jobject jLocalPaths, jboolean isLazy,
                                                                               jboolean isNewjob)
{
    if (UNLIKELY(env == nullptr || jRestorePaths == nullptr || jRemotePaths == nullptr || jLocalPaths == nullptr)) {
        LOG_ERROR("Input param is nullptr.");
        return JNI_FALSE;
    }
    LOG_INFO("Boost state db handle restore start, dbHandle:" << jDBHandle);
    auto *boostStateDB = reinterpret_cast<BoostStateDB *>(jDBHandle);
    if (UNLIKELY(boostStateDB == nullptr)) {
        LOG_ERROR("DB handle is nullptr.");
        return JNI_FALSE;
    }
    jclass listClass = env->GetObjectClass(jRemotePaths);
    if (UNLIKELY(listClass == nullptr || env->ExceptionCheck())) {
        LOG_ERROR("Failed to get restore path list class.");
        return JNI_FALSE;
    }
    jmethodID sizeMid = env->GetMethodID(listClass, "size", "()I");
    if (UNLIKELY(sizeMid == nullptr || env->ExceptionCheck())) {
        LOG_ERROR("Failed to get restore path list size method.");
        env->DeleteLocalRef(listClass);
        return JNI_FALSE;
    }
    jmethodID getMid = env->GetMethodID(listClass, "get", "(I)Ljava/lang/Object;");
    if (UNLIKELY(getMid == nullptr || env->ExceptionCheck())) {
        LOG_ERROR("Failed to get restore path list get method.");
        env->DeleteLocalRef(listClass);
        return JNI_FALSE;
    }
    jint metaSize = env->CallIntMethod(jRestorePaths, sizeMid);
    if (UNLIKELY(env->ExceptionCheck() || metaSize < 0 || metaSize > static_cast<jint>(NO_1000000))) {
        LOG_ERROR("MetaSize too big, metaSize: " << metaSize << ", maxSize: " << NO_1000000);
        env->DeleteLocalRef(listClass);
        return JNI_FALSE;
    }
    std::vector<std::string> metaPaths;
    for (jint i = 0; i < metaSize; i++) {
        jstring restorePath = (jstring)env->CallObjectMethod(jRestorePaths, getMid, i);
        if (UNLIKELY(restorePath == nullptr || env->ExceptionCheck())) {
            LOG_ERROR("Failed to get restore path at index: " << i);
            env->DeleteLocalRef(listClass);
            return JNI_FALSE;
        }
        std::string path = ConstructPath(env, restorePath);
        env->DeleteLocalRef(restorePath);
        if (UNLIKELY(!CheckPathValid(path))) {
            LOG_ERROR("Invalid Restore Paths, path: " << PathTransform::ExtractFileName(path));
            env->DeleteLocalRef(listClass);
            return JNI_FALSE;
        }
        metaPaths.emplace_back(path);
    }
    // 获取列表大小
    jint size = env->CallIntMethod(jRemotePaths, sizeMid);
    if (UNLIKELY(env->ExceptionCheck())) {
        LOG_ERROR("Failed to get remote restore path list size.");
        env->DeleteLocalRef(listClass);
        return JNI_FALSE;
    }
    jint localSize = env->CallIntMethod(jLocalPaths, sizeMid);
    if (UNLIKELY(env->ExceptionCheck() || size < 0 || size > static_cast<jint>(NO_1000000) || localSize != size)) {
        LOG_ERROR("MetaSize too big, size: " << size << ", maxSize: " << NO_1000000);
        env->DeleteLocalRef(listClass);
        return JNI_FALSE;
    }
    std::unordered_map<std::string, std::string> lazyPathMapping;
    for (jint i = 0; i < size; i++) {
        // 获取第 i 个元素，是一个 String 对象
        jstring remote = (jstring)env->CallObjectMethod(jRemotePaths, getMid, i);
        if (UNLIKELY(remote == nullptr || env->ExceptionCheck())) {
            LOG_ERROR("Failed to get remote restore path at index: " << i);
            env->DeleteLocalRef(listClass);
            return JNI_FALSE;
        }
        std::string remotePath = ConstructPath(env, remote);
        env->DeleteLocalRef(remote);
        if (UNLIKELY(!CheckPathValid(remotePath, true))) {
            env->DeleteLocalRef(listClass);
            LOG_ERROR("Invalid Remote Paths, remotePath: " << PathTransform::ExtractFileName(remotePath));
            return JNI_FALSE;
        }
        jstring local = (jstring)env->CallObjectMethod(jLocalPaths, getMid, i);
        if (UNLIKELY(local == nullptr || env->ExceptionCheck())) {
            LOG_ERROR("Failed to get local restore path at index: " << i);
            env->DeleteLocalRef(listClass);
            return JNI_FALSE;
        }
        std::string localPath = ConstructPath(env, local);
        env->DeleteLocalRef(local);
        if (UNLIKELY(!CheckPathValid(localPath, true))) {
            env->DeleteLocalRef(listClass);
            LOG_ERROR("Invalid Local Paths, localPath: " << PathTransform::ExtractFileName(localPath));
            return JNI_FALSE;
        }
        lazyPathMapping.emplace(localPath, remotePath);
        LOG_INFO("lazy restore from:" << PathTransform::ExtractFileName(remotePath) << " to "
                                      << PathTransform::ExtractFileName(localPath));
    }
    env->DeleteLocalRef(listClass);
    auto ret = boostStateDB->Restore(metaPaths, lazyPathMapping, static_cast<bool>(isLazy),
                                     static_cast<bool>(isNewjob));
    return ret == BSS_OK ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jlong JNICALL Java_com_huawei_ock_bss_common_BoostStateDB_newIterator(JNIEnv *env, jobject, jlong jDBPath)
{
    auto *boostStateDB = reinterpret_cast<BoostStateDB *>(jDBPath);
    if (UNLIKELY(boostStateDB == nullptr)) {
        LOG_ERROR("DB handle is nullptr.");
        return 0;
    }
    auto savepointDataView = boostStateDB->TriggerSavepoint();
    if (UNLIKELY(savepointDataView == nullptr)) {
        LOG_ERROR("Failed to create savepointDataView.");
        return 0;
    }
    return static_cast<jlong>(reinterpret_cast<uintptr_t>(savepointDataView));
}

JNIEXPORT jlong JNICALL Java_com_huawei_ock_bss_common_BoostStateDB_changeHeapAvailableSize(JNIEnv *env, jclass,
                                                                                            jlong heapSize)
{
    if (UNLIKELY(heapSize < 0 || static_cast<uint64_t>(heapSize) > IO_SIZE_8G)) {
        LOG_ERROR("heapSize is invalid, heapSize" << heapSize);
        return 0;
    }
    return BoostStateDB::ChangeHeapAvailableSize(heapSize);
}

JNIEXPORT void JNICALL Java_com_huawei_ock_bss_common_BoostStateDB_notifyDBSnapshotAbort(JNIEnv *env, jobject,
                                                                                         jlong jDBHandle,
                                                                                         jlong jCheckpointId)
{
    if (UNLIKELY(env == nullptr)) {
        LOG_ERROR("Input env is nullptr.");
        return;
    }
    auto *boostStateDB = reinterpret_cast<BoostStateDB *>(jDBHandle);
    if (UNLIKELY(boostStateDB == nullptr)) {
        LOG_ERROR("boostStateDB is nullptr.");
        return;
    }
    if (UNLIKELY(jCheckpointId < 0)) {
        LOG_ERROR("jCheckpointId less than zero.");
        return;
    }
    LOG_INFO("Notify snapshot abort, checkpointId:" << static_cast<uint64_t>(jCheckpointId));
    boostStateDB->NotifyDBSnapshotAbort(static_cast<uint64_t>(jCheckpointId));
}

JNIEXPORT void JNICALL Java_com_huawei_ock_bss_common_BoostStateDB_notifyDBSnapshotComplete(JNIEnv *env, jobject,
                                                                                            jlong jDBHandle,
                                                                                            jlong jCheckpointId)
{
    if (UNLIKELY(env == nullptr)) {
        LOG_ERROR("Input env is nullptr.");
        return;
    }
    auto *boostStateDB = reinterpret_cast<BoostStateDB *>(jDBHandle);
    if (UNLIKELY(boostStateDB == nullptr)) {
        LOG_ERROR("boostStateDB is nullptr.");
        return;
    }
    if (UNLIKELY(jCheckpointId < 0)) {
        LOG_ERROR("jCheckpointId less than zero.");
        return;
    }
    LOG_INFO("Notify snapshot complete, checkpointId:" << static_cast<uint64_t>(jCheckpointId));
    boostStateDB->NotifyDBSnapshotComplete(static_cast<uint64_t>(jCheckpointId));
}

JNIEXPORT void JNICALL Java_com_huawei_ock_bss_common_BoostStateDB_registerBoostNativeMetric(JNIEnv *, jobject,
                                                                                             jlong jDBHandle,
                                                                                             jlong jMetricHandle)
{
    auto *boostStateDB = reinterpret_cast<BoostStateDB *>(jDBHandle);
    if (UNLIKELY(boostStateDB == nullptr)) {
        LOG_ERROR("boostStateDB is nullptr.");
        return;
    }
    auto *metricPtr = reinterpret_cast<BoostNativeMetricPtr>(jMetricHandle);
    if (UNLIKELY(metricPtr == nullptr)) {
        LOG_ERROR("BoostNativeMetric is nullptr.");
        return;
    }
    boostStateDB->RegisterMetric(metricPtr);
}
