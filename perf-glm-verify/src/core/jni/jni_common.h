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

#ifndef JNI_COMMON_H
#define JNI_COMMON_H
#include <jni.h>

#include "include/bss_types.h"

#ifdef __cplusplus
extern "C" {
#endif
extern jclass gEntryClass;
extern jfieldID gKeyAddrField;
extern jfieldID gKeyLenField;
extern jfieldID gValueAddrField;
extern jfieldID gValueLenField;
extern jfieldID gSubKeyAddrField;
extern jfieldID gSubKeyLenField;
extern jclass gStateListCountClass;
extern jfieldID gResIdField;
extern jfieldID gReadSectionIdField;
extern jfieldID gSizeField;
extern jfieldID gAddressesField;
extern jfieldID gLengthsField;

JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM *vm, void *reserved);
JNIEXPORT void JNICALL JNI_OnUnload(JavaVM *vm, void *reserved);

bool SubTableEntryInit(JNIEnv *env);

void SubTableEntryExit(JNIEnv *env);

bool KListTableImplInit(JNIEnv *env);

void KListTableImplExit(JNIEnv *env);

#ifdef __cplusplus
}
#endif
#endif