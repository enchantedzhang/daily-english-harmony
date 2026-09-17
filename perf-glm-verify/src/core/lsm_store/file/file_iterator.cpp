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

#include "file_iterator.h"

#include "lsm_store/block/data_block.h"

namespace ock {
namespace bss {
KeyValueRef FileIterator::Advance()
{
    while ((mDataBlockIterator != nullptr && mDataBlockIterator->HasNext()) ||
           (mBlockHandleIterator != nullptr && mBlockHandleIterator->HasNext())) {
        if (mDataBlockIterator == nullptr || !mDataBlockIterator->HasNext()) {
            BlockHandleRef blockHandle = mBlockHandleIterator->Next();
            ByteBufferRef byteBuffer = nullptr;
            mFileReader->FetchBlock(*blockHandle, byteBuffer, BlockType::DATA);
            RETURN_NULLPTR_AS_NULLPTR_NO_LOG(byteBuffer);
            DataBlock dataBlock(byteBuffer);
            auto ret = dataBlock.Init(mMemManager, mHolder);
            if (UNLIKELY(ret != BSS_OK)) {
                LOG_ERROR("Failed to init dataBlock.");
                return nullptr;
            }
            mDataBlockIterator = dataBlock.Iterator();
            if (mDataBlockIterator == nullptr || !mDataBlockIterator->HasNext()) {
                mDataBlockIterator = nullptr;
                continue;
            }
        }

        KeyValueRef keyValue = mDataBlockIterator->Next();
        if (!mKeyValueFilter->Filter(keyValue)) {
            return keyValue;
        }
    }
    return nullptr;
}

bool FileIterator::HasNext()
{
    return mCurrentKeyValue != nullptr;
}

KeyValueRef FileIterator::Next()
{
    KeyValueRef result = mCurrentKeyValue;
    mCurrentKeyValue = Advance();
    return result;
}

void FileIterator::Close()
{
    if (mFileReader != nullptr && !mShareReader) {
        mFileReader->Close();
        mFileReader = nullptr;
    }
    mBlockHandleIterator = nullptr;
}

void FileIterator::PrintUsefulInfo()
{
    LOG_INFO("fileName:" << PathTransform::ExtractFileName(mFileReader->GetFileMeta()->GetIdentifier())
                         << ", smallestKey:" << mFileReader->GetFileMeta()->GetSmallest()->ToString()
                         << ", largest:" << mFileReader->GetFileMeta()->GetLargest()->ToString()
                         << ", fileAddress:" << mFileReader->GetFileMeta()->GetFileAddress()
                         << ", fileId:" << mFileReader->GetFileMeta()->GetFileId()
                         << ", fileSize:" << mFileReader->GetFileMeta()->GetFileSize());
}

KeyValueRef FileSubIterator::Advance()
{
    while ((mDataBlockIterator != nullptr && mDataBlockIterator->HasNext()) ||
           (mBlockHandleIterator != nullptr && mBlockHandleIterator->HasNext())) {
        if (mDataBlockIterator == nullptr || !mDataBlockIterator->HasNext()) {
            BlockHandleRef blockHandle = mBlockHandleIterator->Next();
            auto dataBlock = std::dynamic_pointer_cast<DataBlock>(mFileReader->GetOrLoadDataBlock(*blockHandle));
            RETURN_NULLPTR_AS_NULLPTR(dataBlock);
            if (mStartKey.IsPqKey()) {
                auto keyFilter = [this](const Key &key) -> bool {
                    return PQBinaryDataComparator::ComparePrefix(key.PriKey().KeyData(), key.PriKey().KeyLen(),
                                                                 mStartKey.PriKey().KeyData(),
                                                                 mStartKey.PriKey().KeyLen()) != 0;
                };
                mDataBlockIterator = dataBlock->Iterator(keyFilter);
            } else {
                mDataBlockIterator = dataBlock->SubIterator(mStartKey, mEndKey, mReverseOrder);
            }
            if (mDataBlockIterator == nullptr || !mDataBlockIterator->HasNext()) {
                mDataBlockIterator = nullptr;
                continue;
            }
        }
        KeyValueRef keyValue = mDataBlockIterator->Next();
        if (!mInternalKeyFilter->Filter(keyValue)) {
            return keyValue;
        }
    }
    return nullptr;
}

bool FileSubIterator::HasNext()
{
    return mCurrentKeyValue != nullptr;
}

KeyValueRef FileSubIterator::Next()
{
    KeyValueRef result = mCurrentKeyValue;
    mCurrentKeyValue = Advance();
    return result;
}

void FileSubIterator::Close()
{
}

}  // namespace bss
}  // namespace ock