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

#include "com_huawei_ock_bss_common_memory_DirectBuffer.h"

#include "kv_helper.h"

using namespace ock::bss;

JNIEXPORT jlong JNICALL Java_com_huawei_ock_bss_common_memory_DirectBuffer_nativeGetDirectBufferData(JNIEnv *env,
                                                                                                     jclass,
                                                                                                     jlong jBuffer)
{
    if (UNLIKELY(jBuffer == 0)) {
        return 0;
    }
    SerializedDataWrapper *buffer = reinterpret_cast<SerializedDataWrapper *>(jBuffer);
    return reinterpret_cast<jlong>(buffer->Data());
}

JNIEXPORT jint JNICALL Java_com_huawei_ock_bss_common_memory_DirectBuffer_nativeGetDirectBufferLength(JNIEnv *env,
                                                                                                      jclass,
                                                                                                      jlong jBuffer)
{
    if (UNLIKELY(jBuffer == 0)) {
        return 0;
    }
    SerializedDataWrapper *node = reinterpret_cast<SerializedDataWrapper *>(jBuffer);
    return node->Length();
}

JNIEXPORT void JNICALL Java_com_huawei_ock_bss_common_memory_DirectBuffer_nativeReleaseDirectBuffer(JNIEnv *env, jclass,
                                                                                                    jlong jBuffer)
{
    delete reinterpret_cast<SerializedDataWrapper *>(jBuffer);
}

/*
 * just for testing.
 */
JNIEXPORT jlong JNICALL Java_com_huawei_ock_bss_common_memory_DirectBuffer_nativeAcquireDirectBuffer(JNIEnv *, jclass,
                                                                                                     jlong jData,
                                                                                                     jint jLength)
{
    if (UNLIKELY(jLength <= 0 || jData == 0)) {
        LOG_ERROR("jLength cannot be less than zero.");
        return 0;
    }
    ByteBufferRef buffer = MakeRef<ByteBuffer>(reinterpret_cast<uint8_t *>(jData), jLength, true);
    return reinterpret_cast<jlong>(new SerializedDataWrapper(buffer, 0, jLength));
}

JNIEXPORT jobject JNICALL Java_com_huawei_ock_bss_common_memory_DirectBuffer_nativeAllocDirectBuffer(JNIEnv *mEnv,
                                                                                                     jclass,
                                                                                                     jint jLength)
{
    if (UNLIKELY(static_cast<uint32_t>(jLength) > IO_SIZE_128M)) {
        LOG_ERROR("length is invalid , length: " << jLength);
        return nullptr;
    }

    void *addr = malloc(jLength);
    if (UNLIKELY(addr == nullptr)) {
        return nullptr;
    }

    jobject object = mEnv->NewDirectByteBuffer(addr, jLength);
    if (UNLIKELY(object == nullptr)) {
        free(addr);
        return nullptr;
    }
    return object;
}

JNIEXPORT void JNICALL Java_com_huawei_ock_bss_common_memory_DirectBuffer_nativeFreeDirectBuffer(JNIEnv *mEnv, jclass,
                                                                                                 jlong data)
{
    if (UNLIKELY(data == 0)) {
        return;
    }
    void *addr = reinterpret_cast<void *>(data);
    free(addr);
}