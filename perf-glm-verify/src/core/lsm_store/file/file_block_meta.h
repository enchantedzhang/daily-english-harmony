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

#ifndef BOOST_SS_FILE_BLOCK_META_H
#define BOOST_SS_FILE_BLOCK_META_H

#include <memory>

#include "common/path.h"
#include "file_record_meta.h"
#include "lsm_store/block/data_block_stat.h"
#include "lsm_store/block/index_block_stat.h"
#include "state_id_interval.h"

namespace ock {
namespace bss {
class FileBlockMeta {
public:
    FileBlockMeta(const PathRef &path, const DataBlockStatRef &dataBlockStat, const IndexBlockStatRef &indexBlockStat,
                  uint32_t filterBlockSize, uint32_t filterBlockRawSize, uint32_t fileSize,
                  StateIdInterval stateIdInterval, const FileRecordMeta &recordMeta)
        : mPath(path),
          mDataBlockStat(dataBlockStat),
          mIndexBlockStat(indexBlockStat),
          mFilterBlockSize(filterBlockSize),
          mFilterBlockRawSize(filterBlockRawSize),
          mFileSize(fileSize),
          mStateIdInterval(stateIdInterval),
          mRecordMeta(recordMeta)
    {
    }

    inline uint32_t GetFileSize() const
    {
        return mFileSize;
    }

    inline FullKeyRef GetStartKey() const
    {
        return mDataBlockStat->GetStartKey();
    }

    inline FullKeyRef GetEndKey() const
    {
        return mDataBlockStat->GetEndKey();
    }

    inline StateIdInterval GetStateIdInterval() const
    {
        return mStateIdInterval;
    }

    inline uint32_t GetFilterBlockSize() const
    {
        return mFilterBlockSize;
    }

    inline uint32_t GetFilterBlockRawSize() const
    {
        return mFilterBlockRawSize;
    }

    inline uint32_t GetDataBlockCount() const
    {
        return mDataBlockStat->GetNumDataBlock();
    }

    inline uint32_t GetTotalDataBlockSize() const
    {
        return mDataBlockStat->GetTotalDataBlockSize();
    };

    inline uint32_t GetTotalDataBlockRawSize() const
    {
        return mDataBlockStat->GetTotalDataBlockRawSize();
    }

    inline uint32_t GetTotalIndexBlockSize() const
    {
        return mIndexBlockStat->GetTotalIndexBlockSize();
    }

    inline uint32_t GetTotalIndexBlockRawSize() const
    {
        return mIndexBlockStat->GetTotalIndexBlockRawSize();
    }

    inline uint32_t GetKeyCount()
    {
        return mDataBlockStat->GetTotalNumKeys();
    }

    inline const FileRecordMeta &GetRecordMeta() const
    {
        return mRecordMeta;
    }

private:
    PathRef mPath = nullptr;
    DataBlockStatRef mDataBlockStat = nullptr;
    IndexBlockStatRef mIndexBlockStat = nullptr;
    uint32_t mFilterBlockSize = 0;
    uint32_t mFilterBlockRawSize = 0;
    uint32_t mFileSize = 0;
    StateIdInterval mStateIdInterval;
    FileRecordMeta mRecordMeta;
};
using FileBlockMetaRef = std::shared_ptr<FileBlockMeta>;

}  // namespace bss
}  // namespace ock
#endif  // BOOST_SS_FILE_BLOCK_META_H
