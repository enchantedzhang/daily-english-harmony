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

#include "slice_table_snapshot_operator.h"

#include "snapshot_compression_utils.h"

namespace ock {
namespace bss {
namespace {
BResult WriteSliceSnapshotPayload(const FileOutputViewRef &fileOutputView, const SliceAddressRef &sliceAddress,
                                  const ConfigRef &config, const MemManagerRef &memManager, int64_t fileOffset,
                                  uint64_t snapshotId, uint32_t &storedLength, CompressAlgo &storedAlgo)
{
    RETURN_INVALID_PARAM_AS_NULLPTR(fileOutputView);
    RETURN_INVALID_PARAM_AS_NULLPTR(sliceAddress);
    RETURN_INVALID_PARAM_AS_NULLPTR(memManager);
    if (UNLIKELY(sliceAddress->GetDataSlice() == nullptr || sliceAddress->GetDataSlice()->GetSlice() == nullptr)) {
        LOG_WARN("Async snapshot failed, Data slice is nullptr, sliceId:" << sliceAddress->GetSliceId());
        return BSS_NOT_EXISTS;
    }
    ByteBufferRef sliceBuffer = sliceAddress->GetDataSlice()->GetSlice()->GetByteBuffer();
    if (UNLIKELY(sliceBuffer == nullptr)) {
        LOG_ERROR("Data slice buffer is nullptr, sliceId:" << sliceAddress->GetSliceId());
        return BSS_ERR;
    }
    uint32_t rawLength = sliceAddress->GetDataLen();
    if (UNLIKELY(rawLength == 0 || rawLength > sliceBuffer->Capacity())) {
        LOG_ERROR("Invalid slice snapshot raw length, sliceId:" << sliceAddress->GetSliceId()
                                                                << ", rawLength:" << rawLength
                                                                << ", capacity:" << sliceBuffer->Capacity());
        return BSS_INVALID_PARAM;
    }

    storedAlgo = CompressAlgo::NONE;
    storedLength = rawLength;
    CompressAlgo policy = config == nullptr ? CompressAlgo::NONE : config->GetSliceTableSnapshotCompressionPolicy();
    MemorySegmentRef compressedSegment = nullptr;
    if (policy != CompressAlgo::NONE) {
        uintptr_t addr = 0;
        BResult allocRet = memManager->GetMemory(MemoryType::SLICE_TABLE, rawLength, addr, false, 0);
        if (allocRet == BSS_OK) {
            compressedSegment = MakeRef<MemorySegment>(rawLength, reinterpret_cast<uint8_t *>(addr), memManager);
            if (UNLIKELY(compressedSegment == nullptr)) {
                memManager->ReleaseMemory(addr);
                LOG_WARN("Alloc slice snapshot compression segment failed, fallback raw, sliceId:"
                         << sliceAddress->GetSliceId());
            } else {
                BResult compressRet = SnapshotCompressionUtils::TryCompressInto(policy, sliceBuffer->Data(), rawLength,
                                                                                compressedSegment->GetSegment(),
                                                                                rawLength, storedAlgo, storedLength);
                if (UNLIKELY(compressRet != BSS_OK)) {
                    LOG_WARN("Compress slice snapshot failed, fallback raw, ret:" << compressRet << ", sliceId:"
                                                                                  << sliceAddress->GetSliceId());
                    storedAlgo = CompressAlgo::NONE;
                    storedLength = rawLength;
                }
            }
        } else {
            LOG_WARN("Alloc slice snapshot compression memory failed, fallback raw, ret:"
                     << allocRet << ", sliceId:" << sliceAddress->GetSliceId() << ", rawLength:" << rawLength);
        }
    }

    BResult ret;
    if (storedAlgo == CompressAlgo::LZ4) {
        ret = fileOutputView->WriteBuffer(compressedSegment->GetSegment(), fileOffset, storedLength);
    } else {
        storedAlgo = CompressAlgo::NONE;
        storedLength = rawLength;
        ret = fileOutputView->WriteByteBuffer(sliceBuffer, fileOffset, rawLength);
    }
    if (UNLIKELY(ret != BSS_OK)) {
        LOG_ERROR("Slice table snapshot write failed, ret:" << ret << ", sliceId:" << sliceAddress->GetSliceId()
                                                            << ", rawLength:" << rawLength
                                                            << ", storedLength:" << storedLength);
        return ret;
    }
    sliceAddress->SetRawSnapshotCompressionMeta(storedLength, storedAlgo);
    double ratio = rawLength == 0 ? 0.0 : static_cast<double>(storedLength) * 100.0 / rawLength;
    LOG_DEBUG("SliceTable snapshot compression, checkpointId:"
              << snapshotId << ", sliceId:" << sliceAddress->GetSliceId()
              << ", algo:" << static_cast<uint32_t>(storedAlgo) << ", rawLength:" << rawLength
              << ", storedLength:" << storedLength << ", startOffset:" << fileOffset
              << ", file:" << fileOutputView->GetFilePath()->ExtractFileName() << ", ratio:" << ratio << "%");
    return BSS_OK;
}
}  // namespace

BResult SliceTableSnapshotOperator::SyncSnapshot(bool isSavepoint)
{
    if (UNLIKELY(mSliceTable->IsSnapshotIdExist(mSnapshotId))) {
        LOG_ERROR("Current checkpoint snapshotId " << mSnapshotId << " has been done.");
        return BSS_ALREADY_DONE;
    }

    {
        std::lock_guard<std::mutex> lock(mMutex);
        if (mState != State::WAITING_BARRIER) {
            LOG_DEBUG("Receive barrier on unexpected state for operator");
            return BSS_INVALID_PARAM;
        }
        mState = State::SYNC;
    }

    SliceTableSnapshotRef sliceTableSnapshot = std::make_shared<SliceTableSnapshot>();
    RETURN_NOT_OK(sliceTableSnapshot->Initialize(mSliceTable->GetSliceBucketIndex(),
                                                 mSliceTable->GetBucketGroupManager(), mMemManager, isSavepoint,
                                                 mSnapshotId, mSliceTable->GetKeyGroupUtil(),
                                                 mSliceTable->GetStartKeyGroup(), mSliceTable->GetEndKeyGroup()));
    mSliceTable->AddSliceTableSnapshot(mSnapshotId, sliceTableSnapshot);
    mSliceTableSnapshot = sliceTableSnapshot;

    // 生成sliceTable快照.
    auto ret = sliceTableSnapshot->GenSliceTableIndexSnapshot();
    if (UNLIKELY(ret != BSS_OK)) {
        LOG_ERROR("Generate slice table snapshot failed, ret:" << ret << ", snapshotId:" << mSnapshotId);
    }
    return ret;
}

BResult SliceTableSnapshotOperator::AsyncSnapshot(uint64_t snapshotId, const PathRef &snapshotPath, bool isIncremental,
                                                  bool enableLocalRecovery, const PathRef &backupPath,
                                                  std::unordered_map<std::string, uint32_t> &sliceRefCounts)
{
    RETURN_INVALID_PARAM_AS_NULLPTR(snapshotPath);

    if (!enableLocalRecovery) {
        return AsyncSnapshotWithoutLocalRecovery(snapshotId, snapshotPath, isIncremental);
    }
    // 仅当在开启localRecovery时才需要校验backupPath
    RETURN_INVALID_PARAM_AS_NULLPTR(backupPath);

    std::lock_guard<std::mutex> lk(mResourceMutex);
    if (mIsReleased.load()) {
        return BSS_OK;
    }

    uint64_t dataTotalSize = 0;
    BResult ret = BSS_OK;
    std::string file;
    std::unordered_set<std::string> fileSet;
    IteratorRef<SliceAddressRef> sliceIterator = mSliceTableSnapshot->GetSnapshotSliceFlushIterator();
    while (!mIsReleased.load() && sliceIterator->HasNext()) {
        SliceAddressRef sliceAddress = sliceIterator->Next();
        if (UNLIKELY(sliceAddress == nullptr || sliceAddress->IsEvicted())) {  // 当前slice已经被淘汰则直接跳过.
            continue;
        }

        // 写满的文件硬链到指定目录
        if (mFileOutputView != nullptr && mFileOutputView->Size() >= IO_SIZE_32M) {  // 快照文件要凑满32M
            mFileOutputView->Close();
            ret = createHardlinks(backupPath, snapshotPath, mFileOutputView->GetFilePath()->ExtractFileName());
            file = mFileOutputView->GetFilePath()->ExtractFileName();
            mFileOutputView = nullptr;
        }
        if (UNLIKELY(ret != BSS_OK)) {
            LOG_ERROR("Failed to create hardlink for new slice file: " << file);
            return ret;
        }
        // 增量场景下，slice的LocalAddress为空，说明当前slice还没有做过checkpoint,
        // 反之则代表当前slice已经做过checkpoint； 之前版本会在当前slice已经做过checkpoint时创建空文件用于后续增量比较；
        // 为了支持本地恢复，现在版本不再使用创建空文件的方式，而是将新文件创建到cp本地目录父目录的自定义子目录，
        // 每次创建cp都为cp本地目录所需的新文件和旧文件创建指向cp本地目录父目录的自定义子目录中文件的硬链接。
        if (sliceAddress->GetLocalAddress().empty()) {
            if (mFileOutputView == nullptr) {
                // 1. 创建sliceTable的快照输出文件fileOutputView, 文件名为sliceId.slice.
                std::string sliceFileName = std::to_string(sliceAddress->GetSliceId()) + ".slice";
                // 引用计数map中添加新文件
                sliceRefCounts[sliceFileName] = 1;
                PathRef sliceTableFilePath = std::make_shared<Path>(backupPath, sliceFileName);
                mFileOutputView = std::make_shared<FileOutputView>();
                mFileOutputView->Init(sliceTableFilePath, mConfig);
                LOG_DEBUG("Create slice snapshot file, file path:" << sliceTableFilePath->ExtractFileName());
            }

            // 2. 将DataSlice中的数据写入到文件中.
            auto startOffset = mFileOutputView->Size();
            auto sizeToCheck = mFileOutputView->Size();
            if (UNLIKELY(sizeToCheck > INT64_MAX)) {
                LOG_ERROR("File size is too large :" << sizeToCheck);
                return BSS_ERR;
            }
            uint32_t storedLength = 0;
            CompressAlgo storedAlgo = CompressAlgo::NONE;
            ret = WriteSliceSnapshotPayload(mFileOutputView, sliceAddress, mConfig, mMemManager,
                                            static_cast<int64_t>(sizeToCheck), snapshotId, storedLength, storedAlgo);
            if (ret == BSS_NOT_EXISTS) {
                continue;
            }
            if (UNLIKELY(ret != BSS_OK)) {
                LOG_ERROR("Slice table snapshot write failed, ret:"
                          << ret << ", writenSize:" << storedLength
                          << ", file path:" << mFileOutputView->GetFilePath()->ExtractFileName());
                mFileOutputView->Close();
                mFileOutputView = nullptr;
                return BSS_ERR;
            }
            // 设置当前slice checkpoint的文件和文件的偏移起始地址, 在恢复的时候从这个位置开始读取.
            sliceAddress->SetRawStartOffset(startOffset);
            sliceAddress->SetRawLocalAddress(snapshotPath->Name() + '/' +
                                             mFileOutputView->GetFilePath()->ExtractFileName());
            dataTotalSize += storedLength;
            LOG_DEBUG("Slice table async checkpoint write data slice success, file path:"
                      << mFileOutputView->GetFilePath()->ExtractFileName() << ", sliceId:" << sliceAddress->GetSliceId()
                      << ", startOffset:" << startOffset << ", writenSize:" << storedLength << ", checkSum:"
                      << sliceAddress->GetCheckSum() << ", algo:" << static_cast<uint32_t>(storedAlgo));
            continue;
        }
        // 已经做过cp的slice文件直接从backup目录硬链到snapshot目录
        std::string fileName = PathTransform::ExtractFileName(sliceAddress->GetLocalAddress());
        auto filePath = std::make_shared<Path>(snapshotPath, fileName);
        if (fileSet.find(fileName) == fileSet.end()) {
            sliceRefCounts[fileName] = 1;
            ret = createHardlinks(backupPath, snapshotPath, fileName);
            if (UNLIKELY(ret != BSS_OK)) {
                LOG_ERROR("Failed to create hardlink for old slice file: " << fileName);
                return ret;
            }
            fileSet.emplace(fileName);
        }
        sliceAddress->SetRawLocalAddress(filePath->Name());
        dataTotalSize += sliceAddress->GetStoredDataLen();
    }

    if (mFileOutputView != nullptr) {
        mFileOutputView->Close();
        ret = createHardlinks(backupPath, snapshotPath, mFileOutputView->GetFilePath()->ExtractFileName());
        file = mFileOutputView->GetFilePath()->ExtractFileName();
        mFileOutputView = nullptr;
    }
    if (UNLIKELY(ret != BSS_OK)) {
        LOG_ERROR("Failed to create hardlink for new slice file: " << file);
        return ret;
    }

    // 所有文件引用计数减一操作在complete此次checkpoint的方法中执行，否则checkpoint上传失败的场景也会减少引用计数
    LOG_DEBUG("SliceTable write data slice success, checkpointId:" << snapshotId << ", totalSize:" << dataTotalSize);
    return BSS_OK;
}

BResult SliceTableSnapshotOperator::AsyncSnapshotWithoutLocalRecovery(uint64_t snapshotId, const PathRef &snapshotPath,
                                                                      bool isIncremental)
{
    std::lock_guard<std::mutex> lk(mResourceMutex);
    if (mIsReleased.load()) {
        return BSS_OK;
    }

    uint64_t dataTotalSize = 0;
    std::unordered_set<std::string> fileSet;
    IteratorRef<SliceAddressRef> sliceIterator = mSliceTableSnapshot->GetSnapshotSliceFlushIterator();
    while (!mIsReleased.load() && sliceIterator->HasNext()) {
        SliceAddressRef sliceAddress = sliceIterator->Next();
        if (UNLIKELY(sliceAddress == nullptr || sliceAddress->IsEvicted())) {  // 当前slice已经被淘汰则直接跳过.
            continue;
        }

        if (mFileOutputView != nullptr && mFileOutputView->Size() >= IO_SIZE_32M) {  // 快照文件要凑满32M
            mFileOutputView->Close();
            mFileOutputView = nullptr;
        }

        // 增量场景下, slice的LocalAddress为非空，说明当前slice已经有做过checkpoint, 则本次不做,
        // 只创一个空文件用于后续增量比较.
        if (isIncremental && !sliceAddress->GetLocalAddress().empty()) {
            auto fileName = PathTransform::ExtractFileName(sliceAddress->GetLocalAddress());
            auto filePath = std::make_shared<Path>(snapshotPath, fileName);
            auto fileOutputView = std::make_shared<FileOutputView>();
            if (fileSet.find(fileName) == fileSet.end()) {  // 如果之前没有创建，则创建一个空文件，用于增量比较.
                RETURN_NOT_OK(fileOutputView->Init(filePath, mConfig));
                LOG_DEBUG("Create empty file when incremental checkpoint, file path:" << filePath->ExtractFileName());
                fileSet.emplace(fileName);
            }
            // 更新此时的LocalAddress, 因为恢复的时候都要下载到对应的checkpoint目录下,
            // 所以每次checkpoint都要更新LocalAddress.
            sliceAddress->SetRawLocalAddress(filePath->Name());
            dataTotalSize += sliceAddress->GetStoredDataLen();
            continue;
        }
        // 必须在增量判断结束之后新建, 防止创建空文件造成上传失败.
        if (mFileOutputView == nullptr) {
            // 1. 创建sliceTable的快照输出文件fileOutputView, 文件名为sliceId.slice.
            std::string sliceFileName = std::to_string(sliceAddress->GetSliceId()) + ".slice";
            PathRef sliceTableFilePath = std::make_shared<Path>(snapshotPath, sliceFileName);
            mFileOutputView = std::make_shared<FileOutputView>();
            mFileOutputView->Init(sliceTableFilePath, mConfig);
            LOG_DEBUG("Create slice snapshot file, file path:" << sliceTableFilePath->ExtractFileName());
        }

        // 2. 将DataSlice中的数据写入到文件中.
        auto startOffset = mFileOutputView->Size();
        auto sizeToCheck = mFileOutputView->Size();
        if (UNLIKELY(sizeToCheck > INT64_MAX)) {
            LOG_ERROR("File size is too large :" << sizeToCheck);
            return BSS_ERR;
        }
        uint32_t storedLength = 0;
        CompressAlgo storedAlgo = CompressAlgo::NONE;
        auto ret = WriteSliceSnapshotPayload(mFileOutputView, sliceAddress, mConfig, mMemManager,
                                             static_cast<int64_t>(sizeToCheck), snapshotId, storedLength, storedAlgo);
        if (ret == BSS_NOT_EXISTS) {
            continue;
        }
        if (UNLIKELY(ret != BSS_OK)) {
            LOG_ERROR("Slice table snapshot write failed, ret:" << ret << ", writenSize:" << storedLength
                                                                << ", file path:"
                                                                << mFileOutputView->GetFilePath()->ExtractFileName());
            return BSS_ERR;
        }
        // 设置当前slice checkpoint的文件和文件的偏移起始地址, 在恢复的时候从这个位置开始读取.
        sliceAddress->SetRawStartOffset(startOffset);
        sliceAddress->SetRawLocalAddress(mFileOutputView->GetFilePath()->Name());
        dataTotalSize += storedLength;
        LOG_DEBUG("Slice table async checkpoint write data slice success, file path:"
                  << mFileOutputView->GetFilePath()->ExtractFileName() << ", sliceId:" << sliceAddress->GetSliceId()
                  << ", startOffset:" << startOffset << ", writenSize:" << storedLength
                  << ", checkSum:" << sliceAddress->GetCheckSum() << ", algo:" << static_cast<uint32_t>(storedAlgo));
    }
    if (mFileOutputView != nullptr) {
        mFileOutputView->Close();
        mFileOutputView = nullptr;
    }

    LOG_DEBUG("SliceTable write data slice success, checkpointId:" << snapshotId << ", totalSize:" << dataTotalSize);
    return BSS_OK;
}

SnapshotMetaRef SliceTableSnapshotOperator::OutputMeta(uint64_t snapshotId, const FileOutputViewRef &localOutputView)
{
    std::lock_guard<std::mutex> lk(mResourceMutex);
    if (mIsReleased.load()) {
        return nullptr;
    }
    RETURN_NULLPTR_AS_NULLPTR(mSliceTableSnapshot);
    return mSliceTableSnapshot->SnapshotMetaFunc(snapshotId, localOutputView);
}

BResult SliceTableSnapshotOperator::WriteInfo(const FileOutputViewRef &localOutputView)
{
    SliceTableSnapshotOperatorInfoRef sliceTableSnapshotOperatorInfo =
        std::make_shared<SliceTableSnapshotOperatorInfo>(SnapshotOperatorType::SLICE_TABLE);
    return sliceTableSnapshotOperatorInfo->Serialize(localOutputView);
}

void SliceTableSnapshotOperator::InternalRelease()
{
    AbstractSnapshotOperator::InternalRelease();
    mSliceTable->EraseSnapshotId(GetSnapshotId());
    mSliceTableSnapshot->ReleaseResource();
    // 开启SliceCompaction
    mSliceTable->SetCompactionStatus(SliceTableCompaction::OPEN);
    LOG_DEBUG("SliceTable release snapshot success.");
}

}  // namespace bss
}  // namespace ock
