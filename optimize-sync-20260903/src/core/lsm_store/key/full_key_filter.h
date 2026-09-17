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

#ifndef BOOST_SS_FULL_KEY_FILTER_H
#define BOOST_SS_FULL_KEY_FILTER_H

#include "binary/lsm_binary.h"

namespace ock {
namespace bss {
class FullKeyFilter {
public:
    virtual ~FullKeyFilter() = default;

    virtual bool Filter(const KeyValueRef &keyValue) = 0;

    virtual bool Filter(const Key &key, const Value &value) = 0;
};
using FullKeyFilterRef = std::shared_ptr<FullKeyFilter>;

}  // namespace bss
}  // namespace ock
#endif  // BOOST_SS_FULL_KEY_FILTER_H