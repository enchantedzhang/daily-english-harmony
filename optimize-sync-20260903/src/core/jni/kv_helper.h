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

#ifndef BOOST_SS_KV_HELPER_H
#define BOOST_SS_KV_HELPER_H
#include <fcntl.h>
#include <jni.h>
#include <linux/limits.h>
#include <unistd.h>

#include <cassert>
#include <list>
#include <string>
#include <vector>

#include "bss_def.h"
#include "bss_log.h"
#include "common/path_transform.h"
#include "common/util/string_util.h"
#include "include/bss_types.h"
#include "include/config.h"
#include "include/state_type.h"
#include "kv_table/serialized_data.h"
#include "securec.h"
#include "snapshot/binary_key_value_Item_iterator.h"
using namespace ock::bss;

namespace ock {
namespace bss {

// cache java class/fieldID/methodID for better performance
static jclass stateTypeClass = nullptr;
static jclass binaryKVItemClass = nullptr;
static jmethodID stateTypeOfMethod = nullptr;
static jmethodID setStateTypeMethod = nullptr;
static jmethodID setStateNameMethod = nullptr;
static jmethodID setKeyGroupMethod = nullptr;
static jfieldID keyFiled = nullptr;
static jfieldID keyLenFiled = nullptr;
static jfieldID namespaceFiled = nullptr;
static jfieldID namespaceLenFiled = nullptr;
static jfieldID mapKeyFiled = nullptr;
static jfieldID mapKeyLenFiled = nullptr;
static jfieldID valueFiled = nullptr;
static jfieldID valueLenFiled = nullptr;

inline bool StringToKeyedStateType(const std::string &str, StateType &outType)
{
    // 后续还需要改str的判断，建议用switch case
    if (str == "KVTableImpl") {
        outType = StateType::VALUE;
        return true;
    } else if (str == "KListTableImpl") {
        outType = StateType::LIST;
        return true;
    } else if (str == "KMapTableImpl") {
        outType = StateType::MAP;
        return true;
    } else if (str == "NsKVTableImpl") {
        outType = StateType::SUB_VALUE;
        return true;
    } else if (str == "NsKListTableImpl") {
        outType = StateType::SUB_LIST;
        return true;
    } else if (str == "NsKMapTableImpl") {
        outType = StateType::SUB_MAP;
        return true;
    } else {
        return false;  // 转换失败
    }
}

// 获取最大并行度
inline uint32_t GetMaxParallelism(JNIEnv *env, jclass boostConfigClass, jobject jBoostConfig)
{
    jmethodID getMaxParallelismMethod = env->GetMethodID(boostConfigClass, "getMaxParallelism", "()I");
    if (getMaxParallelismMethod == nullptr) {
        return 0;
    }

    auto result = env->CallIntMethod(jBoostConfig, getMaxParallelismMethod);
    if (result < 0) {
        return 0;
    }

    return static_cast<uint32_t>(result);
}

// 获取KeyGroupRange
inline std::pair<uint32_t, uint32_t> GetKeyGroupRange(JNIEnv *env, jclass boostConfigClass, jobject jBoostConfig)
{
    // 获取getKeyGroupRange方法
    jmethodID getKeyGroupRangeMethod = env->GetMethodID(boostConfigClass, "getKeyGroupRange",
                                                        "()Lorg/apache/flink/runtime/state/KeyGroupRange;");
    if (getKeyGroupRangeMethod == nullptr) {
        return { INVALID_U32, INVALID_U32 };
    }

    // 调用getKeyGroupRange方法获取KeyGroupRange对象
    jobject keyGroupRange = env->CallObjectMethod(jBoostConfig, getKeyGroupRangeMethod);
    if (keyGroupRange == nullptr) {
        return { INVALID_U32, INVALID_U32 };
    }

    // 获取KeyGroupRange类
    jclass keyGroupRangeClass = env->GetObjectClass(keyGroupRange);
    if (keyGroupRangeClass == nullptr) {
        return { INVALID_U32, INVALID_U32 };
    }

    // 获取getStartKeyGroup和getEndKeyGroup方法
    jmethodID getStartKeyGroupMethod = env->GetMethodID(keyGroupRangeClass, "getStartKeyGroup", "()I");
    jmethodID getEndKeyGroupMethod = env->GetMethodID(keyGroupRangeClass, "getEndKeyGroup", "()I");
    if (getStartKeyGroupMethod == nullptr || getEndKeyGroupMethod == nullptr) {
        LOG_ERROR("StartKeyGroupMethod or EndKeyGroupMethod is nullptr.");
        env->DeleteLocalRef(keyGroupRangeClass);
        return { INVALID_U32, INVALID_U32 };
    }

    // 调用方法获取 startKeyGroup  和 endKeyGroup
    jint startKeyGroup = env->CallIntMethod(keyGroupRange, getStartKeyGroupMethod);
    if (UNLIKELY(startKeyGroup < 0)) {
        LOG_ERROR("startKeyGroup is less than zero.");
        env->DeleteLocalRef(keyGroupRangeClass);
        return { INVALID_U32, INVALID_U32 };
    }
    jint endKeyGroup = env->CallIntMethod(keyGroupRange, getEndKeyGroupMethod);
    if (UNLIKELY(endKeyGroup < 0)) {
        LOG_ERROR("endKeyGroup is less than zero.");
        env->DeleteLocalRef(keyGroupRangeClass);
        return { INVALID_U32, INVALID_U32 };
    }
    env->DeleteLocalRef(keyGroupRangeClass);
    return { static_cast<uint32_t>(startKeyGroup), static_cast<uint32_t>(endKeyGroup) };
}

// 获取String字符串
inline std::string GetStringFromJava(JNIEnv *env, jclass boostConfigClass, jobject jBoostConfig, const char *methodName,
                                     const char *className)
{
    if (UNLIKELY(methodName == nullptr || className == nullptr)) {
        LOG_ERROR("Method name or class name is null.");
        return {};
    }

    jmethodID getInstanceBasePathMethod = env->GetMethodID(boostConfigClass, methodName, className);
    if (getInstanceBasePathMethod == nullptr) {
        LOG_ERROR("Get java failed, instanceBasePathMethod is null, method: " << std::string(methodName));
        return {};  // 获取方法失败
    }

    // 调用 getInstanceBasePathMethod 方法获取 instanceBasePathMethod 对象
    jstring jStringTmp = static_cast<jstring>(env->CallObjectMethod(jBoostConfig, getInstanceBasePathMethod));
    if (jStringTmp == nullptr) {
        LOG_ERROR("Invoke java method failed, basePath is null, method: " << std::string(methodName));
        return {};  // 获取对象失败
    }

    const char *finalChars = env->GetStringUTFChars(jStringTmp, nullptr);
    if (finalChars == nullptr) {
        LOG_ERROR("Transform java object string failed, localPathChars is null, method: " << std::string(methodName));
        return {};  // 处理错误
    }
    std::string outputString(finalChars);
    env->ReleaseStringUTFChars(jStringTmp, finalChars);
    return outputString;
}

// 获取BackendUID
inline std::string GetBackendUID(JNIEnv *env, jclass boostConfigClass, jobject jBoostConfig)
{
    jmethodID getBackendUIDMethod = env->GetMethodID(boostConfigClass, "getBackendUID", "()Ljava/lang/String;");
    if (getBackendUIDMethod == nullptr) {
        return {};  // 获取方法失败
    }

    // 调用 getBackendUID 方法获取 backendUID 对象
    jstring jbackendUID = static_cast<jstring>(env->CallObjectMethod(jBoostConfig, getBackendUIDMethod));
    if (jbackendUID == nullptr) {
        return {};  // 获取对象失败
    }

    const char *backendUIDChars = env->GetStringUTFChars(jbackendUID, nullptr);
    if (backendUIDChars == nullptr) {
        return {};  // 处理错误
    }
    std::string backendUID(backendUIDChars);
    env->ReleaseStringUTFChars(jbackendUID, backendUIDChars);
    return backendUID;
}

inline uint64_t GetDBSize(JNIEnv *env, jclass boostConfigClass, jobject jBoostConfig)
{
    jmethodID getTotalDBSizeMethod = env->GetMethodID(boostConfigClass, "getTotalDBSize", "()J");
    if (getTotalDBSizeMethod == nullptr) {
        return 0;
    }
    jlong dBSize = env->CallLongMethod(jBoostConfig, getTotalDBSizeMethod);
    if (UNLIKELY(dBSize < 0)) {
        LOG_ERROR("dBSize is less than zero.");
        return 0;
    }
    return static_cast<uint64_t>(dBSize);
}

inline uint64_t GetBorrowHeapSize(JNIEnv *env, jclass boostConfigClass, jobject jBoostConfig)
{
    jmethodID getBorrowHeapSizeMethod = env->GetMethodID(boostConfigClass, "getBorrowHeapSize", "()J");
    if (getBorrowHeapSizeMethod == nullptr) {
        return 0;
    }
    jlong borrowHeapSize = env->CallLongMethod(jBoostConfig, getBorrowHeapSizeMethod);
    if (UNLIKELY(borrowHeapSize < 0)) {
        LOG_ERROR("borrowHeapSize is less than zero.");
        return 0;
    }
    return static_cast<uint64_t>(borrowHeapSize);
}

inline float GetSliceMemWaterMark(JNIEnv *env, jclass boostConfigClass, jobject jBoostConfig)
{
    jmethodID getTotalDBSizeMethod = env->GetMethodID(boostConfigClass, "getSliceMemWaterMark", "()F");
    if (getTotalDBSizeMethod == nullptr) {
        return 0;
    }
    return static_cast<float>(env->CallFloatMethod(jBoostConfig, getTotalDBSizeMethod));
}

inline float GetCacheFilterAndIndexRatio(JNIEnv *env, jclass boostConfigClass, jobject jBoostConfig)
{
    jmethodID getIndexCacheRatioMethod = env->GetMethodID(boostConfigClass, "getCacheFilterAndIndexRatio", "()F");
    if (getIndexCacheRatioMethod == nullptr) {
        return 0;
    }
    return static_cast<float>(env->CallFloatMethod(jBoostConfig, getIndexCacheRatioMethod));
}

inline float GetFileMemoryRatio(JNIEnv *env, jclass boostConfigClass, jobject jBoostConfig)
{
    jmethodID getFileMemoryFraction = env->GetMethodID(boostConfigClass, "getFileMemoryFraction", "()F");
    if (getFileMemoryFraction == nullptr) {
        return 0;
    }
    return static_cast<float>(env->CallFloatMethod(jBoostConfig, getFileMemoryFraction));
}

inline int32_t GetLsmStoreCompactionSwitch(JNIEnv *env, jclass boostConfigClass, jobject jBoostConfig)
{
    jmethodID getCompactionSwitch = env->GetMethodID(boostConfigClass, "getLsmCompactionSwitch", "()I");
    if (getCompactionSwitch == nullptr) {
        return 0;
    }
    return static_cast<int32_t>(env->CallIntMethod(jBoostConfig, getCompactionSwitch));
}

inline bool GetTtlFilterSwitch(JNIEnv *env, jclass boostConfigClass, jobject jBoostConfig)
{
    jmethodID getTtlFilterSwitch = env->GetMethodID(boostConfigClass, "getTtlFilterSwitch", "()Z");
    if (getTtlFilterSwitch == nullptr) {
        return false;
    }
    return static_cast<bool>(env->CallBooleanMethod(jBoostConfig, getTtlFilterSwitch));
}

inline bool GetZeroCopySwitch(JNIEnv *env, jclass boostConfigClass, jobject jBoostConfig)
{
    jmethodID getZeroCopySwitch = env->GetMethodID(boostConfigClass, "getZeroCopySwitch", "()Z");
    if (getZeroCopySwitch == nullptr) {
        return false;
    }
    return static_cast<bool>(env->CallBooleanMethod(jBoostConfig, getZeroCopySwitch));
}

inline bool GetCacheFilterAndIndexSwitch(JNIEnv *env, jclass boostConfigClass, jobject jBoostConfig)
{
    jmethodID getCacheFilterAndIndexSwitch = env->GetMethodID(boostConfigClass, "getCacheFilterAndIndexSwitch", "()Z");
    if (getCacheFilterAndIndexSwitch == nullptr) {
        return false;
    }
    return static_cast<bool>(env->CallBooleanMethod(jBoostConfig, getCacheFilterAndIndexSwitch));
}

inline int32_t GetTaskSlotFlag(JNIEnv *env, jclass boostConfigClass, jobject jBoostConfig)
{
    jmethodID getTaskSlotFlag = env->GetMethodID(boostConfigClass, "getTaskSlotFlag", "()I");
    if (getTaskSlotFlag == nullptr) {
        return 0;
    }
    return static_cast<int32_t>(env->CallIntMethod(jBoostConfig, getTaskSlotFlag));
}

inline bool GetEnableLocalRecovery(JNIEnv *env, jclass boostConfigClass, jobject jBoostConfig)
{
    jmethodID isEnableLocalRecovery = env->GetMethodID(boostConfigClass, "isEnableLocalRecovery", "()Z");
    if (isEnableLocalRecovery == nullptr) {
        return false;
    }
    return static_cast<bool>(env->CallBooleanMethod(jBoostConfig, isEnableLocalRecovery));
}

inline bool GetEnableKVSeparate(JNIEnv *env, jclass boostConfigClass, jobject jBoostConfig)
{
    jmethodID getKVSeparateSwitch = env->GetMethodID(boostConfigClass, "isKVSeparate", "()Z");
    if (getKVSeparateSwitch == nullptr) {
        return false;
    }
    return static_cast<bool>(env->CallBooleanMethod(jBoostConfig, getKVSeparateSwitch));
}

inline int32_t GetKVSeparateThreshold(JNIEnv *env, jclass boostConfigClass, jobject jBoostConfig)
{
    jmethodID getKVSeparateThreshold = env->GetMethodID(boostConfigClass, "getKVSeparateThreshold", "()I");
    if (getKVSeparateThreshold == nullptr) {
        return 0;
    }
    return static_cast<int32_t>(env->CallIntMethod(jBoostConfig, getKVSeparateThreshold));
}

inline bool CheckNumInValidRange(float arg)
{
    if (UNLIKELY(arg < 0 || arg > 1)) {
        return false;
    }
    return true;
}

inline bool CheckPathValid(const std::string &inputPath, bool allowPathNotExist = false, bool isDirectory = false)
{
    // 检查非空
    if (UNLIKELY(inputPath.empty())) {
        LOG_ERROR("InputPath is null");
        return false;
    }
    std::string path = inputPath;
    // 去掉 'file://' 文件头
    std::string filterPrefixeStr = "file://";
    std::size_t filterSize = filterPrefixeStr.size();
    if (path.size() > filterSize && strncmp(path.c_str(), filterPrefixeStr.c_str(), filterSize) == 0) {
        path = path.substr(filterSize);
    }

    if (UNLIKELY(path == ".." || path.rfind("../", 0) == 0 || path.find("/../") != std::string::npos ||
                 (path.size() >= 3 && path.compare(path.size() - 3, 3, "/..") == 0))) {
        LOG_ERROR("InputPath contains parent path traversal");
        return false;
    }

    // 检查真实路径
    auto realPath = realpath(path.c_str(), nullptr);
    if (UNLIKELY(realPath == nullptr)) {
        int errorCode = errno;
        if (allowPathNotExist && LIKELY(errorCode == ENOENT)) {
            return true;
        }
        LOG_ERROR("Path transform realpath failed, path: " << PathTransform::ExtractFileName(path)
                                                           << ", error: " << strerror(errorCode));
        return false;
    }

    if (UNLIKELY(std::string(realPath).size() > PATH_MAX)) {
        LOG_ERROR("InputPath size too big, size: " << std::string(realPath).size() << ", limit: " << PATH_MAX
                                                   << ", path: " << PathTransform::ExtractFileName(inputPath));
        free(realPath);
        return false;
    }
    if (access(realPath, R_OK) != 0) {  // the file is not readable
        LOG_ERROR("InputPath is not readable, path: " << PathTransform::ExtractFileName(inputPath));
        free(realPath);
        return false;
    }

    struct stat s {
    };
    // The path has been checked before, it is readable and exists.
    if (lstat(realPath, &s) != 0) {
        LOG_ERROR("Failed to get the inputPath stat, path: " << PathTransform::ExtractFileName(inputPath));
        free(realPath);
        return false;
    }
    if (isDirectory && !S_ISDIR(s.st_mode)) {
        LOG_ERROR("InputPath is not directory, path: " << PathTransform::ExtractFileName(inputPath));
        free(realPath);
        return false;
    }

    free(realPath);
    return true;
}

inline std::string ConstructPath(JNIEnv *env, jstring jPath)
{
    const char *rPath = env->GetStringUTFChars(jPath, nullptr);
    if (UNLIKELY(rPath == nullptr)) {
        return "";
    }
    std::string path(rPath);
    env->ReleaseStringUTFChars(jPath, rPath);
    return path;
}

inline int32_t GetPeakFilterElemNum(JNIEnv *env, jclass boostConfigClass, jobject jBoostConfig)
{
    jmethodID getPeakFilterElemNum = env->GetMethodID(boostConfigClass, "peakFilterElemNum", "()I");
    if (getPeakFilterElemNum == nullptr) {
        return false;
    }
    return static_cast<int32_t>(env->CallIntMethod(jBoostConfig, getPeakFilterElemNum));
}

inline ConfigRef CreateConfig(JNIEnv *env, jobject jBoostConfig)
{
    jclass boostConfigClass = env->GetObjectClass(jBoostConfig);
    if (boostConfigClass == nullptr) {
        LOG_ERROR("Get boostConfigClass failed.");
        return nullptr;
    }

    // 设置所有配置项.
    ConfigRef config = std::make_shared<Config>();
    // 获取最大并行度
    auto maxParallelism = GetMaxParallelism(env, boostConfigClass, jBoostConfig);
    if (UNLIKELY(maxParallelism == 0 || maxParallelism > MAX_PARALLELISM)) {
        env->DeleteLocalRef(boostConfigClass);
        LOG_ERROR("Get configuration item maxParallelism failed.");
        return nullptr;
    }
    LOG_INFO("Parse configuration item maxParallelism:" << maxParallelism);

    // 获取KeyGroupRange
    auto keyGroupRange = GetKeyGroupRange(env, boostConfigClass, jBoostConfig);
    if (keyGroupRange == std::pair<uint32_t, uint32_t>{ INVALID_U32, INVALID_U32 } ||
        keyGroupRange.first > maxParallelism || keyGroupRange.second > maxParallelism ||
        (keyGroupRange.second - keyGroupRange.first + NO_1) > maxParallelism) {
        env->DeleteLocalRef(boostConfigClass);
        LOG_ERROR("Get configuration item KeyGroupRange failed.");
        return nullptr;
    }
    LOG_INFO("Parse configuration item keyGroupRange:" << keyGroupRange.first << "-" << keyGroupRange.second);

    // 获取localPath
    auto localPath = GetStringFromJava(env, boostConfigClass, jBoostConfig, "getInstanceBasePath",
                                       "()Ljava/lang/String;");
    if (UNLIKELY(!CheckPathValid(localPath))) {
        env->DeleteLocalRef(boostConfigClass);
        LOG_ERROR("Get configuration item localPath failed. localPath: " << PathTransform::ExtractFileName(localPath));
        return nullptr;
    }
    LOG_INFO("Parse configuration item localPath:" << PathTransform::ExtractFileName(localPath));

    // 获取backendUID
    auto backendUID = GetBackendUID(env, boostConfigClass, jBoostConfig);
    if (backendUID.empty()) {
        env->DeleteLocalRef(boostConfigClass);
        LOG_ERROR("Get configuration item backendUID failed.");
        return nullptr;
    }
    LOG_INFO("Parse configuration item backendUID:" << backendUID);

    // 获取DBSize, 应当设置最小值
    auto dbSize = GetDBSize(env, boostConfigClass, jBoostConfig);
    if (dbSize < IO_SIZE_128M) {
        env->DeleteLocalRef(boostConfigClass);
        LOG_ERROR("Get configuration item dbSize failed, dbSize: " << dbSize);
        return nullptr;
    }
    LOG_INFO("Parse configuration item totalDBSize:" << dbSize);

    auto heapAvailableSize = GetBorrowHeapSize(env, boostConfigClass, jBoostConfig);
    heapAvailableSize = heapAvailableSize > IO_SIZE_8G ? IO_SIZE_8G : heapAvailableSize;
    LOG_INFO("Parse configuration item heapAvailableSize:" << heapAvailableSize);
    auto sliceMemWaterMark = GetSliceMemWaterMark(env, boostConfigClass, jBoostConfig);
    LOG_INFO("Parse configuration item sliceMemWaterMark:" << sliceMemWaterMark);
    auto fileMemoryRatio = GetFileMemoryRatio(env, boostConfigClass, jBoostConfig);
    if (UNLIKELY(!CheckNumInValidRange(fileMemoryRatio) || !CheckNumInValidRange(sliceMemWaterMark))) {
        env->DeleteLocalRef(boostConfigClass);
        LOG_ERROR("fileMemoryRatio or sliceMemWaterMark is invalid.");
        return nullptr;
    }
    LOG_INFO("Parse configuration item fileMemoryRatio:" << fileMemoryRatio);
    auto peakFilterElemNum = GetPeakFilterElemNum(env, boostConfigClass, jBoostConfig);
    LOG_INFO("Parse configuration peak filter elements num:" << peakFilterElemNum);

    // 获取lsmStoreCompactionSwitch
    auto compactionSwitch = GetLsmStoreCompactionSwitch(env, boostConfigClass, jBoostConfig);
    if (compactionSwitch != 0 && compactionSwitch != 1) {
        env->DeleteLocalRef(boostConfigClass);
        LOG_ERROR("Get configuration item lsmStoreCompactionSwitch failed.");
        return nullptr;
    }
    LOG_INFO("Parse configuration item lsmStoreCompactionSwitch:" << compactionSwitch);

    // 获取ttlFilterSwitch
    auto ttlFilterSwitch = GetTtlFilterSwitch(env, boostConfigClass, jBoostConfig);
    LOG_INFO("Parse configuration item ttlFilterSwitch:" << ttlFilterSwitch);

    auto zeroCopySwitch = GetZeroCopySwitch(env, boostConfigClass, jBoostConfig);
    LOG_INFO("Parse configuration item zeroCopySwitch:" << zeroCopySwitch);

    // 获取cacheFilterAndIndexSwitch
    auto cacheFilterAndIndexSwitch = GetCacheFilterAndIndexSwitch(env, boostConfigClass, jBoostConfig);
    LOG_INFO("Parse configuration item cacheFilterAndIndexSwitch:" << cacheFilterAndIndexSwitch);

    // 获取cacheIndexAndFilterRatio
    auto cacheIndexAndFilterRatio = GetCacheFilterAndIndexRatio(env, boostConfigClass, jBoostConfig);
    if (UNLIKELY(!CheckNumInValidRange(cacheIndexAndFilterRatio))) {
        env->DeleteLocalRef(boostConfigClass);
        LOG_ERROR("cacheIndexAndFilterRatio is invalid.");
        return nullptr;
    }
    LOG_INFO("Parse configuration item cacheIndexAndFilterRatio:" << cacheIndexAndFilterRatio);

    // 获取lsmStoreCompressionPolicy
    auto lsmStoreCompressionPolicy = GetStringFromJava(env, boostConfigClass, jBoostConfig,
                                                       "getLsmStoreCompressionPolicy", "()Ljava/lang/String;");
    if (lsmStoreCompressionPolicy.empty()) {
        env->DeleteLocalRef(boostConfigClass);
        LOG_ERROR("Get configuration item lsmStoreCompressionPolicy failed.");
        return nullptr;
    }
    LOG_INFO("Parse configuration item lsmStoreCompressionPolicy:" << lsmStoreCompressionPolicy);

    // 获取lsmStoreCompressionLevelPolicy
    auto lsmStoreCompressionLevelPolicy = GetStringFromJava(env, boostConfigClass, jBoostConfig,
                                                            "getLsmStoreCompressionLevelPolicy",
                                                            "()Ljava/lang/String;");
    if (lsmStoreCompressionLevelPolicy.empty()) {
        env->DeleteLocalRef(boostConfigClass);
        LOG_ERROR("Get configuration item lsmStoreCompressionLevelPolicy failed.");
        return nullptr;
    }
    std::vector<std::string> compressionLevel;
    StringUtil::SplitStringToVector(lsmStoreCompressionLevelPolicy, compressionLevel, config->GetFileStoreNumLevels());
    LOG_INFO("Parse configuration item lsmStoreCompressionLevelPolicy:"
             << StringUtil::MergeVectorToString(compressionLevel, config->GetFileStoreNumLevels()) << ".");

    auto freshTableSnapshotCompressionPolicy = GetStringFromJava(env, boostConfigClass, jBoostConfig,
                                                                 "getFreshTableSnapshotCompressionPolicy",
                                                                 "()Ljava/lang/String;");
    if (freshTableSnapshotCompressionPolicy.empty()) {
        env->DeleteLocalRef(boostConfigClass);
        LOG_ERROR("Get configuration item freshTableSnapshotCompressionPolicy failed.");
        return nullptr;
    }
    LOG_INFO("Parse configuration item freshTableSnapshotCompressionPolicy:" << freshTableSnapshotCompressionPolicy);

    auto sliceTableSnapshotCompressionPolicy = GetStringFromJava(env, boostConfigClass, jBoostConfig,
                                                                 "getSliceTableSnapshotCompressionPolicy",
                                                                 "()Ljava/lang/String;");
    if (sliceTableSnapshotCompressionPolicy.empty()) {
        env->DeleteLocalRef(boostConfigClass);
        LOG_ERROR("Get configuration item sliceTableSnapshotCompressionPolicy failed.");
        return nullptr;
    }
    LOG_INFO("Parse configuration item sliceTableSnapshotCompressionPolicy:" << sliceTableSnapshotCompressionPolicy);

    auto taskSlotFlag = GetTaskSlotFlag(env, boostConfigClass, jBoostConfig);
    if (UNLIKELY(taskSlotFlag < 0)) {
        env->DeleteLocalRef(boostConfigClass);
        LOG_ERROR("taskSlotFlag is less than zero.");
        return nullptr;
    }
    LOG_INFO("Parse configuration item taskSlotFlag:" << taskSlotFlag);
    auto isEnableKVSeparate = GetEnableKVSeparate(env, boostConfigClass, jBoostConfig);
    LOG_INFO("Parse configuration item kvSeparateSwitch:" << isEnableKVSeparate);

    auto kVSeparateThreshold = GetKVSeparateThreshold(env, boostConfigClass, jBoostConfig);
    LOG_INFO("Parse configuration item kVSeparateThreshold:" << kVSeparateThreshold);

    bool isEnableLocalRecovery = GetEnableLocalRecovery(env, boostConfigClass, jBoostConfig);
    std::string backupPath = "";
    if (isEnableLocalRecovery) {
        // 获取备份.slice文件的backupPath
        backupPath = GetStringFromJava(env, boostConfigClass, jBoostConfig, "getSnapshotBackupDir",
                                       "()Ljava/lang/String;");
        if (UNLIKELY(!CheckPathValid(backupPath))) {
            env->DeleteLocalRef(boostConfigClass);
            LOG_ERROR(
                "Get configuration item backupPath failed. backupPath: " << PathTransform::ExtractFileName(backupPath));
            return nullptr;
        }
        LOG_INFO("Parse configuration item backupPath:" << PathTransform::ExtractFileName(backupPath));
    }

    env->DeleteLocalRef(boostConfigClass);

    config->Init(keyGroupRange.first, keyGroupRange.second, maxParallelism);
    config->SetBackendUID(backendUID);
    config->SetLocalPath(localPath);
    config->SetTotalMemHighMarkRatio(sliceMemWaterMark);
    config->SetTotalDBSize(dbSize);
    config->SetLsmStoreCompactionSwitch(compactionSwitch);
    config->SetTtlFilterSwitch(ttlFilterSwitch);
    config->SetZeroCopySwitch(zeroCopySwitch);
    config->SetCacheIndexAndFilterSwitch(cacheFilterAndIndexSwitch);
    config->SetCacheIndexAndFilterRatio(cacheIndexAndFilterRatio);
    config->SetLsmStoreCompressionPolicy(lsmStoreCompressionPolicy);
    config->SetCompressionLevelPolicy(compressionLevel);
    config->SetFreshTableSnapshotCompressionPolicy(freshTableSnapshotCompressionPolicy);
    config->SetSliceTableSnapshotCompressionPolicy(sliceTableSnapshotCompressionPolicy);
    config->SetFileMemoryRatio(fileMemoryRatio);
    config->SetHeapAvailableSize(heapAvailableSize);
    config->SetTaskSlotFlag(taskSlotFlag);
    config->SetPeakFilterElemNum(peakFilterElemNum);
    config->SetEnableLocalRecovery(isEnableLocalRecovery);
    config->SetBackupPath(backupPath);
    config->SetEnableKVSeparate(isEnableKVSeparate);
    config->SetBlobValueSizeThreshold(kVSeparateThreshold);
    return config;
}

inline jlong CreateJavaByteArrayWithSizeCheck(JNIEnv *env, const SerializedDataWrapper *data)
{
    return reinterpret_cast<jlong>(data);
}

inline jobject ConvertStateType(JNIEnv *env, StateType stateType)
{
    // 获取 Java 枚举类的引用
    if (UNLIKELY(stateTypeClass == nullptr)) {
        jclass localStateTypeClass = env->FindClass("com/huawei/ock/bss/common/BoostStateType");
        if (localStateTypeClass == nullptr) {
            env->ExceptionClear();
            LOG_ERROR("Failed to FindClass for stateTypeClass");
            return nullptr;  // 如果类未找到，返回空对象
        }
        stateTypeClass = (jclass)env->NewGlobalRef(localStateTypeClass);
        env->DeleteLocalRef(localStateTypeClass);
        if (UNLIKELY(stateTypeClass == nullptr)) {
            LOG_ERROR("Failed to NewGlobalRef for stateTypeClass");
            return nullptr;
        }
    }

    // 获取 BoostStateType.of(int) 方法的 ID
    if (UNLIKELY(stateTypeOfMethod == nullptr)) {
        stateTypeOfMethod = env->GetStaticMethodID(stateTypeClass, "of",
                                                   "(I)Lcom/huawei/ock/bss/common/BoostStateType;");
    }

    if (UNLIKELY(stateTypeOfMethod == nullptr)) {
        LOG_ERROR("Failed to GetStaticMethodID for stateTypeOfMethod");
        env->ExceptionClear();
        return nullptr;  // 如果方法未找到，返回空对象
    }

    // 调用 BoostStateType.of(int) 方法创建 Java 枚举对象
    jobject stateTypeObj = env->CallStaticObjectMethod(stateTypeClass, stateTypeOfMethod, static_cast<jint>(stateType));
    if (UNLIKELY(stateTypeObj == nullptr)) {
        LOG_ERROR("Failed to CallStaticObjectMethod for stateTypeObj");
        env->ExceptionClear();
        return nullptr;  // 如果枚举对象创建失败，返回空对象
    }

    return stateTypeObj;
}

inline bool SetStateType(JNIEnv *env, jobject javaItem, jclass clazz, const BinaryKeyValueItemRef &cppItem)
{
    if (UNLIKELY(setStateTypeMethod == nullptr)) {
        setStateTypeMethod = env->GetMethodID(clazz, "setStateType", "(Lcom/huawei/ock/bss/common/BoostStateType;)V");
    }

    if (UNLIKELY(setStateTypeMethod == nullptr)) {
        env->ExceptionClear();
        LOG_ERROR("Failed to get method ID for setStateType");
        return false;
    }
    jobject stateTypeObj = ConvertStateType(env, cppItem->mStateType);
    if (UNLIKELY(stateTypeObj == nullptr)) {
        env->ExceptionClear();
        LOG_ERROR("Failed to get object for StateType");
        return false;
    }
    env->CallVoidMethod(javaItem, setStateTypeMethod, stateTypeObj);
    env->DeleteLocalRef(stateTypeObj);
    return true;
}

inline bool SetStateName(JNIEnv *env, jobject javaItem, jclass clazz, const BinaryKeyValueItemRef &cppItem)
{
    if (UNLIKELY(setStateNameMethod == nullptr)) {
        setStateNameMethod = env->GetMethodID(clazz, "setStateName", "(Ljava/lang/String;)V");
    }

    if (UNLIKELY(setStateNameMethod == nullptr)) {
        env->ExceptionClear();
        LOG_ERROR("Failed to get method ID for setStateName");
        return false;
    }
    jstring stateNameStr = env->NewStringUTF(cppItem->mStateName.c_str());
    if (UNLIKELY(stateNameStr == nullptr)) {
        LOG_ERROR("Failed to create stateName string");
        return false;
    }
    env->CallVoidMethod(javaItem, setStateNameMethod, stateNameStr);
    env->DeleteLocalRef(stateNameStr);
    return true;
}

inline bool SetKeyGroup(JNIEnv *env, jobject javaItem, jclass clazz, const BinaryKeyValueItemRef &cppItem)
{
    if (UNLIKELY(setKeyGroupMethod == nullptr)) {
        setKeyGroupMethod = env->GetMethodID(clazz, "setKeyGroup", "(I)V");
    }

    if (UNLIKELY(setKeyGroupMethod == nullptr)) {
        env->ExceptionClear();
        LOG_ERROR("Failed to get method ID for setKeyGroup");
        return false;
    }
    env->CallVoidMethod(javaItem, setKeyGroupMethod, static_cast<jint>(cppItem->mKeyGroup));
    return true;
}

inline void SetKey(JNIEnv *env, jobject javaItem, jclass clazz, const BinaryKeyValueItemRef &cppItem)
{
    if (UNLIKELY(keyFiled == nullptr)) {
        keyFiled = env->GetFieldID(clazz, "key", "J");
    }
    if (UNLIKELY(keyLenFiled == nullptr)) {
        keyLenFiled = env->GetFieldID(clazz, "keyLen", "I");
    }
    env->SetLongField(javaItem, keyFiled, reinterpret_cast<jlong>(cppItem->mKey));
    env->SetIntField(javaItem, keyLenFiled, cppItem->mKeyLength);
}

inline void SetNamespace(JNIEnv *env, jobject javaItem, jclass clazz, const BinaryKeyValueItemRef &cppItem)
{
    if (cppItem->mNs != nullptr) {
        if (UNLIKELY(namespaceFiled == nullptr)) {
            namespaceFiled = env->GetFieldID(clazz, "namespace", "J");
        }
        if (UNLIKELY(namespaceLenFiled == nullptr)) {
            namespaceLenFiled = env->GetFieldID(clazz, "namespaceLen", "I");
        }
        env->SetLongField(javaItem, namespaceFiled, reinterpret_cast<jlong>(cppItem->mNs));
        env->SetIntField(javaItem, namespaceLenFiled, cppItem->mNsLength);
    }
}

inline void SetMapKey(JNIEnv *env, jobject javaItem, jclass clazz, const BinaryKeyValueItemRef &cppItem)
{
    if (cppItem->mMapKey != nullptr) {
        if (UNLIKELY(mapKeyFiled == nullptr)) {
            mapKeyFiled = env->GetFieldID(clazz, "mapKey", "J");
        }
        if (UNLIKELY(mapKeyLenFiled == nullptr)) {
            mapKeyLenFiled = env->GetFieldID(clazz, "mapKeyLen", "I");
        }
        env->SetLongField(javaItem, mapKeyFiled, reinterpret_cast<jlong>(cppItem->mMapKey));
        env->SetIntField(javaItem, mapKeyLenFiled, cppItem->mMapKeyLength);
    }
}

inline void SetValue(JNIEnv *env, jobject javaItem, jclass clazz, const BinaryKeyValueItemRef &cppItem)
{
    if (UNLIKELY(valueFiled == nullptr)) {
        valueFiled = env->GetFieldID(clazz, "value", "J");
    }
    if (UNLIKELY(valueLenFiled == nullptr)) {
        valueLenFiled = env->GetFieldID(clazz, "valueLen", "I");
    }
    env->SetLongField(javaItem, valueFiled, reinterpret_cast<jlong>(cppItem->mValue));
    env->SetIntField(javaItem, valueLenFiled, cppItem->mValueLength);
}

inline jobject ConvertKeyValueItem(JNIEnv *env, jobject object, const BinaryKeyValueItemRef &cppItem)
{
    if (UNLIKELY(cppItem == nullptr)) {
        env->ExceptionClear();
        LOG_ERROR("Failed to get cppItem");
        return nullptr;  // 如果指针为空，返回空对象
    }

    // 1. 获取 Java 类的引用
    if (UNLIKELY(binaryKVItemClass == nullptr)) {
        jclass localBinaryKVItemClass = env->FindClass("com/huawei/ock/bss/common/BinaryKeyValueItem");
        if (localBinaryKVItemClass == nullptr) {
            env->ExceptionClear();
            LOG_ERROR("Failed to FindClass BinaryKeyValueItem");
            return nullptr;  // 如果类未找到，返回空对象
        }
        binaryKVItemClass = (jclass)env->NewGlobalRef(localBinaryKVItemClass);
        env->DeleteLocalRef(localBinaryKVItemClass);
        if (UNLIKELY(binaryKVItemClass == nullptr)) {
            LOG_ERROR("Failed to NewGlobalRef BinaryKeyValueItem");
            return nullptr;
        }
    }

    jclass clazz = env->GetObjectClass(object);
    jfieldID fieldId = env->GetFieldID(clazz, "current", "Lcom/huawei/ock/bss/common/BinaryKeyValueItem;");
    if (UNLIKELY(fieldId == nullptr)) {
        return nullptr;
    }
    // 2. 获取 Java 对象
    jobject javaItem = env->GetObjectField(object, fieldId);
    if (UNLIKELY(javaItem == nullptr)) {
        env->ExceptionClear();
        LOG_ERROR("Failed to NewObject BinaryKeyValueItem");
        return nullptr;  // 如果对象创建失败，返回空对象
    }

    if (!SetStateType(env, javaItem, binaryKVItemClass, cppItem)) {
        LOG_ERROR("Failed to SetStateType for KeyValueItem");
        return nullptr;
    }
    if (!SetStateName(env, javaItem, binaryKVItemClass, cppItem)) {
        LOG_ERROR("Failed to SetStateName for KeyValueItem");
        return nullptr;
    }
    if (!SetKeyGroup(env, javaItem, binaryKVItemClass, cppItem)) {
        LOG_ERROR("Failed to SetKeyGroup for KeyValueItem");
        return nullptr;
    }
    SetKey(env, javaItem, binaryKVItemClass, cppItem);
    SetNamespace(env, javaItem, binaryKVItemClass, cppItem);
    SetMapKey(env, javaItem, binaryKVItemClass, cppItem);
    SetValue(env, javaItem, binaryKVItemClass, cppItem);

    // 6. 返回 Java 对象
    return javaItem;
}

}  // namespace bss
}  // namespace ock
#endif  // BOOST_SS_KV_HELPER_H
