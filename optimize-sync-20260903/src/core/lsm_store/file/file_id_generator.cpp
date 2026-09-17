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

#include "file_id_generator.h"

#include "common/bss_log.h"

namespace ock {
namespace bss {

uint32_t FileIdGenerator::mInitValue = 0;
uint32_t FileIdGenerator::maxAllowedUniqueID = 0;
FileIdRecorderRef FileIdGenerator::mFileIdRecorder;
std::atomic<uint32_t> FileIdGenerator::mUniqueId = { 1 };
std::mutex FileIdGenerator::mMutex;

uint32_t FileIdRecorder::mCardinality = 0;
uint32_t FileIdRecorder::mMaxAllowedUniqueID = 0;
std::atomic<bool> FileIdRecorder::isInitialized{ false };
std::vector<bool> FileIdRecorder::mUsedFileUniqueIDs;

FileIdRef FileIdGenerator::Generate()
{
    std::lock_guard<std::mutex> lock(mMutex);
    if (UNLIKELY(mFileIdRecorder->RecordedFileIdNum() >= GetMaxAllowUniqueId())) {
        LOG_ERROR("The number of unique file ids exceeds the maximum limit.");
        return nullptr;
    }
    uint32_t currentUniqueId = mUniqueId.fetch_add(1);
    while (mFileIdRecorder->IsIDRecorded(currentUniqueId)) {
        currentUniqueId = mUniqueId.fetch_add(1);
        if (currentUniqueId > GetMaxAllowUniqueId()) {
            mUniqueId.store(1);
            currentUniqueId = 1;
        }
    }

    mFileIdRecorder->RecordUniqueID(currentUniqueId);
    FileIdRef fileId = std::make_shared<FileId>();
    BResult result = fileId->Init(mInitValue | currentUniqueId);
    if (result != BSS_OK) {
        LOG_ERROR("Init fileId failed, initValue: " << mInitValue << ", currentUniqueId:" << currentUniqueId);
        return nullptr;
    }
    return fileId;
}

}  // namespace bss
}  // namespace ock