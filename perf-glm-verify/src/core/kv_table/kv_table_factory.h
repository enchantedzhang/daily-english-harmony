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

#ifndef KV_TABLE_FACTORY_H
#define KV_TABLE_FACTORY_H
#include "include/boost_state_table.h"

namespace ock {
namespace bss {
class KVTableFactory {
public:
    static AbstractTableRef CreateFromDescription(TableDescriptionRef description)
    {
        if (!description) {
            return nullptr;
        }

        switch (description->GetStateType()) {
            case StateType::VALUE: {
                KVTableRef table = std::make_shared<KVTable>();
                return table;
            }
            case StateType::LIST: {
                KListTableRef table = std::make_shared<KListTable>();
                return table;
            }
            case StateType::MAP: {
                KMapTableRef table = std::make_shared<KMapTable>();
                return table;
            }
            case StateType::SUB_VALUE: {
                NsKVTableRef table = std::make_shared<NsKVTable>();
                return table;
            }
            case StateType::SUB_LIST: {
                NsKListTableRef table = std::make_shared<NsKListTable>();
                return table;
            }
            case StateType::SUB_MAP: {
                NsKMapTableRef table = std::make_shared<NsKMapTable>();
                return table;
            }
            default:
                return nullptr;
        }
    }
};
}  // namespace bss
}  // namespace ock
#endif  // KV_TABLE_FACTORY_H
