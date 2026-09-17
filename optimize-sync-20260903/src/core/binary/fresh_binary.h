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

#ifndef BOOST_SS_FRESH_BINARY_H
#define BOOST_SS_FRESH_BINARY_H

#include <iostream>

#include "binary/key/key.h"
#include "binary/value/value.h"
#include "boost/node_type.h"
#include "common/bss_def.h"
#include "value/value_type.h"
namespace ock {
namespace bss {

#pragma pack(1)
/**
 * The data structure of the key in the memory of the fresh table
 */
struct FreshIndexNode {
private:
    uint32_t mKeyHashCode;
    uint32_t mKeyNodeOffset;
    uint32_t mValueNodeOffset;
    uint32_t mNextIndexNodeOffset;

public:
    /**
     * Key hash code.
     * @return
     */
    inline uint32_t KeyHashCode() const
    {
        return mKeyHashCode;
    }
    /**
     * Key node offset from segment start.
     * @return
     */
    inline uint32_t KeyNodeOffset() const
    {
        return mKeyNodeOffset;
    }
    /**
     * Value node offset from segment start.
     * @return
     */
    inline uint32_t ValueNodeOffset() const
    {
        return mValueNodeOffset;
    }
    /**
     * Next index node offset from segment start.
     * @return
     */
    inline uint32_t NextIndexNodeOffset() const
    {
        return mNextIndexNodeOffset;
    }
};

struct FreshSglKeyData {
    uint32_t mHashCode;
    uint16_t mStateId;
    uint8_t mKey[0];
};

using FreshPriKeyData = FreshSglKeyData;

struct FreshSecKeyData {
    uint32_t mHashCode;
    uint8_t mKey[0];
};
struct FreshKeyNode;
using FreshKeyNodePtr = FreshKeyNode *;

struct FreshKeyNode {
private:
    uint32_t mKeyLen;  // Key length, include HashCode, StateId and KeyData length for primary key, include HashCode and
                       // KeyData for secondary key.
    union {
        /**
         * for single key.
         */
        FreshSglKeyData mSglKey;
        /**
         * for dual key.
         */
        // primary key.
        FreshPriKeyData mPriKey;
        // secondary key.
        FreshSecKeyData mSecKey;
    };

public:
    static FreshKeyNodePtr FromBuffer(uint8_t *buffer)
    {
        return reinterpret_cast<FreshKeyNode *>(buffer);
    }

    /**
     * DUAL KEY BEGIN.
     */
    /**
     * Primary key data length, exclude HashCode(4) and StateId(2)
     * @return
     */
    inline uint32_t PriKeyDataLen() const
    {
        return mKeyLen - NO_6;
    }
    /**
     * Primary key data
     * @return
     */
    inline const uint8_t *PriKeyData() const
    {
        return mPriKey.mKey;
    }
    /**
     * Primary key hash code, calculated by keyHashCode ^ NameSpaceHashCode(if it has name space), else it is the same
     * with keyHashCode.
     * @return
     */
    inline uint32_t PriKeyHashCode() const
    {
        return mPriKey.mHashCode;
    }
    /**
     * Primary key state id, include table type and key group information.
     * @return
     */
    inline uint16_t PriKeyStateId() const
    {
        return mPriKey.mStateId;
    }
    /**
     * Key length exclude HashCode(4), the length of key data.
     * @return
     */
    inline uint32_t SecKeyDataLen() const
    {
        return mKeyLen - NO_4;
    }
    /**
     * Secondary key data
     * @return
     */
    inline const uint8_t *SecKeyData() const
    {
        return mSecKey.mKey;
    }
    /**
     * Secondary hash code, calculated by HashCode::Hash()
     * keyHashCode.
     * @return
     */
    inline uint32_t SecKeyHashCode() const
    {
        return mSecKey.mHashCode;
    }

    /**
     * Equal lookup primary key
     */
    inline bool Equal(const PriKeyNode &other) const
    {
        return (PriKeyStateId() == other.StateId() && (PriKeyHashCode() == other.HashCode()) &&
                (PriKeyDataLen() == other.KeyLen()) && (memcmp(PriKeyData(), other.KeyData(), other.KeyLen()) == 0));
    }

    inline bool Equal(const SecKeyNode &other) const
    {
        return (SecKeyHashCode() == other.HashCode()) && (SecKeyDataLen() == other.KeyLen()) &&
               (memcmp(SecKeyData(), other.KeyData(), other.KeyLen()) == 0);
    }

    /**
     * DUAL KEY END
     */

    /**
     * SINGLE KEY BEGIN
     */
    /**
     * Single key data length, exclude HashCode(4) and StateId(2)
     * @return
     */
    inline uint32_t SglKeyDataLen() const
    {
        return mKeyLen - NO_6;
    }
    /**
     * Single key data
     * @return
     */
    inline const uint8_t *SglKeyData() const
    {
        return mSglKey.mKey;
    }
    /**
     * Single key hash code, calculated by keyHashCode ^ NameSpaceHashCode(if it has name space), else it is the same
     * with keyHashCode.
     * @return
     */
    inline uint32_t SglKeyHashCode() const
    {
        return mSglKey.mHashCode;
    }
    /**
     * Single key state id, include table type and key group information.
     * @return
     */
    inline uint16_t SglKeyStateId() const
    {
        return mSglKey.mStateId;
    }
    /**
     * SINGLE KEY END
     */

    /**
     * To single key or primary key node.
     * @param priKeyNode primary key node.
     */
    inline void ToPriKeyNode(PriKeyNode &priKeyNode) const
    {
        priKeyNode.Init(PriKeyStateId(), PriKeyHashCode(), PriKeyData(), PriKeyDataLen());
    }

    /**
     * To secondary key.
     * @param secKeyNode secondary key node.
     */
    inline void ToSecKeyNode(SecKeyNode &secKeyNode) const
    {
        secKeyNode.Init(SecKeyHashCode(), SecKeyData(), SecKeyDataLen());
    }
};

struct FreshValueNode;
using FreshValueNodePtr = FreshValueNode *;
struct FreshValueNode {
private:
    // If this is a value, mValueLen includes ValueType, SeqId and Value length.
    // If this is a map, then it is the lengths of mIndexNodeSize(4),mBucketCount(4),mSectionIndexBase(4) fields, it is
    // 12 bytes.
    uint32_t mValueLen;
    NodeType mNodeType;
    union {
        uint8_t mBase[0];
        struct {
            uint64_t mSeqId;
            ValueType mValueType;
            uint8_t mValue[0];
        } mValue;
        struct {
            uint32_t mIndexNodeSize;
            uint32_t mBucketCount;
            uint32_t mSectionIndexBase;
        } mMap;
        struct {
            uint64_t mSeqId;
            ValueType mValueType;
            uint8_t mValue[0];
        } mList;
    };

public:
    static FreshValueNodePtr FromBuffer(uint8_t *buffer)
    {
        return reinterpret_cast<FreshValueNode *>(buffer);
    }

    FreshValueNode() = default;

    FreshValueNode(NodeType bType, uint32_t valLen, ValueType valType, uint64_t seqId)
        : mValueLen(valLen), mNodeType(bType), mList{ seqId, valType, {} }
    {
    }

    /**
     * Binary type, see @NodeType
     * @return
     */
    inline NodeType NodeType() const
    {
        return mNodeType;
    }
    /**
     * Value length, exclude GValueType and SeqId.
     * @return
     */
    inline uint32_t ValueDataLen() const
    {
        return mValueLen - NO_9;
    }

    /**
     * Value type, @ValueType
     * @return
     */
    inline ValueType ValueType() const
    {
        return mValue.mValueType;
    }
    /**
     * Value seq id, used for putting multiple times with the same key.
     * @return
     */
    inline uint64_t ValueSeqId() const
    {
        return mValue.mSeqId;
    }
    /**
     * Value data.
     * @return
     */
    inline const uint8_t *Value() const
    {
        return mValue.mValue;
    }

    inline const uint8_t *MapData() const
    {
        return mBase;
    }
    /**
     * Value data base address.
     * @return
     */
    inline const uint8_t *ValueDataBase() const
    {
        return mBase;
    }

    /**
     * @Visitor f 签名为：bool(FreshValueNode &)，当停止遍历时返回false
     */
    template <class VisitorF> void VisitAsList(const MemorySegment &freshSegment, VisitorF f)
    {
        bool continueVisit = true;
        if (mNodeType == NodeType::SERIALIZED) {
            continueVisit = f(*this);
            return;
        }

        auto curNode = this;
        while (continueVisit && curNode->mNodeType == NodeType::COMPOSITE) {
            continueVisit = f(*curNode);
            auto curPos = *reinterpret_cast<uint32_t *>(curNode->mBase + curNode->mValueLen);
            curNode = FreshValueNode::FromBuffer(freshSegment.GetSegment() + curPos);
        }

        if (continueVisit && curNode->mNodeType == NodeType::SERIALIZED) {
            f(*curNode);
        }
    }
    inline std::string ToString() const
    {
        std::ostringstream oss;
        oss << "ValueType: " << static_cast<int>(mValue.mValueType) << ", ValueLen: " << mValueLen
            << ", seqId: " << mValue.mSeqId << ", ValueData: [";
        if (UNLIKELY(mValue.mValue != nullptr)) {
            uint32_t printLen = std::min(NO_10, mValueLen);
            for (uint32_t i = 0; i < printLen; i++) {
                oss << std::hex << std::uppercase << std::setw(NO_2) << std::setfill('0')
                    << static_cast<int>(mValue.mValue[i]);
            }
        } else {
            oss << "null";
        }

        oss << "]";
        return oss.str();
    }
};

struct FreshNode {
private:
    const FreshIndexNode *mIndexNode{ nullptr };
    const FreshKeyNode *mKeyNode{ nullptr };
    const FreshValueNode *mValueNode{ nullptr };
    const FreshIndexNode *mNextIndexNode{ nullptr };

public:
    /**
     * Unpack in-memory data into a data node data structure
     * @param buffer memory data.
     * @param offset memory start position offset
     */
    FreshNode(const uint8_t *data, uint32_t offset)
    {
        mIndexNode = reinterpret_cast<const FreshIndexNode *>(data + offset);
        mKeyNode = reinterpret_cast<const FreshKeyNode *>(data + mIndexNode->KeyNodeOffset());
        mValueNode = reinterpret_cast<const FreshValueNode *>(data + mIndexNode->ValueNodeOffset());
        mNextIndexNode = reinterpret_cast<const FreshIndexNode *>(data + mIndexNode->NextIndexNodeOffset());
    }
    /**
     * Get index node.
     * @return
     */
    inline const FreshIndexNode *IndexNode() const
    {
        return mIndexNode;
    }
    inline const FreshKeyNode *KeyNode() const
    {
        return mKeyNode;
    }
    inline const FreshValueNode *ValueNode() const
    {
        return mValueNode;
    }
};
#pragma pack()
}  // namespace bss
}  // namespace ock
#endif  // BOOST_SS_FRESH_BINARY_H
