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

#ifndef BOOST_SS_BLOB_STRUCTURE_H
#define BOOST_SS_BLOB_STRUCTURE_H

#include <memory>

struct BlobIndexEntry {
    uint64_t mBlobId = 0;
    uint64_t mSeqId = 0;
    uint32_t mOffset = 0;

    BlobIndexEntry(uint64_t blobId, uint64_t seqId, uint32_t offset)
    {
        mBlobId = blobId;
        mSeqId = seqId;
        mOffset = offset;
    }
};
using BlobIndexEntryRef = std::shared_ptr<BlobIndexEntry>;

#endif