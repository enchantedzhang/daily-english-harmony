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

#ifndef BOOST_SS_VERSION_EDIT_H
#define BOOST_SS_VERSION_EDIT_H

#include <unordered_map>
#include <unordered_set>

#include "lsm_store/file/file_meta_data.h"
#include "lsm_store/file/group_range.h"

namespace ock {
namespace bss {
struct GroupRangeHash {
    size_t operator()(const GroupRangeRef &groupRange) const
    {
        return groupRange->HashCode();
    }
};

struct GroupRangeEqual {
    bool operator()(const GroupRangeRef &lhs, const GroupRangeRef &rhs) const
    {
        return lhs->Equals(rhs);
    }
};

using NewFiles = std::unordered_map<GroupRangeRef, std::unordered_map<uint32_t, std::unordered_set<FileMetaDataRef>>,
                                    GroupRangeHash, GroupRangeEqual>;
using DelFiles = std::unordered_map<GroupRangeRef, std::unordered_map<uint32_t, std::unordered_set<std::string>>,
                                    GroupRangeHash, GroupRangeEqual>;

class VersionEdit;
using VersionEditRef = std::shared_ptr<VersionEdit>;
class VersionEdit {
public:
    VersionEdit(const NewFiles &newFiles, const DelFiles &deletedFiles)
        : mNewFiles(newFiles), mDeletedFiles(deletedFiles)
    {
    }

    inline NewFiles GetNewFiles() const
    {
        return mNewFiles;
    }

    inline DelFiles GetDeletedFiles() const
    {
        return mDeletedFiles;
    }

    class Builder {
    public:
        Builder *DeleteFile(const GroupRangeRef &groupRange, uint32_t levelId, const std::string &fileIdentifier)
        {
            auto iter = mDeletedFiles.find(groupRange);
            if (iter == mDeletedFiles.end()) {
                std::unordered_map<uint32_t, std::unordered_set<std::string>> m;
                std::unordered_set<std::string> fileNames;
                fileNames.insert(fileIdentifier);
                m[levelId] = fileNames;
                mDeletedFiles[groupRange] = m;
            } else {
                auto &m = iter->second;
                auto fileSetIt = m.find(levelId);
                if (fileSetIt == m.end()) {
                    std::unordered_set<std::string> fileNames;
                    fileNames.insert(fileIdentifier);
                    m[levelId] = fileNames;
                } else {
                    fileSetIt->second.insert(fileIdentifier);
                }
            }
            return this;
        }

        Builder *AddFile(const GroupRangeRef &groupRange, const uint32_t levelId, const FileMetaDataRef &fileMetaData)
        {
            auto &map = mNewFiles[groupRange];
            map[levelId].insert(fileMetaData);
            return this;
        }

        VersionEditRef Build()
        {
            return std::make_shared<VersionEdit>(mNewFiles, mDeletedFiles);
        }

        static Builder *NewBuilder()
        {
            return new (std::nothrow) Builder();
        }

    public:
        NewFiles mNewFiles;
        DelFiles mDeletedFiles;
    };

private:
    NewFiles mNewFiles;
    DelFiles mDeletedFiles;
};

}  // namespace bss
}  // namespace ock
#endif