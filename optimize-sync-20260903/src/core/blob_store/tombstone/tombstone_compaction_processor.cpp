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

#include "tombstone_compaction_processor.h"

#include "record_delete_iterator_wrapper.h"
#include "tombstone_file_iterator.h"
#include "tombstone_file_merging_iterator.h"
#include "tombstone_file_writer.h"

namespace ock {
namespace bss {

BResult TombstoneCompactionProcessor::Process(std::vector<TombstoneFileRef> &ret)
{
    std::vector<TombstoneFileIteratorRef> fileIterators;
    fileIterators.reserve(mInputFiles.size() + mNextLevelFiles.size());
    uint64_t maxVersion = 0;
    for (const auto &item : mInputFiles) {
        maxVersion = std::max(maxVersion, item->GetVersion());
        auto iterator = std::make_shared<TombstoneFileIterator>(item, mMemManager);
        RETURN_NOT_OK(iterator->Init());
        fileIterators.emplace_back(iterator);
    }
    for (const auto &item : mNextLevelFiles) {
        maxVersion = std::max(maxVersion, item->GetVersion());
        auto iterator = std::make_shared<TombstoneFileIterator>(item, mMemManager);
        RETURN_NOT_OK(iterator->Init());
        fileIterators.emplace_back(iterator);
    }
    auto mergingIterator = std::make_shared<TombstoneFileMergingIterator>(fileIterators);
    RETURN_NOT_OK(mergingIterator->Init());
    IteratorPtr<TombstoneRef> inputIterator;
    if (mIsTopLevel) {
        auto blobFileIterator = std::make_shared<BlobFileGroupIterator>(mBlobFileGroupManager);
        IteratorPtr<TombstoneRef> recordIterator = std::make_shared<RecordDeleteIteratorWrapper>(mergingIterator,
                                                                                                 blobFileIterator);
        inputIterator = recordIterator;
    } else {
        inputIterator = mergingIterator;
    }

    TombstoneFileWriterRef fileWriter = std::make_shared<TombstoneFileWriter>(mConfig, mFileManager, maxVersion,
                                                                              mMemManager);
    while (inputIterator->HasNext()) {
        auto tombstone = inputIterator->Next();
        if (tombstone == nullptr) {
            continue;
        }
        RETURN_NOT_OK(fileWriter->WriteTombstone(tombstone));
    }
    mFileManager->DiscardFile(mInputFiles);
    mFileManager->DiscardFile(mNextLevelFiles);
    return fileWriter->FinishWrite(ret);
}
}  // namespace bss
}  // namespace ock