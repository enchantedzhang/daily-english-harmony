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

#include "file_factory.h"

namespace ock {
namespace bss {
std::shared_ptr<FileFactory> FileFactory::CreateFileFactory(const ConfigRef &config, const MemManagerRef &memManager)
{
    auto fileFactoryType = static_cast<FileFactoryType>(config->GetFileFactoryType());
    if (fileFactoryType != FileFactoryType::HASH) {
        LOG_ERROR("Invalid file factory type, type:" << static_cast<uint32_t>(fileFactoryType));
        return nullptr;
    }
    auto fileFactory = std::make_shared<FileFactory>(config, memManager);
    auto ret = fileFactory->Initialize();
    if (UNLIKELY(ret != BSS_OK)) {
        return nullptr;
    }
    return fileFactory;
}

}  // namespace bss
}  // namespace ock