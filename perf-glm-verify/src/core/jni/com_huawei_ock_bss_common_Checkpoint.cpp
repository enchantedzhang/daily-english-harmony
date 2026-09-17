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

#include "com_huawei_ock_bss_common_Checkpoint.h"

#include "include/boost_state_db.h"
#include "include/bss_err.h"
#include "kv_helper.h"

using namespace ock::bss;
JNIEXPORT jlong JNICALL Java_com_huawei_ock_bss_common_Checkpoint_newCheckpoint(JNIEnv *env, jclass, jlong jDBHandle,
                                                                                jlong jCheckpointId,
                                                                                jstring jCheckpointPath)
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
    if (UNLIKELY(jCheckpointPath == nullptr)) {
        LOG_ERROR("jCheckpointPath is nullptr.");
        return 0;
    }
    const char *checkpointPath = env->GetStringUTFChars(jCheckpointPath, nullptr);
    if (UNLIKELY(checkpointPath == nullptr)) {
        LOG_ERROR("checkpointPath is nullptr.");
        return 0;
    }
    std::string checkpointPathStr(checkpointPath);
    if (UNLIKELY(!CheckPathValid(checkpointPathStr))) {
        LOG_ERROR("Invalid Check Point Paths, checkpointPath: " << PathTransform::ExtractFileName(checkpointPathStr));
        env->ReleaseStringUTFChars(jCheckpointPath, checkpointPath);
        return 0;
    }
    env->ReleaseStringUTFChars(jCheckpointPath, checkpointPath);
    if (UNLIKELY(jCheckpointId < 0)) {
        LOG_ERROR("jCheckpointId less than zero.");
        return 0;
    }
    auto checkpointCoordinator = boostStateDB->CreateSyncCheckpoint(checkpointPathStr,
                                                                    static_cast<uint64_t>(jCheckpointId));
    if (UNLIKELY(checkpointCoordinator == nullptr)) {
        LOG_ERROR("Failed to create checkpointCoordinator.");
        return 0;
    }
    return static_cast<jlong>(reinterpret_cast<uintptr_t>(checkpointCoordinator));
}

JNIEXPORT jboolean JNICALL Java_com_huawei_ock_bss_common_Checkpoint_createCheckpoint(JNIEnv *env, jobject,
                                                                                      jlong jDBHandle,
                                                                                      jlong jCheckpointId,
                                                                                      jboolean jIsIncremental)
{
    auto *boostStateDB = reinterpret_cast<BoostStateDB *>(jDBHandle);
    if (UNLIKELY(boostStateDB == nullptr)) {
        return JNI_FALSE;
    }
    if (UNLIKELY(jCheckpointId < 0)) {
        LOG_ERROR("jCheckpointId less than zero.");
        return JNI_FALSE;
    }
    auto ret = boostStateDB->CreateAsyncCheckpoint(static_cast<uint64_t>(jCheckpointId),
                                                   static_cast<bool>(jIsIncremental));
    return ret == BSS_OK ? JNI_TRUE : JNI_FALSE;
}