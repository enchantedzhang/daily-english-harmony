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

#include "list_state_file_reuse_planner.h"

#include <algorithm>
#include <unordered_set>
#include <vector>

#include "include/bss_types.h"

namespace ock {
namespace bss {
namespace {
bool ContainsKey(const FileMetaDataRef &file, const Key &key)
{
    return file != nullptr && file->GetSmallest() != nullptr && file->GetLargest() != nullptr &&
           file->GetSmallest()->CompareKey(key) <= 0 && file->GetLargest()->CompareKey(key) >= 0;
}

bool IsExactSingleRecordForKey(const FileMetaDataRef &file, const Key &key)
{
    return file != nullptr && file->GetRecordMeta().IsSingleRecord() && file->GetSmallest() != nullptr &&
           file->GetLargest() != nullptr && file->GetSmallest()->EqualsFullKey(file->GetLargest()) &&
           file->GetSmallest()->CompareKey(key) == 0;
}

bool IsLargeListStatePut(const FileMetaDataRef &file)
{
    if (file == nullptr || file->GetSmallest() == nullptr || file->GetLargest() == nullptr) {
        return false;
    }
    const FileRecordMeta &recordMeta = file->GetRecordMeta();
    return recordMeta.IsSingleRecord() && recordMeta.GetSingleValueLength() > IO_SIZE_4M &&
           recordMeta.GetMaxSeqId() == file->GetSmallest()->SeqId() &&
           file->GetSmallest()->ValueType() == ValueType::PUT &&
           file->GetSmallest()->EqualsFullKey(file->GetLargest()) && StateId::IsList(file->GetSmallest()->StateId());
}

bool IsSelected(const FileMetaDataRef &file, const std::unordered_set<std::string> &selectedFiles)
{
    return file != nullptr && selectedFiles.find(file->GetIdentifier()) != selectedFiles.end();
}

bool IsNewestSelectedRecord(const FileMetaDataRef &candidate, const std::unordered_set<std::string> &selectedFiles,
                            const std::vector<FileMetaDataRef> &participatingFiles)
{
    const Key &key = *candidate->GetSmallest();
    uint64_t candidateSeqId = candidate->GetRecordMeta().GetMaxSeqId();
    for (const auto &file : participatingFiles) {
        if (!ContainsKey(file, key)) {
            continue;
        }
        // A file left at either participating level could mask the adopted
        // file after it moves, so adoption requires a complete clean cut.
        if (!IsSelected(file, selectedFiles)) {
            return false;
        }
        if (file->GetIdentifier() == candidate->GetIdentifier()) {
            continue;
        }
        const FileRecordMeta &recordMeta = file->GetRecordMeta();
        if (!recordMeta.IsAvailable() || recordMeta.GetMaxSeqId() >= candidateSeqId) {
            return false;
        }
    }
    return true;
}

FileMetaDataRef FindCandidateForKey(const std::vector<FileMetaDataRef> &candidates, const Key &key)
{
    for (const auto &candidate : candidates) {
        if (candidate->GetSmallest()->CompareKey(key) == 0) {
            return candidate;
        }
    }
    return nullptr;
}

std::vector<FileMetaDataRef> ExcludeFiles(const std::vector<FileMetaDataRef> &files,
                                          const std::unordered_set<std::string> &excludedFiles)
{
    std::vector<FileMetaDataRef> result;
    result.reserve(files.size());
    for (const auto &file : files) {
        if (file != nullptr && excludedFiles.find(file->GetIdentifier()) == excludedFiles.end()) {
            result.emplace_back(file);
        }
    }
    return result;
}

void CollectCandidates(const std::vector<FileMetaDataRef> &files, std::vector<FileMetaDataRef> &candidates)
{
    for (const auto &file : files) {
        if (!IsLargeListStatePut(file)) {
            continue;
        }
        FileMetaDataRef existing = FindCandidateForKey(candidates, *file->GetSmallest());
        if (existing == nullptr) {
            candidates.emplace_back(file);
            continue;
        }
        if (existing->GetRecordMeta().GetMaxSeqId() >= file->GetRecordMeta().GetMaxSeqId()) {
            continue;
        }
        for (auto &candidate : candidates) {
            if (candidate->GetSmallest()->CompareKey(*file->GetSmallest()) == 0) {
                candidate = file;
                break;
            }
        }
    }
}
}  // namespace

void ListStateFileReusePlanner::Build(const CompactionRef &compaction, const AdoptableFilter &adoptableFilter)
{
    if (compaction == nullptr || compaction->GetInputVersion() == nullptr ||
        compaction->GetInputVersion()->GetCompactionReason() != Reason::LEVEL0_NUM_TRIGGERED ||
        compaction->GetInputLevelId() != 0 || compaction->GetOutputLevelId() != compaction->GetInputLevelId() + 1 ||
        compaction->GetOutputLevelId() >= compaction->GetInputVersion()->GetNumLevels()) {
        return;
    }

    // Keep only the newest large PUT candidate for each user key.
    std::vector<FileMetaDataRef> candidates;
    CollectCandidates(compaction->GetLevelInputs(), candidates);
    CollectCandidates(compaction->GetOutputLevelInputs(), candidates);
    if (candidates.empty()) {
        return;
    }

    std::vector<FileMetaDataRef> allInputs = compaction->GetLevelInputs();
    allInputs.insert(allInputs.end(), compaction->GetOutputLevelInputs().begin(),
                     compaction->GetOutputLevelInputs().end());

    std::unordered_set<std::string> selectedFiles;
    selectedFiles.reserve(allInputs.size());
    for (const auto &file : allInputs) {
        if (file != nullptr) {
            selectedFiles.emplace(file->GetIdentifier());
        }
    }

    VersionPtr version = compaction->GetInputVersion();
    std::vector<FileMetaDataRef> participatingFiles = version->GetFileMetaDatas(compaction->GetInputLevelId(),
                                                                                compaction->GetGroupRange());
    std::vector<FileMetaDataRef> outputLevelFiles = version->GetFileMetaDatas(compaction->GetOutputLevelId(),
                                                                              compaction->GetGroupRange());
    participatingFiles.insert(participatingFiles.end(), outputLevelFiles.begin(), outputLevelFiles.end());

    std::vector<FileMetaDataRef> adoptedFiles;
    std::unordered_set<std::string> bypassedFiles;
    for (const auto &candidate : candidates) {
        uint16_t stateId = candidate->GetSmallest()->StateId();
        uint64_t seqId = candidate->GetRecordMeta().GetMaxSeqId();
        if ((adoptableFilter && !adoptableFilter(stateId, seqId)) ||
            !IsNewestSelectedRecord(candidate, selectedFiles, participatingFiles)) {
            continue;
        }

        adoptedFiles.emplace_back(candidate);
        const Key &candidateKey = *candidate->GetSmallest();
        for (const auto &file : allInputs) {
            if (IsExactSingleRecordForKey(file, candidateKey) && file->GetRecordMeta().GetMaxSeqId() <= seqId) {
                bypassedFiles.emplace(file->GetIdentifier());
            }
        }
    }

    if (adoptedFiles.empty()) {
        return;
    }
    std::sort(adoptedFiles.begin(), adoptedFiles.end(), [](const FileMetaDataRef &left, const FileMetaDataRef &right) {
        return left->GetSmallest()->CompareKey(*right->GetSmallest()) < 0;
    });

    compaction->SetListStateFileReusePlan(ExcludeFiles(compaction->GetLevelInputs(), bypassedFiles),
                                          ExcludeFiles(compaction->GetOutputLevelInputs(), bypassedFiles),
                                          adoptedFiles);
    LOG_DEBUG("Build ListState file reuse plan success, adopted files:" << adoptedFiles.size() << ", bypassed files:"
                                                                        << bypassedFiles.size() << ".");
}
}  // namespace bss
}  // namespace ock
