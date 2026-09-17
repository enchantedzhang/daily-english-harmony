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

#ifndef BOOST_SS_DATA_BLOCK_WRITER_H
#define BOOST_SS_DATA_BLOCK_WRITER_H

#include <vector>

#include "binary/slice_binary.h"
#include "common/io/output_view.h"
#include "data_block_index_writer.h"
#include "lsm_store/file/file_mem_allocator.h"

namespace ock {
namespace bss {
class DataBlockWriter {
public:
    DataBlockWriter(float hashIndexLoadRatio, const MemManagerRef &memManager, FileProcHolder holder)
        : mMemManager(memManager), mHolder(holder)
    {
        mIndexBuilder = std::make_shared<DataBlockIndexWriter>(hashIndexLoadRatio, holder);
        mPrimaryOutputView = std::make_shared<OutputView>(mMemManager, holder);
        mSecondaryOutputView = std::make_shared<OutputView>(mMemManager, holder);
    }

    ~DataBlockWriter()
    {
        std::vector<uint32_t>().swap(mPrimaryOffset);
        std::vector<uint32_t>().swap(mSecondaryOffset);
    }

    uint32_t EstimateAfter(const KeyValueRef &keyValue);

    uint32_t CurrentEstimateSize();

    inline bool IsEmpty() const
    {
        return mNumKeyValuePairs == 0;
    }

    BResult Finish(ByteBufferRef &byteBuffer);

    BResult WritePrimaryValue(const KeyValueRef &keyValue);

    BResult WriteSecondaryKeyAndValue(const KeyValueRef &keyValue, const OutputViewRef &outputView,
                                      uint32_t primaryKeyIndex, uint32_t secondaryKeyIndex);

    BResult WriteValue(const Value &value, const OutputViewRef &outputView);

    BResult Add(const KeyValueRef &keyValue);

    inline uint32_t NumKeys() const
    {
        return mNumKeyValuePairs;
    }

    inline FullKeyRef GetStartKey() const
    {
        return mStartKey;
    }

    inline FullKeyRef GetEndKey() const
    {
        return mEndKey;
    }

    inline KeyValueRef GetEndKeyValue() const
    {
        return mEndKeyValue;
    }

    void Reset();

private:
    DataBlockIndexWriterRef mIndexBuilder = nullptr;
    OutputViewRef mPrimaryOutputView = nullptr;
    OutputViewRef mSecondaryOutputView = nullptr;
    FullKeyRef mLastInternalKey = nullptr;
    std::vector<uint32_t> mPrimaryOffset;
    std::vector<uint32_t> mSecondaryOffset;
    uint32_t mNumKeyValuePairs = 0;
    FullKeyRef mStartKey = nullptr;
    FullKeyRef mEndKey = nullptr;
    KeyValueRef mEndKeyValue = nullptr;
    KeyValueRef mLastKeyValue = nullptr;
    MemManagerRef mMemManager = nullptr;
    FileProcHolder mHolder;
};
using DataBlockWriterRef = std::shared_ptr<DataBlockWriter>;

}  // namespace bss
}  // namespace ock
#endif  // BOOST_SS_DATA_BLOCK_WRITER_H