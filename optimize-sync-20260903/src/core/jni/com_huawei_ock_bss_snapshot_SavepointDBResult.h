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
#include <jni.h>
/* Header for class com_huawei_ock_bss_snapshot_SavepointDBResult */

#ifndef _Included_com_huawei_ock_bss_snapshot_SavepointDBResult
#define _Included_com_huawei_ock_bss_snapshot_SavepointDBResult
#ifdef __cplusplus
extern "C" {
#endif
/*
 * Class:     com_huawei_ock_bss_snapshot_SavepointDBResult
 * Method:    hasNext
 * Signature: (J)Z
 */
JNIEXPORT jboolean JNICALL Java_com_huawei_ock_bss_snapshot_SavepointDBResult_hasNext(JNIEnv *, jobject, jlong);

/*
 * Class:     com_huawei_ock_bss_snapshot_SavepointDBResult
 * Method:    next
 * Signature: (J)Lcom/huawei/ock/bss/common/BinaryKeyValueItem;
 */
JNIEXPORT jobject JNICALL Java_com_huawei_ock_bss_snapshot_SavepointDBResult_next(JNIEnv *, jobject, jlong);

/*
 * Class:     com_huawei_ock_bss_snapshot_SavepointDBResult
 * Method:    close
 * Signature: (J)V
 */
JNIEXPORT void JNICALL Java_com_huawei_ock_bss_snapshot_SavepointDBResult_close(JNIEnv *, jobject, jlong);

#ifdef __cplusplus
}
#endif
#endif
