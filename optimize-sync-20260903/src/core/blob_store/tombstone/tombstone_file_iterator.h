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

#ifndef BOOST_SS_TOMBSTONE_FILE_ITERATOR_H
#define BOOST_SS_TOMBSTONE_FILE_ITERATOR_H

#include "tombstone.h"
#include "tombstone_file.h"
#include "tombstone_structure.h"
#include "util/iterator.h"

namespace ock {
namespace bss {
class TombstoneFileIterator : public Iterator<TombstoneRef> {
public:
    TombstoneFileIterator(const TombstoneFileRef &tombstoneFile, const MemManagerRef &memManager)
        : mFile(tombstoneFile), mMemManager(memManager){};

    bool HasNext() override
    {
        if (UNLIKELY(!mInitialized)) {
            LOG_ERROR("tombstone file iterator not init");
            return false;
        }
        if (mNext != nullptr) {
            return true;
        }
        if (UNLIKELY(mFileMeta->GetValidGroupRange() == nullptr)) {
            LOG_ERROR("tombstone file valid group range is null");
            return false;
        }
        do {
            mNext = nullptr;  // keyGroup不满足时，如果遍历完所有data block，mNext不为null会一直循环
            auto ret = Advance();
            if (UNLIKELY(ret != BSS_OK)) {
                LOG_ERROR("Advance fail ret: " << ret);
                mNext = nullptr;
            }
        } while (mNext != nullptr && !mFileMeta->GetValidGroupRange()->ContainsGroup(mNext->GetKeyGroup()));

        return mNext != nullptr;
    }

    BResult Init()
    {
        RETURN_ERROR_AS_NULLPTR(mFile);
        mFileMeta = mFile->GetFileMeta();
        RETURN_ERROR_AS_NULLPTR(mFileMeta);
        mFileBaseOffset = FileAddressUtils::GetFileOffset(mFileMeta->GetFileAddress());
        mNextDataBlockIndex = 0;
        mNext = nullptr;
        mFileInputView = std::make_shared<FileInputView>();
        BResult ret = mFile->OpenFile(mFileInputView);
        RETURN_NOT_OK(ret);
        ret = mFileInputView->ReadBuffer(reinterpret_cast<uint8_t *>(&mFooter), sizeof(TombstoneFooterStructure),
                                         mFileMeta->GetFileSize() - sizeof(TombstoneFooterStructure));
        if (UNLIKELY(ret != BSS_OK)) {
            LOG_ERROR("read footer fail ret: " << ret << ", Tombstone file address: " << mFile->GetFileAddress());
            Close();
            return ret;
        }
        mIndexBuffer = MakeRef<ByteBuffer>(mFooter.indexBlockSize, MemoryType::FILE_STORE, mMemManager);
        RETURN_ERROR_AS_NULLPTR(mIndexBuffer);
        ret = mFileInputView->ReadBuffer(mIndexBuffer->Data(), mIndexBuffer->Capacity(), mFooter.indexBlockOffset);
        if (UNLIKELY(ret != BSS_OK)) {
            LOG_ERROR("read index block fail ret: " << ret << ", Tombstone file address: " << mFile->GetFileAddress());
            Close();
            return ret;
        }
        mInitialized = true;
        return BSS_OK;
    }

    TombstoneRef Next() override
    {
        if (UNLIKELY(!mInitialized)) {
            LOG_ERROR("tombstone file iterator not init");
            return nullptr;
        }
        TombstoneRef next = mNext;
        mNext = nullptr;
        return next;
    }

    void Close() override
    {
        if (mFileInputView != nullptr) {
            mFileInputView->Close();
        }
    }

private:
    BResult Advance()
    {
        if (mCurDataBlock == nullptr) {
            if (mNextDataBlockIndex >= mFooter.dataBlockNum) {
                return BSS_OK;
            }
            RETURN_NOT_OK(LoadDataBlock());
        }
        if (mCurDataIndex >= mCurDataBlockTombstoneNum) {
            return BSS_OK;
        }
        uint32_t base = mCurDataIndex * NO_10;
        uint64_t blobId = *(reinterpret_cast<uint64_t *>(mCurDataBlock->Data() + base));
        uint16_t keyGroup = *(reinterpret_cast<uint16_t *>(mCurDataBlock->Data() + base + sizeof(uint64_t)));
        mNext = std::make_shared<Tombstone>(blobId, keyGroup);
        mCurDataIndex++;
        if (mCurDataIndex >= mCurDataBlockTombstoneNum) {
            mCurDataBlock = nullptr;
        }
        return BSS_OK;
    }

    BResult LoadDataBlock()
    {
        uint32_t base = mNextDataBlockIndex * NO_8;
        uint32_t dataBlockOffset = *(reinterpret_cast<uint32_t *>(mIndexBuffer->Data() + base));
        uint32_t dataBlockSize = *(reinterpret_cast<uint32_t *>(mIndexBuffer->Data() + base + sizeof(uint32_t)));
        if (UNLIKELY((dataBlockSize % NO_10) != 0)) {  // dataBlock = uint64_t blobId + uint16_t keyGroup
            LOG_ERROR("data block size is unexpect:" << dataBlockSize);
            return BSS_ERR;
        }
        mCurDataBlock = MakeRef<ByteBuffer>(dataBlockSize, MemoryType::FILE_STORE, mMemManager);
        RETURN_ERROR_AS_NULLPTR(mCurDataBlock);
        auto ret = mFileInputView->ReadBuffer(mCurDataBlock->Data(), dataBlockSize, dataBlockOffset);
        if (UNLIKELY(ret != BSS_OK)) {
            LOG_ERROR("read data block fail ret : " << ret);
            return ret;
        }
        mCurDataIndex = 0;
        mCurDataBlockTombstoneNum = dataBlockSize / NO_10;
        mNextDataBlockIndex++;
        return BSS_OK;
    }

private:
    TombstoneFileRef mFile = nullptr;
    TombstoneFileMetaRef mFileMeta = nullptr;
    FileInputViewRef mFileInputView;
    uint32_t mFileBaseOffset = 0;
    TombstoneFooterStructure mFooter{};
    ByteBufferRef mIndexBuffer;
    ByteBufferRef mCurDataBlock = nullptr;
    uint32_t mNextDataBlockIndex = 0;
    uint32_t mCurDataIndex = 0;
    uint32_t mCurDataBlockTombstoneNum = 0;
    TombstoneRef mNext = nullptr;
    MemManagerRef mMemManager = nullptr;
    bool mInitialized = false;
};
using TombstoneFileIteratorRef = std::shared_ptr<TombstoneFileIterator>;
}  // namespace bss
}  // namespace ock

#endif  // BOOST_SS_TOMBSTONE_FILE_ITERATOR_H
