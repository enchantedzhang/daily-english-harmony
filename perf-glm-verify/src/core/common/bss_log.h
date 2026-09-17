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

#ifndef BOOST_STATE_STORE_LOG_H
#define BOOST_STATE_STORE_LOG_H

#include <sys/time.h>

#include <cstdint>
#include <cstring>
#include <sstream>
#include <utility>

#include "common/bss_def.h"
#include "common/util/timestamp_util.h"
#include "spdlog/common.h"
#include "spdlog/spdlog.h"

namespace ock {
namespace bss {
#ifndef BSS_LOG_FILENAME
#define BSS_LOG_FILENAME (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)
#endif

#define SPDLOG_LEVEL_DEBUG 1
#define SPDLOG_LEVEL_INFO 2
#define SPDLOG_LEVEL_WARN 3
#define SPDLOG_LEVEL_ERROR 4
#define SPDLOG_LEVEL_CRITICAL 5
#define SPDLOG_LEVEL_OFF 6

#ifdef DEBUG_UT
#define BSS_LOG_RESET_LEVEL(level)
#define BSS_LOG_INTERNAL(level, file, line, func, msg)
#else
#define BSS_LOG_RESET_LEVEL(level)                         \
    do {                                                   \
        ock::bss::Logger::gInstance->ResetLogLevel(level); \
    } while (0)

/* for default logger */
#define BSS_LOG_INTERNAL(level, file, line, func, msg)                             \
    do {                                                                           \
        if (ock::bss::Logger::gInstance != nullptr &&                              \
            ock::bss::Logger::gInstance->IsHigherLevel(static_cast<int>(level))) { \
            std::ostringstream oss;                                                \
            oss.str("");                                                           \
            oss.clear();                                                           \
            oss << "[" << file << ":" << line << "]"                               \
                << "[" << func << "] " << msg;                                     \
            ock::bss::Logger::gInstance->Log(level, oss.str());                    \
        }                                                                          \
    } while (0)
#endif

#define LOG_CRITICAL(msg) BSS_LOG_INTERNAL(SPDLOG_LEVEL_CRITICAL, BIO_LOG_FILENAME, __LINE__, __FUNCTION__, msg)
#define LOG_ERROR(msg) BSS_LOG_INTERNAL(SPDLOG_LEVEL_ERROR, BSS_LOG_FILENAME, __LINE__, __FUNCTION__, msg)
#define LOG_WARN(msg) BSS_LOG_INTERNAL(SPDLOG_LEVEL_WARN, BSS_LOG_FILENAME, __LINE__, __FUNCTION__, msg)
#define LOG_INFO(msg) BSS_LOG_INTERNAL(SPDLOG_LEVEL_INFO, BSS_LOG_FILENAME, __LINE__, __FUNCTION__, msg)
#define LOG_DEBUG(msg) BSS_LOG_INTERNAL(SPDLOG_LEVEL_DEBUG, BSS_LOG_FILENAME, __LINE__, __FUNCTION__, msg)

#define BSS_LOG_BURST (1)
#define BSS_LOG_INTERV (1000)

constexpr int MIN_LOG_LEVEL_MAX = 5;
static bool LogFrequencyLimit(uint16_t interv, uint16_t burst)
{
    static uint64_t tokensMax = (burst) * (interv);
    static uint64_t tokensCnt = (burst) * (interv);
    static uint32_t missCnt = 0;
    static uint64_t lastTime = 0;
    uint64_t nowTime = TimeStampUtil::GetCurrentTime();
    tokensCnt += nowTime - lastTime;
    tokensCnt = tokensCnt < tokensMax ? tokensCnt : tokensMax;

    bool can;
    if (tokensCnt >= (interv)) {
        tokensCnt -= (interv);
        missCnt = 0;
        can = true;
    } else {
        missCnt++;
        can = false;
    }
    lastTime = nowTime;
    return can;
}

#ifdef DEBUG_UT
#define BSS_LOG_LIMIT_INTERNAL(level, file, line, func, msg)
#else
/* for limit logger */
#define BSS_LOG_LIMIT_INTERNAL(level, file, line, func, msg)                       \
    do {                                                                           \
        bool notLimit = LogFrequencyLimit(BSS_LOG_INTERV, BSS_LOG_BURST);          \
        if (ock::bss::Logger::gInstance != nullptr && notLimit &&                  \
            ock::bss::Logger::gInstance->IsHigherLevel(static_cast<int>(level))) { \
            std::ostringstream oss;                                                \
            oss.str("");                                                           \
            oss.clear();                                                           \
            oss << "[" << file << ":" << line << "]"                               \
                << "[" << func << "] " << msg;                                     \
            ock::bss::Logger::gInstance->Log(level, oss.str());                    \
        }                                                                          \
    } while (0)
#endif

#define LOG_LIMIT_ERROR(msg) BSS_LOG_LIMIT_INTERNAL(SPDLOG_LEVEL_ERROR, BSS_LOG_FILENAME, __LINE__, __FUNCTION__, msg)
#define LOG_LIMIT_WARN(msg) BSS_LOG_LIMIT_INTERNAL(SPDLOG_LEVEL_WARN, BSS_LOG_FILENAME, __LINE__, __FUNCTION__, msg)
#define LOG_LIMIT_INFO(msg) BSS_LOG_LIMIT_INTERNAL(SPDLOG_LEVEL_INFO, BSS_LOG_FILENAME, __LINE__, __FUNCTION__, msg)
#define LOG_LIMIT_DEBUG(msg) BSS_LOG_LIMIT_INTERNAL(SPDLOG_LEVEL_DEBUG, BSS_LOG_FILENAME, __LINE__, __FUNCTION__, msg)

struct LoggerOptions {
    uint8_t logType = 0;
    int32_t minLogLevel = 0;
    uint32_t rotationFileSizeInMB = 50;
    uint32_t rotationFileCount = 20;
    std::string path;

    LoggerOptions(uint8_t logType, int32_t minLogLevel, uint32_t rotationFileSizeInMB, uint32_t rotationFileCount,
                  const std::string &path)
        : logType(logType),
          minLogLevel(minLogLevel),
          rotationFileSizeInMB(rotationFileSizeInMB),
          rotationFileCount(rotationFileCount),
          path(path)
    {
    }
};

class Logger {
public:
    static Logger *Instance(const LoggerOptions &options);
    static void Destroy();

    explicit Logger(LoggerOptions options) : mOptions(std::move(options))
    {
    }

    int32_t Init();
    void Exit();

    int32_t Log(int level, const std::string &message) const;

    void ResetLogLevel(int32_t logLevel);

    inline bool IsHigherLevel(int nowLevel) const
    {
        return nowLevel >= mOptions.minLogLevel;
    }

    static Logger *gInstance;

private:
    static bool ValidateParams(const LoggerOptions &options);
    static void LogToStdErr(const std::ostringstream &oss);

private:
    std::shared_ptr<spdlog::logger> mSpdLogger; /* spd logger for normal log */
    LoggerOptions mOptions;

private:
    static std::mutex gMutex;
    static bool gInited;
};

#define RETURN_NOT_OK(result)                                          \
    do {                                                               \
        BResult _ret = (result);                                       \
        if (UNLIKELY(_ret != BSS_OK)) {                                \
            LOG_ERROR("Result is not ok, error code:" << _ret << "."); \
            return _ret;                                               \
        }                                                              \
    } while (0)

#define RETURN_NOT_OK_WITH_WARN_LOG(result)                     \
    do {                                                        \
        BResult _ret = (result);                                \
        if (UNLIKELY(_ret != BSS_OK)) {                         \
            LOG_WARN("Result is not ok, code:" << _ret << "."); \
            return _ret;                                        \
        }                                                       \
    } while (0)

#define RETURN_NOT_OK_AS_FALSE(result, ret)                                     \
    do {                                                                        \
        BResult _ret = (ret);                                                   \
        if (UNLIKELY(result)) {                                                 \
            LOG_ERROR("Doesn't meet expected conditions, error code:" << _ret); \
            return _ret;                                                        \
        }                                                                       \
    } while (0)

#define RETURN_NULLPTR_AS_FALSE(result, msg)                                     \
    do {                                                                         \
        if (UNLIKELY(result)) {                                                  \
            LOG_ERROR("Doesn't meet expected conditions, error msg: " << (msg)); \
            return nullptr;                                                      \
        }                                                                        \
    } while (0)

#define RETURN_NULLPTR_AS_NOT_OK(result)                        \
    do {                                                        \
        BResult _ret = (result);                                \
        if (UNLIKELY(_ret != BSS_OK)) {                         \
            LOG_ERROR("Result is not ok, error code:" << _ret); \
            return nullptr;                                     \
        }                                                       \
    } while (0)

#define RETURN_NULLPTR_AS_NULLPTR(ptr)                   \
    if (UNLIKELY((ptr) == nullptr)) {                    \
        LOG_ERROR("Error: return nullptr for " << #ptr); \
        return nullptr;                                  \
    }

#define RETURN_NULLPTR_AS_NULLPTR_WARN(ptr)      \
    if (UNLIKELY((ptr) == nullptr)) {            \
        LOG_WARN("Return nullptr for " << #ptr); \
        return nullptr;                          \
    }

#define RETURN_NULLPTR_AS_NULLPTR_NO_LOG(ptr) \
    if (UNLIKELY((ptr) == nullptr)) {         \
        return nullptr;                       \
    }

#define RETURN_ALLOC_FAIL_AS_NULLPTR(ptr)                          \
    if (UNLIKELY((ptr) == nullptr)) {                              \
        LOG_WARN("Error: Failed to allocate memory for " << #ptr); \
        return BSS_ALLOC_FAIL;                                     \
    }

#define RETURN_INVALID_PARAM_AS_NULLPTR(ptr)                \
    if (UNLIKELY((ptr) == nullptr)) {                       \
        LOG_ERROR("Error: BSS_INVALID_PARAM for " << #ptr); \
        return BSS_INVALID_PARAM;                           \
    }

#define CONTINUE_LOOP_AS_NULLPTR(ptr)                        \
    if (UNLIKELY((ptr) == nullptr)) {                        \
        LOG_ERROR("Error: In loop, got a nullptr " << #ptr); \
        continue;                                            \
    }

#define RETURN_FALSE_AS_NULLPTR(ptr)                                \
    if (UNLIKELY((ptr) == nullptr)) {                               \
        LOG_ERROR("Error: return false when nullptr for " << #ptr); \
        return false;                                               \
    }

#define RETURN_ERROR_AS_NULLPTR(ptr)                                  \
    if (UNLIKELY((ptr) == nullptr)) {                                 \
        LOG_ERROR("Error: Unexpected nullptr for variable " << #ptr); \
        return BSS_ERR;                                               \
    }

#define RETURN_AS_NULLPTR(ptr)                                        \
    if (UNLIKELY((ptr) == nullptr)) {                                 \
        LOG_ERROR("Error: Unexpected nullptr for variable " << #ptr); \
        return;                                                       \
    }

#define RETURN_NOT_OK_NO_LOG(result)    \
    do {                                \
        BResult _ret = (result);        \
        if (UNLIKELY(_ret != BSS_OK)) { \
            return _ret;                \
        }                               \
    } while (0)

#define RETURN_AS_NOT_OK_NO_LOG(result) \
    do {                                \
        BResult _ret = (result);        \
        if (UNLIKELY(_ret != BSS_OK)) { \
            return;                     \
        }                               \
    } while (0)

#define RETURN_NOT_OK_AS_READ_ERROR(readResult)               \
    do {                                                      \
        BResult _ret = (readResult);                          \
        if (UNLIKELY((_ret) != BSS_OK)) {                     \
            LOG_ERROR("Read file failed, result:" << (_ret)); \
            return _ret;                                      \
        }                                                     \
    } while (0)

#define RETURN_AS_READ_ERROR(readResult)                      \
    do {                                                      \
        BResult _ret = (readResult);                          \
        if (UNLIKELY((_ret) != BSS_OK)) {                     \
            LOG_ERROR("Read file failed, result:" << (_ret)); \
            return;                                           \
        }                                                     \
    } while (0)

#define RETURN_NULLPTR_AS_READ_ERROR(readResult)              \
    do {                                                      \
        BResult _ret = (readResult);                          \
        if (UNLIKELY((_ret) != BSS_OK)) {                     \
            LOG_ERROR("Read file failed, result:" << (_ret)); \
            return nullptr;                                   \
        }                                                     \
    } while (0)

#define RETURN_NULLPTR_AS_READ_BUFFER_ERROR(readResult)         \
    do {                                                        \
        BResult _ret = (readResult);                            \
        if (UNLIKELY((_ret) != BSS_OK)) {                       \
            LOG_ERROR("Read buffer failed, result:" << (_ret)); \
            return nullptr;                                     \
        }                                                       \
    } while (0)

#define RETURN_NO_OK_AS_READ_BUFFER_ERROR(readResult)           \
    do {                                                        \
        BResult _ret = (readResult);                            \
        if (UNLIKELY((_ret) != BSS_OK)) {                       \
            LOG_ERROR("Read buffer failed, result:" << (_ret)); \
            return readResult;                                  \
        }                                                       \
    } while (0)

#define RETURN_INNER_ERR_AS_BUFFER_OVER_FLOW(capacity, offset)                                 \
    do {                                                                                       \
        if (UNLIKELY((capacity) < (offset))) {                                                 \
            LOG_ERROR("Buffer over flow, capacity:" << (capacity) << ", offset:" << (offset)); \
            return BSS_INNER_ERR;                                                              \
        }                                                                                      \
    } while (0)

#define HASSERT_LOG(assertttt)                                    \
    if (!(assertttt)) {                                           \
        LOG_ERROR("Failed assert as " + std::string(#assertttt)); \
    }
#define HTRY \
    do {     \
        try

#define HCATCHKEY catch

#define HCATCHEND \
    }             \
    while (0)

#define HCATCH(hr)                                        \
    HCATCHKEY(int32_t hrc)                                \
    {                                                     \
        (hr) = hrc;                                       \
    }                                                     \
    HCATCHKEY(std::exception &ex)                         \
    {                                                     \
        (hr) = BSS_ERR;                                   \
        LOG_ERROR(ex.what());                             \
    }                                                     \
    HCATCHKEY(...)                                        \
    {                                                     \
        (hr) = BSS_ERR;                                   \
        std::string source = std::string(__FILE__) + ":"; \
        source += std::to_string(__LINE__);               \
        LOG_ERROR(source);                                \
    }                                                     \
    HCATCHEND
}  // namespace bss
}  // namespace ock

#endif  // BOOST_STATE_STORE_LOG_H
