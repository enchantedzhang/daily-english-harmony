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

#include "fresh_table_snapshot_operator.h"

#include "snapshot_compression_utils.h"

namespace ock {
namespace bss {
BResult FreshTableSnapshotOperator::SyncSnapshot(bool isSavepoint)
{
    {
        std::lock_guard<std::mutex> lock(mMutex);
        if (mState != State::WAITING_BARRIER) {
            LOG_DEBUG("Receive barrier on unexpected state for operator");
            return BSS_INVALID_PARAM;
        }
        mState = State::SYNC;
    }

    if (!mPqTables.empty()) {
        for (const auto &item : mPqTables) {
            RETURN_NOT_OK(item->TriggerSegmentFlush(true));  // pq表数据先刷到文件里。
        }
    }

    // 1. Savepoint流程首先确保将FreshTable的数据Flush到SliceTable中, 然后直接返回.
    RETURN_ERROR_AS_NULLPTR(mFreshTable);
    if (isSavepoint) {
        return mFreshTable->ForceFlushToSlice();
    }

    // 2. Checkpoint流程首先确保处于待淘汰队列的MemorySegment Flush完成, 然后拷贝activeSegment到内存中, async
    // checkpoint写入文件.
    uint32_t times = NO_1;
    auto start = std::chrono::high_resolution_clock::now();
    while (!mFreshTable->IsSnapshotQueueEmpty()) {
        if ((times++) % NO_100 == 0) {
            auto end = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
            double elapsed = duration.count() / 1e3;  // 转换为ms
            LOG_WARN("Fresh table check snapshot queue cost time:" << elapsed << "ms.");
        }
        usleep(NO_100000);  // 100ms
    }
    auto activeSegment = mFreshTable->GetActiveSegment();
    RETURN_ERROR_AS_NULLPTR(activeSegment);
    MemorySegmentRef origin = activeSegment->GetMemorySegment();
    uintptr_t addr = 0;
    uint32_t capacity = activeSegment->GetMemorySegment()->GetLen();
    auto ret = mMemManager->GetMemory(MemoryType::SNAPSHOT, capacity, addr);
    if (UNLIKELY(ret != BSS_OK)) {
        LOG_ERROR("Alloc fresh table snapshot memory failed, ret:" << ret);
        return ret;
    }
    MemorySegmentRef newSegment = MakeRef<MemorySegment>(capacity, reinterpret_cast<uint8_t *>(addr), mMemManager);
    if (UNLIKELY(newSegment == nullptr)) {
        mMemManager->ReleaseMemory(addr);
        LOG_ERROR("Make ref buffer failed, memorySegment is null.");
        return BSS_ALLOC_FAIL;
    }
    mToUpload = newSegment;
    mByteLength = origin->GetCurPos();
    CompressAlgo policy = mConfig == nullptr ? CompressAlgo::NONE : mConfig->GetFreshTableSnapshotCompressionPolicy();
    ret = SnapshotCompressionUtils::TryCompressInto(policy, origin->GetSegment(), mByteLength, newSegment->GetSegment(),
                                                    mByteLength, mCompressAlgo, mCompressLength);
    if (UNLIKELY(ret != BSS_OK)) {
        return ret;
    }
    newSegment->UpdatePosition(mCompressLength);

    LOG_DEBUG("Fresh table sync checkpoint end, active memory data size:"
              << activeSegment->GetBinaryData()->Size()
              << ", bucketCount:" << activeSegment->GetBinaryData()->BucketCount() << ", rawLength:" << mByteLength
              << ", storedLength:" << mCompressLength << ", algo:" << static_cast<uint32_t>(mCompressAlgo));
    return BSS_OK;
}

BResult FreshTableSnapshotOperator::AsyncSnapshot(uint64_t snapshotId, const PathRef &snapshotPath, bool isIncremental,
                                                  bool enableLocalRecovery, const PathRef &backupPath,
                                                  std::unordered_map<std::string, uint32_t> &sliceRefCounts)
{
    if (UNLIKELY(snapshotPath == nullptr)) {
        LOG_ERROR("snapshotPath is nullptr.");
        mToUpload = nullptr;
        return BSS_ERR;
    }
    std::lock_guard<std::mutex> lk(mResourceMutex);
    if (mIsReleased.load()) {
        mToUpload = nullptr;
        return BSS_OK;
    }

    // 1. 检查传入的snapshotPath是否存在.
    if (UNLIKELY(access(snapshotPath->Name().c_str(), F_OK) != 0)) {
        LOG_ERROR("Snapshot path directory: " << snapshotPath->ExtractFileName() << " does not exist, errno:" << errno);
        mToUpload = nullptr;
        return BSS_ERR;
    }

    // 2. 创建freshTable的输出文件fileOutputView, 文件后缀为fresh_table.dat.
    PathRef freshTableFile = std::make_shared<Path>(snapshotPath, "fresh_table.dat");
    FileOutputViewRef fileOutputView = std::make_shared<FileOutputView>();
    auto ret = fileOutputView->Init(freshTableFile, mConfig);
    if (UNLIKELY(ret != BSS_OK)) {
        LOG_ERROR("Create fresh table snapshot file output view failed, ret:" << ret);
        mToUpload = nullptr;
        return ret;
    }

    // 3. 将copyMemorySegment中的数据写入到snapshot文件中.
    uint32_t uploadingLength = mCompressLength;
    ret = fileOutputView->WriteBuffer(mToUpload->GetSegment(), 0, uploadingLength);
    fileOutputView->Close();
    if (UNLIKELY(ret != BSS_OK)) {
        LOG_ERROR("Fresh table snapshot write file failed, ret:" << ret << ", writeSize:" << uploadingLength);
        mToUpload = nullptr;
        return ret;
    }

    // 4. 记录写入的文件地址信息.
    mLocalAddress = freshTableFile->Name();
    mSnapshotMeta->AddLocalFilePath(freshTableFile);
    mSnapshotMeta->AddLocalIncrementalSize(uploadingLength);
    mSnapshotMeta->AddLocalFullSize(uploadingLength);
    double ratio = mByteLength == 0 ? 0.0 : static_cast<double>(mCompressLength) * 100.0 / mByteLength;
    LOG_DEBUG("FreshTable snapshot compression, checkpointId:"
              << snapshotId << ", algo:" << static_cast<uint32_t>(mCompressAlgo) << ", rawLength:" << mByteLength
              << ", storedLength:" << mCompressLength << ", ratio:" << ratio << "%");
    LOG_DEBUG("FreshTable write snapshot meta success, checkpointId:" << snapshotId << ", fileAddress:"
                                                                      << freshTableFile->ExtractFileName()
                                                                      << ", dataSize:" << uploadingLength);
    mToUpload = nullptr;
    return BSS_OK;
}

SnapshotMetaRef FreshTableSnapshotOperator::OutputMeta(uint64_t snapshotId, const FileOutputViewRef &localOutputView)
{
    RETURN_NULLPTR_AS_NULLPTR(localOutputView);
    std::lock_guard<std::mutex> lk(mResourceMutex);
    if (mIsReleased.load()) {
        return nullptr;
    }
    // 文件中的元数据内容: 文件名+数据长度+压缩标记+压缩长度.
    RETURN_NULLPTR_AS_NOT_OK(localOutputView->WriteUTF(mLocalAddress));
    RETURN_NULLPTR_AS_NOT_OK(localOutputView->WriteUint32(mByteLength));
    RETURN_NULLPTR_AS_NOT_OK(localOutputView->WriteUint8(mCompressAlgo));
    RETURN_NULLPTR_AS_NOT_OK(localOutputView->WriteUint32(mCompressLength));
    return mSnapshotMeta;
}

BResult FreshTableSnapshotOperator::WriteInfo(const FileOutputViewRef &localOutputView)
{
    FreshTableSnapshotOperatorInfoRef freshTableSnapshotOperatorInfo =
        std::make_shared<FreshTableSnapshotOperatorInfo>(SnapshotOperatorType::FRESH_TABLE);
    return freshTableSnapshotOperatorInfo->Serialize(localOutputView);
}

void FreshTableSnapshotOperator::InternalRelease()
{
    AbstractSnapshotOperator::InternalRelease();
    mToUpload = nullptr;
    LOG_DEBUG("FreshTable ReleaseSnapshot Success!");
}

}  // namespace bss
}  // namespace ock
