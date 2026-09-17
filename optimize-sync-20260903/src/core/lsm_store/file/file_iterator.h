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

#ifndef BOOST_SS_FILE_ITERATOR_H
#define BOOST_SS_FILE_ITERATOR_H

#include "file_reader_base.h"
#include "lsm_store/block/block_handle.h"
#include "lsm_store/block/data_block.h"
#include "lsm_store/key/full_key_filter.h"

namespace ock {
namespace bss {
class FileIterator : public Iterator<KeyValueRef> {
public:
    FileIterator(const FileReaderBaseRef &fileReader, const FullKeyFilterRef &keyValueFilter,
                 const IteratorRef<BlockHandleRef> &blockHandleIterator, const MemManagerRef &memManager,
                 FileProcHolder holder, bool shareReader)
        : mFileReader(fileReader),
          mKeyValueFilter(keyValueFilter),
          mBlockHandleIterator(blockHandleIterator),
          mMemManager(memManager),
          mHolder(holder),
          mShareReader(shareReader)
    {
        mCurrentKeyValue = Advance();
        LOG_DEBUG("Create FileIterator success.");
    }

    ~FileIterator() override
    {
        LOG_DEBUG("Delete FileIterator success.");
    }

    bool HasNext() override;
    KeyValueRef Next() override;
    void Close() override;
    void PrintUsefulInfo() override;

protected:
    KeyValueRef Advance();

private:
    FileReaderBaseRef mFileReader = nullptr;
    FullKeyFilterRef mKeyValueFilter = nullptr;
    IteratorRef<BlockHandleRef> mBlockHandleIterator = nullptr;
    KeyValueIteratorRef mDataBlockIterator = nullptr;
    KeyValueRef mCurrentKeyValue = nullptr;
    MemManagerRef mMemManager = nullptr;
    FileProcHolder mHolder;
    bool mShareReader;
};

class FileSubIterator : public Iterator<KeyValueRef> {
public:
    FileSubIterator(const FileReaderBaseRef &fileReader, const FullKeyFilterRef &internalKeyFilter,
                    const IteratorRef<BlockHandleRef> &blockHandleIterator, const Key startKey, const Key endKey,
                    bool reverseOrder)
        : mFileReader(fileReader),
          mInternalKeyFilter(internalKeyFilter),
          mBlockHandleIterator(blockHandleIterator),
          mStartKey(startKey),
          mEndKey(endKey),
          mReverseOrder(reverseOrder)
    {
        mCurrentKeyValue = Advance();
    }

    bool HasNext() override;
    KeyValueRef Next() override;
    void Close() override;

protected:
    KeyValueRef Advance();

private:
    FileReaderBaseRef mFileReader = nullptr;
    FullKeyFilterRef mInternalKeyFilter = nullptr;
    IteratorRef<BlockHandleRef> mBlockHandleIterator = nullptr;
    KeyValueIteratorRef mDataBlockIterator = nullptr;
    KeyValueRef mCurrentKeyValue = nullptr;
    const Key mStartKey;
    const Key mEndKey;
    bool mReverseOrder;
};

}  // namespace bss
}  // namespace ock
#endif  // BOOST_SS_FILE_ITERATOR_H