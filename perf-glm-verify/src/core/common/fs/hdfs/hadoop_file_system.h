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

#ifndef BOOST_SS_HADOOP_FILE_SYSTEM_H
#define BOOST_SS_HADOOP_FILE_SYSTEM_H
#include <jni.h>

#include "common/fs/file_system.h"
#include "common/jvm_instance.h"
#include "common/path.h"

namespace ock {
namespace bss {

class HadoopFileSystem : public FileSystem {
public:
    explicit HadoopFileSystem(const PathRef &path) : FileSystem(path)
    {
    }

    ~HadoopFileSystem() override
    {
        if (UNLIKELY(mEnv == nullptr)) {
            return;
        }
        if (mClassHdfs != nullptr) {
            mEnv->DeleteGlobalRef(mClassHdfs);
            mClassHdfs = nullptr;
        }
        if (mObjectHdfs != nullptr) {
            mEnv->DeleteGlobalRef(mObjectHdfs);
            mObjectHdfs = nullptr;
        }
    }

    BResult Open(int flags) override
    {
        auto jvm = JVMInstance::GetInstance();
        if (UNLIKELY(jvm == nullptr)) {
            return BSS_ERR;
        }
        auto ret = jvm->AttachCurrentThread(reinterpret_cast<void **>(&mEnv), nullptr);
        if (ret != JNI_OK || mEnv == nullptr) {
            LOG_ERROR("Get JNI env fail:" << ret);
            return BSS_ERR;
        }
        jclass clazz = mEnv->FindClass("com/huawei/ock/bss/common/fs/HadoopFileSystem");
        mClassHdfs = (jclass)mEnv->NewGlobalRef(clazz);
        mEnv->DeleteLocalRef(clazz);
        RETURN_ALLOC_FAIL_AS_NULLPTR(mClassHdfs);
        jmethodID constructor = mEnv->GetMethodID(mClassHdfs, "<init>", "(Ljava/lang/String;)V");
        RETURN_ALLOC_FAIL_AS_NULLPTR(constructor);
        std::string path = mFilePath->Name();
        jstring jFilePath = mEnv->NewStringUTF(path.c_str());
        RETURN_ALLOC_FAIL_AS_NULLPTR(jFilePath);
        jobject object = mEnv->NewObject(mClassHdfs, constructor, jFilePath);
        mObjectHdfs = mEnv->NewGlobalRef(object);
        mEnv->DeleteLocalRef(object);
        RETURN_ALLOC_FAIL_AS_NULLPTR(mObjectHdfs);
        if (mEnv->ExceptionCheck()) {
            LOG_ERROR("Create jHdfs fail:" << mFilePath->ExtractFileName());
            mEnv->ExceptionDescribe();
            mEnv->ExceptionClear();
            Close();
            return BSS_ERR;
        }
        return InitJMethod();
    }

    BResult Read(uint8_t *buffer, uint64_t count, int64_t offset) override
    {
        if (UNLIKELY(count > INT32_MAX || offset > INT32_MAX)) {
            LOG_ERROR("count or offset is larger than INT32_MAX, count:" << count << ", offset:" << offset << ".");
            return BSS_ERR;
        }
        mOffset = offset;
        jint jOffset = static_cast<jint>(mOffset);
        jint jCount = static_cast<jint>(count);
        auto byteArray = (jbyteArray)mEnv->CallObjectMethod(mObjectHdfs, mRead, jOffset, jCount);
        RETURN_ALLOC_FAIL_AS_NULLPTR(byteArray);
        jbyte *byte = mEnv->GetByteArrayElements(byteArray, nullptr);
        RETURN_ALLOC_FAIL_AS_NULLPTR(byte);
        uint32_t len = mEnv->GetArrayLength(byteArray);
        if (len == 0) {
            LOG_ERROR("Read remote file fail,expect size :" << count << " actual size:" << len
                                                            << " path:" << mFilePath->ExtractFileName());
            return BSS_ERR;
        }
        if (count != len) {
            LOG_ERROR("Read remote file fail,expect size :" << count << " actual size:" << len
                                                            << " path:" << mFilePath->ExtractFileName());
            return BSS_ERR;
        }
        for (uint32_t i = 0; i < len; ++i) {
            buffer[i] = static_cast<uint8_t>(byte[i] & 0xFF);  // 确保无符号
        }
        mOffset += len;
        mEnv->ReleaseByteArrayElements(byteArray, byte, JNI_ABORT);
        mEnv->DeleteLocalRef(byteArray);
        return BSS_OK;
    }

    BResult Read(uint8_t *buffer, uint64_t count) override
    {
        return Read(buffer, count, mOffset);
    }

    BResult Write(const uint8_t *buffer, uint64_t count, int64_t offset) override
    {
        return BSS_OK;
    }

    BResult Write(const uint8_t *buffer, uint64_t count) override
    {
        return BSS_OK;
    }

    BResult Sync() override
    {
        return BSS_OK;
    }

    BResult Flush() override
    {
        return BSS_OK;
    }

    void Close() override
    {
    }

    void Seek(int64_t offset) override
    {
        mOffset = offset;
    }

    BResult Download(const PathRef &localPath)
    {
        RETURN_INVALID_PARAM_AS_NULLPTR(localPath);
        if (mObjectHdfs == nullptr) {
            LOG_ERROR("mObjectHdfs is null");
            return BSS_ERR;
        }
        if (mDownload == nullptr) {
            LOG_ERROR("mDownload is null");
            return BSS_ERR;
        }
        jstring jFilePath = mEnv->NewStringUTF(localPath->Name().c_str());
        mEnv->CallVoidMethod(mObjectHdfs, mDownload, jFilePath);
        if (mEnv->ExceptionCheck()) {
            LOG_ERROR("Download hdfs file failed:" << mFilePath->ExtractFileName());
            mEnv->ExceptionDescribe();
            mEnv->ExceptionClear();
            mEnv->DeleteLocalRef(jFilePath);
            return BSS_ERR;
        }
        mEnv->DeleteLocalRef(jFilePath);
        LOG_INFO("Download :" << mFilePath->ExtractFileName() << " to " << localPath->ExtractFileName());
        return BSS_OK;
    }

    void Remove() override
    {
        if (mEnv == nullptr) {
            return;
        }
        mEnv->CallVoidMethod(mObjectHdfs, mClose);
        if (mEnv->ExceptionCheck()) {
            LOG_ERROR("Close java file system failed.");
            mEnv->ExceptionDescribe();
            mEnv->ExceptionClear();
        }
    }

private:
    BResult InitJMethod()
    {
        mRead = mEnv->GetMethodID(mClassHdfs, "readBytes", "(II)[B");
        RETURN_ALLOC_FAIL_AS_NULLPTR(mRead);
        mClose = mEnv->GetMethodID(mClassHdfs, "close", "()V");
        RETURN_ALLOC_FAIL_AS_NULLPTR(mClose);
        mDownload = mEnv->GetMethodID(mClassHdfs, "download", "(Ljava/lang/String;)V");
        RETURN_ALLOC_FAIL_AS_NULLPTR(mDownload);
        mRemove = mEnv->GetMethodID(mClassHdfs, "remove", "()V");
        RETURN_ALLOC_FAIL_AS_NULLPTR(mRemove);
        return BSS_OK;
    }

private:
    JNIEnv *mEnv = nullptr;
    jobject mObjectHdfs = nullptr;
    jclass mClassHdfs = nullptr;
    jmethodID mRead = nullptr;
    jmethodID mClose = nullptr;
    jmethodID mDownload = nullptr;
    jmethodID mRemove = nullptr;
};

using HadoopFileSystemRef = std::shared_ptr<HadoopFileSystem>;

}  // namespace bss
}  // namespace ock
#endif  // BOOST_SS_HADOOP_FILE_SYSTEM_H