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

#include "include/config.h"

#include <linux/limits.h>
#include <unistd.h>

namespace ock {
namespace bss {

void Config::Init(uint32_t startGroup, uint32_t endGroup, uint32_t maxParallelism)
{
    mStartGroup = startGroup;
    mEndGroup = endGroup;
    mMaxParallelism = maxParallelism;

    std::string localBasePath = ".";
    char buffer[PATH_MAX];
    if (getcwd(buffer, sizeof(buffer)) != nullptr) {
        localBasePath = std::string(buffer);
    }
    mLocalPath = localBasePath;
    mRemotePath = localBasePath;
    mBackendUID = "test";
}

void Config::SetCompressionLevelPolicy(const std::vector<std::string> &compressionLevelPolicy)
{
    if (compressionLevelPolicy.empty()) {
        return;
    }
    mCompressionLevelPolicy.clear();
    for (std::size_t i = 0; i < compressionLevelPolicy.size(); ++i) {
        if (i > GetFileStoreNumLevels()) {
            break;
        }
        mCompressionLevelPolicy.emplace_back(CompressAlgoUtil::CompressAlgoTransform(compressionLevelPolicy[i]));
    }
}

}  // namespace bss
}  // namespace ock