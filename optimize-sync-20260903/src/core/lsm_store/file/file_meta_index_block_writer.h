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

#ifndef BOOST_SS_FILE_META_INDEX_BLOCK_WRITER_H
#define BOOST_SS_FILE_META_INDEX_BLOCK_WRITER_H

#include <memory>

#include "common/io/output_view.h"
#include "lsm_store/block/block_meta.h"

namespace ock {
namespace bss {
class FileMetaIndexBlockWriter {
public:
    explicit FileMetaIndexBlockWriter(OutputViewRef &outputView) : mOutputView(outputView)
    {
    }

    inline void AddMetaBlock(const BlockHandleRef &metaBlockMeta)
    {
        mMetaBlockVec.push_back(metaBlockMeta);
    }

    BResult Finish(ByteBufferRef &byteBuffer);

private:
    OutputViewRef mOutputView = nullptr;
    std::vector<BlockHandleRef> mMetaBlockVec;
};
using FileMetaIndexBlockWriterRef = std::shared_ptr<FileMetaIndexBlockWriter>;

class FileMetaIndex;
using FileMetaIndexRef = std::shared_ptr<FileMetaIndex>;
class FileMetaIndex {
public:
    inline BlockHandleRef GetMetaBlock(uint8_t index) const
    {
        if (UNLIKELY(index >= mMetaVec.size())) {
            return nullptr;
        }
        return mMetaVec[index];
    }

    inline void AddMetaBlock(const BlockHandleRef &metaBlockMeta)
    {
        mMetaVec.push_back(metaBlockMeta);
    }

    static FileMetaIndexRef CreateFileMetaIndex(ByteBufferRef &byteBuffer);

private:
    std::vector<BlockHandleRef> mMetaVec;
};

}  // namespace bss
}  // namespace ock
#endif  // BOOST_SS_FILE_META_INDEX_BLOCK_WRITER_H