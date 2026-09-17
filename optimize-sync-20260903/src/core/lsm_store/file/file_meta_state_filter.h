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

#ifndef BOOST_SS_FILE_META_STATE_FILTER_H
#define BOOST_SS_FILE_META_STATE_FILTER_H

#include "group_range.h"
#include "lsm_store/key/full_key_filter.h"
#include "order_range.h"
#include "state_filter_manager.h"

namespace ock {
namespace bss {
class FileMetaStateFilter : public FullKeyFilter {
public:
    FileMetaStateFilter(const GroupRangeRef &fileMetaGroupRange, const HashCodeOrderRangeRef &fileMetaOrderRange,
                        const StateFilterManagerRef &stateFilterManager)
        : mFileMetaGroupRange(fileMetaGroupRange),
          mFileMetaOrderRange(fileMetaOrderRange),
          mStateFilterManager(stateFilterManager)
    {
    }

    bool Filter(const KeyValueRef &keyValue) override
    {
        Key &key = keyValue->key;
        uint32_t keyHashCode = key.KeyHashCode();
        if (!mFileMetaOrderRange->Contains(keyHashCode)) {
            return true;
        }
        int group = mStateFilterManager->GetGroup(keyHashCode, key.StateId());
        if (!mFileMetaGroupRange->ContainsGroup(group)) {
            return true;
        }

        Value &value = keyValue->value;
        return mStateFilterManager->StateFilter(key.StateId(), value.SeqId());
    }

    bool Filter(const Key &key, const Value &value) override
    {
        return false;
    }

private:
    GroupRangeRef mFileMetaGroupRange = nullptr;
    HashCodeOrderRangeRef mFileMetaOrderRange = nullptr;
    StateFilterManagerRef mStateFilterManager = nullptr;
};

}  // namespace bss
}  // namespace ock
#endif  // BOOST_SS_FILE_META_STATE_FILTER_H
