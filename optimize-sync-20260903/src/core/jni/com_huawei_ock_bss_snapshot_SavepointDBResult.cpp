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
#include "com_huawei_ock_bss_snapshot_SavepointDBResult.h"

#include "include/boost_state_db.h"
#include "include/bss_err.h"
#include "include/config.h"
#include "kv_helper.h"
#include "snapshot/savepoint_data_view.h"
using namespace ock::bss;

JNIEXPORT jboolean JNICALL Java_com_huawei_ock_bss_snapshot_SavepointDBResult_hasNext(JNIEnv *env, jobject,
                                                                                      jlong jSavepointPath)
{
    auto *savepointDataViewImpl = reinterpret_cast<SavepointDataView *>(jSavepointPath);
    if (UNLIKELY(savepointDataViewImpl == nullptr)) {
        return JNI_FALSE;
    }
    return savepointDataViewImpl->SavepointIterator()->HasNext() ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jobject JNICALL Java_com_huawei_ock_bss_snapshot_SavepointDBResult_next(JNIEnv *env, jobject jobject,
                                                                                  jlong jSavepointPath)
{
    if (UNLIKELY(env == nullptr)) {
        LOG_ERROR("Input env is nullptr.");
        return nullptr;
    }
    auto *savepointDataViewImpl = reinterpret_cast<SavepointDataView *>(jSavepointPath);
    if (UNLIKELY(savepointDataViewImpl == nullptr)) {
        return nullptr;
    }
    auto savepointIterator = savepointDataViewImpl->SavepointIterator();
    if (UNLIKELY(savepointIterator == nullptr)) {
        LOG_ERROR("MakeRef failed, savepointIterator is nullptr.");
        return nullptr;
    }
    auto item = savepointIterator->Next();
    return ConvertKeyValueItem(env, jobject, item);
}

JNIEXPORT void JNICALL Java_com_huawei_ock_bss_snapshot_SavepointDBResult_close(JNIEnv *env, jobject,
                                                                                jlong jSavepointPath)
{
    auto *savepointDataViewImpl = reinterpret_cast<SavepointDataView *>(jSavepointPath);
    if (UNLIKELY(savepointDataViewImpl == nullptr)) {
        return;
    }
    LOG_INFO("Close savepoint iterator, SnapshotId:" << savepointDataViewImpl->GetSnapshotId() << ", maxParallelism:"
                                                     << savepointDataViewImpl->GetMaxParallelism());
    savepointDataViewImpl->Close();
    delete savepointDataViewImpl;
}