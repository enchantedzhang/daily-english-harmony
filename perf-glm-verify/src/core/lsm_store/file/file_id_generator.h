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

#ifndef BOOST_SS_FILE_ID_GENERRATOR_H
#define BOOST_SS_FILE_ID_GENERRATOR_H

#include <atomic>
#include <memory>
#include <mutex>
#include <vector>

#include "file_info.h"

namespace ock {
namespace bss {
class FileIdRecorder {
public:
    FileIdRecorder() = default;

    ~FileIdRecorder()
    {
        bool expected = true;
        if (isInitialized.compare_exchange_strong(expected, false)) {
            LOG_INFO("clear FileId Recorder.");
            std::vector<bool>().swap(mUsedFileUniqueIDs);
        }
    }

    static inline void Initialize(uint32_t maxAllowedUniqueID)
    {
        bool expected = false;
        if (isInitialized.compare_exchange_strong(expected, true)) {
            LOG_INFO("init FileId Recorder.");
            mUsedFileUniqueIDs.resize(maxAllowedUniqueID, false);
            mMaxAllowedUniqueID = maxAllowedUniqueID;
        }
    }

    static inline bool IsIDRecorded(uint32_t uniqueID)
    {
        if (uniqueID >= mMaxAllowedUniqueID) {
            LOG_ERROR("Invalid id: " << uniqueID);
            return false;
        }
        return mUsedFileUniqueIDs[uniqueID];
    }

    static inline void RecordUniqueID(uint32_t uniqueID)
    {
        if (UNLIKELY(uniqueID >= mMaxAllowedUniqueID)) {
            LOG_ERROR("Invalid id: " << uniqueID);
            return;
        }
        bool isRecorded = mUsedFileUniqueIDs[uniqueID];
        mUsedFileUniqueIDs[uniqueID] = true;
        if (!isRecorded) {
            mCardinality++;
        }
    }

    static inline uint32_t RecordedFileIdNum()
    {
        return mCardinality;
    }

    static inline void RecycleFileId(uint32_t uniqueId)
    {
        if (uniqueId >= mMaxAllowedUniqueID) {
            return;
        }
        bool isRecorded = mUsedFileUniqueIDs[uniqueId];
        if (isRecorded) {
            mUsedFileUniqueIDs[uniqueId] = false;
            mCardinality--;
        }
    }

private:
    static uint32_t mCardinality;
    static uint32_t mMaxAllowedUniqueID;
    static std::atomic<bool> isInitialized;
    static std::vector<bool> mUsedFileUniqueIDs;
};
using FileIdRecorderRef = std::shared_ptr<FileIdRecorder>;

class FileIdGenerator {
public:
    explicit FileIdGenerator(FileModeInfo fileModeInfo)
    {
        mInitValue = fileModeInfo.initValue;
        maxAllowedUniqueID = fileModeInfo.maxAllowedUniqueID;
        mFileIdRecorder->Initialize(maxAllowedUniqueID);
    }

    FileIdRef Generate();

    static inline uint32_t GetMaxAllowUniqueId()
    {
        return maxAllowedUniqueID;
    }

    static inline void Recycle(const FileIdRef &fileId)
    {
        RETURN_AS_NULLPTR(fileId);
        std::lock_guard<std::mutex> lock(mMutex);
        RecycleUniqueId(fileId->GetUniqueId());
    }

    static inline void RecycleUniqueId(uint32_t uniqueId)
    {
        mFileIdRecorder->RecycleFileId(uniqueId);
    }

private:
    static uint32_t mInitValue;
    static uint32_t maxAllowedUniqueID;
    static FileIdRecorderRef mFileIdRecorder;
    static std::atomic<uint32_t> mUniqueId;
    static std::mutex mMutex;
};
using FileIdGeneratorRef = std::shared_ptr<FileIdGenerator>;

}  // namespace bss
}  // namespace ock
#endif  // BOOST_SS_FILE_ID_GENERRATOR_H