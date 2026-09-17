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

#include "binary_key_value_Item_iterator.h"

#include "blob_store.h"

namespace ock {
namespace bss {
bool BinaryKeyValueItemIterator::HasNext()
{
    Advance();
    if (mCurrentItem == nullptr) {
        return false;
    }
    auto keyValue = mCurrentItem->mKeyValue;
    if (UNLIKELY(!BlobStore::ToBlobValue(keyValue->key, keyValue->value, mTransFunc))) {
        LOG_ERROR("Get value from blob failed, KeyHashCode: " << keyValue->key.KeyHashCode());
        return false;
    }
    mCurrentItem->SetValue(const_cast<uint8_t *>(keyValue->value.ValueData()), keyValue->value.ValueLen());
    return true;
}

BinaryKeyValueItemRef BinaryKeyValueItemIterator::Next()
{
    return mCurrentItem;
}

void BinaryKeyValueItemIterator::Close()
{
    if (mKVIterator != nullptr) {
        mKVIterator->Close();
    }
    if (mPQIterator != nullptr) {
        mPQIterator->Close();
    }
}

void BinaryKeyValueItemIterator::Advance()
{
    AdvanceKVIterator();
    AdvancePQIterator();

    if (mKVItem == nullptr) {
        SetCurrentAsPQ();
        return;
    }
    if (mPQItem == nullptr) {
        SetCurrentAsKV();
        return;
    }

    if (mKVItem->mKeyGroup <= mPQItem->mKeyGroup) {
        SetCurrentAsKV();
    } else {
        SetCurrentAsPQ();
    }
}

void BinaryKeyValueItemIterator::AdvancePQIterator()
{
    if (mPQItem != nullptr) {
        return;
    }
    auto prevItem = mPQItem;
    mPQItem = nullptr;
    if (mPQIterator == nullptr) {
        return;
    }
    while (mPQItem == nullptr && mPQIterator->HasNext()) {
        auto pair = mPQIterator->Next();
        if (pair->value.ValueType() == ValueType::DELETE) {
            continue;
        }

        mPQItem = Convert(pair, prevItem);
    }
}

void BinaryKeyValueItemIterator::AdvanceKVIterator()
{
    if (mKVItem != nullptr) {
        return;
    }
    auto prevItem = mKVItem;
    mKVItem = nullptr;
    if (mKVIterator == nullptr) {
        return;
    }
    while (mKVItem == nullptr && mKVIterator->HasNext()) {
        auto pair = mKVIterator->Next();
        if (pair->value.ValueType() == ValueType::DELETE) {
            continue;
        }

        mKVItem = Convert(pair, prevItem);
    }
}

BinaryKeyValueItemRef BinaryKeyValueItemIterator::Convert(const KeyValueRef &pair, BinaryKeyValueItemRef &reuseItem)
{
    auto binaryRowKey = pair->key;
    auto binaryValue = pair->value;

    auto stateId = binaryRowKey.StateId();
    auto stateTypeFromId = StateId::GetStateType(stateId);
    auto keyGroup = stateTypeFromId == PQ ? mKeyGroupUtil->ComputePQKeyGroupForKeyHash(binaryRowKey.KeyHashCode()) :
                                            mKeyGroupUtil->ComputeKeyGroupForKeyHash(binaryRowKey.KeyHashCode());

    auto description = mStateIdProvider->GetTableDescription(stateId);
    if (description == nullptr) {
        LOG_ERROR("Can't find table description for stateId:" << stateId << " in key group:" << keyGroup);
        return {};
    }

    auto item = (reuseItem != nullptr) ? reuseItem : std::make_shared<BinaryKeyValueItem>();
    auto stateType = description->GetStateType();
    item->SetStateName(description->GetTableName());
    item->SetStateType(stateType);
    item->SetKeyGroup(keyGroup);
    item->SetKeyValue(pair);

    BuildKey(binaryRowKey, description->GetTableName(), stateType, item);
    item->SetValue(const_cast<uint8_t *>(binaryValue.ValueData()), binaryValue.ValueLen());
    return item;
}

void BinaryKeyValueItemIterator::BuildKey(const Key &key, const std::string &stateName, const StateType &stateType,
                                          BinaryKeyValueItemRef &reuseItem)
{
    std::pair<std::pair<uint8_t *, uint32_t>, std::pair<uint8_t *, uint32_t>> pair;
    auto primaryKey = key.PriKey();
    uint32_t keyLen = primaryKey.RealKeyLen();
    switch (stateType) {
        case StateType::PQ:
        case StateType::VALUE:
        case StateType::LIST:
        case StateType::MAP:
        case StateType::SUB_VALUE:
            reuseItem->SetKey(const_cast<uint8_t *>(primaryKey.KeyData()), primaryKey.KeyLen());
            reuseItem->SetNs(nullptr, NO_0);
            break;
        case StateType::SUB_LIST:
        case StateType::SUB_MAP:
            reuseItem->SetKey(const_cast<uint8_t *>(primaryKey.RealKeyData()), keyLen);
            reuseItem->SetNs(const_cast<uint8_t *>(primaryKey.KeyData()) + NO_4 + keyLen,
                             primaryKey.KeyLen() - NO_8 - keyLen);
            break;
        default:
            LOG_ERROR("Unknown state type:" << stateType << " with state name:" << stateName);
    }

    BuildSecondaryKey(key, stateName, stateType, reuseItem);
}

void BinaryKeyValueItemIterator::BuildSecondaryKey(const Key &key, const std::string &stateName,
                                                   const StateType &stateType, BinaryKeyValueItemRef &reuseItem) const
{
    auto secondaryKey = key.SecKey();
    if (stateType == MAP || stateType == SUB_MAP) {
        reuseItem->SetMapKey(const_cast<uint8_t *>(secondaryKey.KeyData()), secondaryKey.KeyLen());
    } else if (stateType == SUB_VALUE) {
        reuseItem->SetNs(const_cast<uint8_t *>(secondaryKey.KeyData()), secondaryKey.KeyLen());
        reuseItem->SetMapKey(nullptr, NO_0);
    } else {
        reuseItem->SetMapKey(nullptr, NO_0);
    }
}

}  // namespace bss
}  // namespace ock
