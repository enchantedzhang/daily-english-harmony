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

#ifndef BOOST_SS_FILE_META_DATA_H
#define BOOST_SS_FILE_META_DATA_H

#include <functional>
#include <string>
#include <utility>

#include "binary/key/full_key.h"
#include "common/state_id_interval.h"
#include "file_address_util.h"
#include "file_cache_type.h"
#include "file_meta.h"
#include "file_meta_base.h"
#include "file_record_meta.h"
#include "group_range.h"
#include "include/bss_types.h"
#include "order_range.h"
#include "path_transform.h"

namespace ock {
namespace bss {
class FileMetaData;
using FileMetaDataRef = std::shared_ptr<FileMetaData>;

class FileMetaData : public FileMetaBase {
public:
    // 定义FileMetaData的比较器FileMetaDataComparator.
    using FileMetaDataComparator = std::function<bool(const FileMetaDataRef &, const FileMetaDataRef &)>;

    static bool Compare(const FileMetaDataRef &f1, const FileMetaDataRef &f2)
    {
        // 1. 比较两个文件元数据的GroupRange的epoch值.
        int64_t result = f1->GetGroupRange()->GetEpoch() - f2->GetGroupRange()->GetEpoch();
        if (result != 0) {
            return result < 0;
        }
        // 2. 如果Epoch值相等，再比较两个文件元数据的seqId值.
        int64_t diff = f1->GetSeqId() - f2->GetSeqId();
        return diff < 0;
    }

    FileMetaData(uint64_t fileAddress, int64_t seqId, uint64_t fileSize, const FullKeyRef &smallest,
                 const FullKeyRef &largest, GroupRangeRef groupRange, HashCodeOrderRangeRef orderRange,
                 std::string identifier, StateIdInterval stateIdInterval, FileStatus fileStatus = FileStatus::LOCAL,
                 const FileRecordMeta &recordMeta = FileRecordMeta())
        : FileMetaBase(identifier, fileAddress, fileSize),
          mSeqId(seqId),
          mSmallest(std::move(smallest)),
          mLargest(std::move(largest)),
          mGroupRange(std::move(groupRange)),
          mOrderRange(std::move(orderRange)),
          mStateIdInterval(stateIdInterval),
          mFileStatus(fileStatus),
          mRecordMeta(recordMeta)
    {
    }

    std::string GetShortFileName()
    {
        if (mShortName.empty()) {
            if (mIdentifier.length() > NO_6) {
                // 获取最后一个'_'的位置.
                size_t pos = mIdentifier.find_last_of('_');
                // 判断是否存在'_'且不是最后一个.
                if (pos != std::string::npos && pos != mIdentifier.size() - 1) {
                    // 将mIdentifier的前6个字符和最后一个'_'之后的字符拼接作为mShortName.
                    mShortName = mIdentifier.substr(0, NO_6) + mIdentifier.substr(pos);
                } else {
                    mShortName = mIdentifier;
                }
            } else {
                mShortName = mIdentifier;
            }
        }
        return mShortName;
    }

    inline int64_t GetSeqId() const
    {
        return mSeqId;
    }

    inline FullKeyRef GetSmallest() const
    {
        return mSmallest;
    }

    inline FullKeyRef GetLargest() const
    {
        return mLargest;
    }

    inline GroupRangeRef GetGroupRange() const
    {
        return mGroupRange;
    }

    inline HashCodeOrderRangeRef GetOrderRange() const
    {
        return mOrderRange;
    }

    inline StateIdInterval GetStateIdInterval() const
    {
        return mStateIdInterval;
    }

    inline const FileRecordMeta &GetRecordMeta() const
    {
        return mRecordMeta;
    }

    inline static uint64_t HashCodeInt(uint64_t value)
    {
        return (value ^ (value >> NO_32));
    }

    inline uint64_t HashCode() const
    {
        return HashCodeInt(mSeqId) + NO_31 * HashCodeInt(mFileSize) + NO_31 * HashCodeInt(mFileAddress) +
               NO_31 * HashCodeInt(NO_1);
    }

    bool Equals(const FileMetaDataRef &other) const
    {
        return mIdentifier == other->GetIdentifier() && mFileAddress == other->GetFileAddress() &&
               mFileSize == other->GetFileSize() && mSeqId == other->GetSeqId() &&
               mSmallest->EqualsFullKey(other->GetSmallest()) == 0 && mLargest->EqualsFullKey(other->GetLargest()) &&
               mGroupRange->Equals(other->GetGroupRange()) && mOrderRange == other->GetOrderRange() &&
               mShortName == other->GetShortFileName() && mStateIdInterval == other->mStateIdInterval &&
               mRecordMeta.Equals(other->GetRecordMeta());
    }

    inline FileStatus GetFileStatus() const
    {
        return mFileStatus;
    }

    std::string ToString()
    {
        return "FileMetaData { fileAddress=" + std::to_string(mFileAddress) + ", seqID=" + std::to_string(mSeqId) +
               ", fileSize=" + std::to_string(mFileSize) + ", mGroupRange=" + mGroupRange->ToString() +
               ", orderRange=" + mOrderRange->ToString() +
               ", identifier=" + PathTransform::ExtractFileName(mIdentifier) +
               ", fileId=" + std::to_string(FileAddressUtil::GetFileId(mFileAddress)) +
               ", fileStatus=" + std::to_string(mFileStatus) + "}";
    }

    class Builder {
    public:
        void Fill(const FullKeyRef &smallest, const FullKeyRef &largest, uint64_t fileSize, uint64_t fileAddress,
                  int64_t seqId, const GroupRangeRef &groupRange, const HashCodeOrderRangeRef &orderRange,
                  std::string identifier, StateIdInterval interval, FileStatus fileStatus = FileStatus::LOCAL,
                  const FileRecordMeta &recordMeta = FileRecordMeta())
        {
            mSmallest = smallest;
            mLargest = largest;
            mFileSize = fileSize;
            mFileAddress = fileAddress;
            mSeqId = seqId;
            mGroupRange = groupRange;
            mOrderRange = orderRange;
            mIdentifier = identifier;
            mStateIdInterval = interval;
            mFileStatus = fileStatus;
            mRecordMeta = recordMeta;
        }

        FileMetaDataRef Build() const
        {
            if (UNLIKELY(mSeqId < 0L)) {
                return nullptr;
            }
            return std::make_shared<FileMetaData>(mFileAddress, mSeqId, mFileSize, mSmallest, mLargest, mGroupRange,
                                                  mOrderRange, mIdentifier, mStateIdInterval, mFileStatus, mRecordMeta);
        }

    private:
        FullKeyRef mSmallest = nullptr;
        FullKeyRef mLargest = nullptr;
        uint64_t mFileSize = 0;
        uint64_t mFileAddress = 0;
        int64_t mSeqId = -1;
        GroupRangeRef mGroupRange = nullptr;
        HashCodeOrderRangeRef mOrderRange;
        std::string mIdentifier;
        StateIdInterval mStateIdInterval;
        FileStatus mFileStatus = FileStatus::LOCAL;
        FileRecordMeta mRecordMeta;
    };
    using BuilderRef = std::shared_ptr<Builder>;

    static BuilderRef NewBuilder()
    {
        return std::make_shared<Builder>();
    }

private:
    int64_t mSeqId = 0;
    FullKeyRef mSmallest = nullptr;
    FullKeyRef mLargest = nullptr;
    GroupRangeRef mGroupRange = nullptr;
    HashCodeOrderRangeRef mOrderRange;
    std::string mShortName;
    // 原则上来说，通过mSmallest和mLargest过滤就可以同时完成state id的过滤，不过目前key的比较器中把state id放在最后了
    // 将key的比较器(FileMetaDataGroup::Builder::CreateFileMetaDataComparator)修改为先比较state id, 可删除该字段.
    StateIdInterval mStateIdInterval;
    FileStatus mFileStatus;
    FileRecordMeta mRecordMeta;
};

}  // namespace bss
}  // namespace ock
#endif  // BOOST_SS_FILE_META_DATA_H
