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

#include "slice_address.h"

namespace ock {
namespace bss {
SliceAddressRef SliceAddress::mInvalidAddress = GetInvalidAddress();
std::atomic<uint64_t> SliceAddress::mGenerateSliceId{ 1 };

void SliceAddress::Init(const DataSliceRef &dataSlice, uint64_t accessNumber)
{
    if (UNLIKELY(dataSlice == nullptr)) {
        return;
    }
    mDataSlice = dataSlice;
    mOriDataLen = dataSlice->GetSize();
    mStoredDataLen = mOriDataLen;
    mCompressAlgo = CompressAlgo::NONE;
    mBorn = accessNumber;
    mHitCount = 0;
}

float SliceAddress::Score(uint64_t tick)
{
    if (tick <= mBorn) {
        return 0.0;
    }

    float score = static_cast<float>(mHitCount) / static_cast<float>(tick - mBorn);
    LOG_DEBUG("Computing score, score: " << score << ", hitCount: " << mHitCount << ", born: " << mBorn
                                         << ", tick: " << tick);
    return score;
}

void SliceAddress::ReduceRequestCount(std::vector<SliceAddressRef> &invalidSliceAddressList)
{
    for (auto &sliceAddress : invalidSliceAddressList) {
        if (UNLIKELY(sliceAddress == nullptr)) {
            LOG_ERROR("Slice address is nullptr, Failed to reduce request count.");
            continue;
        }
        mBorn = std::min(mBorn, sliceAddress->mBorn);
        mHitCount += sliceAddress->mHitCount;
    }
}

void SliceAddress::SnapshotMeta(FileOutputViewRef localOutputView, SnapshotMetaRef &snapshotMeta)
{
    // sliceAddress的元数据内容: 数据长度+checksum+snapshot文件地址+snapshot文件偏移+sliceId.
    localOutputView->WriteUint32(mOriDataLen);
    localOutputView->WriteUint32(mStoredDataLen);
    localOutputView->WriteUint8(mCompressAlgo);
    localOutputView->WriteUint32(mCheckSum);
    localOutputView->WriteUTF(mLocalAddress);
    localOutputView->WriteUint64(mStartOffset);
    localOutputView->WriteUint64(mSliceId);

    snapshotMeta->AddLocalFilePaths(std::make_shared<Path>(mLocalAddress));
    snapshotMeta->AddLocalFullSize(mStoredDataLen);
}

BResult SliceAddress::Restore(const FileInputViewRef &reader, SliceAddressRef &sliceAddress)
{
    uint32_t sliceLen = 0;
    RETURN_NOT_OK_AS_READ_ERROR(reader->Read(sliceLen));
    uint32_t storedLen = 0;
    RETURN_NOT_OK_AS_READ_ERROR(reader->Read(storedLen));
    CompressAlgo compressAlgo = CompressAlgo::NONE;
    RETURN_NOT_OK_AS_READ_ERROR(reader->Read(compressAlgo));
    uint32_t checkSum = 0;
    RETURN_NOT_OK_AS_READ_ERROR(reader->Read(checkSum));
    std::string localAddress;
    RETURN_NOT_OK_AS_READ_ERROR(reader->ReadUTF(localAddress));
    uint64_t startOffset = 0;
    RETURN_NOT_OK_AS_READ_ERROR(reader->Read(startOffset));
    uint64_t sliceId = 0;
    RETURN_NOT_OK_AS_READ_ERROR(reader->Read(sliceId));
    sliceAddress = std::make_shared<SliceAddress>(sliceLen, checkSum, 0, startOffset, sliceId);
    RETURN_ALLOC_FAIL_AS_NULLPTR(sliceAddress);
    sliceAddress->SetLocalAddress(localAddress);
    sliceAddress->SetSnapshotCompressionMeta(storedLen, compressAlgo);
    LOG_DEBUG("Restore slice address success, checkSum:" << checkSum << ", sliceId:" << sliceId << ", startOffset:"
                                                         << startOffset << ", localAddress:" << localAddress
                                                         << ", sliceLen:" << sliceLen << ", storedLen:" << storedLen
                                                         << ", compressAlgo:" << static_cast<uint32_t>(compressAlgo));
    return BSS_OK;
}

}  // namespace bss
}  // namespace ock
