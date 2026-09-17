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

#include "compaction_picker.h"

#include "input_level_compaction.h"

namespace ock {
namespace bss {
std::vector<FileMetaDataRef> CompactionPicker::GetInputFiles(const VersionPtr &current,
                                                             const GroupRangeRef &curGroupRange)
{
    static const double MIN_SCORE = 1.0;
    // 获取当前版本的compaction得分.
    double compactionScore = current->GetCompactionScore();
    if (compactionScore < MIN_SCORE) {
        LOG_ERROR("Invalid compaction score from version, score:" << compactionScore << ".");
        return {};
    }

    // 获取当前版本的compaction级别.
    uint32_t levelId = current->GetCompactionLevel();
    if (levelId + 1 > mConf->GetFileStoreNumLevels() || levelId >= current->GetLevels().size()) {
        LOG_ERROR("Invalid level id form version, levelId:" << levelId << ".");
        return {};
    }

    // 获取当前级别的文件元数据组
    std::vector<FileMetaDataRef> levelInputs;
    if (fabs(compactionScore - DOUBLE_MAX_VALUE) == 0) {
        auto fileMetaDataGroups = (current->GetLevels().at(levelId)).GetFileMetaDataGroups();
        if (current->GetCompactionReason() == Reason::KEY_GROUP_RESCALED ||
            current->GetCompactionReason() == Reason::ORDER_RANGE_REDUNDANT) {
            for (auto &fileMetaDataGroup : fileMetaDataGroups) {
                if (UNLIKELY(fileMetaDataGroup == nullptr)) {
                    LOG_ERROR("File meta group is nullptr, levelId:" << levelId << ".");
                    return {};
                }
                for (auto &file : fileMetaDataGroup->GetFiles()) {
                    if (UNLIKELY(file == nullptr)) {
                        LOG_ERROR("File is nullptr, levelId:" << levelId << ".");
                        return {};
                    }
                    if (!file->GetGroupRange()->Equals(current->GetGroupRange()) ||
                        file->GetOrderRange()->HasRedundantData()) {
                        levelInputs.emplace_back(file);
                    }
                }
            }
        }
        return levelInputs;
    }

    if (levelId + 1 == mConf->GetFileStoreNumLevels()) {
        return levelInputs;
    }

    std::pair<FileMetaDataRef, bool> pair = GetInitialInputFile(current, levelId, curGroupRange);
    if (pair.first == nullptr) {
        return levelInputs;
    }
    levelInputs.emplace_back(pair.first);
    if (pair.second || levelId == 0) {
        return ExpandInputFilesIfNeeded(current, levelId, curGroupRange, levelInputs);
    }

    FilterFileMetaList(levelInputs);
    return levelInputs;
}

CompactionRef CompactionPicker::GenerateCompactionBaseOnInputs(const VersionPtr &current,
                                                               const GroupRangeRef &curGroupRange,
                                                               std::vector<FileMetaDataRef> &levelInputs)
{
    if (current->GetCompactionScore() == DOUBLE_MAX_VALUE) {
        return std::make_shared<InputLevelCompaction>(current, current->GetCompactionLevel(), curGroupRange,
                                                      mConf->GetFileBaseSize(), levelInputs);
    }

    uint32_t levelId = current->GetCompactionLevel();
    std::vector<GroupRangeRef> inputLevelGroupRanges = current->GetLevel(levelId).GetGroupRanges();
    for (const auto &groupRange : inputLevelGroupRanges) {
        ExpandInputsToCleanCut(current->GetFileMetaDatas(levelId, groupRange), levelInputs);
    }

    std::vector<std::vector<FileMetaDataRef>> levelInputs2;
    levelInputs2.emplace_back(levelInputs);
    std::pair<FullKeyRef, FullKeyRef> keyRange = GetKeyRange(levelInputs2);
    FullKeyRef smallest = keyRange.first;
    FullKeyRef largest = keyRange.second;

    std::vector<FileMetaDataRef> outputLevelInputs;
    std::vector<GroupRangeRef> outputLevelGroupRanges = current->GetLevel(levelId + 1).GetGroupRanges();
    for (auto &groupRange : outputLevelGroupRanges) {
        auto overlappingInputs = GetOverlappingInputs(current, levelId + 1, groupRange, smallest, largest);
        for (auto &fileMetaData : overlappingInputs) {
            outputLevelInputs.emplace_back(fileMetaData);
        }
    }
    FilterFileMetaList(outputLevelInputs);

    FullKeyRef allSmallest = smallest;
    FullKeyRef allLargest = largest;
    findAllSmallestAndLargest(current, levelId, inputLevelGroupRanges, levelInputs, largest, outputLevelInputs,
                              allSmallest, allLargest);

    std::vector<FileMetaDataRef> grandparents;
    for (auto &groupRange : inputLevelGroupRanges) {
        if (levelId + NO_2 < mConf->GetFileStoreNumLevels()) {
            auto overlappingInputs = GetOverlappingInputs(current, levelId + NO_2, groupRange, allSmallest, allLargest);
            for (auto &fileMetaData : overlappingInputs) {
                grandparents.emplace_back(fileMetaData);
            }
        }
    }

    CompactionRef compaction = std::make_shared<Compaction>(current, levelId, curGroupRange, mConf->GetFileBaseSize(),
                                                            smallest, largest, levelInputs, outputLevelInputs,
                                                            grandparents);
    uint64_t inputSize = GetTotalFileSize(levelInputs);
    if (UNLIKELY(inputSize == 0)) {
        LOG_WARN("total file size is zero.");
        return compaction;
    }
    uint64_t outputLevelSize = GetTotalFileSize(outputLevelInputs);
    uint64_t grandParentSize = GetTotalFileSize(grandparents);
    double ratio = 1.0 * static_cast<double>(outputLevelSize) / static_cast<double>(inputSize);
    long totalInputLevelSize = current->GetLevel(levelId).TotalFileSize();
    long totalOutputLevelSize = current->GetLevel(levelId + 1).TotalFileSize();
    float inputLevelOverlapRatio = (totalInputLevelSize == 0L) ?
                                       0.0F :
                                       (static_cast<float>(inputSize) / static_cast<float>(totalInputLevelSize));
    float outputLevelOverlapRatio =
        (totalOutputLevelSize == 0L) ? 0.0F :
                                       (static_cast<float>(outputLevelSize) / static_cast<float>(totalOutputLevelSize));
    LOG_INFO("Pick compaction result, level-"
             << levelId << " overlapped ratio: " << ratio << ", input-level: (" << GetFileNames(levelInputs) << " "
             << inputSize << " bytes with ratio " << inputLevelOverlapRatio << "), output-level: ("
             << outputLevelInputs.size() << " files " << outputLevelSize << " bytes bytes with ratio "
             << outputLevelOverlapRatio << "), overlapped grandParentSize: " << grandParentSize << " bytes.");
    return compaction;
}

void CompactionPicker::findAllSmallestAndLargest(const VersionPtr &current, uint32_t levelId,
                                                 std::vector<GroupRangeRef> &inputLevelGroupRanges,
                                                 std::vector<FileMetaDataRef> &levelInputs, FullKeyRef &largest,
                                                 std::vector<FileMetaDataRef> &outputLevelInputs,
                                                 FullKeyRef &allSmallest, FullKeyRef &allLargest)
{
    if (outputLevelInputs.empty()) {
        LOG_INFO("outputLevelInputs is empty.");
        return;
    }

    std::vector<std::vector<FileMetaDataRef>> levelInputs3;
    levelInputs3.emplace_back(levelInputs);
    levelInputs3.emplace_back(outputLevelInputs);
    std::pair<FullKeyRef, FullKeyRef> allKeyRange = GetKeyRange(levelInputs3);
    allSmallest = allKeyRange.first;
    allLargest = allKeyRange.second;

    for (auto &groupRange : inputLevelGroupRanges) {
        auto expandedInputList = GetOverlappingInputs(current, levelId, groupRange, allSmallest, allLargest);
        ExpandInputsToCleanCut(current->GetFileMetaDatas(levelId, groupRange), expandedInputList);
        uint64_t l1 = BasicUtils::TotalFileSize(outputLevelInputs);
        uint64_t expandedInputSize = BasicUtils::TotalFileSize(expandedInputList);
        if (expandedInputList.size() > levelInputs.size() &&
            l1 + expandedInputSize < mConf->GetFileStoreMaxCompactionSize()) {
            std::vector<std::vector<FileMetaDataRef>> levelInputs4;
            levelInputs4.emplace_back(expandedInputList);
            std::pair<FullKeyRef, FullKeyRef> expandedRange = GetKeyRange(levelInputs4);
            const FullKeyRef &expandedSmallest = expandedRange.first;
            const FullKeyRef &expandedLargest = expandedRange.second;
            std::vector<FileMetaDataRef> expandedOutputList = GetOverlappingInputs(current, levelId + 1, groupRange,
                                                                                   expandedSmallest, expandedLargest);

            if (expandedOutputList.size() == outputLevelInputs.size()) {
                largest = expandedLargest;
                levelInputs = expandedInputList;
                outputLevelInputs = expandedOutputList;
                std::vector<std::vector<FileMetaDataRef>> levelInputs5;
                levelInputs5.emplace_back(levelInputs);
                levelInputs5.emplace_back(outputLevelInputs);
                std::pair<FullKeyRef, FullKeyRef> newRange = GetKeyRange(levelInputs5);
                allSmallest = newRange.first;
                allLargest = newRange.second;
            }
        }
    }
}

std::pair<FileMetaDataRef, bool> CompactionPicker::GetInitialInputFile(const VersionPtr &current, uint32_t levelId,
                                                                       const GroupRangeRef &curGroupRange)
{
    GroupRangeRef groupRange = current->GetLevel(levelId).GetBottomLeftMostGroupRange();
    auto mayOverlappedFiles = current->GetMayOverlappedFiles(levelId, groupRange);
    uint64_t position = mNextFilePositions[levelId].fetch_add(NO_1, std::memory_order_seq_cst);
    uint32_t numFiles = (mayOverlappedFiles.first).size();
    if (numFiles == 0) {
        return { nullptr, false };
    }
    auto index = static_cast<int32_t>(position % numFiles);
    return { mayOverlappedFiles.first[index], mayOverlappedFiles.second };
}

std::vector<FileMetaDataRef> CompactionPicker::ExpandInputFilesIfNeeded(const VersionPtr &current, uint32_t levelId,
                                                                        const GroupRangeRef &curGroupRange,
                                                                        std::vector<FileMetaDataRef> levelInputs)
{
    std::vector<std::vector<FileMetaDataRef>> levelInputs2;
    levelInputs2.emplace_back(levelInputs);
    std::pair<FullKeyRef, FullKeyRef> keyRange = GetKeyRange(levelInputs2);
    std::vector<GroupRangeRef> groupRanges = current->GetLevel(levelId).GetGroupRanges();
    levelInputs.clear();

    for (auto &inputGroupRange : groupRanges) {
        auto overlappingInputs = GetOverlappingInputs(current, levelId, inputGroupRange, keyRange.first,
                                                      keyRange.second);
        for (auto &fileMetaData : overlappingInputs) {
            levelInputs.emplace_back(fileMetaData);
        }
    }

    FilterFileMetaList(levelInputs);
    if (levelInputs.empty()) {
        LOG_ERROR("Inputs are empty at level-" << levelId << ".");
    }
    return levelInputs;
}

void CompactionPicker::FilterFileMetaList(std::vector<FileMetaDataRef> &fileMetaDataList)
{
    std::unordered_set<FileMetaDataRef, FileMetaDataHash, FileMetaDataEqual> files;
    auto iter = fileMetaDataList.begin();
    while (iter != fileMetaDataList.end()) {
        FileMetaDataRef fileMetaData = *iter;
        auto exist = files.find(fileMetaData);
        if (exist != files.end()) {
            iter = fileMetaDataList.erase(iter);
            continue;
        }
        files.emplace(fileMetaData);
        iter++;
    }
}

std::string CompactionPicker::GetFileNames(const std::vector<FileMetaDataRef> &fileMetaList)
{
    std::string result = "[";
    for (uint32_t idx = 0; idx < fileMetaList.size(); idx++) {
        std::string fileName = PathTransform::ExtractFileName(fileMetaList[idx]->GetShortFileName());
        if (idx == fileMetaList.size() - 1) {
            result += fileName;
        } else {
            result += fileName;
            result += ",";
        }
    }
    result += "]";
    return result;
}

std::vector<FileMetaDataRef> CompactionPicker::GetOverlappingInputs(const VersionPtr &current, uint32_t levelId,
                                                                    GroupRangeRef queryGroupRange, FullKeyRef begin,
                                                                    FullKeyRef end)
{
    if (levelId + 1 > current->GetNumLevels()) {
        return {};
    }

    std::vector<FileMetaDataRef> inputs;
    auto mayOverlappedFiles = current->GetMayOverlappedFiles(levelId, queryGroupRange);
    std::vector<FileMetaDataRef> filesInLevel = mayOverlappedFiles.first;
    for (uint32_t i = 0; i < filesInLevel.size(); i++) {
        FileMetaDataRef fileMetaData = filesInLevel.at(i);
        if (fileMetaData == nullptr) {
            LOG_ERROR("FileMetaData is null.");
            continue;
        }
        auto smallest = fileMetaData->GetSmallest();
        auto largest = fileMetaData->GetLargest();
        if (smallest->CompareKey(*end) <= 0 && largest->CompareKey(*begin) >= 0) {
            inputs.emplace_back(fileMetaData);
            if (mayOverlappedFiles.second || levelId == 0) {
                bool beginExpanded = (smallest->CompareKey(*begin) < 0);
                bool endExpanded = (largest->CompareKey(*end) > 0);
                if (beginExpanded || endExpanded) {
                    inputs.clear();
                    i = -1;
                    if (beginExpanded) {
                        begin = smallest;
                    }
                    if (endExpanded) {
                        end = largest;
                    }
                }
            }
        }
    }
    return inputs;
}

std::pair<FullKeyRef, FullKeyRef> CompactionPicker::GetKeyRange(const std::vector<std::vector<FileMetaDataRef>> &inputs)
{
    FullKeyRef smallest = nullptr;
    FullKeyRef largest = nullptr;
    for (auto &inputList : inputs) {
        for (auto &fileMetaData : inputList) {
            if (smallest == nullptr) {
                smallest = fileMetaData->GetSmallest();
                largest = fileMetaData->GetLargest();
                continue;
            }
            if (fileMetaData->GetSmallest()->CompareFullKey(*smallest, smallest->SeqId()) < 0) {
                smallest = fileMetaData->GetSmallest();
            }
            if (fileMetaData->GetLargest()->CompareFullKey(*largest, largest->SeqId()) > 0) {
                largest = fileMetaData->GetLargest();
            }
        }
    }

    return { smallest, largest };
}

void CompactionPicker::ExpandInputsToCleanCut(const std::vector<FileMetaDataRef> &levelFiles,
                                              std::vector<FileMetaDataRef> &toCompactionFiles)
{
    FullKeyRef largestKey = FindLargestKey(toCompactionFiles);
    if (largestKey == nullptr) {
        return;
    }

    while (true) {
        FileMetaDataRef smallestBoundaryFile = FindSmallestBoundaryFile(levelFiles, largestKey);
        if (smallestBoundaryFile != nullptr) {
            toCompactionFiles.emplace_back(smallestBoundaryFile);
            largestKey = smallestBoundaryFile->GetLargest();
            continue;
        }
        break;
    }
}

FullKeyRef CompactionPicker::FindLargestKey(const std::vector<FileMetaDataRef> &files)
{
    if (files.empty() || files.at(0) == nullptr) {
        return nullptr;
    }

    FullKeyRef largestKey = (files.at(0))->GetLargest();
    for (auto &file : files) {
        if (file->GetLargest()->CompareFullKey(*largestKey, largestKey->SeqId()) > 0) {
            largestKey = file->GetLargest();
        }
    }
    return largestKey;
}

FileMetaDataRef CompactionPicker::FindSmallestBoundaryFile(const std::vector<FileMetaDataRef> &levelFiles,
                                                           const FullKeyRef &largestKey)
{
    FileMetaDataRef smallestBoundaryFile = nullptr;
    for (auto &f : levelFiles) {
        if (f == nullptr) {
            continue;
        }
        if (f->GetSmallest()->CompareFullKey(*largestKey, largestKey->SeqId()) > 0 &&
            f->GetSmallest()->Compare(*largestKey) == 0 &&
            (smallestBoundaryFile == nullptr ||
             f->GetSmallest()->CompareFullKey(*smallestBoundaryFile->GetSmallest(),
                                              smallestBoundaryFile->GetSmallest()->SeqId()) < 0)) {
            smallestBoundaryFile = f;
        }
    }
    return smallestBoundaryFile;
}

}  // namespace bss
}  // namespace ock