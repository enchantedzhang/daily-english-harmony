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

#ifndef BSS_DEV_FILE_STORE_ID_H
#define BSS_DEV_FILE_STORE_ID_H

#include "common/io/file_input_view.h"
#include "common/io/file_output_view.h"
#include "group_range.h"
#include "slice_table/index/hash_code_range.h"

namespace ock {
namespace bss {
class FileStoreID {
public:
    FileStoreID() = default;

    FileStoreID(const GroupRangeRef &groupRange, int32_t internalIndex, const HashCodeRangeRef &hashCodeRange)
        : mGroupRange(groupRange), mInternalIndex(internalIndex), mHashCodeRange(hashCodeRange)
    {
    }

    FileStoreID(const GroupRangeRef &groupRange, int32_t internalIndex)
        : mGroupRange(groupRange), mInternalIndex(internalIndex), mHashCodeRange(std::make_shared<HashCodeRange>())
    {
    }

    inline GroupRangeRef GetGroupRange() const
    {
        return mGroupRange;
    }

    inline int32_t GetInternalIndex() const
    {
        return mInternalIndex;
    }

    inline HashCodeRangeRef GetHashCodeRange() const
    {
        return mHashCodeRange;
    }

    inline void Serialize(const FileOutputViewRef &outputView)
    {
        outputView->WriteInt32(mGroupRange->GetStartGroup());
        outputView->WriteInt32(mGroupRange->GetEndGroup());
        outputView->WriteUint64(static_cast<uint64_t>(mGroupRange->GetEpoch()));
        outputView->WriteInt32(mInternalIndex);
    }

    inline static std::shared_ptr<FileStoreID> Deserialize(const FileInputViewRef &inputView)
    {
        RETURN_NULLPTR_AS_NULLPTR(inputView);
        int32_t startGroup;
        auto ret = inputView->Read(startGroup);
        if (UNLIKELY(ret != BSS_OK)) {
            LOG_ERROR("Read start group failed form input view.");
            return nullptr;
        }
        int32_t endGroup;
        ret = inputView->Read(endGroup);
        if (UNLIKELY(ret != BSS_OK)) {
            LOG_ERROR("Read end group failed form input view.");
            return nullptr;
        }
        uint64_t epoch;
        ret = inputView->Read(epoch);
        if (UNLIKELY(ret != BSS_OK)) {
            LOG_ERROR("Read epoch failed form input view.");
            return nullptr;
        }
        int32_t internalIndex;
        ret = inputView->Read(internalIndex);
        if (UNLIKELY(ret != BSS_OK)) {
            LOG_ERROR("Read internal index failed form input view.");
            return nullptr;
        }
        return std::make_shared<FileStoreID>(std::make_shared<GroupRange>(startGroup, endGroup, epoch), internalIndex);
    }

    inline bool Equals(const FileStoreID &other)
    {
        return mInternalIndex == other.mInternalIndex && mGroupRange->Equals(other.mGroupRange) &&
               mHashCodeRange == other.mHashCodeRange;
    }

    inline uint32_t HashCode()
    {
        return std::hash<int32_t>()(mGroupRange->GetStartGroup()) + std::hash<int32_t>()(mGroupRange->GetEndGroup()) +
               std::hash<int64_t>()(mGroupRange->GetEpoch()) + std::hash<int32_t>()(mInternalIndex) +
               std::hash<uint32_t>()(mHashCodeRange->GetStartHashCode()) +
               std::hash<uint32_t>()(mHashCodeRange->GetEndHashCode());
    }

    inline std::string ToString()
    {
        return "FileStoreID{mGroupRange=" + mGroupRange->ToString() + ", hashcodeRange=" + mHashCodeRange->ToString() +
               ", index=" + std::to_string(mInternalIndex) + '}';
    }

private:
    GroupRangeRef mGroupRange = nullptr;
    int32_t mInternalIndex = 0;
    HashCodeRangeRef mHashCodeRange = nullptr;
};
using FileStoreIDRef = std::shared_ptr<FileStoreID>;

}  // namespace bss
}  // namespace ock
#endif  // BSS_DEV_FILE_STORE_ID_H