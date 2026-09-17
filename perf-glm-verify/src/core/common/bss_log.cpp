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

#include "bss_log.h"

#include <iostream>

#include "spdlog/sinks/rotating_file_sink.h"
#include "spdlog/sinks/stdout_sinks.h"

namespace ock {
namespace bss {
Logger *Logger::gInstance = nullptr;
std::mutex Logger::gMutex;
bool Logger::gInited = false;

const int STDOUT_TYPE = 0;
const int FILE_TYPE = 1;
const int STDERR_TYPE = 2;

constexpr int SIZE_MB_SHIFT = 20;
constexpr auto ROTATION_FILE_SIZE_MAX_MB = 100UL;                                    // 100MB
constexpr auto ROTATION_FILE_SIZE_MAX = ROTATION_FILE_SIZE_MAX_MB << SIZE_MB_SHIFT;  // 100MB
constexpr auto ROTATION_FILE_SIZE_MIN_MB = 2;                                        // 2MB
constexpr auto ROTATION_FILE_SIZE_MIN = ROTATION_FILE_SIZE_MIN_MB << SIZE_MB_SHIFT;  // 2MB
constexpr int ROTATION_FILE_COUNT_MAX = 50;

#define BIO_LOG_STD_ERR(msg)      \
    do {                          \
        std::ostringstream oss;   \
        oss.str("");              \
        oss.clear();              \
        oss << msg;               \
        Logger::LogToStdErr(oss); \
    } while (0)

void Logger::LogToStdErr(const std::ostringstream &oss)
{
    struct timeval tv {
    };
    char strTime[24];

    gettimeofday(&tv, nullptr);
    if (strftime(strTime, sizeof strTime, "%Y-%m-%d %H:%M:%S.", localtime(&tv.tv_sec)) != 0) {
        std::cout << strTime << tv.tv_usec << " info " << syscall(SYS_gettid) << " " << oss.str() << std::endl;
    } else {
        std::cout << " Invalid time info " << syscall(SYS_gettid) << " " << oss.str() << std::endl;
    }
}

bool Logger::ValidateParams(const LoggerOptions &options)
{
    /* for normal log */
    if (options.minLogLevel < 0 || options.minLogLevel > MIN_LOG_LEVEL_MAX) {
        BIO_LOG_STD_ERR("Invalid min log level for logger, which should be 0,1,2,3,4,5");
        return false;
    }

    if (options.logType != FILE_TYPE) {
        return true;
    }

    if (options.path.empty()) {
        BIO_LOG_STD_ERR("Invalid path for logger, which is empty");
        return false;
    }

    if (options.rotationFileSizeInMB > ROTATION_FILE_SIZE_MAX_MB ||
        options.rotationFileSizeInMB < ROTATION_FILE_SIZE_MIN_MB) {
        BIO_LOG_STD_ERR("Invalid max file size for logger, which should be between 2MB to 100MB");
        return false;
    }

    if (options.rotationFileCount > ROTATION_FILE_COUNT_MAX || options.rotationFileCount < 1) {
        BIO_LOG_STD_ERR("Invalid max file count for logger, which should be less than 50");
        return false;
    }

    return true;
}

Logger *Logger::Instance(const LoggerOptions &options)
{
    if (gInstance != nullptr) {
        return gInstance;
    }
    std::lock_guard<std::mutex> guard(gMutex);
    /* already created */
    if (gInstance != nullptr) {
        return gInstance;
    }

    /* create */
    if (!ValidateParams(options)) {
        return nullptr;
    }
    gInstance = new (std::nothrow) Logger(options);
    if (gInstance == nullptr) {
        BIO_LOG_STD_ERR("Failed to new Logger object, probably out of memory");
        return nullptr;
    }

    return gInstance;
}

void Logger::Destroy()
{
    std::lock_guard<std::mutex> guard(gMutex);
    if (gInstance != nullptr) {
        gInstance->Exit();
        delete gInstance;
        gInstance = nullptr;
    }
}

int32_t Logger::Init()
{
    std::lock_guard<std::mutex> guard(gMutex);
    if (gInited) {
        return 0;
    }
    try {
        if (mOptions.logType == STDOUT_TYPE) {  // stdout
            mSpdLogger = spdlog::stdout_logger_mt("console");
            mSpdLogger->set_pattern("%Y-%m-%d %H:%M:%S.%f %t %l %v");
        } else if (mOptions.logType == FILE_TYPE) {  // file
            std::string logName = std::string("ns:0").append(";log:normal");
            mSpdLogger = spdlog::rotating_logger_mt(logName, mOptions.path,
                                                    mOptions.rotationFileSizeInMB << SIZE_MB_SHIFT,
                                                    mOptions.rotationFileCount);
            mSpdLogger->set_pattern("%v");
            mSpdLogger->info("", "");
            mSpdLogger->set_pattern("%Y-%m-%d %H:%M:%S.%f %t %v");
            mSpdLogger->info(
                "Log started at [{}] level",
                spdlog::level::to_string_view(static_cast<spdlog::level::level_enum>(mOptions.minLogLevel)).data());
            mSpdLogger->info("Log default format: yyyy-mm-dd hh:mm:ss.uuuuuu threadid loglevel msg");
            mSpdLogger->set_pattern("%Y-%m-%d %H:%M:%S.%f %t %l %v");
            spdlog::flush_every(std::chrono::seconds(1));
        } else if (mOptions.logType == STDERR_TYPE) {  // stderr
            mSpdLogger = spdlog::stderr_logger_mt("console");
            mSpdLogger->set_pattern("%C/%m/%d %H:%M:%S.%f %t %l %v");
        }
        mSpdLogger->set_level(static_cast<spdlog::level::level_enum>(mOptions.minLogLevel));
        mSpdLogger->flush_on(spdlog::level::err);
    } catch (const spdlog::spdlog_ex &ex) {
        mSpdLogger = nullptr;
        BIO_LOG_STD_ERR("Failed to create log." + std::string(ex.what()));
        return -1L;
    }
    gInited = true;
    return 0;
}

void Logger::Exit()
{
    if (mSpdLogger != nullptr) {
        mSpdLogger->flush();
        mSpdLogger = nullptr;
    }
}

int32_t Logger::Log(int level, const std::string &message) const
{
    if (mSpdLogger == nullptr) {
        return -2L;
    }

    if (level < 0 || level > 5) {  // 5
        return -3L;
    }

    mSpdLogger->log(static_cast<spdlog::level::level_enum>(level), "{}", message);
    return 0L;
}

void Logger::ResetLogLevel(int32_t logLevel)
{
    mOptions.minLogLevel = logLevel;
    if (mSpdLogger != nullptr) {
        mSpdLogger->set_level(static_cast<spdlog::level::level_enum>(mOptions.minLogLevel));
    }
}
}  // namespace bss
}  // namespace ock
