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

#ifndef BOOST_SS_TABLE_DESCRIPTION_H
#define BOOST_SS_TABLE_DESCRIPTION_H

#include <cstdint>
#include <memory>
#include <string>

#include "config.h"
#include "state_type.h"

namespace ock {
namespace bss {
enum class SerializerType : uint8_t { UNKNOWN, BINARY_ROW_DATA_SERIALIZER };

struct FieldType {
    uint16_t mFixedBytes;
    bool mIsIntegral;
};

struct TypeSerializer {
    SerializerType mSerType = SerializerType::UNKNOWN;
    std::vector<FieldType> mFields;
};

struct TableSerializer {
    TypeSerializer mKeySerializer;
    TypeSerializer mKey2Serializer;
};

class TableDescription {
public:
    TableDescription(StateType stateType, std::string tableName, int64_t tableTTL, TableSerializer tblSerializer,
                     const Config &config)
        : mTableName(std::move(tableName)),
          mTableSerializer(std::move(tblSerializer)),
          mStateType(stateType),
          mTableTTL(tableTTL),
          mStartGroup(config.mStartGroup),
          mEndGroup(config.mEndGroup),
          mMaxParallelism(config.mMaxParallelism),
          mPeakFilterElemNum(config.mPeakFilterElemNum)
    {
    }
    inline const std::string &GetTableName() const
    {
        return mTableName;
    }
    inline const TableSerializer &GetTableSerializer() const
    {
        return mTableSerializer;
    }
    inline StateType GetStateType() const
    {
        return mStateType;
    }
    inline int64_t GetTableTTL() const
    {
        return mTableTTL;
    }
    inline void SetTableTTL(int64_t ttl)
    {
        mTableTTL = ttl;
    }
    inline uint32_t GetStartGroup() const
    {
        return mStartGroup;
    }
    inline uint32_t GetEndGroup() const
    {
        return mEndGroup;
    }
    inline uint32_t GetMaxParallelism() const
    {
        return mMaxParallelism;
    }
    inline int32_t GetPeakFilterElemNum() const
    {
        return mPeakFilterElemNum;
    }

    inline bool Equals(const TableDescription &des) const
    {
        if (mTableName != des.mTableName) {
            return false;
        }
        if (mStateType != des.mStateType) {
            return false;
        }
        if (mStartGroup != des.mStartGroup || mEndGroup != des.mEndGroup || mMaxParallelism != des.mMaxParallelism) {
            return false;
        }
        return true;
    }

    std::string ToString() const
    {
        return "TableDescription(name: " + mTableName + ", type: " + std::to_string(mStateType) +
               ", ttl: " + std::to_string(mTableTTL) + ")";
    }

    inline uint32_t HashCode()
    {
        return std::hash<std::string>()(mTableName) + static_cast<uint32_t>(mStateType) + mStartGroup + mEndGroup +
               mMaxParallelism;
    }

private:
    std::string mTableName;
    TableSerializer mTableSerializer;
    StateType mStateType;
    int64_t mTableTTL = -1;
    uint32_t mStartGroup;
    uint32_t mEndGroup;
    uint32_t mMaxParallelism;
    int32_t mPeakFilterElemNum;
};
using TableDescriptionRef = std::shared_ptr<TableDescription>;
}  // namespace bss
}  // namespace ock

#endif
